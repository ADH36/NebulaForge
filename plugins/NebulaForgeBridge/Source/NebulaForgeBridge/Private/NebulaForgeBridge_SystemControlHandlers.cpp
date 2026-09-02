#include "NebulaForgeBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "NebulaForgeBridgeHelpers.h"
#include "NebulaForgeBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Internationalization/Internationalization.h"
#include "Kismet/GameplayStatics.h"
#include "Components/Widget.h"
#include "Components/SlateWrapperTypes.h"
#include "Engine/UserInterfaceSettings.h"
#include "Misc/Guid.h"
#include "Misc/Base64.h"
#include "Misc/AutomationTest.h"
#include "Scalability.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

namespace {

bool BuildSaveGameSchemaInspection(
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FJsonObject> &OutResult,
    FString &OutError,
    FString &OutErrorCode) {
  FString ObjectPath;
  if (!Payload.IsValid() || !Payload->TryGetStringField(TEXT("saveGameObject"), ObjectPath) || ObjectPath.TrimStartAndEnd().IsEmpty()) {
    OutError = TEXT("saveGameObject is required");
    OutErrorCode = TEXT("INVALID_ARGUMENT");
    return false;
  }
  USaveGame *SaveGame = LoadObject<USaveGame>(nullptr, *ObjectPath);
  if (!SaveGame) {
    OutError = TEXT("saveGameObject must resolve to a loaded USaveGame object");
    OutErrorCode = TEXT("OBJECT_NOT_FOUND");
    return false;
  }

  UClass *SaveClass = SaveGame->GetClass();
  TArray<TSharedPtr<FJsonValue>> Properties;
  for (TFieldIterator<FProperty> It(SaveClass); It; ++It) {
    FProperty *Property = *It;
    if (!Property || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)) continue;
    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetStringField(TEXT("name"), Property->GetName());
    Entry->SetStringField(TEXT("cppType"), Property->GetCPPType());
    Entry->SetBoolField(TEXT("serialized"), Property->HasAnyPropertyFlags(CPF_SaveGame));
    Properties.Add(MakeShared<FJsonValueObject>(Entry));
  }

  OutResult = McpHandlerUtils::CreateResultObject();
  OutResult->SetStringField(TEXT("classPath"), SaveClass->GetPathName());
  OutResult->SetArrayField(TEXT("saveGameProperties"), Properties);
  if (FIntProperty *SchemaProperty = FindFProperty<FIntProperty>(SaveClass, TEXT("SaveSchemaVersion"))) {
    OutResult->SetBoolField(TEXT("hasSchemaVersion"), true);
    OutResult->SetNumberField(TEXT("schemaVersion"), SchemaProperty->GetPropertyValue_InContainer(SaveGame));
  } else {
    OutResult->SetBoolField(TEXT("hasSchemaVersion"), false);
  }
  return true;
}

} // namespace

#if WITH_EDITOR
#include "Editor/UnrealEd/Public/Editor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/MonitoredProcess.h"
#include "Logging/MessageLog.h"
#include "EditorAssetLibrary.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "GameFramework/Actor.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/Material.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceConstant.h"
#include "GameplayTagsManager.h"
#include "Exporters/Exporter.h"
#include "IPythonScriptPlugin.h"
#include "Misc/FileHelper.h"
#include "Subsystems/EngineSubsystem.h"
#include "EditorSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Subsystems/Subsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/LatentActionManager.h"
#include "LatentActions.h"
#include "Tickable.h"
#include "UObject/UObjectIterator.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

#if WITH_EDITOR
namespace {

void SendManagedLifecycleEvent(UNebulaForgeBridgeSubsystem *Owner,
                               const FString &EventName,
                               const FString &OperationId,
                               const TSharedPtr<FJsonObject> &Data) {
  if (!Owner) return;
  TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
  Event->SetStringField(TEXT("type"), TEXT("automation_event"));
  Event->SetStringField(TEXT("event"), EventName);
  Event->SetStringField(TEXT("operationId"), OperationId);
  if (Data.IsValid()) Event->SetObjectField(TEXT("result"), Data.ToSharedRef());

  FString Serialized;
  const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
  FJsonSerializer::Serialize(Event, Writer);
  Owner->SendRawMessage(Serialized);
}

class FMcpBridgeLatentAction final : public FPendingLatentAction {
public:
  FMcpBridgeLatentAction(UNebulaForgeBridgeSubsystem *InOwner,
                         const FString &InLatentId, float InDuration,
                         const FLatentActionInfo &InLatentInfo)
      : Owner(InOwner), LatentId(InLatentId), Remaining(InDuration),
        CallbackTarget(InLatentInfo.CallbackTarget),
        ExecutionFunction(InLatentInfo.ExecutionFunction),
        Linkage(InLatentInfo.Linkage) {}

  void Cancel() { bCancelled = true; }

  FString GetDescription() const override {
    return FString::Printf(TEXT("MCP latent action '%s': %.3f seconds remaining"),
                           *LatentId, FMath::Max(0.0f, Remaining));
  }

  void UpdateOperation(FLatentResponse &Response) override {
    if (!bCancelled) Remaining -= Response.ElapsedTime();
    const bool bDone = bCancelled || Remaining <= 0.0f;
    if (bDone && !bReported) {
      bReported = true;
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("latentId"), LatentId);
      Result->SetBoolField(TEXT("cancelled"), bCancelled);
      SendManagedLifecycleEvent(Owner.Get(),
                                bCancelled ? TEXT("latent_action_cancelled")
                                           : TEXT("latent_action_completed"),
                                LatentId, Result);
    }

    if (bDone && !bCancelled && CallbackTarget.IsValid() &&
        !ExecutionFunction.IsNone()) {
      Response.FinishAndTriggerIf(true, ExecutionFunction, Linkage,
                                  CallbackTarget);
    } else {
      Response.DoneIf(bDone);
    }
  }

private:
  TWeakObjectPtr<UNebulaForgeBridgeSubsystem> Owner;
  FString LatentId;
  float Remaining = 0.0f;
  FWeakObjectPtr CallbackTarget;
  FName ExecutionFunction;
  int32 Linkage = 0;
  bool bCancelled = false;
  bool bReported = false;
};

} // namespace
#endif

#if !WITH_EDITOR
namespace {
void SendManagedLifecycleEvent(UNebulaForgeBridgeSubsystem *Owner,
                               const FString &EventName,
                               const FString &OperationId,
                               const TSharedPtr<FJsonObject> &Data) {
  if (!Owner) return;
  TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
  Event->SetStringField(TEXT("type"), TEXT("automation_event"));
  Event->SetStringField(TEXT("event"), EventName);
  Event->SetStringField(TEXT("operationId"), OperationId);
  if (Data.IsValid()) Event->SetObjectField(TEXT("result"), Data.ToSharedRef());
  FString Serialized;
  const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
  FJsonSerializer::Serialize(Event, Writer);
  Owner->SendRawMessage(Serialized);
}

void CompleteRuntimeSaveGame(
    const TWeakObjectPtr<UNebulaForgeBridgeSubsystem> &WeakOwner,
    const FString &AsyncId, const TSharedPtr<FMcpAsyncState> &State,
    const FString &EventName, bool bSucceeded,
    const TSharedPtr<FJsonObject> &Result,
    const TSharedRef<FTSTicker::FDelegateHandle> &TickerHandle,
    bool bTimedOut = false) {
  if (!State.IsValid() || State->bCompleted.exchange(true)) return;
  const bool bCancelled = State->bCancelled.load();
  State->bTimedOut.store(bTimedOut);
  State->bSucceeded.store(bSucceeded && !bCancelled && !bTimedOut);
  FTSTicker::GetCoreTicker().RemoveTicker(*TickerHandle);
  if (UNebulaForgeBridgeSubsystem *Owner = WeakOwner.Get()) {
    TSharedPtr<FJsonObject> EventResult = Result.IsValid() ? Result : McpHandlerUtils::CreateResultObject();
    EventResult->SetStringField(TEXT("asyncId"), AsyncId);
    EventResult->SetBoolField(TEXT("succeeded"), bSucceeded && !bCancelled && !bTimedOut);
    EventResult->SetBoolField(TEXT("cancelled"), bCancelled);
    EventResult->SetBoolField(TEXT("timedOut"), bTimedOut);
    EventResult->SetStringField(TEXT("state"), bCancelled ? TEXT("cancelled") : (bTimedOut ? TEXT("timed_out") : (bSucceeded ? TEXT("completed") : TEXT("failed"))));
    SendManagedLifecycleEvent(Owner, EventName, AsyncId, EventResult);
  }
}

void StartRuntimeSaveGameTimeout(
    const TWeakObjectPtr<UNebulaForgeBridgeSubsystem> &WeakOwner,
    const FString &AsyncId, const TSharedPtr<FMcpAsyncState> &State,
    const FString &EventName, double TimeoutSeconds,
    const TSharedRef<FTSTicker::FDelegateHandle> &TickerHandle) {
  const double StartedAt = FPlatformTime::Seconds();
  *TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([WeakOwner, AsyncId, State, EventName, TimeoutSeconds, StartedAt, TickerHandle](float) {
        if (!State.IsValid() || State->bCompleted.load()) return false;
        if (FPlatformTime::Seconds() - StartedAt >= TimeoutSeconds) {
          CompleteRuntimeSaveGame(WeakOwner, AsyncId, State, EventName, false, nullptr, TickerHandle, true);
          return false;
        }
        return true;
      }), 0.1f);
}
}
#endif

#if WITH_EDITOR
FString UNebulaForgeBridgeSubsystem::BeginManagedAsyncAction(const FString &Execution,
                                                             const FString &Label) {
  if (!CanRegisterManagedAsyncAction()) return FString();
  const FString AsyncId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
  FMcpAsyncRecord Record;
  Record.AsyncId = AsyncId;
  Record.Execution = Execution;
  Record.Label = Label;
  Record.State = MakeShared<FMcpAsyncState>();
  ManagedAsyncActions.Add(AsyncId, Record);
  return AsyncId;
}

bool UNebulaForgeBridgeSubsystem::IsManagedAsyncActionCancelled(const FString &AsyncId) const {
  const FMcpAsyncRecord *Record = ManagedAsyncActions.Find(AsyncId);
  return Record && Record->State.IsValid() && Record->State->bCancelled.load();
}

void UNebulaForgeBridgeSubsystem::CompleteManagedAsyncAction(
    const FString &AsyncId, bool bSucceeded, const FString &EventName,
    const TSharedPtr<FJsonObject> &Result) {
  FMcpAsyncRecord *Record = ManagedAsyncActions.Find(AsyncId);
  if (!Record || !Record->State.IsValid()) return;
  // Completion can race a cancellation/timeout callback or a late engine
  // delegate. Only the first terminal transition may publish an event.
  if (Record->State->bCompleted.exchange(true)) return;
  const bool bCancelled = Record->State->bCancelled.load();
  Record->State->bSucceeded.store(bSucceeded && !bCancelled);
  TSharedPtr<FJsonObject> EventResult = Result.IsValid() ? Result : McpHandlerUtils::CreateResultObject();
  EventResult->SetStringField(TEXT("asyncId"), AsyncId);
  EventResult->SetBoolField(TEXT("succeeded"), bSucceeded && !bCancelled);
  EventResult->SetBoolField(TEXT("cancelled"), bCancelled);
  EventResult->SetStringField(TEXT("state"), bCancelled ? TEXT("cancelled") : (bSucceeded ? TEXT("completed") : TEXT("failed")));
  SendManagedLifecycleEvent(this, EventName, AsyncId, EventResult);
}
#endif

#if !WITH_EDITOR
bool UNebulaForgeBridgeSubsystem::HandleRuntimeSaveGameAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("System control payload missing"), TEXT("INVALID_PAYLOAD"));
    return true;
  }
  FString SlotName;
  Payload->TryGetStringField(TEXT("slotName"), SlotName);
  SlotName.TrimStartAndEndInline();
  int32 UserIndex = 0;
  Payload->TryGetNumberField(TEXT("userIndex"), UserIndex);
  if (UserIndex < 0 || UserIndex > 7) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("userIndex must be between 0 and 7"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (Lower != TEXT("list_save_game_slots") && SlotName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("slotName is required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  for (const TCHAR Character : SlotName) {
    if (!(FChar::IsAlnum(Character) || Character == TCHAR('_') || Character == TCHAR('-'))) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("slotName contains unsupported characters"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  if (Lower == TEXT("save_data_to_slot")) {
    FString EncodedData;
    Payload->TryGetStringField(TEXT("dataBase64"), EncodedData);
    TArray<uint8> SaveData;
    constexpr int32 MaxSaveGameMemoryBytes = 8 * 1024 * 1024;
    if (EncodedData.IsEmpty() || !FBase64::Decode(EncodedData, SaveData) || SaveData.Num() == 0) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("dataBase64 must contain valid non-empty Base64 data"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (SaveData.Num() > MaxSaveGameMemoryBytes) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Save data exceeds the 8 MiB transport limit"), TEXT("PAYLOAD_TOO_LARGE"));
      return true;
    }
    const bool bSaved = UGameplayStatics::SaveDataToSlot(SaveData, SlotName, UserIndex);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("slotName"), SlotName);
    Result->SetNumberField(TEXT("userIndex"), UserIndex);
    Result->SetNumberField(TEXT("byteLength"), SaveData.Num());
    Result->SetBoolField(TEXT("saved"), bSaved);
    SendAutomationResponse(RequestingSocket, RequestId, bSaved, bSaved ? TEXT("Raw SaveGame data written") : TEXT("Raw SaveGame data write failed"), Result, bSaved ? FString() : TEXT("SAVE_DATA_FAILED"));
    return true;
  }
  if (Lower == TEXT("load_data_from_slot")) {
    TArray<uint8> SaveData;
    constexpr int32 MaxSaveGameMemoryBytes = 8 * 1024 * 1024;
    const bool bLoaded = UGameplayStatics::LoadDataFromSlot(SaveData, SlotName, UserIndex);
    if (bLoaded && SaveData.Num() > MaxSaveGameMemoryBytes) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Loaded save data exceeds the 8 MiB transport limit"), TEXT("PAYLOAD_TOO_LARGE"));
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("slotName"), SlotName);
    Result->SetNumberField(TEXT("userIndex"), UserIndex);
    Result->SetNumberField(TEXT("byteLength"), SaveData.Num());
    Result->SetBoolField(TEXT("loaded"), bLoaded);
    if (bLoaded) Result->SetStringField(TEXT("dataBase64"), FBase64::Encode(SaveData));
    SendAutomationResponse(RequestingSocket, RequestId, bLoaded, bLoaded ? TEXT("Raw SaveGame data loaded") : TEXT("Raw SaveGame data not found"), Result, bLoaded ? FString() : TEXT("SLOT_NOT_FOUND"));
    return true;
  }

  if (Lower == TEXT("save_game_to_slot")) {
    FString ObjectPath;
    Payload->TryGetStringField(TEXT("saveGameObject"), ObjectPath);
    USaveGame *SaveGame = LoadObject<USaveGame>(nullptr, *ObjectPath);
    if (!SaveGame) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("saveGameObject must resolve to a loaded USaveGame object"), TEXT("OBJECT_NOT_FOUND"));
      return true;
    }
    bool bAsync = false;
    Payload->TryGetBoolField(TEXT("async"), bAsync);
    if (bAsync) {
      if (!CanRegisterManagedAsyncAction()) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Too many active or recently completed async actions"), TEXT("ASYNC_ACTION_CAPACITY"));
        return true;
      }
      const FString AsyncId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
      const TSharedPtr<FMcpAsyncState> State = MakeShared<FMcpAsyncState>();
      const double TimeoutSeconds = FMath::Clamp(GetJsonNumberField(Payload, TEXT("timeoutMs"), 30000.0) / 1000.0, 1.0, 600.0);
      const TSharedRef<FTSTicker::FDelegateHandle> TickerHandle = MakeShared<FTSTicker::FDelegateHandle>();
      FMcpAsyncRecord Record;
      Record.AsyncId = AsyncId;
      Record.Execution = TEXT("save_game");
      Record.Label = FString::Printf(TEXT("SaveGame:%s"), *SlotName);
      Record.State = State;
      ManagedAsyncActions.Add(AsyncId, Record);
      const TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
      StartRuntimeSaveGameTimeout(WeakThis, AsyncId, State, TEXT("save_game_completed"), TimeoutSeconds, TickerHandle);
      UGameplayStatics::AsyncSaveGameToSlot(SaveGame, SlotName, UserIndex,
        FAsyncSaveGameToSlotDelegate::CreateLambda([WeakThis, AsyncId, State, TickerHandle](const FString&, const int32, bool bSuccess) {
          CompleteRuntimeSaveGame(WeakThis, AsyncId, State, TEXT("save_game_completed"), bSuccess, nullptr, TickerHandle);
        }));
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("asyncId"), AsyncId);
      Result->SetStringField(TEXT("slotName"), SlotName);
      Result->SetNumberField(TEXT("userIndex"), UserIndex);
      Result->SetStringField(TEXT("state"), TEXT("running"));
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SaveGame write started"), Result, FString());
      return true;
    }
    const bool bSaved = UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, UserIndex);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("slotName"), SlotName);
    Result->SetNumberField(TEXT("userIndex"), UserIndex);
    Result->SetBoolField(TEXT("saved"), bSaved);
    SendAutomationResponse(RequestingSocket, RequestId, bSaved, bSaved ? TEXT("SaveGame slot written") : TEXT("SaveGame slot write failed"), Result, bSaved ? FString() : TEXT("SAVE_FAILED"));
    return true;
  }
  if (Lower == TEXT("load_game_from_slot")) {
    bool bAsync = false;
    Payload->TryGetBoolField(TEXT("async"), bAsync);
    if (bAsync) {
      if (!CanRegisterManagedAsyncAction()) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Too many active or recently completed async actions"), TEXT("ASYNC_ACTION_CAPACITY"));
        return true;
      }
      const FString AsyncId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
      const TSharedPtr<FMcpAsyncState> State = MakeShared<FMcpAsyncState>();
      const double TimeoutSeconds = FMath::Clamp(GetJsonNumberField(Payload, TEXT("timeoutMs"), 30000.0) / 1000.0, 1.0, 600.0);
      const TSharedRef<FTSTicker::FDelegateHandle> TickerHandle = MakeShared<FTSTicker::FDelegateHandle>();
      FMcpAsyncRecord Record;
      Record.AsyncId = AsyncId;
      Record.Execution = TEXT("load_game");
      Record.Label = FString::Printf(TEXT("LoadGame:%s"), *SlotName);
      Record.State = State;
      ManagedAsyncActions.Add(AsyncId, Record);
      const TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
      StartRuntimeSaveGameTimeout(WeakThis, AsyncId, State, TEXT("load_game_completed"), TimeoutSeconds, TickerHandle);
      UGameplayStatics::AsyncLoadGameFromSlot(SlotName, UserIndex,
        FAsyncLoadGameFromSlotDelegate::CreateLambda([WeakThis, AsyncId, State, TickerHandle](const FString&, const int32, USaveGame *LoadedGame) {
          const bool bSuccess = LoadedGame != nullptr;
          TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
          if (LoadedGame) Result->SetStringField(TEXT("classPath"), LoadedGame->GetClass()->GetPathName());
          CompleteRuntimeSaveGame(WeakThis, AsyncId, State, TEXT("load_game_completed"), bSuccess, Result, TickerHandle);
        }));
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("asyncId"), AsyncId);
      Result->SetStringField(TEXT("slotName"), SlotName);
      Result->SetNumberField(TEXT("userIndex"), UserIndex);
      Result->SetStringField(TEXT("state"), TEXT("running"));
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SaveGame load started"), Result, FString());
      return true;
    }
    USaveGame *Loaded = UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("slotName"), SlotName);
    Result->SetNumberField(TEXT("userIndex"), UserIndex);
    Result->SetBoolField(TEXT("exists"), Loaded != nullptr);
    if (Loaded) Result->SetStringField(TEXT("classPath"), Loaded->GetClass()->GetPathName());
    SendAutomationResponse(RequestingSocket, RequestId, Loaded != nullptr, Loaded ? TEXT("SaveGame slot loaded") : TEXT("SaveGame slot not found"), Result, Loaded ? FString() : TEXT("SLOT_NOT_FOUND"));
    return true;
  }
  if (Lower == TEXT("delete_save_game_slot")) {
    const bool bDeleted = UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("slotName"), SlotName);
    Result->SetNumberField(TEXT("userIndex"), UserIndex);
    Result->SetBoolField(TEXT("deleted"), bDeleted);
    SendAutomationResponse(RequestingSocket, RequestId, bDeleted, bDeleted ? TEXT("SaveGame slot deleted") : TEXT("SaveGame slot delete failed"), Result, bDeleted ? FString() : TEXT("DELETE_FAILED"));
    return true;
  }
  if (Lower == TEXT("check_save_game_slot")) {
    const bool bExists = UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("slotName"), SlotName);
    Result->SetNumberField(TEXT("userIndex"), UserIndex);
    Result->SetBoolField(TEXT("exists"), bExists);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SaveGame slot checked"), Result, FString());
    return true;
  }
  // The default UE save-game provider stores slots under Saved/SaveGames. This
  // gives packaged games truthful local-slot discovery without pretending that
  // platform cloud providers expose a universal enumeration API.
  TArray<FString> SlotFiles;
  const FString SaveDirectory = FPaths::ProjectSavedDir() / TEXT("SaveGames/");
  IFileManager::Get().FindFiles(SlotFiles, *(SaveDirectory / TEXT("*.sav")), true, false);
  SlotFiles.Sort();
  TArray<TSharedPtr<FJsonValue>> Slots;
  for (const FString& FileName : SlotFiles)
  {
    const FString Slot = FPaths::GetBaseFilename(FileName);
    if (Slot.IsEmpty())
    {
      continue;
    }
    TSharedPtr<FJsonObject> SlotObject = McpHandlerUtils::CreateResultObject();
    SlotObject->SetStringField(TEXT("slotName"), Slot);
    SlotObject->SetNumberField(TEXT("userIndex"), UserIndex);
    SlotObject->SetBoolField(TEXT("exists"), UGameplayStatics::DoesSaveGameExist(Slot, UserIndex));
    SlotObject->SetNumberField(TEXT("sizeBytes"), IFileManager::Get().FileSize(*(SaveDirectory / FileName)));
    Slots.Add(MakeShared<FJsonValueObject>(SlotObject));
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetArrayField(TEXT("slots"), Slots);
  Result->SetNumberField(TEXT("count"), Slots.Num());
  Result->SetNumberField(TEXT("userIndex"), UserIndex);
  Result->SetStringField(TEXT("storage"), TEXT("local_filesystem"));
  Result->SetStringField(TEXT("directory"), SaveDirectory);
  Result->SetBoolField(TEXT("providerEnumerationSupported"), false);
  Result->SetStringField(TEXT("providerNote"), TEXT("Platform cloud/provider slots are not enumerable through a universal UE API; use check_save_game_slot for a specific slot."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Local SaveGame slots listed"), Result, FString());
  return true;
}
#endif

// Subsystem actions resolve Unreal-managed lifetime scopes. They intentionally
// do not construct detached UObject instances, which would bypass subsystem
// collection initialization and deinitialization.

bool UNebulaForgeBridgeSubsystem::HandleSubsystemAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Subsystem payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  auto SendInvalid = [&](const FString &Message) {
    SendAutomationResponse(RequestingSocket, RequestId, false, Message,
                            nullptr, TEXT("INVALID_ARGUMENT"));
  };
  auto ResolveWorld = [&]() -> UWorld * {
    FString WorldContext;
    Payload->TryGetStringField(TEXT("worldContext"), WorldContext);
    WorldContext = WorldContext.TrimStartAndEnd().ToLower();
    if (WorldContext == TEXT("pie") || WorldContext == TEXT("play")) {
      return GEditor && GEditor->PlayWorld ? GEditor->PlayWorld.Get() : nullptr;
    }
    if (WorldContext == TEXT("editor")) {
      return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    }
    if (GEditor && GEditor->PlayWorld) return GEditor->PlayWorld.Get();
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  };
  auto ScopeForClass = [](UClass *Class) -> FString {
    if (!Class) return TEXT("unknown");
    if (Class->IsChildOf(UEngineSubsystem::StaticClass())) return TEXT("engine");
    if (Class->IsChildOf(UGameInstanceSubsystem::StaticClass())) return TEXT("game_instance");
    if (Class->IsChildOf(UWorldSubsystem::StaticClass())) return TEXT("world");
    if (Class->IsChildOf(ULocalPlayerSubsystem::StaticClass())) return TEXT("local_player");
    if (Class->IsChildOf(UEditorSubsystem::StaticClass())) return TEXT("editor");
    return TEXT("unknown");
  };
  auto ScopeForObject = [&](USubsystem *Subsystem) -> FString {
    return Subsystem ? ScopeForClass(Subsystem->GetClass()) : TEXT("unknown");
  };
  auto TickTypeName = [](ETickableTickType Type) -> FString {
    switch (Type) {
    case ETickableTickType::Always: return TEXT("always");
    case ETickableTickType::Never: return TEXT("never");
    case ETickableTickType::Conditional: return TEXT("conditional");
    default: return TEXT("new_object");
    }
  };
  auto AddSubsystemData = [&](USubsystem *Subsystem,
                              const FString &Scope,
                              TSharedPtr<FJsonObject> &Result) {
    Result->SetStringField(TEXT("subsystemClass"), Subsystem->GetClass()->GetPathName());
    Result->SetStringField(TEXT("subsystemName"), Subsystem->GetClass()->GetName());
    Result->SetStringField(TEXT("objectPath"), Subsystem->GetPathName());
    Result->SetStringField(TEXT("scope"), Scope);
    Result->SetStringField(TEXT("outerPath"), Subsystem->GetOuter() ? Subsystem->GetOuter()->GetPathName() : TEXT(""));
    Result->SetBoolField(TEXT("managedLifecycle"), true);
    if (UTickableWorldSubsystem *Tickable = Cast<UTickableWorldSubsystem>(Subsystem)) {
      Result->SetBoolField(TEXT("tickable"), true);
      Result->SetBoolField(TEXT("isInitialized"), Tickable->IsInitialized());
      Result->SetBoolField(TEXT("isAllowedToTick"), Tickable->IsAllowedToTick());
      Result->SetBoolField(TEXT("isTickable"), Tickable->IsTickable());
      Result->SetStringField(TEXT("tickType"), TickTypeName(Tickable->GetTickableTickType()));
    } else {
      Result->SetBoolField(TEXT("tickable"), false);
      Result->SetStringField(TEXT("tickType"), TEXT("not_exposed"));
    }
  };

  if (Action == TEXT("list_subsystems")) {
    FString ScopeFilter;
    Payload->TryGetStringField(TEXT("subsystemScope"), ScopeFilter);
    ScopeFilter = ScopeFilter.TrimStartAndEnd().ToLower();
    if (ScopeFilter == TEXT("gameinstance")) ScopeFilter = TEXT("game_instance");
    if (ScopeFilter == TEXT("localplayer")) ScopeFilter = TEXT("local_player");
    FString ClassFilter;
    Payload->TryGetStringField(TEXT("subsystemClass"), ClassFilter);
    if (ClassFilter.IsEmpty()) Payload->TryGetStringField(TEXT("subsystemName"), ClassFilter);
    UClass *FilterClass = ClassFilter.IsEmpty() ? nullptr : ResolveClassByName(ClassFilter);

    TArray<TSharedPtr<FJsonValue>> Items;
    for (TObjectIterator<USubsystem> It; It; ++It) {
      USubsystem *Subsystem = *It;
      if (!Subsystem || Subsystem->HasAnyFlags(RF_ClassDefaultObject)) continue;
      const FString Scope = ScopeForObject(Subsystem);
      if (!ScopeFilter.IsEmpty() && Scope != ScopeFilter) continue;
      if (FilterClass && !Subsystem->GetClass()->IsChildOf(FilterClass)) continue;
      TSharedPtr<FJsonObject> Item = McpHandlerUtils::CreateResultObject();
      AddSubsystemData(Subsystem, Scope, Item);
      Items.Add(MakeShared<FJsonValueObject>(Item));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("subsystems"), Items);
    Result->SetNumberField(TEXT("count"), Items.Num());
    if (!ScopeFilter.IsEmpty()) Result->SetStringField(TEXT("scope"), ScopeFilter);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Subsystems listed"), Result, FString());
    return true;
  }

  FString ClassName;
  Payload->TryGetStringField(TEXT("subsystemClass"), ClassName);
  if (ClassName.IsEmpty()) Payload->TryGetStringField(TEXT("subsystemName"), ClassName);
  if (ClassName.IsEmpty()) {
    SendInvalid(TEXT("subsystemClass or subsystemName is required."));
    return true;
  }
  UClass *SubsystemClass = ResolveClassByName(ClassName);
  if (!SubsystemClass || !SubsystemClass->IsChildOf(USubsystem::StaticClass())) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Subsystem class not found or is not a USubsystem"),
                           nullptr, TEXT("SUBSYSTEM_CLASS_NOT_FOUND"));
    return true;
  }

  FString RequestedScope;
  Payload->TryGetStringField(TEXT("subsystemScope"), RequestedScope);
  RequestedScope = RequestedScope.TrimStartAndEnd().ToLower();
  if (RequestedScope == TEXT("gameinstance")) RequestedScope = TEXT("game_instance");
  if (RequestedScope == TEXT("localplayer")) RequestedScope = TEXT("local_player");
  const FString InferredScope = ScopeForClass(SubsystemClass);
  const FString Scope = RequestedScope.IsEmpty() ? InferredScope : RequestedScope;
  if (Scope != InferredScope) {
    SendInvalid(FString::Printf(TEXT("subsystemScope '%s' does not match class scope '%s'."),
                                *Scope, *InferredScope));
    return true;
  }

  USubsystem *Subsystem = nullptr;
  if (Scope == TEXT("engine")) {
    if (!GEngine) { SendInvalid(TEXT("Engine is unavailable.")); return true; }
    Subsystem = GEngine->GetEngineSubsystemBase(SubsystemClass);
  } else if (Scope == TEXT("editor")) {
    if (!GEditor) { SendInvalid(TEXT("Editor is unavailable.")); return true; }
    Subsystem = GEditor->GetEditorSubsystemBase(SubsystemClass);
  } else {
    UWorld *World = ResolveWorld();
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("World context is unavailable"), nullptr,
                             TEXT("WORLD_NOT_FOUND"));
      return true;
    }
    UGameInstance *GameInstance = World->GetGameInstance();
    if (Scope == TEXT("game_instance")) {
      if (!GameInstance) { SendInvalid(TEXT("Game instance is unavailable.")); return true; }
      Subsystem = GameInstance->GetSubsystemBase(SubsystemClass);
    } else if (Scope == TEXT("world")) {
      Subsystem = World->GetSubsystemBase(SubsystemClass);
    } else if (Scope == TEXT("local_player")) {
      double PlayerNumber = 0.0;
      Payload->TryGetNumberField(TEXT("playerIndex"), PlayerNumber);
      const int32 PlayerIndex = FMath::Max(0, FMath::RoundToInt(PlayerNumber));
      ULocalPlayer *LocalPlayer = GameInstance ? GameInstance->GetLocalPlayerByIndex(PlayerIndex) : nullptr;
      if (!LocalPlayer) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Local player context is unavailable"), nullptr,
                               TEXT("LOCAL_PLAYER_NOT_FOUND"));
        return true;
      }
      Subsystem = LocalPlayer->GetSubsystemBase(SubsystemClass);
    }
  }

  if (!Subsystem) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Subsystem is not initialized in the requested context"),
                           nullptr, TEXT("SUBSYSTEM_NOT_FOUND"));
    return true;
  }

  if (Action == TEXT("configure_subsystem_tick")) {
    UTickableWorldSubsystem *Tickable = Cast<UTickableWorldSubsystem>(Subsystem);
    if (!Tickable) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Only UTickableWorldSubsystem exposes safe runtime tick configuration"),
                             nullptr, TEXT("TICK_NOT_SUPPORTED"));
      return true;
    }
    FString TickTypeValue;
    Payload->TryGetStringField(TEXT("tickType"), TickTypeValue);
    TickTypeValue = TickTypeValue.TrimStartAndEnd().ToLower();
    bool bTickEnabled = true;
    const bool bHasEnabled = Payload->TryGetBoolField(TEXT("tickEnabled"), bTickEnabled);
    if (TickTypeValue.IsEmpty() && !bHasEnabled) {
      SendInvalid(TEXT("tickType or tickEnabled is required."));
      return true;
    }
    ETickableTickType NewType = bTickEnabled ? ETickableTickType::Conditional : ETickableTickType::Never;
    if (!TickTypeValue.IsEmpty()) {
      if (TickTypeValue == TEXT("conditional")) NewType = ETickableTickType::Conditional;
      else if (TickTypeValue == TEXT("always")) NewType = ETickableTickType::Always;
      else if (TickTypeValue == TEXT("never")) NewType = ETickableTickType::Never;
      else { SendInvalid(TEXT("tickType must be conditional, always, or never.")); return true; }
    }
    if (bHasEnabled && !bTickEnabled) NewType = ETickableTickType::Never;
    Tickable->SetTickableTickType(NewType);
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  AddSubsystemData(Subsystem, Scope, Result);
  Result->SetStringField(TEXT("action"), Action);
  if (Action.StartsWith(TEXT("create_"))) Result->SetBoolField(TEXT("resolved"), true);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         Action == TEXT("configure_subsystem_tick")
                           ? TEXT("Subsystem tick configured")
                           : TEXT("Managed subsystem resolved"),
                         Result, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                       TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

void UNebulaForgeBridgeSubsystem::ShutdownManagedAsyncOperations() {
#if WITH_EDITOR
  for (TPair<FString, FMcpTimerRecord> &Pair : ManagedTimers) {
    FMcpTimerRecord &Record = Pair.Value;
    if (Record.World.IsValid()) {
      Record.World->GetTimerManager().ClearTimer(Record.Handle);
    }
  }
  ManagedTimers.Empty();

  for (const TPair<FString, FMcpLatentRecord> &Pair : ManagedLatentActions) {
    if (Pair.Value.World.IsValid()) {
      Pair.Value.World->GetLatentActionManager().RemoveActionsForObject(this);
    }
  }
  ManagedLatentActions.Empty();

  for (TPair<FString, FMcpAsyncRecord> &Pair : ManagedAsyncActions) {
    if (Pair.Value.State.IsValid()) Pair.Value.State->bCancelled.store(true);
  }
  ManagedAsyncActions.Empty();
  if (AutomationTestEndDelegateHandle.IsValid()) {
    FAutomationTestFramework::Get().OnTestEndEvent.Remove(AutomationTestEndDelegateHandle);
    AutomationTestEndDelegateHandle.Reset();
  }
  if (AutomationTestsCompleteDelegateHandle.IsValid()) {
    FAutomationTestFramework::Get().OnAfterAllTestsEvent.Remove(AutomationTestsCompleteDelegateHandle);
    AutomationTestsCompleteDelegateHandle.Reset();
  }
  ActiveAutomationTestAsyncId.Empty();

  for (TPair<FString, TObjectPtr<UMcpManagedGameplayTask>> &Pair : ManagedGameplayTasks) {
    if (Pair.Value) Pair.Value->EndTask();
  }
#endif
  ManagedGameplayTasks.Empty();
  ManagedGameplayTaskOwners.Empty();
  ManagedGameplayTaskPriorities.Empty();
  ManagedGameplayTaskAutoActivate.Empty();
}

bool UNebulaForgeBridgeSubsystem::CanRegisterManagedAsyncAction() {
  constexpr int32 MaxManagedAsyncActions = 256;
  const FDateTime Now = FDateTime::UtcNow();
  constexpr double RetentionSeconds = 10.0 * 60.0;
  for (auto It = ManagedAsyncActions.CreateIterator(); It; ++It) {
    const FMcpAsyncRecord &Record = It.Value();
    const bool bCompleted = Record.State.IsValid() && Record.State->bCompleted.load();
    if (bCompleted && (Now - Record.CreatedAt).GetTotalSeconds() >= RetentionSeconds) {
      It.RemoveCurrent();
    }
  }
  return ManagedAsyncActions.Num() < MaxManagedAsyncActions;
}

bool UNebulaForgeBridgeSubsystem::HandleAsyncTimerAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Async/timer payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  auto SendInvalid = [&](const FString &Message) {
    SendAutomationResponse(RequestingSocket, RequestId, false, Message,
                            nullptr, TEXT("INVALID_ARGUMENT"));
  };
  auto ResolveWorld = [&]() -> UWorld * {
    FString WorldContext;
    Payload->TryGetStringField(TEXT("worldContext"), WorldContext);
    WorldContext = WorldContext.TrimStartAndEnd().ToLower();
    if ((WorldContext == TEXT("pie") || WorldContext == TEXT("play")) &&
        GEditor && GEditor->PlayWorld) {
      return GEditor->PlayWorld.Get();
    }
    if (WorldContext == TEXT("editor") && GEditor) {
      return GEditor->GetEditorWorldContext().World();
    }
    if (GEditor && GEditor->PlayWorld) return GEditor->PlayWorld.Get();
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  };
  auto GetId = [&](const TCHAR *Field) -> FString {
    FString Value;
    Payload->TryGetStringField(Field, Value);
    Value.TrimStartAndEndInline();
    return Value;
  };
  auto ResolveCallback = [&](const FString &ObjectPath,
                             const FString &FunctionName,
                             TWeakObjectPtr<UObject> &OutObject,
                             FName &OutFunction) -> bool {
    if (ObjectPath.IsEmpty() != FunctionName.IsEmpty()) {
      SendInvalid(TEXT("callbackObject and callbackFunction must be provided together."));
      return false;
    }
    if (ObjectPath.IsEmpty()) return true;
    UObject *Object = FindObject<UObject>(nullptr, *ObjectPath);
    if (!Object) {
      SendInvalid(TEXT("callbackObject must reference a loaded UObject."));
      return false;
    }
    UFunction *Function = Object->FindFunction(FName(*FunctionName));
    if (!Function || Function->ParmsSize != 0) {
      SendInvalid(TEXT("callbackFunction must resolve to a zero-argument UFunction."));
      return false;
    }
    OutObject = Object;
    OutFunction = Function->GetFName();
    return true;
  };
  auto AddTimerData = [&](const FMcpTimerRecord &Record,
                          TSharedPtr<FJsonObject> &Result) {
    const bool bExists = Record.World.IsValid() &&
                         Record.World->GetTimerManager().TimerExists(Record.Handle);
    Result->SetStringField(TEXT("timerId"), Record.TimerId);
    Result->SetStringField(TEXT("world"), Record.World.IsValid()
                                               ? Record.World->GetPathName()
                                               : TEXT(""));
    Result->SetNumberField(TEXT("rate"), Record.Rate);
    Result->SetNumberField(TEXT("firstDelay"), Record.FirstDelay);
    Result->SetBoolField(TEXT("looping"), Record.bLooping);
    Result->SetNumberField(TEXT("fireCount"), Record.FireCount);
    Result->SetBoolField(TEXT("completed"), Record.bCompleted || !bExists);
    Result->SetBoolField(TEXT("exists"), bExists);
    if (bExists) {
      const FTimerManager &TimerManager = Record.World->GetTimerManager();
      Result->SetBoolField(TEXT("active"), TimerManager.IsTimerActive(Record.Handle));
      Result->SetBoolField(TEXT("paused"), TimerManager.IsTimerPaused(Record.Handle));
      Result->SetBoolField(TEXT("pending"), TimerManager.IsTimerPending(Record.Handle));
      Result->SetNumberField(TEXT("elapsed"), TimerManager.GetTimerElapsed(Record.Handle));
      Result->SetNumberField(TEXT("remaining"), TimerManager.GetTimerRemaining(Record.Handle));
    }
    if (!Record.CallbackObjectPath.IsEmpty()) {
      Result->SetStringField(TEXT("callbackObject"), Record.CallbackObjectPath);
      Result->SetStringField(TEXT("callbackFunction"), Record.CallbackFunction);
    }
  };

  if (Action == TEXT("set_timer")) {
    const FString TimerId = GetId(TEXT("timerId"));
    if (TimerId.IsEmpty()) { SendInvalid(TEXT("timerId is required.")); return true; }
    if (ManagedTimers.Contains(TimerId)) {
      SendInvalid(TEXT("timerId is already registered; clear it before reusing the id."));
      return true;
    }
    double RateValue = 0.0;
    if (!Payload->TryGetNumberField(TEXT("rate"), RateValue)) {
      Payload->TryGetNumberField(TEXT("duration"), RateValue);
    }
    if (RateValue <= 0.0 || RateValue > 86400.0) {
      SendInvalid(TEXT("rate or duration must be greater than 0 and at most 86400 seconds."));
      return true;
    }
    double FirstDelayValue = RateValue;
    Payload->TryGetNumberField(TEXT("firstDelay"), FirstDelayValue);
    if (FirstDelayValue < 0.0 || FirstDelayValue > 86400.0) {
      SendInvalid(TEXT("firstDelay must be between 0 and 86400 seconds."));
      return true;
    }
    bool bLooping = false;
    Payload->TryGetBoolField(TEXT("looping"), bLooping);
    const FString CallbackObjectPath = GetId(TEXT("callbackObject"));
    const FString CallbackFunction = GetId(TEXT("callbackFunction"));
    TWeakObjectPtr<UObject> CallbackObject;
    FName CallbackFunctionName;
    if (!ResolveCallback(CallbackObjectPath, CallbackFunction,
                         CallbackObject, CallbackFunctionName)) return true;
    UWorld *World = ResolveWorld();
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("World context is unavailable"), nullptr,
                             TEXT("WORLD_NOT_FOUND"));
      return true;
    }

    FMcpTimerRecord Record;
    Record.World = World;
    Record.TimerId = TimerId;
    Record.Rate = static_cast<float>(RateValue);
    Record.FirstDelay = static_cast<float>(FirstDelayValue);
    Record.bLooping = bLooping;
    Record.CallbackObjectPath = CallbackObjectPath;
    Record.CallbackFunction = CallbackFunction;
    TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
    FTimerDelegate Delegate = FTimerDelegate::CreateLambda(
        [WeakThis, TimerId, CallbackObject, CallbackFunctionName]() {
          UNebulaForgeBridgeSubsystem *Owner = WeakThis.Get();
          if (!Owner) return;
          FMcpTimerRecord *Found = Owner->ManagedTimers.Find(TimerId);
          if (!Found) return;
          ++Found->FireCount;
          Found->bCompleted = !Found->bLooping;
          if (CallbackObject.IsValid() && !CallbackFunctionName.IsNone()) {
            if (UFunction *Function = CallbackObject->FindFunction(CallbackFunctionName)) {
              CallbackObject->ProcessEvent(Function, nullptr);
            }
          }
          TSharedPtr<FJsonObject> EventResult = McpHandlerUtils::CreateResultObject();
          EventResult->SetStringField(TEXT("timerId"), TimerId);
          EventResult->SetNumberField(TEXT("fireCount"), Found->FireCount);
          EventResult->SetBoolField(TEXT("looping"), Found->bLooping);
          SendManagedLifecycleEvent(Owner, TEXT("timer_fired"), TimerId, EventResult);
        });
    World->GetTimerManager().SetTimer(Record.Handle, Delegate, Record.Rate,
                                      Record.bLooping, Record.FirstDelay);
    ManagedTimers.Add(TimerId, MoveTemp(Record));
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    AddTimerData(ManagedTimers[TimerId], Result);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Timer scheduled"), Result, FString());
    return true;
  }

  if (Action == TEXT("list_timers")) {
    TArray<TSharedPtr<FJsonValue>> Items;
    for (const TPair<FString, FMcpTimerRecord> &Pair : ManagedTimers) {
      TSharedPtr<FJsonObject> Item = McpHandlerUtils::CreateResultObject();
      AddTimerData(Pair.Value, Item);
      Items.Add(MakeShared<FJsonValueObject>(Item));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("timers"), Items);
    Result->SetNumberField(TEXT("count"), Items.Num());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Timers listed"), Result, FString());
    return true;
  }

  if (Action == TEXT("clear_timer") || Action == TEXT("pause_timer") ||
      Action == TEXT("resume_timer") || Action == TEXT("get_timer")) {
    const FString TimerId = GetId(TEXT("timerId"));
    FMcpTimerRecord *Record = ManagedTimers.Find(TimerId);
    if (TimerId.IsEmpty() || !Record) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Managed timer was not found"), nullptr,
                             TEXT("TIMER_NOT_FOUND"));
      return true;
    }
    if (!Record->World.IsValid()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Timer world is no longer valid"), nullptr,
                             TEXT("WORLD_NOT_FOUND"));
      return true;
    }
    FTimerManager &TimerManager = Record->World->GetTimerManager();
    if (Action == TEXT("clear_timer")) {
      TimerManager.ClearTimer(Record->Handle);
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("timerId"), TimerId);
      Result->SetBoolField(TEXT("cleared"), true);
      ManagedTimers.Remove(TimerId);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Timer cleared"), Result, FString());
      return true;
    }
    if (Action == TEXT("pause_timer")) TimerManager.PauseTimer(Record->Handle);
    if (Action == TEXT("resume_timer")) TimerManager.UnPauseTimer(Record->Handle);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    AddTimerData(*Record, Result);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           Action == TEXT("get_timer") ? TEXT("Timer inspected")
                                                        : TEXT("Timer updated"),
                           Result, FString());
    return true;
  }

  if (Action == TEXT("create_latent_action")) {
    const FString LatentId = GetId(TEXT("latentId"));
    if (LatentId.IsEmpty()) { SendInvalid(TEXT("latentId is required.")); return true; }
    if (ManagedLatentActions.Contains(LatentId)) {
      SendInvalid(TEXT("latentId is already registered.")); return true;
    }
    double DurationValue = 0.0;
    Payload->TryGetNumberField(TEXT("duration"), DurationValue);
    if (DurationValue < 0.0 || DurationValue > 86400.0) {
      SendInvalid(TEXT("duration must be between 0 and 86400 seconds.")); return true;
    }
    UWorld *World = ResolveWorld();
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("World context is unavailable"), nullptr,
                             TEXT("WORLD_NOT_FOUND"));
      return true;
    }
    static int32 NextLatentUUID = 1000000;
    double UUIDValue = 0.0;
    const bool bHasUUID = Payload->TryGetNumberField(TEXT("uuid"), UUIDValue);
    const int32 UUID = bHasUUID ? FMath::RoundToInt(UUIDValue) : NextLatentUUID++;
    if (UUID <= 0) { SendInvalid(TEXT("uuid must be a positive integer.")); return true; }
    if (World->GetLatentActionManager().FindExistingAction<FMcpBridgeLatentAction>(this, UUID)) {
      SendInvalid(TEXT("uuid is already registered for the bridge latent-action owner.")); return true;
    }
    const FString CallbackObjectPath = GetId(TEXT("callbackObject"));
    const FString CallbackFunction = GetId(TEXT("callbackFunction"));
    TWeakObjectPtr<UObject> CallbackObject;
    FName CallbackFunctionName;
    if (!ResolveCallback(CallbackObjectPath, CallbackFunction,
                         CallbackObject, CallbackFunctionName)) return true;
    FLatentActionInfo LatentInfo;
    LatentInfo.Linkage = 0;
    double LinkageValue = 0.0;
    if (Payload->TryGetNumberField(TEXT("linkage"), LinkageValue)) {
      LatentInfo.Linkage = FMath::RoundToInt(LinkageValue);
    }
    LatentInfo.ExecutionFunction = CallbackFunctionName;
    LatentInfo.CallbackTarget = CallbackObject.Get();
    World->GetLatentActionManager().AddNewAction(
        this, UUID, new FMcpBridgeLatentAction(this, LatentId,
                                               static_cast<float>(DurationValue),
                                               LatentInfo));
    FMcpLatentRecord Record;
    Record.World = World;
    Record.LatentId = LatentId;
    Record.UUID = UUID;
    Record.Duration = static_cast<float>(DurationValue);
    ManagedLatentActions.Add(LatentId, Record);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("latentId"), LatentId);
    Result->SetNumberField(TEXT("uuid"), UUID);
    Result->SetNumberField(TEXT("duration"), DurationValue);
    Result->SetBoolField(TEXT("managedByWorldLatentActionManager"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Latent action created"), Result, FString());
    return true;
  }

  if (Action == TEXT("list_latent_actions") || Action == TEXT("get_latent_action") ||
      Action == TEXT("clear_latent_action")) {
    const FString LatentId = GetId(TEXT("latentId"));
    if (Action != TEXT("list_latent_actions") && LatentId.IsEmpty()) {
      SendInvalid(TEXT("latentId is required.")); return true;
    }
    if (Action == TEXT("list_latent_actions")) {
      TArray<TSharedPtr<FJsonValue>> Items;
      for (const TPair<FString, FMcpLatentRecord> &Pair : ManagedLatentActions) {
        const FMcpLatentRecord &Record = Pair.Value;
        if (!Record.World.IsValid()) continue;
        TSharedPtr<FJsonObject> Item = McpHandlerUtils::CreateResultObject();
        Item->SetStringField(TEXT("latentId"), Record.LatentId);
        Item->SetNumberField(TEXT("uuid"), Record.UUID);
        Item->SetNumberField(TEXT("duration"), Record.Duration);
        Item->SetBoolField(TEXT("active"),
          Record.World->GetLatentActionManager().FindExistingAction<FMcpBridgeLatentAction>(this, Record.UUID) != nullptr);
        Item->SetStringField(TEXT("description"),
          Record.World->GetLatentActionManager().GetDescription(this, Record.UUID));
        Items.Add(MakeShared<FJsonValueObject>(Item));
      }
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetArrayField(TEXT("latentActions"), Items);
      Result->SetNumberField(TEXT("count"), Items.Num());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Latent actions listed"), Result, FString());
      return true;
    }
    FMcpLatentRecord *Record = ManagedLatentActions.Find(LatentId);
    if (!Record || !Record->World.IsValid()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Managed latent action was not found"), nullptr,
                             TEXT("LATENT_ACTION_NOT_FOUND"));
      return true;
    }
    FMcpBridgeLatentAction *LatentAction =
        Record->World->GetLatentActionManager().FindExistingAction<FMcpBridgeLatentAction>(this, Record->UUID);
    if (Action == TEXT("clear_latent_action")) {
      if (LatentAction) LatentAction->Cancel();
      ManagedLatentActions.Remove(LatentId);
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("latentId"), LatentId);
      Result->SetBoolField(TEXT("cancelled"), LatentAction != nullptr);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Latent action cancellation requested"), Result, FString());
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("latentId"), LatentId);
    Result->SetNumberField(TEXT("uuid"), Record->UUID);
    Result->SetNumberField(TEXT("duration"), Record->Duration);
    Result->SetBoolField(TEXT("active"), LatentAction != nullptr);
    Result->SetStringField(TEXT("description"),
                           Record->World->GetLatentActionManager().GetDescription(this, Record->UUID));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Latent action inspected"), Result, FString());
    return true;
  }

  if (Action == TEXT("create_async_action")) {
    const FString AsyncId = GetId(TEXT("asyncId"));
    if (AsyncId.IsEmpty()) { SendInvalid(TEXT("asyncId is required.")); return true; }
    if (ManagedAsyncActions.Contains(AsyncId)) {
      SendInvalid(TEXT("asyncId is already registered.")); return true;
    }
    if (!CanRegisterManagedAsyncAction()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Too many active or recently completed async actions"), TEXT("ASYNC_ACTION_CAPACITY"));
      return true;
    }
    double DurationValue = 0.0;
    Payload->TryGetNumberField(TEXT("duration"), DurationValue);
    if (DurationValue < 0.0 || DurationValue > 3600.0) {
      SendInvalid(TEXT("duration must be between 0 and 3600 seconds.")); return true;
    }
    FString Execution = GetId(TEXT("execution"));
    Execution = Execution.IsEmpty() ? TEXT("thread_pool") : Execution.ToLower();
    EAsyncExecution ExecutionMode = EAsyncExecution::ThreadPool;
    bool bSleepDuringWork = true;
    if (Execution == TEXT("task_graph")) { ExecutionMode = EAsyncExecution::TaskGraph; bSleepDuringWork = false; }
    else if (Execution == TEXT("task_graph_main_thread")) { ExecutionMode = EAsyncExecution::TaskGraphMainThread; bSleepDuringWork = false; }
    else if (Execution == TEXT("task_graph_main_tick")) { ExecutionMode = EAsyncExecution::TaskGraphMainTick; bSleepDuringWork = false; }
    else if (Execution == TEXT("thread")) ExecutionMode = EAsyncExecution::Thread;
    else if (Execution == TEXT("thread_if_fork_safe")) ExecutionMode = EAsyncExecution::ThreadIfForkSafe;
    else if (Execution == TEXT("thread_pool")) ExecutionMode = EAsyncExecution::ThreadPool;
    else if (Execution == TEXT("large_thread_pool")) ExecutionMode = EAsyncExecution::LargeThreadPool;
    else { SendInvalid(TEXT("execution must be task_graph, task_graph_main_thread, task_graph_main_tick, thread, thread_if_fork_safe, thread_pool, or large_thread_pool.")); return true; }
    FMcpAsyncRecord Record;
    Record.AsyncId = AsyncId;
    Record.Execution = Execution;
    Record.Label = GetId(TEXT("label"));
    Record.Duration = static_cast<float>(DurationValue);
    Record.State = MakeShared<FMcpAsyncState>();
    const TSharedPtr<FMcpAsyncState> State = Record.State;
    ManagedAsyncActions.Add(AsyncId, Record);
    TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
    Async(ExecutionMode, [State, Duration = static_cast<float>(DurationValue),
                          bSleepDuringWork, WeakThis, AsyncId]() {
      if (bSleepDuringWork) {
        const double EndTime = FPlatformTime::Seconds() + Duration;
        while (!State->bCancelled.load() && FPlatformTime::Seconds() < EndTime) {
          FPlatformProcess::Sleep(FMath::Min(0.01f, FMath::Max(0.0f, Duration)));
        }
      }
      State->bSucceeded.store(!State->bCancelled.load());
      State->bCompleted.store(true);
      AsyncTask(ENamedThreads::GameThread, [WeakThis, AsyncId, State]() {
        UNebulaForgeBridgeSubsystem *Owner = WeakThis.Get();
        if (!Owner) return;
        if (!Owner->ManagedAsyncActions.Contains(AsyncId)) return;
        TSharedPtr<FJsonObject> EventResult = McpHandlerUtils::CreateResultObject();
        EventResult->SetStringField(TEXT("asyncId"), AsyncId);
        EventResult->SetBoolField(TEXT("cancelled"), State->bCancelled.load());
        EventResult->SetBoolField(TEXT("succeeded"), State->bSucceeded.load());
        SendManagedLifecycleEvent(Owner, TEXT("async_action_completed"), AsyncId, EventResult);
      });
    });
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("asyncId"), AsyncId);
    Result->SetStringField(TEXT("execution"), Execution);
    Result->SetNumberField(TEXT("duration"), DurationValue);
    Result->SetStringField(TEXT("label"), Record.Label);
    Result->SetStringField(TEXT("state"), TEXT("running"));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Async action started"), Result, FString());
    return true;
  }

  if (Action == TEXT("list_async_actions") || Action == TEXT("get_async_action") ||
      Action == TEXT("cancel_async_action")) {
    const FString AsyncId = GetId(TEXT("asyncId"));
    if (Action != TEXT("list_async_actions") && AsyncId.IsEmpty()) {
      SendInvalid(TEXT("asyncId is required.")); return true;
    }
    auto AddAsyncData = [&](const FMcpAsyncRecord &Record,
                            TSharedPtr<FJsonObject> &Result) {
      const bool bComplete = Record.State.IsValid() && Record.State->bCompleted.load();
      const bool bCancelled = Record.State.IsValid() && Record.State->bCancelled.load();
      const bool bTimedOut = Record.State.IsValid() && Record.State->bTimedOut.load();
      Result->SetStringField(TEXT("asyncId"), Record.AsyncId);
      Result->SetStringField(TEXT("execution"), Record.Execution);
      Result->SetStringField(TEXT("label"), Record.Label);
      Result->SetNumberField(TEXT("duration"), Record.Duration);
      Result->SetStringField(TEXT("state"), bComplete ? (bCancelled ? TEXT("cancelled") : (bTimedOut ? TEXT("timed_out") : TEXT("completed"))) : TEXT("running"));
      Result->SetBoolField(TEXT("completed"), bComplete);
      Result->SetBoolField(TEXT("cancelled"), bCancelled);
      Result->SetBoolField(TEXT("timedOut"), bTimedOut);
      Result->SetBoolField(TEXT("succeeded"), Record.State.IsValid() && Record.State->bSucceeded.load());
    };
    if (Action == TEXT("list_async_actions")) {
      TArray<TSharedPtr<FJsonValue>> Items;
      for (const TPair<FString, FMcpAsyncRecord> &Pair : ManagedAsyncActions) {
        TSharedPtr<FJsonObject> Item = McpHandlerUtils::CreateResultObject();
        AddAsyncData(Pair.Value, Item);
        Items.Add(MakeShared<FJsonValueObject>(Item));
      }
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetArrayField(TEXT("asyncActions"), Items);
      Result->SetNumberField(TEXT("count"), Items.Num());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Async actions listed"), Result, FString());
      return true;
    }
    FMcpAsyncRecord *Record = ManagedAsyncActions.Find(AsyncId);
    if (!Record) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Managed async action was not found"), nullptr,
                             TEXT("ASYNC_ACTION_NOT_FOUND"));
      return true;
    }
    if (Action == TEXT("cancel_async_action")) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      AddAsyncData(*Record, Result);
      if (!Record->State.IsValid() || Record->State->bCompleted.load()) {
        Result->SetBoolField(TEXT("cancellationRequested"), false);
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Async action is already terminal"), Result,
                               TEXT("ASYNC_ACTION_TERMINAL"));
        return true;
      }
      Record->State->bCancelled.store(true);
      Result->SetBoolField(TEXT("cancellationRequested"), true);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Async action cancellation requested"), Result, FString());
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    AddAsyncData(*Record, Result);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Async action inspected"), Result, FString());
    return true;
  }

  auto AddGameplayTaskData = [&](UMcpManagedGameplayTask *Task,
                                 const FString &TaskId,
                                 TSharedPtr<FJsonObject> &Result) {
    Result->SetStringField(TEXT("taskId"), TaskId);
    if (!Task) { Result->SetStringField(TEXT("state"), TEXT("missing")); return; }
    Result->SetStringField(TEXT("taskClass"), Task->GetClass()->GetPathName());
    Result->SetStringField(TEXT("objectPath"), Task->GetPathName());
    Result->SetStringField(TEXT("state"), Task->GetTaskStateName());
    Result->SetBoolField(TEXT("active"), Task->IsActive());
    Result->SetBoolField(TEXT("finished"), Task->IsFinished());
    Result->SetNumberField(TEXT("priority"), Task->GetPriority());
    if (const TWeakObjectPtr<UObject> *Owner = ManagedGameplayTaskOwners.Find(TaskId)) {
      Result->SetStringField(TEXT("ownerObject"), Owner->IsValid() ? Owner->Get()->GetPathName() : TEXT(""));
    }
  };

  if (Action == TEXT("create_gameplay_task")) {
    const FString TaskId = GetId(TEXT("taskId"));
    const FString OwnerPath = GetId(TEXT("ownerObject"));
    if (TaskId.IsEmpty() || OwnerPath.IsEmpty()) {
      SendInvalid(TEXT("taskId and ownerObject are required.")); return true;
    }
    if (ManagedGameplayTasks.Contains(TaskId)) {
      SendInvalid(TEXT("taskId is already registered.")); return true;
    }
    FString TaskType = GetId(TEXT("taskType")).ToLower();
    if (TaskType.IsEmpty()) TaskType = TEXT("generic");
    if (TaskType != TEXT("generic")) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Only the managed generic gameplay task is supported; concrete task classes need their own factory API."), nullptr,
                             TEXT("TASK_TYPE_NOT_SUPPORTED"));
      return true;
    }
    UObject *OwnerObject = FindObject<UObject>(nullptr, *OwnerPath);
    if (!OwnerObject) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("ownerObject must reference a loaded UObject"), nullptr,
                             TEXT("TASK_OWNER_NOT_FOUND"));
      return true;
    }
    IGameplayTaskOwnerInterface *TaskOwner = Cast<IGameplayTaskOwnerInterface>(OwnerObject);
    if (!TaskOwner) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("ownerObject does not implement a gameplay-task owner interface"), nullptr,
                             TEXT("TASK_OWNER_INVALID"));
      return true;
    }
    double PriorityValue = 0.0;
    Payload->TryGetNumberField(TEXT("priority"), PriorityValue);
    if (PriorityValue < 0.0 || PriorityValue > 255.0) {
      SendInvalid(TEXT("priority must be between 0 and 255.")); return true;
    }
    bool bActivate = true;
    Payload->TryGetBoolField(TEXT("activate"), bActivate);
    FString InstanceNameValue = GetId(TEXT("instanceName"));
    if (InstanceNameValue.IsEmpty()) InstanceNameValue = TaskId;
    UMcpManagedGameplayTask *Task = NewObject<UMcpManagedGameplayTask>(OwnerObject);
    Task->InitializeForMcp(*TaskOwner, static_cast<uint8>(FMath::RoundToInt(PriorityValue)));
    if (bActivate) Task->ReadyForActivation();
    ManagedGameplayTasks.Add(TaskId, Task);
    ManagedGameplayTaskOwners.Add(TaskId, OwnerObject);
    ManagedGameplayTaskPriorities.Add(TaskId, static_cast<uint8>(FMath::RoundToInt(PriorityValue)));
    ManagedGameplayTaskAutoActivate.Add(TaskId, bActivate);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    AddGameplayTaskData(Task, TaskId, Result);
    Result->SetStringField(TEXT("instanceName"), InstanceNameValue);
    Result->SetBoolField(TEXT("managedLifecycle"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Gameplay task created"), Result, FString());
    return true;
  }

  if (Action == TEXT("list_gameplay_tasks") || Action == TEXT("get_gameplay_task") ||
      Action == TEXT("end_gameplay_task") || Action == TEXT("configure_task_priority")) {
    const FString TaskId = GetId(TEXT("taskId"));
    if (Action != TEXT("list_gameplay_tasks") && TaskId.IsEmpty()) {
      SendInvalid(TEXT("taskId is required.")); return true;
    }
    if (Action == TEXT("list_gameplay_tasks")) {
      TArray<TSharedPtr<FJsonValue>> Items;
      for (const TPair<FString, TObjectPtr<UMcpManagedGameplayTask>> &Pair : ManagedGameplayTasks) {
        TSharedPtr<FJsonObject> Item = McpHandlerUtils::CreateResultObject();
        AddGameplayTaskData(Pair.Value, Pair.Key, Item);
        Items.Add(MakeShared<FJsonValueObject>(Item));
      }
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetArrayField(TEXT("gameplayTasks"), Items);
      Result->SetNumberField(TEXT("count"), Items.Num());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Gameplay tasks listed"), Result, FString());
      return true;
    }
    TObjectPtr<UMcpManagedGameplayTask> *TaskPtr = ManagedGameplayTasks.Find(TaskId);
    if (!TaskPtr || !*TaskPtr) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Managed gameplay task was not found"), nullptr,
                             TEXT("GAMEPLAY_TASK_NOT_FOUND"));
      return true;
    }
    UMcpManagedGameplayTask *Task = *TaskPtr;
    if (Action == TEXT("end_gameplay_task")) {
      Task->EndTask();
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      AddGameplayTaskData(Task, TaskId, Result);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Gameplay task ended"), Result, FString());
      return true;
    }
    if (Action == TEXT("configure_task_priority")) {
      double PriorityValue = -1.0;
      if (!Payload->TryGetNumberField(TEXT("priority"), PriorityValue) ||
          PriorityValue < 0.0 || PriorityValue > 255.0) {
        SendInvalid(TEXT("priority between 0 and 255 is required.")); return true;
      }
      if (Task->IsActive()) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("End the active task before changing its priority; the managed task will be recreated."), nullptr,
                               TEXT("TASK_ACTIVE"));
        return true;
      }
      const TWeakObjectPtr<UObject> *OwnerObject = ManagedGameplayTaskOwners.Find(TaskId);
      if (!OwnerObject || !OwnerObject->IsValid()) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Gameplay task owner is no longer valid"), nullptr,
                               TEXT("TASK_OWNER_NOT_FOUND"));
        return true;
      }
      IGameplayTaskOwnerInterface *TaskOwner = Cast<IGameplayTaskOwnerInterface>(OwnerObject->Get());
      if (!TaskOwner) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Gameplay task owner is no longer usable"), nullptr,
                               TEXT("TASK_OWNER_INVALID"));
        return true;
      }
      const bool bActivate = ManagedGameplayTaskAutoActivate.FindRef(TaskId);
      Task->EndTask();
      UMcpManagedGameplayTask *Replacement = NewObject<UMcpManagedGameplayTask>(OwnerObject->Get());
      Replacement->InitializeForMcp(*TaskOwner, static_cast<uint8>(FMath::RoundToInt(PriorityValue)));
      if (bActivate) Replacement->ReadyForActivation();
      ManagedGameplayTasks[TaskId] = Replacement;
      ManagedGameplayTaskPriorities[TaskId] = static_cast<uint8>(FMath::RoundToInt(PriorityValue));
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      AddGameplayTaskData(Replacement, TaskId, Result);
      Result->SetBoolField(TEXT("recreated"), true);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Gameplay task priority configured"), Result, FString());
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    AddGameplayTaskData(Task, TaskId, Result);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Gameplay task inspected"), Result, FString());
    return true;
  }

  return false;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSystemControlAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  // The sub-action is in the payload's "action" field
  FString SubAction;
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("action"), SubAction);
  }

  if (Payload.IsValid() &&
      (SubAction.Equals(TEXT("save_checkpoint"), ESearchCase::IgnoreCase) ||
       SubAction.Equals(TEXT("load_checkpoint"), ESearchCase::IgnoreCase) ||
       SubAction.Equals(TEXT("save_checkpoint_async"), ESearchCase::IgnoreCase) ||
       SubAction.Equals(TEXT("load_checkpoint_async"), ESearchCase::IgnoreCase))) {
    const bool bAsyncCheckpoint = SubAction.EndsWith(TEXT("_async"), ESearchCase::IgnoreCase);
    if (bAsyncCheckpoint) Payload->SetBoolField(TEXT("async"), true);
    if (!Payload->HasField(TEXT("slotName"))) Payload->SetStringField(TEXT("slotName"), TEXT("Checkpoint"));
    SubAction = SubAction.StartsWith(TEXT("save_"), ESearchCase::IgnoreCase) ? TEXT("save_game_to_slot") : TEXT("load_game_from_slot");
  }

  const FString Lower = SubAction.ToLower();
  const bool bSubsystemAction =
      Lower == TEXT("create_game_instance_subsystem") ||
      Lower == TEXT("create_world_subsystem") ||
      Lower == TEXT("create_local_player_subsystem") ||
      Lower == TEXT("create_engine_subsystem") ||
      Lower == TEXT("configure_subsystem_tick") ||
      Lower == TEXT("get_subsystem") ||
      Lower == TEXT("inspect_subsystem") ||
      Lower == TEXT("list_subsystems");
  const bool bAsyncTimerAction =
      Lower == TEXT("set_timer") || Lower == TEXT("clear_timer") ||
      Lower == TEXT("pause_timer") || Lower == TEXT("resume_timer") ||
      Lower == TEXT("get_timer") || Lower == TEXT("list_timers") ||
      Lower == TEXT("create_latent_action") || Lower == TEXT("clear_latent_action") ||
      Lower == TEXT("get_latent_action") || Lower == TEXT("list_latent_actions") ||
      Lower == TEXT("create_async_action") || Lower == TEXT("cancel_async_action") ||
      Lower == TEXT("get_async_action") || Lower == TEXT("list_async_actions") ||
      Lower == TEXT("create_gameplay_task") || Lower == TEXT("end_gameplay_task") ||
      Lower == TEXT("get_gameplay_task") || Lower == TEXT("list_gameplay_tasks") ||
      Lower == TEXT("configure_task_priority");
  const bool bDelegateInterfaceAction =
      Lower == TEXT("create_event_dispatcher") || Lower == TEXT("bind_to_event") ||
      Lower == TEXT("unbind_from_event") || Lower == TEXT("broadcast_event") ||
      Lower == TEXT("create_delegate") || Lower == TEXT("bind_delegate") ||
      Lower == TEXT("inspect_delegate") || Lower == TEXT("list_delegate_bindings") ||
      Lower == TEXT("create_blueprint_interface") || Lower == TEXT("add_interface_function") ||
      Lower == TEXT("implement_interface") || Lower == TEXT("get_interface_info") ||
      Lower == TEXT("call_interface_function");
  const bool bSaveGameAction =
      Lower == TEXT("save_game_to_slot") || Lower == TEXT("load_game_from_slot") ||
      Lower == TEXT("save_game_to_memory") || Lower == TEXT("load_game_from_memory") ||
      Lower == TEXT("save_data_to_slot") || Lower == TEXT("load_data_from_slot") ||
      Lower == TEXT("inspect_save_game_schema") || Lower == TEXT("delete_save_game_slot") || Lower == TEXT("check_save_game_slot") ||
      Lower == TEXT("list_save_game_slots");
  const bool bGameplayTagContainerAction =
      Lower == TEXT("create_tag_container") || Lower == TEXT("add_tag_to_container") ||
      Lower == TEXT("remove_tag_from_container") || Lower == TEXT("check_tag_match");
  const bool bHostWorkflowAction =
      Lower == TEXT("run_uat") || Lower == TEXT("validate_release") || Lower == TEXT("release_gate") || Lower == TEXT("validate_project") || Lower == TEXT("create_game_architecture_manifest") || Lower == TEXT("add_architecture_requirement") || Lower == TEXT("validate_game_architecture") || Lower == TEXT("inspect_platform_capabilities") || Lower == TEXT("sign_release") || Lower == TEXT("run_packaged") || Lower == TEXT("deploy_package") || Lower == TEXT("run_network_soak") || Lower == TEXT("analyze_trace") || Lower == TEXT("manage_project_plugin") || Lower == TEXT("list_plugins") || Lower == TEXT("enable_plugin") || Lower == TEXT("disable_plugin") || Lower == TEXT("get_plugin_status") ||
      Lower == TEXT("wait_for_job") || Lower == TEXT("wait_for_async_action") || Lower == TEXT("get_job_status") || Lower == TEXT("list_jobs") ||
      Lower == TEXT("cancel_job") || Lower == TEXT("read_project_file") ||
      Lower == TEXT("write_project_file") || Lower == TEXT("generate_save_game_class") ||
      Lower == TEXT("create_automation_test") ||
      Lower == TEXT("get_test_results") ||
      Lower == TEXT("list_gameplay_tags") || Lower == TEXT("get_runtime_gameplay_tag") || Lower == TEXT("add_gameplay_tag") ||
      Lower == TEXT("remove_gameplay_tag") || Lower == TEXT("list_config_layers") || Lower == TEXT("configure_chunking") ||
      Lower == TEXT("get_config_value") || Lower == TEXT("read_config_value") || Lower == TEXT("set_config_value") || Lower == TEXT("write_config_value") ||
      Lower == TEXT("get_section") || Lower == TEXT("create_config_section") ||
      Lower == TEXT("reload_config") || Lower == TEXT("flush_config") || Lower == TEXT("get_config_hierarchy") || Lower == TEXT("configure_scalability_group") || Lower == TEXT("create_device_profile") || Lower == TEXT("set_cvar_for_profile") || Lower == TEXT("configure_build_settings") || Lower == TEXT("configure_platform_settings") || Lower == TEXT("configure_plugin_settings") || Lower == TEXT("configure_windows_build") || Lower == TEXT("configure_linux_build") || Lower == TEXT("configure_mac_build") || Lower == TEXT("configure_ios_build") || Lower == TEXT("configure_android_build") || Lower == TEXT("configure_android_signing") || Lower == TEXT("configure_ios_signing") || Lower == TEXT("take_photo_mode_screenshot") || Lower == TEXT("set_max_audio_channels_scaled") || Lower == TEXT("get_max_audio_channel_count") || Lower == TEXT("are_any_listeners_within_range") || Lower == TEXT("is_game_paused") || Lower == TEXT("get_audio_time_seconds") || Lower == TEXT("is_any_local_player_camera_within_range") || Lower == TEXT("get_num_local_player_controllers") || Lower == TEXT("set_subtitles_enabled") || Lower == TEXT("are_subtitles_enabled") || Lower == TEXT("get_active_spatial_plugin") || Lower == TEXT("set_active_spatial_plugin");
  const bool bDataValidationAction = Lower == TEXT("run_data_validation") || Lower == TEXT("create_asset_validator");
  const bool bGameplayTagConfigAction = Lower == TEXT("create_gameplay_tag");
  const bool bGameplayTagNativeAction = Lower == TEXT("register_native_tag");
  const bool bBuildPipelineAlias = Lower == TEXT("cook_content") || Lower == TEXT("package_project") || Lower == TEXT("create_pak_file") || Lower == TEXT("configure_compression") || Lower == TEXT("configure_asset_encryption") || Lower == TEXT("create_test_level") || Lower == TEXT("configure_test_settings") || Lower == TEXT("configure_demo_settings") || Lower == TEXT("configure_localization_target") || Lower == TEXT("import_localization") || Lower == TEXT("export_localization") || Lower == TEXT("run_gauntlet_test") || Lower == TEXT("create_build_target") || Lower == TEXT("compile_shaders");
  const bool bStringTableAction = Lower == TEXT("create_string_table") || Lower == TEXT("add_string_entry") || Lower == TEXT("get_localized_string");
  const bool bCultureAction = Lower == TEXT("set_culture") || Lower == TEXT("set_language_and_locale") || Lower == TEXT("set_locale");
  const bool bQualityLevelAction = Lower == TEXT("set_quality_level");
  const bool bWorldRenderingAction = Lower == TEXT("set_world_rendering");
  const bool bGlobalTimeDilationAction = Lower == TEXT("set_global_time_dilation");
  const bool bGlobalPitchAction = Lower == TEXT("set_global_pitch_modulation");
  const bool bForceDisableSplitscreenAction = Lower == TEXT("set_force_disable_splitscreen");
  const bool bGamePausedAction = Lower == TEXT("set_game_paused");
  const bool bIsGamePausedAction = Lower == TEXT("is_game_paused");
  const bool bGetAudioTimeSecondsAction = Lower == TEXT("get_audio_time_seconds");
  const bool bIsAnyLocalPlayerCameraWithinRangeAction = Lower == TEXT("is_any_local_player_camera_within_range");
  const bool bGetNumLocalPlayerControllersAction = Lower == TEXT("get_num_local_player_controllers");
  const bool bSetSubtitlesEnabledAction = Lower == TEXT("set_subtitles_enabled");
  const bool bAreSubtitlesEnabledAction = Lower == TEXT("are_subtitles_enabled");
  const bool bGetActiveSpatialPluginAction = Lower == TEXT("get_active_spatial_plugin");
  const bool bSetActiveSpatialPluginAction = Lower == TEXT("set_active_spatial_plugin");
  const bool bMaxAudioChannelsScaledAction = Lower == TEXT("set_max_audio_channels_scaled");
  const bool bGetMaxAudioChannelCountAction = Lower == TEXT("get_max_audio_channel_count");
  const bool bAreAnyListenersWithinRangeAction = Lower == TEXT("are_any_listeners_within_range");
  const bool bProjectFilesAction = Lower == TEXT("generate_project_files");

  // Check if this handler should process this sub-action
  if (!Lower.StartsWith(TEXT("run_ubt")) &&
      !Lower.StartsWith(TEXT("run_tests")) &&
      !Lower.StartsWith(TEXT("test_progress")) &&
      !Lower.StartsWith(TEXT("test_stale")) &&
      Lower != TEXT("export_asset") &&
      Lower != TEXT("start_session") &&
      Lower != TEXT("stop_session") &&
      Lower != TEXT("get_session_status") &&
      Lower != TEXT("check_map_errors") &&
      Lower != TEXT("create_functional_test") &&
      Lower != TEXT("create_automation_test") &&
      Lower != TEXT("get_test_results") &&
      Lower != TEXT("validate_assets") &&
      Lower != TEXT("validate_blueprints") &&
      Lower != TEXT("start_memory_report") &&
      Lower != TEXT("configure_stat_commands") &&
      Lower != TEXT("configure_console_variables") &&
      Lower != TEXT("check_for_errors") &&
      Lower != TEXT("capture_insights_trace") &&
      Lower != TEXT("start_network_profiler") &&
      Lower != TEXT("enable_visual_logger") &&
      Lower != TEXT("add_visual_log_entry") &&
      Lower != TEXT("execute_python") &&
      Lower != TEXT("execute_python_script") &&
      Lower != TEXT("execute_python_string") &&
      Lower != TEXT("execute_python_file") &&
      Lower != TEXT("configure_python_paths") &&
      Lower != TEXT("list_python_packages") &&
      Lower != TEXT("create_editor_utility_widget") &&
      Lower != TEXT("create_editor_utility_blueprint") &&
      Lower != TEXT("create_python_editor_utility") &&
      Lower != TEXT("create_geometry_collection") &&
      Lower != TEXT("create_variant_set") &&
      Lower != TEXT("add_variant") &&
      Lower != TEXT("configure_variant_properties") &&
      Lower != TEXT("set_variant_dependencies") &&
      Lower != TEXT("activate_variant") &&
      Lower != TEXT("get_active_variants") &&
      Lower != TEXT("capture_variant_thumbnail") &&
      Lower != TEXT("set_variant_thumbnail") &&
      Lower != TEXT("export_variant_configuration") &&
      Lower != TEXT("add_geometry_to_collection") &&
      Lower != TEXT("remove_geometry_from_collection") &&
      Lower != TEXT("configure_geometry_collection") &&
      Lower != TEXT("inspect_geometry_collection") &&
      Lower != TEXT("configure_geometry_collection_component") &&
      Lower != TEXT("get_ui_scale") &&
      Lower != TEXT("set_ui_scale") &&
      Lower != TEXT("configure_screen_reader_support") &&
      Lower != TEXT("announce_accessible_string") &&
      Lower != TEXT("set_screen_reader_text") &&
      Lower != TEXT("register_python_command") &&
      Lower != TEXT("unregister_python_command") &&
      Lower != TEXT("run_editor_utility") &&
      Lower != TEXT("inspect_editor_utility") &&
       !bSubsystemAction && !bAsyncTimerAction && !bDelegateInterfaceAction && !bSaveGameAction && !bGameplayTagContainerAction &&
       !bHostWorkflowAction && !bDataValidationAction && !bGameplayTagConfigAction && !bGameplayTagNativeAction && !bBuildPipelineAlias && !bStringTableAction && !bCultureAction && !bQualityLevelAction && !bWorldRenderingAction && !bGlobalTimeDilationAction && !bGlobalPitchAction && !bForceDisableSplitscreenAction && !bGamePausedAction && !bProjectFilesAction) {
    return false; // Not handled by this function
  }

  if (Lower == TEXT("get_ui_scale")) {
    const UUserInterfaceSettings* Settings = GetDefault<UUserInterfaceSettings>();
    if (!Settings) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("User interface settings unavailable"), TEXT("NOT_SUPPORTED"));
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("uiScale"), Settings->ApplicationScale);
    Result->SetBoolField(TEXT("slateInitialized"),
#if WITH_EDITOR
                         FSlateApplication::IsInitialized()
#else
                         false
#endif
    );
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("UI scale inspected"), Result);
    return true;
  }

  if (Lower == TEXT("configure_screen_reader_support")) {
    bool bEnabled = false;
    if (!Payload.IsValid() || !Payload->TryGetBoolField(TEXT("enabled"), bEnabled)) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("configure_screen_reader_support requires enabled"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    IConsoleVariable* AccessibilityEnable = IConsoleManager::Get().FindConsoleVariable(TEXT("Accessibility.Enable"));
    if (!AccessibilityEnable) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Accessibility.Enable is not available in this Unreal build"), TEXT("NOT_SUPPORTED"));
      return true;
    }
    AccessibilityEnable->Set(bEnabled ? 1 : 0, ECVF_SetByCode);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("enabled"), AccessibilityEnable->GetInt() != 0);
    Result->SetStringField(TEXT("consoleVariable"), TEXT("Accessibility.Enable"));
    Result->SetBoolField(TEXT("appliedRuntime"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Screen-reader support configured"), Result);
    return true;
  }

  if (Lower == TEXT("set_ui_scale")) {
    double Scale = 0.0;
    if (!Payload.IsValid() || !Payload->TryGetNumberField(TEXT("uiScale"), Scale) || Scale < 0.1 || Scale > 4.0) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("set_ui_scale requires uiScale between 0.1 and 4.0"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    UUserInterfaceSettings* Settings = GetMutableDefault<UUserInterfaceSettings>();
    if (!Settings) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("User interface settings unavailable"), TEXT("NOT_SUPPORTED"));
      return true;
    }
    Settings->ApplicationScale = static_cast<float>(Scale);
    Settings->SaveConfig();
#if WITH_EDITOR
    if (FSlateApplication::IsInitialized()) {
      FSlateApplication::Get().SetApplicationScale(static_cast<float>(Scale));
    }
#endif
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("uiScale"), Scale);
    Result->SetBoolField(TEXT("persisted"), true);
    Result->SetBoolField(TEXT("appliedLive"),
#if WITH_EDITOR
                         FSlateApplication::IsInitialized()
#else
                         false
#endif
    );
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("UI scale updated"), Result);
    return true;
  }

  if (Lower == TEXT("announce_accessible_string")) {
    FString Announcement;
    if (!Payload.IsValid() || !Payload->TryGetStringField(TEXT("announcement"), Announcement) ||
        Announcement.TrimStartAndEnd().IsEmpty() || Announcement.Len() > 1024) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("announce_accessible_string requires a non-empty announcement of at most 1024 characters"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Announcement.TrimStartAndEndInline();
    UGameplayStatics::AnnounceAccessibleString(Announcement);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("announcement"), Announcement);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Accessibility announcement sent"), Result);
    return true;
  }

  if (Lower == TEXT("set_screen_reader_text")) {
    FString WidgetPath;
    FString AccessibleText;
    FString AccessibleSummaryText;
    FString AccessibleBehavior = TEXT("Custom");
    FString AccessibleSummaryBehavior = TEXT("Auto");
    bool bOverrideAccessibleDefaults = true;
    bool bCanChildrenBeAccessible = false;
    if (!Payload.IsValid() || !Payload->TryGetStringField(TEXT("widgetPath"), WidgetPath) ||
        WidgetPath.TrimStartAndEnd().IsEmpty() || !Payload->TryGetStringField(TEXT("accessibleText"), AccessibleText) ||
        AccessibleText.TrimStartAndEnd().IsEmpty() || AccessibleText.Len() > 4096) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("set_screen_reader_text requires widgetPath and non-empty accessibleText (max 4096 characters)"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    WidgetPath.TrimStartAndEndInline();
    AccessibleText.TrimStartAndEndInline();
    Payload->TryGetStringField(TEXT("accessibleSummaryText"), AccessibleSummaryText);
    Payload->TryGetStringField(TEXT("accessibleBehavior"), AccessibleBehavior);
    Payload->TryGetStringField(TEXT("accessibleSummaryBehavior"), AccessibleSummaryBehavior);
    Payload->TryGetBoolField(TEXT("overrideAccessibleDefaults"), bOverrideAccessibleDefaults);
    Payload->TryGetBoolField(TEXT("canChildrenBeAccessible"), bCanChildrenBeAccessible);

    auto ParseBehavior = [](const FString &Value, ESlateAccessibleBehavior &OutBehavior) -> bool {
      if (Value.Equals(TEXT("Auto"), ESearchCase::IgnoreCase)) { OutBehavior = ESlateAccessibleBehavior::Auto; return true; }
      if (Value.Equals(TEXT("Summary"), ESearchCase::IgnoreCase)) { OutBehavior = ESlateAccessibleBehavior::Summary; return true; }
      if (Value.Equals(TEXT("Custom"), ESearchCase::IgnoreCase)) { OutBehavior = ESlateAccessibleBehavior::Custom; return true; }
      if (Value.Equals(TEXT("ToolTip"), ESearchCase::IgnoreCase)) { OutBehavior = ESlateAccessibleBehavior::ToolTip; return true; }
      if (Value.Equals(TEXT("NotAccessible"), ESearchCase::IgnoreCase)) { OutBehavior = ESlateAccessibleBehavior::NotAccessible; return true; }
      return false;
    };
    ESlateAccessibleBehavior Behavior;
    ESlateAccessibleBehavior SummaryBehavior;
    if (!ParseBehavior(AccessibleBehavior, Behavior) || !ParseBehavior(AccessibleSummaryBehavior, SummaryBehavior)) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("accessibleBehavior and accessibleSummaryBehavior must be Auto, Summary, Custom, ToolTip, or NotAccessible"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    UWidget *Widget = FindObject<UWidget>(nullptr, *WidgetPath);
    if (!Widget) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("widgetPath must resolve to a live UWidget object"), TEXT("OBJECT_NOT_FOUND"));
      return true;
    }
    Widget->bOverrideAccessibleDefaults = bOverrideAccessibleDefaults;
    Widget->AccessibleBehavior = Behavior;
    Widget->AccessibleSummaryBehavior = SummaryBehavior;
    Widget->AccessibleText = FText::FromString(AccessibleText);
    if (!AccessibleSummaryText.IsEmpty()) {
      if (AccessibleSummaryText.Len() > 4096) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("accessibleSummaryText must be at most 4096 characters"), TEXT("INVALID_ARGUMENT"));
        return true;
      }
      Widget->AccessibleSummaryText = FText::FromString(AccessibleSummaryText);
    }
    Widget->bCanChildrenBeAccessible = bCanChildrenBeAccessible;

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("widgetPath"), Widget->GetPathName());
    Result->SetStringField(TEXT("widgetClass"), Widget->GetClass()->GetPathName());
    Result->SetStringField(TEXT("accessibleText"), Widget->GetAccessibleText().ToString());
    Result->SetStringField(TEXT("accessibleSummaryText"), Widget->GetAccessibleSummaryText().ToString());
    Result->SetBoolField(TEXT("overrideAccessibleDefaults"), Widget->bOverrideAccessibleDefaults);
    Result->SetBoolField(TEXT("canChildrenBeAccessible"), Widget->bCanChildrenBeAccessible);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Screen-reader text updated"), Result);
    return true;
  }

#if !WITH_EDITOR
  if (bSaveGameAction) {
    if (Lower == TEXT("inspect_save_game_schema")) {
      TSharedPtr<FJsonObject> Result;
      FString Error;
      FString ErrorCode;
      if (!BuildSaveGameSchemaInspection(Payload, Result, Error, ErrorCode)) {
        SendAutomationError(RequestingSocket, RequestId, Error, ErrorCode);
      } else {
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SaveGame schema inspected"), Result, FString());
      }
      return true;
    }
    if (Lower == TEXT("save_game_to_memory")) {
      FString ObjectPath;
      Payload->TryGetStringField(TEXT("saveGameObject"), ObjectPath);
      USaveGame *SaveGame = LoadObject<USaveGame>(nullptr, *ObjectPath);
      if (!SaveGame) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("saveGameObject must resolve to a loaded USaveGame object"), TEXT("OBJECT_NOT_FOUND"));
        return true;
      }
      TArray<uint8> SaveData;
      if (!UGameplayStatics::SaveGameToMemory(SaveGame, SaveData)) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("SaveGame memory serialization failed"), TEXT("SAVE_MEMORY_FAILED"));
        return true;
      }
      constexpr int32 MaxSaveGameMemoryBytes = 8 * 1024 * 1024;
      if (SaveData.Num() > MaxSaveGameMemoryBytes) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Serialized SaveGame exceeds the 8 MiB transport limit"), TEXT("PAYLOAD_TOO_LARGE"));
        return true;
      }
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("classPath"), SaveGame->GetClass()->GetPathName());
      Result->SetNumberField(TEXT("byteLength"), SaveData.Num());
      Result->SetStringField(TEXT("dataBase64"), FBase64::Encode(SaveData));
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SaveGame serialized to memory"), Result, FString());
      return true;
    }
    if (Lower == TEXT("load_game_from_memory")) {
      FString EncodedData;
      Payload->TryGetStringField(TEXT("dataBase64"), EncodedData);
      TArray<uint8> SaveData;
      constexpr int32 MaxSaveGameMemoryBytes = 8 * 1024 * 1024;
      if (EncodedData.IsEmpty() || !FBase64::Decode(EncodedData, SaveData) || SaveData.Num() == 0) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("dataBase64 must contain valid non-empty Base64 SaveGame data"), TEXT("INVALID_ARGUMENT"));
        return true;
      }
      if (SaveData.Num() > MaxSaveGameMemoryBytes) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Encoded SaveGame exceeds the 8 MiB transport limit"), TEXT("PAYLOAD_TOO_LARGE"));
        return true;
      }
      USaveGame *Loaded = UGameplayStatics::LoadGameFromMemory(SaveData);
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetNumberField(TEXT("byteLength"), SaveData.Num());
      Result->SetBoolField(TEXT("loaded"), Loaded != nullptr);
      if (Loaded) Result->SetStringField(TEXT("classPath"), Loaded->GetClass()->GetPathName());
      SendAutomationResponse(RequestingSocket, RequestId, Loaded != nullptr, Loaded ? TEXT("SaveGame loaded from memory") : TEXT("SaveGame memory data could not be loaded"), Result, Loaded ? FString() : TEXT("LOAD_MEMORY_FAILED"));
      return true;
    }
    return HandleRuntimeSaveGameAction(RequestId, Lower, Payload, RequestingSocket);
  }
#endif

  if (bProjectFilesAction) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Project-file generation is owned by the stdio host UBT job runner"), TEXT("HOST_ONLY"));
    return true;
  }

  if (bGameplayTagNativeAction) {
    if (!Payload.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("System control payload missing"), TEXT("INVALID_PAYLOAD"));
      return true;
    }
    FString TagName;
    FString Comment;
    Payload->TryGetStringField(TEXT("tag"), TagName);
    Payload->TryGetStringField(TEXT("comment"), Comment);
    TagName.TrimStartAndEndInline();
    Comment.TrimStartAndEndInline();
    if (TagName.IsEmpty() || TagName.Len() > 256 || !TagName.Contains(TEXT(".")) || TagName.Contains(TEXT(" ")) || TagName.Contains(TEXT("/"))) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("tag must be a dotted Gameplay Tag name without spaces or slashes"), TEXT("INVALID_TAG"));
      return true;
    }
    const FGameplayTag RegisteredTag = UGameplayTagsManager::Get().AddNativeGameplayTag(FName(*TagName), Comment);
    if (!RegisteredTag.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unreal rejected the native Gameplay Tag"), TEXT("TAG_REGISTRATION_FAILED"));
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("tag"), RegisteredTag.ToString());
    Result->SetStringField(TEXT("comment"), Comment);
    Result->SetBoolField(TEXT("runtimeOnly"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Native Gameplay Tag registered for this process"), Result, FString());
    return true;
  }

  if (bQualityLevelAction) {
    if (!Payload.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("System control payload missing"), TEXT("INVALID_PAYLOAD"));
      return true;
    }
    int32 Level = 1;
    Payload->TryGetNumberField(TEXT("level"), Level);
    Level = FMath::Clamp(Level, 0, 4);
    Scalability::FQualityLevels QualityLevels;
    QualityLevels.SetFromSingleQualityLevel(Level);
    Scalability::SetQualityLevels(QualityLevels);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("level"), Level);
    Result->SetStringField(TEXT("quality"), Level == 0 ? TEXT("low") : Level == 1 ? TEXT("medium") : Level == 2 ? TEXT("high") : Level == 3 ? TEXT("epic") : TEXT("cinematic"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Scalability quality level set"), Result, FString());
    return true;
  }

  if (bWorldRenderingAction) {
    bool bEnabled = true;
    if (!Payload->TryGetBoolField(TEXT("enabled"), bEnabled)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("enabled is required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    UGameplayStatics::SetEnableWorldRendering(GetWorld(), bEnabled);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("enabled"), UGameplayStatics::GetEnableWorldRendering(GetWorld()));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("World rendering state updated"), Result, FString());
    return true;
  }

  if (bGlobalTimeDilationAction) {
    double TimeDilation = 1.0;
    if (!Payload->TryGetNumberField(TEXT("timeDilation"), TimeDilation) || !FMath::IsFinite(TimeDilation) || TimeDilation < 0.0 || TimeDilation > 100.0) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("timeDilation must be finite and between 0 and 100"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), static_cast<float>(TimeDilation));
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("timeDilation"), UGameplayStatics::GetGlobalTimeDilation(GetWorld()));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Global time dilation updated"), Result, FString());
    return true;
  }

  if (bGlobalPitchAction) {
    double PitchModulation = 1.0;
    double TimeSec = 0.0;
    if (!Payload->TryGetNumberField(TEXT("pitchModulation"), PitchModulation) || !FMath::IsFinite(PitchModulation) || PitchModulation < 0.0 || PitchModulation > 100.0) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("pitchModulation must be finite and between 0 and 100"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetNumberField(TEXT("timeSec"), TimeSec);
    if (!FMath::IsFinite(TimeSec) || TimeSec < 0.0 || TimeSec > 600.0) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("timeSec must be finite and between 0 and 600"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    UGameplayStatics::SetGlobalPitchModulation(GetWorld(), static_cast<float>(PitchModulation), static_cast<float>(TimeSec));
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("pitchModulation"), PitchModulation);
    Result->SetNumberField(TEXT("timeSec"), TimeSec);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Global pitch modulation updated"), Result, FString());
    return true;
  }

  if (bForceDisableSplitscreenAction) {
    bool bDisable = false;
    if (!Payload->TryGetBoolField(TEXT("disableSplitscreen"), bDisable)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("disableSplitscreen is required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    UGameplayStatics::SetForceDisableSplitscreen(GetWorld(), bDisable);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("disableSplitscreen"), UGameplayStatics::IsSplitscreenForceDisabled(GetWorld()));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Forced split-screen state updated"), Result, FString());
    return true;
  }

  if (bGamePausedAction) {
    bool bPaused = false;
    if (!Payload->TryGetBoolField(TEXT("paused"), bPaused)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("paused is required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    const bool bChanged = UGameplayStatics::SetGamePaused(GetWorld(), bPaused);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("paused"), bPaused);
    Result->SetBoolField(TEXT("changed"), bChanged);
    SendAutomationResponse(RequestingSocket, RequestId, bChanged, bChanged ? TEXT("Game pause state updated") : TEXT("Game pause state could not be changed"), Result, bChanged ? FString() : TEXT("PAUSE_STATE_FAILED"));
    return true;
  }

  if (bIsGamePausedAction) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("paused"), UGameplayStatics::IsGamePaused(GetWorld()));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Game pause state read"), Result, FString());
    return true;
  }

  if (bGetAudioTimeSecondsAction) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("audioTimeSeconds"), UGameplayStatics::GetAudioTimeSeconds(GetWorld()));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Audio clock time read"), Result, FString());
    return true;
  }

  if (bIsAnyLocalPlayerCameraWithinRangeAction) {
    const TSharedPtr<FJsonObject>* LocationObject = nullptr;
    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
    double MaximumRange = 0.0;
    if (!Payload->TryGetObjectField(TEXT("location"), LocationObject) || LocationObject == nullptr ||
        !(*LocationObject)->TryGetNumberField(TEXT("x"), X) || !(*LocationObject)->TryGetNumberField(TEXT("y"), Y) ||
        !(*LocationObject)->TryGetNumberField(TEXT("z"), Z) || !Payload->TryGetNumberField(TEXT("maximumRange"), MaximumRange) ||
        !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z) || !FMath::IsFinite(MaximumRange) || MaximumRange < 0.0) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("location {x,y,z} and a finite non-negative maximumRange are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    const bool bWithinRange = UGameplayStatics::IsAnyLocalPlayerCameraWithinRange(GetWorld(), FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z)), static_cast<float>(MaximumRange));
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("withinRange"), bWithinRange);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Local player camera proximity queried"), Result, FString());
    return true;
  }

  if (bGetNumLocalPlayerControllersAction) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("count"), UGameplayStatics::GetNumLocalPlayerControllers(GetWorld()));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Local player controller count read"), Result, FString());
    return true;
  }

  if (bSetSubtitlesEnabledAction) {
    bool bEnabled = false;
    if (!Payload->TryGetBoolField(TEXT("enabled"), bEnabled)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("enabled is required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    UGameplayStatics::SetSubtitlesEnabled(bEnabled);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("enabled"), UGameplayStatics::AreSubtitlesEnabled());
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Subtitle state updated"), Result, FString());
    return true;
  }

  if (bAreSubtitlesEnabledAction) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("enabled"), UGameplayStatics::AreSubtitlesEnabled());
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Subtitle state read"), Result, FString());
    return true;
  }

  if (bGetActiveSpatialPluginAction) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("pluginName"), UGameplayStatics::GetActiveSpatialPluginName(GetWorld()).ToString());
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Active spatial audio plugin read"), Result, FString());
    return true;
  }

  if (bSetActiveSpatialPluginAction) {
    FString PluginName;
    if (!Payload->TryGetStringField(TEXT("pluginName"), PluginName) || PluginName.TrimStartAndEnd().IsEmpty() || PluginName.Len() > 128) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("pluginName is required and must be at most 128 characters"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    const bool bChanged = UGameplayStatics::SetActiveSpatialPluginByName(GetWorld(), FName(*PluginName));
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("pluginName"), UGameplayStatics::GetActiveSpatialPluginName(GetWorld()).ToString());
    Result->SetBoolField(TEXT("changed"), bChanged);
    SendAutomationResponse(RequestingSocket, RequestId, bChanged, bChanged ? TEXT("Active spatial audio plugin updated") : TEXT("Active spatial audio plugin could not be updated"), Result, bChanged ? FString() : TEXT("SPATIAL_PLUGIN_FAILED"));
    return true;
  }

  if (bMaxAudioChannelsScaledAction) {
    double MaxChannelCountScale = 1.0;
    if (!Payload->TryGetNumberField(TEXT("maxChannelCountScale"), MaxChannelCountScale) || !FMath::IsFinite(MaxChannelCountScale) || MaxChannelCountScale < 0.0 || MaxChannelCountScale > 1.0) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("maxChannelCountScale must be finite and between 0 and 1"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    UGameplayStatics::SetMaxAudioChannelsScaled(GetWorld(), static_cast<float>(MaxChannelCountScale));
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("maxChannelCountScale"), MaxChannelCountScale);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Maximum audio channel scale updated"), Result, FString());
    return true;
  }

  if (bGetMaxAudioChannelCountAction) {
    const int32 MaxAudioChannelCount = UGameplayStatics::GetMaxAudioChannelCount(GetWorld());
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("maxAudioChannelCount"), MaxAudioChannelCount);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Maximum audio channel count read"), Result, FString());
    return true;
  }

  if (bAreAnyListenersWithinRangeAction) {
    const TSharedPtr<FJsonObject>* LocationObject = nullptr;
    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
    double MaximumRange = 0.0;
    if (!Payload->TryGetObjectField(TEXT("location"), LocationObject) || LocationObject == nullptr ||
        !(*LocationObject)->TryGetNumberField(TEXT("x"), X) || !(*LocationObject)->TryGetNumberField(TEXT("y"), Y) ||
        !(*LocationObject)->TryGetNumberField(TEXT("z"), Z) || !Payload->TryGetNumberField(TEXT("maximumRange"), MaximumRange) ||
        !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z) || !FMath::IsFinite(MaximumRange) || MaximumRange < 0.0) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("location {x,y,z} and a finite non-negative maximumRange are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    const bool bWithinRange = UGameplayStatics::AreAnyListenersWithinRange(GetWorld(), FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z)), static_cast<float>(MaximumRange));
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("withinRange"), bWithinRange);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Audio listener proximity queried"), Result, FString());
    return true;
  }

  if (bCultureAction) {
    if (!Payload.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("System control payload missing"), TEXT("INVALID_PAYLOAD"));
      return true;
    }
    FString Culture;
    Payload->TryGetStringField(TEXT("culture"), Culture);
    Culture.TrimStartAndEndInline();
    if (Culture.IsEmpty() || Culture.Len() > 64) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("culture is required and must be at most 64 characters"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!FInternationalization::IsAvailable()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Internationalization is not available"), TEXT("UNAVAILABLE"));
      return true;
    }
    FInternationalization& Internationalization = FInternationalization::Get();
    bool bSet = false;
    if (Lower == TEXT("set_language_and_locale")) {
      bSet = Internationalization.SetCurrentLanguageAndLocale(Culture);
    } else if (Lower == TEXT("set_locale")) {
      bSet = Internationalization.SetCurrentLocale(Culture);
    } else {
      bSet = Internationalization.SetCurrentCulture(Culture);
    }
    if (!bSet) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Requested culture is not available"), TEXT("CULTURE_NOT_FOUND"));
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("culture"), Internationalization.GetCurrentCulture()->GetName());
    Result->SetStringField(TEXT("language"), Internationalization.GetCurrentLanguage()->GetName());
    Result->SetStringField(TEXT("locale"), Internationalization.GetCurrentLocale()->GetName());
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Culture updated"), Result, FString());
    return true;
  }

  if (bStringTableAction) {
    if (!Payload.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("System control payload missing"), TEXT("INVALID_PAYLOAD"));
      return true;
    }
    FString TableId;
    Payload->TryGetStringField(TEXT("stringTableId"), TableId);
    TableId.TrimStartAndEndInline();
    if (TableId.IsEmpty() || TableId.Len() > 128) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("stringTableId is required and must be at most 128 characters"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    const FName TableName(*TableId);
    FString Key;
    Payload->TryGetStringField(TEXT("stringKey"), Key);
    Key.TrimStartAndEndInline();
    if (Lower == TEXT("create_string_table")) {
      if (FStringTableRegistry::Get().FindStringTable(TableName).IsValid()) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("String table already exists"), TEXT("STRING_TABLE_EXISTS"));
        return true;
      }
      FStringTableRef Table = FStringTable::NewStringTable();
      FString Namespace;
      Payload->TryGetStringField(TEXT("namespace"), Namespace);
      Namespace.TrimStartAndEndInline();
      if (!Namespace.IsEmpty()) Table->SetNamespace(FTextKey(Namespace));
      FStringTableRegistry::Get().RegisterStringTable(TableName, Table);
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("stringTableId"), TableId);
      Result->SetStringField(TEXT("namespace"), Namespace);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("String table created"), Result, FString());
      return true;
    }
    FStringTablePtr Table = FStringTableRegistry::Get().FindMutableStringTable(TableName);
    if (!Table.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("String table is not registered"), TEXT("STRING_TABLE_NOT_FOUND"));
      return true;
    }
    if (Key.IsEmpty() || Key.Len() > 256) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("stringKey is required and must be at most 256 characters"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (Lower == TEXT("add_string_entry")) {
      FString SourceString;
      FString DevNotes;
      Payload->TryGetStringField(TEXT("sourceString"), SourceString);
      Payload->TryGetStringField(TEXT("devNotes"), DevNotes);
      if (SourceString.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("sourceString is required"), TEXT("INVALID_ARGUMENT"));
        return true;
      }
      Table->SetSourceString(FTextKey(Key), SourceString, DevNotes);
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("stringTableId"), TableId);
      Result->SetStringField(TEXT("stringKey"), Key);
      Result->SetStringField(TEXT("sourceString"), SourceString);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("String table entry added"), Result, FString());
      return true;
    }
    const FText LocalizedText = FText::FromStringTable(TableName, Key);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("stringTableId"), TableId);
    Result->SetStringField(TEXT("stringKey"), Key);
    Result->SetStringField(TEXT("text"), LocalizedText.ToString());
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Localized string resolved"), Result, FString());
    return true;
  }

  if (bGameplayTagContainerAction) {
    if (!Payload.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("System control payload missing"), TEXT("INVALID_PAYLOAD"));
      return true;
    }
    auto ReadTagContainer = [](const TSharedPtr<FJsonObject>& Source, const TCHAR* FieldName, FGameplayTagContainer& OutContainer, FString& OutError) -> bool {
      const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
      if (!Source->TryGetArrayField(FieldName, Values) || !Values) {
        OutError = FString::Printf(TEXT("%s must be an array of registered Gameplay Tag names"), FieldName);
        return false;
      }
      for (const TSharedPtr<FJsonValue>& Value : *Values) {
        FString TagName;
        if (!Value.IsValid() || !Value->TryGetString(TagName)) {
          OutError = FString::Printf(TEXT("%s must contain only Gameplay Tag names"), FieldName);
          return false;
        }
        TagName.TrimStartAndEndInline();
        const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagName), false);
        if (!Tag.IsValid()) {
          OutError = FString::Printf(TEXT("Gameplay Tag '%s' is not registered"), *TagName);
          return false;
        }
        OutContainer.AddTag(Tag);
      }
      return true;
    };
    auto WriteTagContainer = [](const FGameplayTagContainer& Container, const TSharedPtr<FJsonObject>& Result) {
      TArray<TSharedPtr<FJsonValue>> Tags;
      for (const FGameplayTag& Tag : Container.GetGameplayTagArray()) {
        Tags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
      }
      Result->SetArrayField(TEXT("tags"), Tags);
      Result->SetNumberField(TEXT("count"), Tags.Num());
    };
    FGameplayTagContainer Container;
    FString Error;
    if (!ReadTagContainer(Payload, TEXT("tags"), Container, Error)) {
      SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (Lower == TEXT("check_tag_match")) {
      FGameplayTagContainer MatchContainer;
      if (!ReadTagContainer(Payload, TEXT("matchTags"), MatchContainer, Error)) {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
        return true;
      }
      FString MatchMode = TEXT("any");
      Payload->TryGetStringField(TEXT("matchMode"), MatchMode);
      MatchMode.TrimStartAndEndInline();
      MatchMode.ToLowerInline();
      bool bMatches = false;
      if (MatchMode == TEXT("any")) bMatches = Container.HasAny(MatchContainer);
      else if (MatchMode == TEXT("all")) bMatches = Container.HasAll(MatchContainer);
      else if (MatchMode == TEXT("any_exact")) bMatches = Container.HasAnyExact(MatchContainer);
      else if (MatchMode == TEXT("all_exact")) bMatches = Container.HasAllExact(MatchContainer);
      else {
        SendAutomationError(RequestingSocket, RequestId, TEXT("matchMode must be any, all, any_exact, or all_exact"), TEXT("INVALID_ARGUMENT"));
        return true;
      }
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      WriteTagContainer(Container, Result);
      Result->SetBoolField(TEXT("matches"), bMatches);
      Result->SetStringField(TEXT("matchMode"), MatchMode);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Gameplay Tag match evaluated"), Result, FString());
      return true;
    }
    if (Lower == TEXT("add_tag_to_container") || Lower == TEXT("remove_tag_from_container")) {
      FString TagName;
      Payload->TryGetStringField(TEXT("tag"), TagName);
      TagName.TrimStartAndEndInline();
      const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagName), false);
      if (!Tag.IsValid()) {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Gameplay Tag '%s' is not registered"), *TagName), TEXT("INVALID_ARGUMENT"));
        return true;
      }
      bool bChanged = false;
      if (Lower == TEXT("add_tag_to_container")) {
        bChanged = !Container.HasTagExact(Tag);
        if (bChanged) Container.AddTag(Tag);
      } else {
        bChanged = Container.RemoveTag(Tag, false);
      }
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      WriteTagContainer(Container, Result);
      Result->SetStringField(TEXT("tag"), Tag.ToString());
      Result->SetBoolField(TEXT("changed"), bChanged);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Gameplay Tag container updated"), Result, FString());
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    WriteTagContainer(Container, Result);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Gameplay Tag container created"), Result, FString());
    return true;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("System control payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // These operations require the TypeScript host process: it owns the
  // external-process job registry and project-file safety boundary. Keep the
  // native endpoint contract explicit rather than falling through as unknown.
  if (bHostWorkflowAction || bDataValidationAction || bGameplayTagConfigAction || bBuildPipelineAlias || bStringTableAction) {
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("This action is available through the stdio MCP host; the native /mcp endpoint does not own the host job or project-file registry"),
        TEXT("HOST_ONLY"));
    return true;
  }

  if (bSubsystemAction) {
    return HandleSubsystemAction(RequestId, Lower, Payload, RequestingSocket);
  }

  if (bAsyncTimerAction) {
    return HandleAsyncTimerAction(RequestId, Lower, Payload, RequestingSocket);
  }

  if (bDelegateInterfaceAction) {
    return HandleDelegateInterfaceAction(RequestId, Lower, Payload, RequestingSocket);
  }

  if (bSaveGameAction) {
    if (Lower == TEXT("inspect_save_game_schema")) {
      TSharedPtr<FJsonObject> Result;
      FString Error;
      FString ErrorCode;
      if (!BuildSaveGameSchemaInspection(Payload, Result, Error, ErrorCode)) {
        SendAutomationError(RequestingSocket, RequestId, Error, ErrorCode);
      } else {
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SaveGame schema inspected"), Result, FString());
      }
      return true;
    }
    if (Lower == TEXT("save_game_to_memory")) {
      FString ObjectPath;
      Payload->TryGetStringField(TEXT("saveGameObject"), ObjectPath);
      USaveGame *SaveGame = LoadObject<USaveGame>(nullptr, *ObjectPath);
      if (!SaveGame) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("saveGameObject must resolve to a loaded USaveGame object"), TEXT("OBJECT_NOT_FOUND"));
        return true;
      }
      TArray<uint8> SaveData;
      if (!UGameplayStatics::SaveGameToMemory(SaveGame, SaveData)) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("SaveGame memory serialization failed"), TEXT("SAVE_MEMORY_FAILED"));
        return true;
      }
      constexpr int32 MaxSaveGameMemoryBytes = 8 * 1024 * 1024;
      if (SaveData.Num() > MaxSaveGameMemoryBytes) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Serialized SaveGame exceeds the 8 MiB transport limit"), TEXT("PAYLOAD_TOO_LARGE"));
        return true;
      }
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("classPath"), SaveGame->GetClass()->GetPathName());
      Result->SetNumberField(TEXT("byteLength"), SaveData.Num());
      Result->SetStringField(TEXT("dataBase64"), FBase64::Encode(SaveData));
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SaveGame serialized to memory"), Result, FString());
      return true;
    }
    if (Lower == TEXT("load_game_from_memory")) {
      FString EncodedData;
      Payload->TryGetStringField(TEXT("dataBase64"), EncodedData);
      TArray<uint8> SaveData;
      constexpr int32 MaxSaveGameMemoryBytes = 8 * 1024 * 1024;
      if (EncodedData.IsEmpty() || !FBase64::Decode(EncodedData, SaveData) || SaveData.Num() == 0) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("dataBase64 must contain valid non-empty Base64 SaveGame data"), TEXT("INVALID_ARGUMENT"));
        return true;
      }
      if (SaveData.Num() > MaxSaveGameMemoryBytes) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Encoded SaveGame exceeds the 8 MiB transport limit"), TEXT("PAYLOAD_TOO_LARGE"));
        return true;
      }
      USaveGame *Loaded = UGameplayStatics::LoadGameFromMemory(SaveData);
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetNumberField(TEXT("byteLength"), SaveData.Num());
      Result->SetBoolField(TEXT("loaded"), Loaded != nullptr);
      if (Loaded) Result->SetStringField(TEXT("classPath"), Loaded->GetClass()->GetPathName());
      SendAutomationResponse(RequestingSocket, RequestId, Loaded != nullptr, Loaded ? TEXT("SaveGame loaded from memory") : TEXT("SaveGame memory data could not be loaded"), Result, Loaded ? FString() : TEXT("LOAD_MEMORY_FAILED"));
      return true;
    }
    const bool bMemorySaveGameAction = Lower == TEXT("save_game_to_memory") || Lower == TEXT("load_game_from_memory");
    FString SlotName;
    Payload->TryGetStringField(TEXT("slotName"), SlotName);
    SlotName.TrimStartAndEndInline();
    int32 UserIndex = 0;
    Payload->TryGetNumberField(TEXT("userIndex"), UserIndex);
    if (UserIndex < 0 || UserIndex > 7) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("userIndex must be between 0 and 7"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!bMemorySaveGameAction && Lower != TEXT("list_save_game_slots") && SlotName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("slotName is required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    for (const TCHAR Character : SlotName) {
      if (!(FChar::IsAlnum(Character) || Character == TCHAR('_') || Character == TCHAR('-'))) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("slotName contains unsupported characters"), TEXT("INVALID_ARGUMENT"));
        return true;
      }
    }

    if (Lower == TEXT("save_data_to_slot")) {
      FString EncodedData;
      Payload->TryGetStringField(TEXT("dataBase64"), EncodedData);
      TArray<uint8> SaveData;
      constexpr int32 MaxSaveGameMemoryBytes = 8 * 1024 * 1024;
      if (EncodedData.IsEmpty() || !FBase64::Decode(EncodedData, SaveData) || SaveData.Num() == 0) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("dataBase64 must contain valid non-empty Base64 data"), TEXT("INVALID_ARGUMENT"));
        return true;
      }
      if (SaveData.Num() > MaxSaveGameMemoryBytes) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Save data exceeds the 8 MiB transport limit"), TEXT("PAYLOAD_TOO_LARGE"));
        return true;
      }
      const bool bSaved = UGameplayStatics::SaveDataToSlot(SaveData, SlotName, UserIndex);
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("slotName"), SlotName);
      Result->SetNumberField(TEXT("userIndex"), UserIndex);
      Result->SetNumberField(TEXT("byteLength"), SaveData.Num());
      Result->SetBoolField(TEXT("saved"), bSaved);
      SendAutomationResponse(RequestingSocket, RequestId, bSaved, bSaved ? TEXT("Raw SaveGame data written") : TEXT("Raw SaveGame data write failed"), Result, bSaved ? FString() : TEXT("SAVE_DATA_FAILED"));
      return true;
    }
    if (Lower == TEXT("load_data_from_slot")) {
      TArray<uint8> SaveData;
      constexpr int32 MaxSaveGameMemoryBytes = 8 * 1024 * 1024;
      const bool bLoaded = UGameplayStatics::LoadDataFromSlot(SaveData, SlotName, UserIndex);
      if (bLoaded && SaveData.Num() > MaxSaveGameMemoryBytes) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Loaded save data exceeds the 8 MiB transport limit"), TEXT("PAYLOAD_TOO_LARGE"));
        return true;
      }
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("slotName"), SlotName);
      Result->SetNumberField(TEXT("userIndex"), UserIndex);
      Result->SetNumberField(TEXT("byteLength"), SaveData.Num());
      Result->SetBoolField(TEXT("loaded"), bLoaded);
      if (bLoaded) Result->SetStringField(TEXT("dataBase64"), FBase64::Encode(SaveData));
      SendAutomationResponse(RequestingSocket, RequestId, bLoaded, bLoaded ? TEXT("Raw SaveGame data loaded") : TEXT("Raw SaveGame data not found"), Result, bLoaded ? FString() : TEXT("SLOT_NOT_FOUND"));
      return true;
    }

    if (Lower == TEXT("save_game_to_slot")) {
      FString ObjectPath;
      Payload->TryGetStringField(TEXT("saveGameObject"), ObjectPath);
      USaveGame *SaveGame = LoadObject<USaveGame>(nullptr, *ObjectPath);
      if (!SaveGame) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("saveGameObject must resolve to a loaded USaveGame object"), TEXT("OBJECT_NOT_FOUND"));
        return true;
      }
      bool bAsyncSave = false;
      Payload->TryGetBoolField(TEXT("async"), bAsyncSave);
      if (bAsyncSave) {
        if (!CanRegisterManagedAsyncAction()) {
          SendAutomationError(RequestingSocket, RequestId, TEXT("Too many active or recently completed async actions"), TEXT("ASYNC_ACTION_CAPACITY"));
          return true;
        }
        const FString AsyncId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        const TSharedPtr<FMcpAsyncState> State = MakeShared<FMcpAsyncState>();
        FMcpAsyncRecord Record;
        Record.AsyncId = AsyncId;
        Record.Execution = TEXT("save_game");
        Record.Label = FString::Printf(TEXT("SaveGame:%s"), *SlotName);
        Record.State = State;
        ManagedAsyncActions.Add(AsyncId, Record);
        const TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
        UGameplayStatics::AsyncSaveGameToSlot(SaveGame, SlotName, UserIndex,
            FAsyncSaveGameToSlotDelegate::CreateLambda([WeakThis, AsyncId, State](const FString&, const int32, bool bSuccess) {
              State->bSucceeded.store(bSuccess);
              State->bCompleted.store(true);
              if (UNebulaForgeBridgeSubsystem* Owner = WeakThis.Get()) {
                TSharedPtr<FJsonObject> EventResult = McpHandlerUtils::CreateResultObject();
                EventResult->SetStringField(TEXT("asyncId"), AsyncId);
                EventResult->SetBoolField(TEXT("succeeded"), bSuccess);
                EventResult->SetBoolField(TEXT("cancelled"), State->bCancelled.load());
                EventResult->SetStringField(TEXT("state"), State->bCancelled.load() ? TEXT("cancelled") : (bSuccess ? TEXT("completed") : TEXT("failed")));
                SendManagedLifecycleEvent(Owner, TEXT("save_game_completed"), AsyncId, EventResult);
              }
            }));
        TSharedPtr<FJsonObject> AsyncResult = McpHandlerUtils::CreateResultObject();
        AsyncResult->SetStringField(TEXT("asyncId"), AsyncId);
        AsyncResult->SetStringField(TEXT("slotName"), SlotName);
        AsyncResult->SetNumberField(TEXT("userIndex"), UserIndex);
        AsyncResult->SetStringField(TEXT("state"), TEXT("running"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SaveGame write started"), AsyncResult, FString());
        return true;
      }
      const bool bSaved = UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, UserIndex);
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("slotName"), SlotName);
      Result->SetNumberField(TEXT("userIndex"), UserIndex);
      Result->SetBoolField(TEXT("saved"), bSaved);
      SendAutomationResponse(RequestingSocket, RequestId, bSaved, bSaved ? TEXT("SaveGame slot written") : TEXT("SaveGame slot write failed"), Result, bSaved ? FString() : TEXT("SAVE_FAILED"));
      return true;
    }
    if (Lower == TEXT("load_game_from_slot")) {
      bool bAsyncLoad = false;
      Payload->TryGetBoolField(TEXT("async"), bAsyncLoad);
      if (bAsyncLoad) {
        if (!CanRegisterManagedAsyncAction()) {
          SendAutomationError(RequestingSocket, RequestId, TEXT("Too many active or recently completed async actions"), TEXT("ASYNC_ACTION_CAPACITY"));
          return true;
        }
        const FString AsyncId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        const TSharedPtr<FMcpAsyncState> State = MakeShared<FMcpAsyncState>();
        FMcpAsyncRecord Record;
        Record.AsyncId = AsyncId;
        Record.Execution = TEXT("load_game");
        Record.Label = FString::Printf(TEXT("LoadGame:%s"), *SlotName);
        Record.State = State;
        ManagedAsyncActions.Add(AsyncId, Record);
        const TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
        UGameplayStatics::AsyncLoadGameFromSlot(SlotName, UserIndex,
            FAsyncLoadGameFromSlotDelegate::CreateLambda([WeakThis, AsyncId, State](const FString&, const int32, USaveGame* LoadedGame) {
              const bool bSuccess = LoadedGame != nullptr;
              State->bSucceeded.store(bSuccess);
              State->bCompleted.store(true);
              if (UNebulaForgeBridgeSubsystem* Owner = WeakThis.Get()) {
                TSharedPtr<FJsonObject> EventResult = McpHandlerUtils::CreateResultObject();
                EventResult->SetStringField(TEXT("asyncId"), AsyncId);
                EventResult->SetBoolField(TEXT("succeeded"), bSuccess);
                EventResult->SetBoolField(TEXT("cancelled"), State->bCancelled.load());
                EventResult->SetStringField(TEXT("state"), State->bCancelled.load() ? TEXT("cancelled") : (bSuccess ? TEXT("completed") : TEXT("failed")));
                if (LoadedGame) EventResult->SetStringField(TEXT("classPath"), LoadedGame->GetClass()->GetPathName());
                SendManagedLifecycleEvent(Owner, TEXT("load_game_completed"), AsyncId, EventResult);
              }
            }));
        TSharedPtr<FJsonObject> AsyncResult = McpHandlerUtils::CreateResultObject();
        AsyncResult->SetStringField(TEXT("asyncId"), AsyncId);
        AsyncResult->SetStringField(TEXT("slotName"), SlotName);
        AsyncResult->SetNumberField(TEXT("userIndex"), UserIndex);
        AsyncResult->SetStringField(TEXT("state"), TEXT("running"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SaveGame load started"), AsyncResult, FString());
        return true;
      }
      USaveGame *Loaded = UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex);
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("slotName"), SlotName);
      Result->SetNumberField(TEXT("userIndex"), UserIndex);
      Result->SetBoolField(TEXT("exists"), Loaded != nullptr);
      if (Loaded) Result->SetStringField(TEXT("classPath"), Loaded->GetClass()->GetPathName());
      SendAutomationResponse(RequestingSocket, RequestId, Loaded != nullptr, Loaded ? TEXT("SaveGame slot loaded") : TEXT("SaveGame slot not found"), Result, Loaded ? FString() : TEXT("SLOT_NOT_FOUND"));
      return true;
    }
    if (Lower == TEXT("delete_save_game_slot")) {
      const bool bDeleted = UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("slotName"), SlotName);
      Result->SetNumberField(TEXT("userIndex"), UserIndex);
      Result->SetBoolField(TEXT("deleted"), bDeleted);
      SendAutomationResponse(RequestingSocket, RequestId, bDeleted, bDeleted ? TEXT("SaveGame slot deleted") : TEXT("SaveGame slot delete failed"), Result, bDeleted ? FString() : TEXT("DELETE_FAILED"));
      return true;
    }
    if (Lower == TEXT("check_save_game_slot")) {
      const bool bExists = UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("slotName"), SlotName);
      Result->SetNumberField(TEXT("userIndex"), UserIndex);
      Result->SetBoolField(TEXT("exists"), bExists);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SaveGame slot status retrieved"), Result, FString());
      return true;
    }
    TArray<FString> SlotFiles;
    const FString SaveDirectory = FPaths::ProjectSavedDir() / TEXT("SaveGames/");
    IFileManager::Get().FindFiles(SlotFiles, *(SaveDirectory / TEXT("*.sav")), true, false);
    TArray<TSharedPtr<FJsonValue>> Slots;
    for (const FString &FileName : SlotFiles) {
      Slots.Add(MakeShared<FJsonValueString>(FPaths::GetBaseFilename(FileName)));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("slots"), Slots);
    Result->SetNumberField(TEXT("count"), Slots.Num());
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SaveGame slots listed"), Result, FString());
    return true;
  }

  if (Lower == TEXT("get_runtime_gameplay_tag")) {
    FString TagName;
    Payload->TryGetStringField(TEXT("tag"), TagName);
    TagName.TrimStartAndEndInline();
    if (TagName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("tag is required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagName), false);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("requestedTag"), TagName);
    Result->SetBoolField(TEXT("valid"), Tag.IsValid());
    Result->SetStringField(TEXT("canonicalTag"), Tag.IsValid() ? Tag.ToString() : FString());
    if (Tag.IsValid()) {
      const FGameplayTag Parent = Tag.RequestDirectParent();
      Result->SetStringField(TEXT("parentTag"), Parent.IsValid() ? Parent.ToString() : FString());
    }
    SendAutomationResponse(RequestingSocket, RequestId, true,
                            Tag.IsValid() ? TEXT("Runtime Gameplay Tag is registered") : TEXT("Runtime Gameplay Tag is not registered"),
                            Result, FString());
    return true;
  }

  if (Lower == TEXT("capture_insights_trace")) {
    return HandleInsightsAction(RequestId, TEXT("manage_insights"), Payload, RequestingSocket);
  }

  if (Lower == TEXT("start_session") ||
      Lower == TEXT("stop_session") ||
      Lower == TEXT("get_session_status")) {
    return HandleInsightsAction(RequestId, TEXT("manage_insights"), Payload, RequestingSocket);
  }

  if (Lower == TEXT("check_map_errors")) {
    if (!GEditor) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Editor is not available."), TEXT("EDITOR_UNAVAILABLE"));
      return true;
    }
    UWorld *EditorWorld = GEditor->GetEditorWorldContext().World();
    if (!EditorWorld) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No editor world is loaded."), TEXT("WORLD_UNAVAILABLE"));
      return true;
    }

    FMessageLog MapCheckLog(FName(TEXT("MapCheck")));
    MapCheckLog.NewPage(FText::FromString(RequestId));
    const bool bHandled = GEditor->Exec(EditorWorld, TEXT("MAP CHECK"));
    const int32 ErrorCount = MapCheckLog.NumMessages(EMessageSeverity::Error);
    const int32 WarningCount = MapCheckLog.NumMessages(EMessageSeverity::Warning);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("action"), TEXT("check_map_errors"));
    Result->SetBoolField(TEXT("commandHandled"), bHandled);
    Result->SetStringField(TEXT("map"), EditorWorld->GetMapName());
    Result->SetNumberField(TEXT("errorCount"), ErrorCount);
    Result->SetNumberField(TEXT("warningCount"), WarningCount);
    Result->SetBoolField(TEXT("valid"), bHandled && ErrorCount == 0);
    SendAutomationResponse(RequestingSocket, RequestId, bHandled && ErrorCount == 0,
                            bHandled && ErrorCount == 0 ? TEXT("Map check passed.") : TEXT("Map check found errors or could not be executed."), Result,
                            bHandled && ErrorCount == 0 ? FString() : TEXT("MAP_CHECK_FAILED"));
    return true;
  }

  if (Lower == TEXT("create_functional_test")) {
    if (!GEditor) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Editor is not available."), TEXT("EDITOR_UNAVAILABLE"));
      return true;
    }
    UWorld *EditorWorld = GEditor->GetEditorWorldContext().World();
    if (!EditorWorld) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No editor world is loaded."), TEXT("WORLD_UNAVAILABLE"));
      return true;
    }
    FString TestName;
    Payload->TryGetStringField(TEXT("testName"), TestName);
    TestName.TrimStartAndEndInline();
    if (TestName.IsEmpty() || TestName.Len() > 128 || !FChar::IsAlpha(TestName[0])) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("testName must start with a letter and be at most 128 characters."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    for (const TCHAR Character : TestName) {
      if (!FChar::IsAlnum(Character) && Character != TEXT('_')) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("testName may contain only letters, numbers, and underscores."), TEXT("INVALID_ARGUMENT"));
        return true;
      }
    }
    bool bSaveLevel = false;
    const bool bHasSaveLevel = Payload->TryGetBoolField(TEXT("saveLevel"), bSaveLevel);
    UClass *FunctionalTestClass = FindObject<UClass>(nullptr, TEXT("/Script/FunctionalTesting.FunctionalTest"));
    if (!FunctionalTestClass || !FunctionalTestClass->IsChildOf(AActor::StaticClass())) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("FunctionalTesting plugin is unavailable or not loaded."), TEXT("FUNCTIONAL_TESTING_UNAVAILABLE"));
      return true;
    }
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Name = MakeUniqueObjectName(EditorWorld, FunctionalTestClass, FName(*TestName));
    SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
    AActor *TestActor = EditorWorld->SpawnActor<AActor>(FunctionalTestClass, FTransform::Identity, SpawnParameters);
    if (!TestActor) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to spawn the functional test actor."), TEXT("SPAWN_FAILED"));
      return true;
    }
    TestActor->SetActorLabel(TestName, true);
    TestActor->Modify();
    bool bEnabled = true;
    const bool bHasEnabled = Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
    double TimeLimit = 0.0;
    const bool bHasTimeLimit = Payload->TryGetNumberField(TEXT("timeLimit"), TimeLimit);
    if (bHasTimeLimit && (!FMath::IsFinite(TimeLimit) || TimeLimit < 0.0 || TimeLimit > 3600.0)) {
      TestActor->Destroy();
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("timeLimit must be a finite value between 0 and 3600 seconds."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    bool bEnabledApplied = false;
    bool bTimeLimitApplied = false;
    if (bHasEnabled) {
      if (FBoolProperty *EnabledProperty = FindFProperty<FBoolProperty>(FunctionalTestClass, TEXT("bIsEnabled"))) {
        EnabledProperty->SetPropertyValue_InContainer(TestActor, bEnabled);
        bEnabledApplied = true;
      }
    }
    if (bHasTimeLimit) {
      if (FNumericProperty *TimeLimitProperty = FindFProperty<FNumericProperty>(FunctionalTestClass, TEXT("TimeLimit"))) {
        TimeLimitProperty->SetFloatingPointPropertyValue(
            TimeLimitProperty->ContainerPtrToValuePtr<void>(TestActor), TimeLimit);
        bTimeLimitApplied = true;
      }
    }
    TestActor->MarkPackageDirty();
    bool bLevelSaved = false;
    if (bHasSaveLevel && bSaveLevel) {
      bLevelSaved = McpSafeLevelSave(EditorWorld->PersistentLevel, EditorWorld->GetOutermost()->GetName());
      if (!bLevelSaved) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Functional test actor was created but the level could not be saved."), TEXT("SAVE_FAILED"));
        return true;
      }
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("action"), TEXT("create_functional_test"));
    Result->SetStringField(TEXT("testName"), TestName);
    Result->SetStringField(TEXT("objectPath"), TestActor->GetPathName());
    Result->SetStringField(TEXT("classPath"), FunctionalTestClass->GetPathName());
    Result->SetBoolField(TEXT("enabled"), bEnabled);
    Result->SetBoolField(TEXT("enabledApplied"), bEnabledApplied);
    Result->SetBoolField(TEXT("saveLevel"), bSaveLevel);
    Result->SetBoolField(TEXT("levelSaved"), bLevelSaved);
    if (bHasTimeLimit) {
      Result->SetNumberField(TEXT("timeLimit"), TimeLimit);
      Result->SetBoolField(TEXT("timeLimitApplied"), bTimeLimitApplied);
    }
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Functional test actor created; configure assertions and run it with run_tests."), Result);
    return true;
  }

  if (Lower == TEXT("validate_assets")) {
    TArray<FString> PathsToValidate;

    const TArray<TSharedPtr<FJsonValue>>* PathsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("paths"), PathsArray) && PathsArray) {
      for (const TSharedPtr<FJsonValue>& PathValue : *PathsArray) {
        if (PathValue.IsValid() && PathValue->Type == EJson::String) {
          FString Path = PathValue->AsString();
          Path.TrimStartAndEndInline();
          if (!Path.IsEmpty()) {
            PathsToValidate.Add(Path);
          }
        }
      }
    }

    FString SinglePath;
    if (Payload->TryGetStringField(TEXT("assetPath"), SinglePath) ||
        Payload->TryGetStringField(TEXT("path"), SinglePath)) {
      SinglePath.TrimStartAndEndInline();
      if (!SinglePath.IsEmpty()) {
        PathsToValidate.AddUnique(SinglePath);
      }
    }

    if (PathsToValidate.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("validate_assets requires paths, assetPath, or path"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    const bool bRecursive = Payload->HasField(TEXT("recursive"))
      ? GetJsonBoolField(Payload, TEXT("recursive"))
      : true;
    TArray<TSharedPtr<FJsonValue>> Results;
    bool bAllValid = true;

    auto AddValidationResult = [&](const FString& OriginalPath, bool bSuccess,
                                   const FString& Kind, const FString& Message,
                                   int32 AssetCount = INDEX_NONE) {
      TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
      Item->SetStringField(TEXT("path"), OriginalPath);
      Item->SetBoolField(TEXT("success"), bSuccess);
      Item->SetBoolField(TEXT("isValid"), bSuccess);
      Item->SetStringField(TEXT("kind"), Kind);
      Item->SetStringField(TEXT("message"), Message);
      if (AssetCount != INDEX_NONE) {
        Item->SetNumberField(TEXT("assetCount"), AssetCount);
      }
      Results.Add(MakeShared<FJsonValueObject>(Item));
      bAllValid = bAllValid && bSuccess;
    };

    for (const FString& RawPath : PathsToValidate) {
      FString Path = RawPath;
      if (Path.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
        Path = FString::Printf(TEXT("/Game%s"), *Path.RightChop(8));
      }

      const FString SafePath = SanitizeProjectRelativePath(Path);
      if (SafePath.IsEmpty()) {
        AddValidationResult(RawPath, false, TEXT("invalid"),
                            TEXT("Invalid or unsafe asset path"));
        continue;
      }

      if (UEditorAssetLibrary::DoesAssetExist(SafePath)) {
        UObject* Asset = UEditorAssetLibrary::LoadAsset(SafePath);
        AddValidationResult(SafePath, Asset != nullptr, TEXT("asset"),
                            Asset ? TEXT("Asset loaded successfully") : TEXT("Asset exists but failed to load"));
        continue;
      }

      if (UEditorAssetLibrary::DoesDirectoryExist(SafePath)) {
        TArray<FString> Assets = UEditorAssetLibrary::ListAssets(SafePath, bRecursive, false);
        AddValidationResult(SafePath, true, TEXT("directory"), TEXT("Directory exists"), Assets.Num());
        continue;
      }

      AddValidationResult(SafePath, false, TEXT("missing"), TEXT("Asset or directory not found"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bAllValid);
    Result->SetBoolField(TEXT("isValid"), bAllValid);
    Result->SetArrayField(TEXT("results"), Results);
    Result->SetNumberField(TEXT("checkedCount"), Results.Num());

    SendAutomationResponse(RequestingSocket, RequestId, bAllValid,
                           bAllValid ? TEXT("Asset validation completed")
                                     : TEXT("Asset validation failed"),
                           Result, bAllValid ? FString() : TEXT("VALIDATION_FAILED"));
    return true;
  }

  if (Lower == TEXT("validate_blueprints")) {
    TArray<FString> CandidatePaths;
    const TArray<TSharedPtr<FJsonValue>>* PathsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("paths"), PathsArray) && PathsArray) {
      for (const TSharedPtr<FJsonValue>& Value : *PathsArray) {
        if (Value.IsValid() && Value->Type == EJson::String) {
          FString Path = Value->AsString();
          Path.TrimStartAndEndInline();
          if (!Path.IsEmpty()) CandidatePaths.Add(Path);
        }
      }
    }

    bool bRecursive = true;
    if (Payload->HasField(TEXT("recursive"))) {
      Payload->TryGetBoolField(TEXT("recursive"), bRecursive);
    }
    int32 MaxAssets = 1000;
    double MaxAssetsNumber = 0.0;
    if (Payload->TryGetNumberField(TEXT("maxAssets"), MaxAssetsNumber)) {
      MaxAssets = FMath::Clamp(FMath::RoundToInt(MaxAssetsNumber), 1, 5000);
    }

    if (CandidatePaths.IsEmpty()) CandidatePaths.Add(TEXT("/Game"));
    TArray<FString> BlueprintPaths;
    for (const FString& RawPath : CandidatePaths) {
      FString Path = RawPath;
      if (Path.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
        Path = FString::Printf(TEXT("/Game%s"), *Path.RightChop(8));
      }
      const FString SafePath = SanitizeProjectRelativePath(Path);
      if (SafePath.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("paths must contain safe Unreal asset or directory paths"), TEXT("INVALID_PATH"));
        return true;
      }
      if (UEditorAssetLibrary::DoesAssetExist(SafePath)) {
        UObject* Asset = UEditorAssetLibrary::LoadAsset(SafePath);
        if (Cast<UBlueprint>(Asset)) BlueprintPaths.AddUnique(SafePath);
      } else if (UEditorAssetLibrary::DoesDirectoryExist(SafePath)) {
        const TArray<FString> Assets = UEditorAssetLibrary::ListAssets(SafePath, bRecursive, false);
        for (const FString& AssetPath : Assets) {
          if (BlueprintPaths.Num() >= MaxAssets) break;
          UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
          if (Cast<UBlueprint>(Asset)) BlueprintPaths.AddUnique(AssetPath);
        }
      } else {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Blueprint path or directory not found: %s"), *SafePath), TEXT("ASSET_NOT_FOUND"));
        return true;
      }
      if (BlueprintPaths.Num() >= MaxAssets) break;
    }

    bool bSaveAfterCompile = false;
    Payload->TryGetBoolField(TEXT("saveAfterCompile"), bSaveAfterCompile);
    TArray<TSharedPtr<FJsonValue>> Results;
    int32 ErrorCount = 0;
    int32 WarningCount = 0;
    for (const FString& BlueprintPath : BlueprintPaths) {
      UBlueprint* Blueprint = Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
      if (!Blueprint) continue;
      FCompilerResultsLog CompilerResults;
      FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &CompilerResults);
      const bool bCompiled = CompilerResults.NumErrors == 0 &&
          (Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings);
      ErrorCount += CompilerResults.NumErrors;
      WarningCount += CompilerResults.NumWarnings;
      if (bSaveAfterCompile && bCompiled) McpSafeAssetSave(Blueprint);
      TSharedPtr<FJsonObject> Item = McpHandlerUtils::CreateResultObject();
      Item->SetStringField(TEXT("assetPath"), BlueprintPath);
      Item->SetBoolField(TEXT("compiled"), bCompiled);
      Item->SetNumberField(TEXT("compilerErrorCount"), CompilerResults.NumErrors);
      Item->SetNumberField(TEXT("compilerWarningCount"), CompilerResults.NumWarnings);
      TArray<TSharedPtr<FJsonValue>> Messages;
      for (const TSharedRef<FTokenizedMessage>& Message : CompilerResults.Messages) {
        Messages.Add(MakeShared<FJsonValueString>(Message->ToText().ToString()));
      }
      Item->SetArrayField(TEXT("compilerMessages"), Messages);
      Results.Add(MakeShared<FJsonValueObject>(Item));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), ErrorCount == 0);
    Result->SetNumberField(TEXT("checkedCount"), Results.Num());
    Result->SetNumberField(TEXT("errorCount"), ErrorCount);
    Result->SetNumberField(TEXT("warningCount"), WarningCount);
    Result->SetBoolField(TEXT("truncated"), BlueprintPaths.Num() >= MaxAssets);
    Result->SetArrayField(TEXT("results"), Results);
    SendAutomationResponse(RequestingSocket, RequestId, ErrorCount == 0,
        ErrorCount == 0 ? TEXT("Blueprint validation completed") : TEXT("Blueprint validation found compiler errors"),
        Result, ErrorCount == 0 ? FString() : TEXT("BLUEPRINT_VALIDATION_FAILED"));
    return true;
  }

  if (Lower == TEXT("start_memory_report")) {
    if (!GEngine || !GEngine->Exec(nullptr, TEXT("memreport -full"))) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to start Unreal memory report"), TEXT("MEMORY_REPORT_FAILED"));
      return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("command"), TEXT("memreport -full"));
    Result->SetBoolField(TEXT("commandHandled"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Memory report requested; inspect the Unreal log for the generated report path."), Result);
    return true;
  }

  if (Lower == TEXT("configure_stat_commands")) {
    const TArray<TSharedPtr<FJsonValue>>* StatValues = nullptr;
    if (!Payload->TryGetArrayField(TEXT("statNames"), StatValues) || !StatValues || StatValues->IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("configure_stat_commands requires a non-empty statNames array"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    bool bEnabled = true;
    if (Payload->HasField(TEXT("enabled"))) Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
    TArray<TSharedPtr<FJsonValue>> Applied;
    TArray<TSharedPtr<FJsonValue>> Rejected;
    for (const TSharedPtr<FJsonValue>& Value : *StatValues) {
      const FString StatName = Value.IsValid() && Value->Type == EJson::String ? Value->AsString().TrimStartAndEnd() : FString();
      bool bSafeStatName = !StatName.IsEmpty() && StatName.Len() <= 64 &&
          ((StatName[0] >= TEXT('A') && StatName[0] <= TEXT('Z')) || (StatName[0] >= TEXT('a') && StatName[0] <= TEXT('z')));
      for (int32 Index = 1; bSafeStatName && Index < StatName.Len(); ++Index) {
        const TCHAR Character = StatName[Index];
        bSafeStatName = (Character >= TEXT('A') && Character <= TEXT('Z')) || (Character >= TEXT('a') && Character <= TEXT('z')) || (Character >= TEXT('0') && Character <= TEXT('9')) || Character == TEXT('_');
      }
      if (!bSafeStatName) {
        Rejected.Add(MakeShared<FJsonValueString>(StatName));
        continue;
      }
      const FString Command = FString::Printf(TEXT("stat %s%s"), *StatName, bEnabled ? TEXT("") : TEXT(" 0"));
      if (GEngine && GEngine->Exec(nullptr, *Command)) Applied.Add(MakeShared<FJsonValueString>(StatName));
      else Rejected.Add(MakeShared<FJsonValueString>(StatName));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), Rejected.IsEmpty() && !Applied.IsEmpty());
    Result->SetBoolField(TEXT("enabled"), bEnabled);
    Result->SetArrayField(TEXT("applied"), Applied);
    Result->SetArrayField(TEXT("rejected"), Rejected);
    Result->SetNumberField(TEXT("appliedCount"), Applied.Num());
    Result->SetNumberField(TEXT("rejectedCount"), Rejected.Num());
    SendAutomationResponse(RequestingSocket, RequestId, Rejected.IsEmpty() && !Applied.IsEmpty(),
        Rejected.IsEmpty() ? TEXT("Stat commands configured") : TEXT("One or more stat commands were rejected"), Result,
        Rejected.IsEmpty() ? FString() : TEXT("STAT_CONFIGURATION_FAILED"));
    return true;
  }

  if (Lower == TEXT("configure_console_variables")) {
    const TArray<TSharedPtr<FJsonValue>>* VariableValues = nullptr;
    if (!Payload->TryGetArrayField(TEXT("consoleVariables"), VariableValues) ||
        !VariableValues || VariableValues->IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("configure_console_variables requires a non-empty consoleVariables array"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TArray<TSharedPtr<FJsonValue>> Applied;
    TArray<TSharedPtr<FJsonValue>> Rejected;
    for (const TSharedPtr<FJsonValue>& Value : *VariableValues) {
      const TSharedPtr<FJsonObject>* Entry = nullptr;
      if (!Value.IsValid() || !Value->TryGetObject(Entry) || !Entry || !Entry->IsValid()) {
        Rejected.Add(MakeShared<FJsonValueString>(TEXT("<invalid>")));
        continue;
      }
      FString Name;
      FString NewValue;
      (*Entry)->TryGetStringField(TEXT("name"), Name);
      (*Entry)->TryGetStringField(TEXT("value"), NewValue);
      Name.TrimStartAndEndInline();
      NewValue.TrimStartAndEndInline();
      bool bSafeName = !Name.IsEmpty() && Name.Len() <= 128;
      for (const TCHAR Character : Name) {
        bSafeName = bSafeName && (FChar::IsAlnum(Character) || Character == TEXT('.') ||
                                  Character == TEXT('_') || Character == TEXT('-'));
      }
      if (!bSafeName || NewValue.Len() > 1024 || NewValue.Contains(TEXT("\n")) || NewValue.Contains(TEXT("\r"))) {
        Rejected.Add(MakeShared<FJsonValueString>(Name.IsEmpty() ? TEXT("<invalid>") : Name));
        continue;
      }
      IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name);
      if (!Variable) {
        Rejected.Add(MakeShared<FJsonValueString>(Name));
        continue;
      }
      Variable->Set(*NewValue, ECVF_SetByCode);
      TSharedPtr<FJsonObject> AppliedEntry = McpHandlerUtils::CreateResultObject();
      AppliedEntry->SetStringField(TEXT("name"), Name);
      AppliedEntry->SetStringField(TEXT("value"), Variable->GetString());
      Applied.Add(MakeShared<FJsonValueObject>(AppliedEntry));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("applied"), Applied);
    Result->SetArrayField(TEXT("rejected"), Rejected);
    Result->SetNumberField(TEXT("appliedCount"), Applied.Num());
    Result->SetNumberField(TEXT("rejectedCount"), Rejected.Num());
    const bool bSuccess = Rejected.IsEmpty() && !Applied.IsEmpty();
    Result->SetBoolField(TEXT("success"), bSuccess);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
                            bSuccess ? TEXT("Console variables configured") : TEXT("One or more console variables were rejected"),
                            Result, bSuccess ? FString() : TEXT("CVAR_CONFIGURATION_FAILED"));
    return true;
  }

  if (Lower == TEXT("check_for_errors")) {
    const TArray<TSharedPtr<FJsonValue>>* CategoryValues = nullptr;
    TArray<FString> Categories;
    if (Payload->TryGetArrayField(TEXT("logCategories"), CategoryValues) && CategoryValues) {
      for (const TSharedPtr<FJsonValue>& Value : *CategoryValues) {
        if (!Value.IsValid() || Value->Type != EJson::String) continue;
        FString Category = Value->AsString().TrimStartAndEnd();
        if (!Category.IsEmpty() && Category.Len() <= 64 && Category.Contains(FString(TEXT("/"))) == false && Category.Contains(FString(TEXT("\\"))) == false) {
          Categories.AddUnique(Category);
        }
      }
    }
    if (Categories.IsEmpty()) {
      Categories = { TEXT("MapCheck"), TEXT("BlueprintLog"), TEXT("LoadErrors"), TEXT("DataValidation") };
    }
    TArray<TSharedPtr<FJsonValue>> Results;
    int32 ErrorCount = 0;
    int32 WarningCount = 0;
    for (const FString& Category : Categories) {
FMessageLog LogListing{FName(*Category)};
      const int32 CategoryErrors = LogListing.NumMessages(EMessageSeverity::Error);
      const int32 CategoryWarnings = LogListing.NumMessages(EMessageSeverity::Warning);
      ErrorCount += CategoryErrors;
      WarningCount += CategoryWarnings;
      TSharedPtr<FJsonObject> Item = McpHandlerUtils::CreateResultObject();
      Item->SetStringField(TEXT("category"), Category);
      Item->SetNumberField(TEXT("errorCount"), CategoryErrors);
      Item->SetNumberField(TEXT("warningCount"), CategoryWarnings);
      Item->SetBoolField(TEXT("valid"), CategoryErrors == 0);
      Results.Add(MakeShared<FJsonValueObject>(Item));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), ErrorCount == 0);
    Result->SetNumberField(TEXT("errorCount"), ErrorCount);
    Result->SetNumberField(TEXT("warningCount"), WarningCount);
    Result->SetArrayField(TEXT("categories"), Results);
    SendAutomationResponse(RequestingSocket, RequestId, ErrorCount == 0,
        ErrorCount == 0 ? TEXT("No errors found in requested message-log categories") : TEXT("Editor errors found in requested message-log categories"),
        Result, ErrorCount == 0 ? FString() : TEXT("EDITOR_ERRORS_FOUND"));
    return true;
  }

  if (Lower == TEXT("start_network_profiler")) {
    bool bEnabled = true;
    if (Payload->HasField(TEXT("enabled"))) Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
    const FString Command = bEnabled ? TEXT("netprofile enable") : TEXT("netprofile disable");
    const bool bHandled = GEngine && GEngine->Exec(nullptr, *Command);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("command"), Command);
    Result->SetBoolField(TEXT("enabled"), bEnabled);
    Result->SetBoolField(TEXT("commandHandled"), bHandled);
    Result->SetStringField(TEXT("outputDirectory"), FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Profiling")));
    SendAutomationResponse(RequestingSocket, RequestId, bHandled,
        bHandled ? TEXT("Network profiler state updated.") : TEXT("Network profiler command was not handled."), Result,
        bHandled ? FString() : TEXT("NETWORK_PROFILER_FAILED"));
    return true;
  }

  if (Lower == TEXT("enable_visual_logger")) {
    bool bEnabled = true;
    if (Payload->HasField(TEXT("enabled"))) Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
    UE_LOG(LogTemp, Display, TEXT("NebulaForge Visual Logger recording requested: %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("enabled"), bEnabled);
    Result->SetBoolField(TEXT("recordingRequested"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
        bEnabled ? TEXT("Visual Logger recording enabled.") : TEXT("Visual Logger recording disabled."), Result);
    return true;
  }

  if (Lower == TEXT("add_visual_log_entry")) {
    FString Text;
    if (!Payload->TryGetStringField(TEXT("visualLogText"), Text) || Text.TrimStartAndEnd().IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("visualLogText is required for add_visual_log_entry"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Text.TrimStartAndEndInline();
    if (Text.Len() > 4096) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("visualLogText must be at most 4096 characters"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    FString Category = TEXT("MCP");
    Payload->TryGetStringField(TEXT("visualLogCategory"), Category);
    Category.TrimStartAndEndInline();
    if (Category.IsEmpty() || Category.Len() > 64) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("visualLogCategory must be 1-64 characters"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    UE_LOG(LogTemp, Display, TEXT("NebulaForge Visual Log [%s]: %s"), *Category, *Text);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("text"), Text);
    Result->SetStringField(TEXT("category"), Category);
    Result->SetBoolField(TEXT("logged"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Visual log entry added."), Result);
    return true;
  }

  if (Lower == TEXT("run_ubt")) {
    // Extract optional parameters
    FString Target;
    Payload->TryGetStringField(TEXT("target"), Target);

    FString Platform;
    Payload->TryGetStringField(TEXT("platform"), Platform);

    FString Configuration;
    Payload->TryGetStringField(TEXT("configuration"), Configuration);

    FString AdditionalArgs;
    Payload->TryGetStringField(TEXT("additionalArgs"), AdditionalArgs);
    if (AdditionalArgs.IsEmpty()) {
      Payload->TryGetStringField(TEXT("arguments"), AdditionalArgs);
    }

    Target.TrimStartAndEndInline();
    Platform.TrimStartAndEndInline();
    Configuration.TrimStartAndEndInline();
    AdditionalArgs.TrimStartAndEndInline();

    auto ValidateBuildToken = [&](const FString& Value, const TCHAR* FieldName) -> bool {
      if (!McpIsSafeUbtPositionalToken(Value)) {
        SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid %s for run_ubt: %s must be a positional token"), FieldName, *Value),
            TEXT("INVALID_ARGUMENT"));
        return false;
      }
      return true;
    };

    if (Target.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Target is required for run_ubt"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (Platform.IsEmpty()) {
#if PLATFORM_WINDOWS
      Platform = TEXT("Win64");
#elif PLATFORM_MAC
      Platform = TEXT("Mac");
#else
      Platform = TEXT("Linux");
#endif
    }

    if (Configuration.IsEmpty()) {
      Configuration = TEXT("Development");
    }

    if (!ValidateBuildToken(Target, TEXT("target")) ||
        !ValidateBuildToken(Platform, TEXT("platform")) ||
        !ValidateBuildToken(Configuration, TEXT("configuration"))) {
      return true;
    }

    if (!McpIsAllowedUbtPlatform(Platform)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Platform is not allowed for run_ubt: %s"), *Platform),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!McpIsAllowedUbtConfiguration(Configuration)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Configuration is not allowed for run_ubt: %s"), *Configuration),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!McpIsSafeUbtArgumentList(AdditionalArgs)) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("additionalArgs contains unsafe UBT argument characters"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Build UBT path
    FString EngineDir = FPaths::EngineDir();
    FString UBTPath;

#if PLATFORM_WINDOWS
    UBTPath = FPaths::Combine(EngineDir, TEXT("Build/BatchFiles/Build.bat"));
#elif PLATFORM_MAC
    UBTPath = FPaths::Combine(EngineDir, TEXT("Build/BatchFiles/Mac/Build.sh"));
#else
    UBTPath = FPaths::Combine(EngineDir, TEXT("Build/BatchFiles/Linux/Build.sh"));
#endif

    if (!FPaths::FileExists(UBTPath)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("UBT not found at: %s"), *UBTPath),
                          TEXT("UBT_NOT_FOUND"));
      return true;
    }

    // Build command line arguments
    FString Arguments;

    // Target (project or engine target)
    Arguments += Target + TEXT(" ");

    // Platform
    if (!Platform.IsEmpty()) {
      Arguments += Platform + TEXT(" ");
    } else {
#if PLATFORM_WINDOWS
      Arguments += TEXT("Win64 ");
#elif PLATFORM_MAC
      Arguments += TEXT("Mac ");
#else
      Arguments += TEXT("Linux ");
#endif
    }

    // Configuration
    if (!Configuration.IsEmpty()) {
      Arguments += Configuration + TEXT(" ");
    } else {
      Arguments += TEXT("Development ");
    }

    const FString ProjectPath = FPaths::GetProjectFilePath();
    if (!ProjectPath.IsEmpty()) {
      Arguments += FString::Printf(TEXT("-Project=\"%s\" "), *ProjectPath);
    }

    // Additional args
    if (!AdditionalArgs.IsEmpty()) {
      Arguments += AdditionalArgs;
    }

    // Use FMonitoredProcess for non-blocking execution with output capture
    // For simplicity, we'll use a synchronous approach with timeout
    int32 ReturnCode = -1;
    FString StdOut;
    FString StdErr;

    // Note: FPlatformProcess::ExecProcess is simpler but blocks
    // Using CreateProc with pipes for better control
    void* ReadPipe = nullptr;
    void* WritePipe = nullptr;
    FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

    FProcHandle ProcessHandle = FPlatformProcess::CreateProc(
        *UBTPath,
        *Arguments,
        false,  // bLaunchDetached
        true,   // bLaunchHidden
        true,   // bLaunchReallyHidden
        nullptr, // OutProcessID
        0,      // PriorityModifier
        nullptr, // OptionalWorkingDirectory
        WritePipe // PipeWriteChild
    );

    if (!ProcessHandle.IsValid()) {
      FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to launch UBT process"),
                          TEXT("PROCESS_LAUNCH_FAILED"));
      return true;
    }

    // Read output with timeout (30 seconds max wait, but check periodically)
    const double TimeoutSeconds = 300.0; // 5 minute timeout for builds
    const double StartTime = FPlatformTime::Seconds();

    while (FPlatformProcess::IsProcRunning(ProcessHandle)) {
      // Read available output
      FString NewOutput = FPlatformProcess::ReadPipe(ReadPipe);
      if (!NewOutput.IsEmpty()) {
        StdOut += NewOutput;
      }

      // Check timeout
      if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds) {
        FPlatformProcess::TerminateProc(ProcessHandle, true);
        FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("output"), StdOut);
        Result->SetBoolField(TEXT("timedOut"), true);

        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("UBT process timed out"), Result,
                               TEXT("TIMEOUT"));
        return true;
      }

      // Small sleep to avoid busy waiting
      FPlatformProcess::Sleep(0.1f);
    }

    // Read any remaining output
    FString FinalOutput = FPlatformProcess::ReadPipe(ReadPipe);
    if (!FinalOutput.IsEmpty()) {
      StdOut += FinalOutput;
    }

    // Get return code
    FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode);
    FPlatformProcess::CloseProc(ProcessHandle);
    FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("output"), StdOut);
    Result->SetNumberField(TEXT("returnCode"), ReturnCode);
    Result->SetStringField(TEXT("ubtPath"), UBTPath);
    Result->SetStringField(TEXT("arguments"), Arguments);

    if (ReturnCode == 0) {
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("UBT completed successfully"), Result);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("UBT failed with code %d"), ReturnCode),
                             Result, TEXT("UBT_FAILED"));
    }
    return true;
  } else if (Lower == TEXT("run_tests")) {
    // Extract test filter
    FString Filter;
    Payload->TryGetStringField(TEXT("filter"), Filter);

    FString TestName;
    Payload->TryGetStringField(TEXT("test"), TestName);

    // If specific test name provided, use it as filter
    if (!TestName.IsEmpty() && Filter.IsEmpty()) {
      Filter = TestName;
    }
    Filter.TrimStartAndEndInline();
    if (!McpIsSafeAutomationTestFilter(Filter)) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Test filter contains unsafe characters"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Build automation test command
    FString TestCommand;
    if (Filter.IsEmpty()) {
      // Run all tests
      TestCommand = TEXT("automation RunAll");
    } else {
      // Run filtered tests
      TestCommand = FString::Printf(TEXT("automation RunTests %s"), *Filter);
    }

    if (!GEngine || !GEditor || !GEditor->GetEditorWorldContext().World()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Editor world not available for running tests"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    if (!ActiveAutomationTestAsyncId.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Automation tests are already running"), TEXT("TESTS_BUSY"));
      return true;
    }
    if (!CanRegisterManagedAsyncAction()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Managed async action capacity is exhausted"),
                          TEXT("ASYNC_CAPACITY_EXCEEDED"));
      return true;
    }

    const FString AsyncId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    const TSharedPtr<FMcpAsyncState> State = MakeShared<FMcpAsyncState>();
    State->bSucceeded.store(true);
    FMcpAsyncRecord Record;
    Record.AsyncId = AsyncId;
    Record.Execution = TEXT("automation_tests");
    Record.Label = Filter.IsEmpty() ? TEXT("automation:all") : FString::Printf(TEXT("automation:%s"), *Filter);
    Record.State = State;
    ManagedAsyncActions.Add(AsyncId, Record);
    ActiveAutomationTestAsyncId = AsyncId;

    FAutomationTestFramework &Framework = FAutomationTestFramework::Get();
    AutomationTestEndDelegateHandle = Framework.OnTestEndEvent.AddLambda(
        [State](FAutomationTestBase *Test) {
          if (Test && !Test->GetLastExecutionSuccessState()) {
            State->bSucceeded.store(false);
          }
        });
    AutomationTestsCompleteDelegateHandle = Framework.OnAfterAllTestsEvent.AddLambda(
        [WeakThis = TWeakObjectPtr<UNebulaForgeBridgeSubsystem>(this), AsyncId, State](void) {
          if (UNebulaForgeBridgeSubsystem *Owner = WeakThis.Get()) {
            if (Owner->AutomationTestEndDelegateHandle.IsValid()) {
              FAutomationTestFramework::Get().OnTestEndEvent.Remove(Owner->AutomationTestEndDelegateHandle);
              Owner->AutomationTestEndDelegateHandle.Reset();
            }
            if (Owner->AutomationTestsCompleteDelegateHandle.IsValid()) {
              FAutomationTestFramework::Get().OnAfterAllTestsEvent.Remove(Owner->AutomationTestsCompleteDelegateHandle);
              Owner->AutomationTestsCompleteDelegateHandle.Reset();
            }
            State->bCompleted.store(true);
            Owner->ActiveAutomationTestAsyncId.Empty();
            TSharedPtr<FJsonObject> EventResult = McpHandlerUtils::CreateResultObject();
            EventResult->SetStringField(TEXT("asyncId"), AsyncId);
            EventResult->SetStringField(TEXT("state"), TEXT("completed"));
            EventResult->SetBoolField(TEXT("succeeded"), State->bSucceeded.load());
            SendManagedLifecycleEvent(Owner, TEXT("automation_tests_completed"), AsyncId, EventResult);
          }
        });

    if (!GEngine->Exec(GEditor->GetEditorWorldContext().World(), *TestCommand)) {
      if (AutomationTestEndDelegateHandle.IsValid()) {
        Framework.OnTestEndEvent.Remove(AutomationTestEndDelegateHandle);
        AutomationTestEndDelegateHandle.Reset();
      }
      if (AutomationTestsCompleteDelegateHandle.IsValid()) {
        Framework.OnAfterAllTestsEvent.Remove(AutomationTestsCompleteDelegateHandle);
        AutomationTestsCompleteDelegateHandle.Reset();
      }
      State->bSucceeded.store(false);
      State->bCompleted.store(true);
      ActiveAutomationTestAsyncId.Empty();
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Unreal rejected the automation test command"), TEXT("TEST_START_FAILED"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("asyncId"), AsyncId);
    Result->SetStringField(TEXT("command"), TestCommand);
    Result->SetStringField(TEXT("filter"), Filter);
    Result->SetStringField(TEXT("state"), TEXT("running"));
    Result->SetBoolField(TEXT("completed"), false);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Automation tests started; poll asyncId with get_async_action"), Result);
    return true;
  } else   if (Lower == TEXT("test_progress_protocol")) {
    // Test action for heartbeat/progress protocol
    // Simulates a long-running operation with progress updates
    UE_LOG(LogNebulaForgeBridgeSubsystem, Log,
           TEXT("test_progress_protocol: Handler called successfully"));
    int32 Steps = 5;
    Payload->TryGetNumberField(TEXT("steps"), Steps);
    Steps = FMath::Clamp(Steps, 1, 20);

    float StepDurationMs = 500.0f;
    Payload->TryGetNumberField(TEXT("stepDurationMs"), StepDurationMs);
    StepDurationMs = FMath::Clamp(StepDurationMs, 100.0f, 5000.0f);

    bool bSendProgress = true;
    if (Payload->HasField(TEXT("sendProgress"))) {
      bSendProgress = Payload->GetBoolField(TEXT("sendProgress"));
    }

    UE_LOG(LogNebulaForgeBridgeSubsystem, Log,
           TEXT("test_progress_protocol: Starting %d steps, %.0fms each, progress=%s"),
           Steps, StepDurationMs, bSendProgress ? TEXT("true") : TEXT("false"));

    for (int32 i = 1; i <= Steps; i++) {
      // Send progress update before each step
      if (bSendProgress) {
        float Percent = (static_cast<float>(i) / static_cast<float>(Steps)) * 100.0f;
        FString StatusMsg = FString::Printf(TEXT("Step %d/%d"), i, Steps);
        SendProgressUpdate(RequestId, Percent, StatusMsg, true);
      }

      // Simulate work by sleeping
      FPlatformProcess::Sleep(StepDurationMs / 1000.0f);
    }

    // Send final progress indicating completion
    if (bSendProgress) {
      SendProgressUpdate(RequestId, 100.0f, TEXT("Complete"), false);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("steps"), Steps);
    Result->SetNumberField(TEXT("stepDurationMs"), StepDurationMs);
    Result->SetBoolField(TEXT("progressSent"), bSendProgress);
    Result->SetStringField(TEXT("message"), TEXT("Progress protocol test completed"));

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Progress protocol test completed successfully"), Result);
    return true;
  } else if (Lower == TEXT("test_stale_progress")) {
    // Test action for stale progress detection
    // Sends the same progress percent multiple times to trigger stale detection
    int32 StaleCount = 5;
    Payload->TryGetNumberField(TEXT("staleCount"), StaleCount);
    StaleCount = FMath::Clamp(StaleCount, 1, 10);

    UE_LOG(LogNebulaForgeBridgeSubsystem, Log,
           TEXT("test_stale_progress: Sending %d stale updates"), StaleCount);

    // Send same progress repeatedly to trigger stale detection
    for (int32 i = 0; i < StaleCount; i++) {
      FString StatusMsg = FString::Printf(TEXT("Stale update %d/%d"), i + 1, StaleCount);
      SendProgressUpdate(RequestId, 50.0f, StatusMsg, true);  // Always 50%
      FPlatformProcess::Sleep(0.1f);  // 100ms between updates
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("staleUpdatesSent"), StaleCount);
    Result->SetBoolField(TEXT("staleDetectionExpected"), true);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Stale progress test completed"), Result);
    return true;
  } else if (Lower == TEXT("export_asset")) {
    // Export asset to FBX/OBJ/other format
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString ExportPath;
    Payload->TryGetStringField(TEXT("exportPath"), ExportPath);

    if (AssetPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("assetPath is required for export"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (SafeAssetPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Invalid asset path for export"),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }

    if (ExportPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("exportPath is required for export"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString SafeExportPath = SanitizeProjectFilePath(ExportPath);
    if (SafeExportPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Invalid or unsafe export path: %s"), *ExportPath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }

    FString AbsoluteExportPath = FPaths::ProjectDir() / SafeExportPath;
    FPaths::MakeStandardFilename(AbsoluteExportPath);

    // CRITICAL: Convert to absolute path for proper comparison
    AbsoluteExportPath = FPaths::ConvertRelativePathToFull(AbsoluteExportPath);
    FPaths::NormalizeFilename(AbsoluteExportPath);

    FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(NormalizedProjectDir);
    if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
      NormalizedProjectDir += TEXT("/");
    }

    // SECURITY: Verify the resolved absolute path is within project bounds
    if (!AbsoluteExportPath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Export path escapes project directory: %s"), *ExportPath),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }

    // Check if asset exists
    if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Asset not found: %s"), *SafeAssetPath),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Ensure export directory exists
    FString ExportDir = FPaths::GetPath(AbsoluteExportPath);
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*ExportDir)) {
      PlatformFile.CreateDirectoryTree(*ExportDir);
    }

    // Load the asset
    UObject* Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath);
    if (!Asset) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Failed to load asset: %s"), *SafeAssetPath),
                          TEXT("LOAD_FAILED"));
      return true;
    }

    // Determine export format from file extension
    FString Extension = FPaths::GetExtension(AbsoluteExportPath).ToLower();

    // Try generic asset export via AssetTools
    bool bExportSuccess = false;
    FString ExportError;

    // CRITICAL FIX: Use AssetTools ExportAssets with explicit export path
    // This performs automated export without showing modal dialogs
    // The bPromptForIndividualFilenames=false suppresses file dialogs
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    IAssetTools& AssetTools = AssetToolsModule.Get();

    // Use ExportAssets with explicit path - this suppresses dialogs for automated export
    // The asset will be exported with its original name to the specified directory
    TArray<UObject*> AssetsToExport;
    AssetsToExport.Add(Asset);

    // ExportAssets exports to the specified directory with the asset's name
    // For custom filename, we need to rename temporarily or use UExporter directly
    AssetTools.ExportAssets(AssetsToExport, ExportDir);

    // Check if file was created
    FString ExpectedExportPath = ExportDir / FPaths::GetBaseFilename(SafeAssetPath) + TEXT(".") + Extension;
    if (FPaths::FileExists(ExpectedExportPath))
    {
      bExportSuccess = true;
    }
    else
    {
      // Try with the actual requested filename
      bExportSuccess = FPaths::FileExists(AbsoluteExportPath);
    }

    if (!bExportSuccess)
    {
      // Fallback: Use UExporter::ExportToFile directly with Prompt=false
      UExporter* Exporter = nullptr;

      // Find appropriate exporter for the asset type and extension
      for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt) {
        UClass* CurrentClass = *ClassIt;
        if (CurrentClass->IsChildOf(UExporter::StaticClass()) && !CurrentClass->HasAnyClassFlags(CLASS_Abstract)) {
          UExporter* DefaultExporter = Cast<UExporter>(CurrentClass->GetDefaultObject());
          if (DefaultExporter && DefaultExporter->SupportedClass) {
            if (Asset->GetClass()->IsChildOf(DefaultExporter->SupportedClass)) {
              if (DefaultExporter->PreferredFormatIndex < DefaultExporter->FormatExtension.Num()) {
                FString PreferredExt = DefaultExporter->FormatExtension[DefaultExporter->PreferredFormatIndex].ToLower();
                if (PreferredExt == Extension || PreferredExt.Contains(Extension)) {
                  Exporter = DefaultExporter;
                  break;
                }
              }
              if (!Exporter) {
                Exporter = DefaultExporter;
              }
            }
          }
        }
      }

      if (Exporter) {
        // ExportToFile signature: (Object, Exporter, Filename, InSelectedOnly, NoReplaceIdentical, Prompt)
        // The last parameter (Prompt=false) should suppress dialogs for most exporters
        int32 ExportResult = UExporter::ExportToFile(Asset, Exporter, *AbsoluteExportPath, false, false, false);
        bExportSuccess = (ExportResult != 0);
      }

      if (!bExportSuccess) {
        ExportError = FString::Printf(TEXT("Export failed for asset type '%s' and format '%s'"),
                                       *Asset->GetClass()->GetName(), *Extension);
      }
    }

    if (bExportSuccess) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      AddAssetVerification(Result, Asset);
      Result->SetStringField(TEXT("assetPath"), SafeAssetPath);
      Result->SetStringField(TEXT("exportPath"), AbsoluteExportPath);
      Result->SetStringField(TEXT("format"), Extension);
      Result->SetBoolField(TEXT("success"), true);

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             FString::Printf(TEXT("Asset exported to: %s"), *AbsoluteExportPath),
                             Result);
    } else {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("assetPath"), SafeAssetPath);
      Result->SetStringField(TEXT("exportPath"), AbsoluteExportPath);
      Result->SetStringField(TEXT("format"), Extension);
      Result->SetStringField(TEXT("error"), ExportError);

      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Export failed: %s"), *ExportError),
                             Result, TEXT("EXPORT_FAILED"));
    }
    return true;
  } else if (Lower == TEXT("execute_python") || Lower == TEXT("execute_python_script") ||
             Lower == TEXT("execute_python_string") || Lower == TEXT("execute_python_file") ||
             Lower == TEXT("configure_python_paths") || Lower == TEXT("list_python_packages") ||
             Lower == TEXT("create_editor_utility_widget") || Lower == TEXT("create_editor_utility_blueprint") ||
             Lower == TEXT("create_python_editor_utility") || Lower == TEXT("register_python_command") ||
             Lower == TEXT("create_geometry_collection") ||
             Lower == TEXT("create_variant_set") ||
             Lower == TEXT("add_variant") ||
             Lower == TEXT("configure_variant_properties") ||
             Lower == TEXT("set_variant_dependencies") ||
             Lower == TEXT("activate_variant") ||
             Lower == TEXT("get_active_variants") ||
             Lower == TEXT("capture_variant_thumbnail") ||
             Lower == TEXT("set_variant_thumbnail") ||
             Lower == TEXT("export_variant_configuration") ||
             Lower == TEXT("add_geometry_to_collection") || Lower == TEXT("remove_geometry_from_collection") ||
             Lower == TEXT("configure_geometry_collection") ||
             Lower == TEXT("inspect_geometry_collection") ||
             Lower == TEXT("configure_geometry_collection_component") ||
             Lower == TEXT("unregister_python_command") ||
             Lower == TEXT("run_editor_utility") || Lower == TEXT("inspect_editor_utility")) {
    // Execute Python code with stdout/stderr capture via temp file wrapper
    FString Code;
    Payload->TryGetStringField(TEXT("code"), Code);
    FString File;
    Payload->TryGetStringField(TEXT("file"), File);

    if (Lower == TEXT("list_python_packages")) {
      Code = TEXT("import importlib.metadata as _m\n"
                  "_names = sorted({d.metadata.get('Name') or d.name for d in _m.distributions()})\n"
                  "print('\\n'.join(_names[:500]))\n");
    }

    if (Lower == TEXT("create_editor_utility_widget")) {
      FString AssetPath;
      FString AssetName;
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
      Payload->TryGetStringField(TEXT("name"), AssetName);
      const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
      if (SafeAssetPath.IsEmpty() || !SafeAssetPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase)) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("create_editor_utility_widget requires a valid /Game asset path"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      const FString DefaultAssetName = FPaths::GetBaseFilename(SafeAssetPath);
      if (AssetName.TrimStartAndEnd().IsEmpty() && DefaultAssetName.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("create_editor_utility_widget requires a non-empty asset name"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      AssetName = SanitizeAssetName(AssetName.TrimStartAndEnd().IsEmpty() ? DefaultAssetName : AssetName);
      FString PythonPackagePath = FPaths::GetPath(SafeAssetPath);
      PythonPackagePath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      AssetName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      Code = FString::Printf(TEXT(
          "import unreal\n"
          "_factory = unreal.EditorUtilityWidgetBlueprintFactory()\n"
          "_factory.set_editor_property('parent_class', unreal.EditorUtilityWidget)\n"
          "_asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset('%s', '%s', unreal.EditorUtilityWidgetBlueprint, _factory)\n"
          "if not _asset:\n"
          "    raise RuntimeError('Editor Utility Widget creation failed')\n"
          "unreal.EditorAssetLibrary.save_asset(_asset.get_path_name())\n"
          "print(_asset.get_path_name())\n"),
          *AssetName, *PythonPackagePath);
    }

    if (Lower == TEXT("create_editor_utility_blueprint") || Lower == TEXT("create_python_editor_utility")) {
      FString AssetPath;
      FString AssetName;
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
      Payload->TryGetStringField(TEXT("name"), AssetName);
      const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
      if (SafeAssetPath.IsEmpty() || !SafeAssetPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase)) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("create_editor_utility_blueprint requires a valid /Game asset path"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      const FString DefaultAssetName = FPaths::GetBaseFilename(SafeAssetPath);
      if (AssetName.TrimStartAndEnd().IsEmpty() && DefaultAssetName.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("create_editor_utility_blueprint requires a non-empty asset name"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      AssetName = SanitizeAssetName(AssetName.TrimStartAndEnd().IsEmpty() ? DefaultAssetName : AssetName);
      FString PythonPackagePath = FPaths::GetPath(SafeAssetPath);
      PythonPackagePath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      AssetName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      Code = FString::Printf(TEXT(
          "import unreal\n"
          "_factory = unreal.EditorUtilityBlueprintFactory()\n"
          "_factory.set_editor_property('parent_class', unreal.EditorUtilityBlueprint)\n"
          "_asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset('%s', '%s', unreal.EditorUtilityBlueprint, _factory)\n"
          "if not _asset:\n"
          "    raise RuntimeError('Editor Utility Blueprint creation failed')\n"
          "unreal.EditorAssetLibrary.save_asset(_asset.get_path_name())\n"
          "print(_asset.get_path_name())\n"),
          *AssetName, *PythonPackagePath);
    }

    if (Lower == TEXT("create_geometry_collection")) {
      FString AssetPath;
      FString AssetName;
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
      Payload->TryGetStringField(TEXT("name"), AssetName);
      const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
      if (SafeAssetPath.IsEmpty() || !SafeAssetPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase)) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("create_geometry_collection requires a valid /Game asset path"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      const FString DefaultAssetName = FPaths::GetBaseFilename(SafeAssetPath);
      if (AssetName.TrimStartAndEnd().IsEmpty() && DefaultAssetName.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("create_geometry_collection requires a non-empty asset name"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      AssetName = SanitizeAssetName(AssetName.TrimStartAndEnd().IsEmpty() ? DefaultAssetName : AssetName);
      FString PythonPackagePath = FPaths::GetPath(SafeAssetPath);
      PythonPackagePath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      AssetName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      Code = FString::Printf(TEXT(
          "import unreal\n"
          "_factory = unreal.GeometryCollectionFactory()\n"
          "_asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset('%s', '%s', unreal.GeometryCollection, _factory)\n"
          "if not _asset:\n"
          "    raise RuntimeError('Geometry Collection creation failed; enable the Geometry Collection plugin')\n"
          "unreal.EditorAssetLibrary.save_asset(_asset.get_path_name())\n"
          "print(_asset.get_path_name())\n"),
          *AssetName, *PythonPackagePath);
    }

    if (Lower == TEXT("create_variant_set")) {
      FString AssetPath;
      FString VariantSetName;
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
      Payload->TryGetStringField(TEXT("variantSetName"), VariantSetName);
      const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
      if (SafeAssetPath.IsEmpty() || !SafeAssetPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase)) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("create_variant_set requires a valid /Game LevelVariantSets asset path"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      VariantSetName = VariantSetName.TrimStartAndEnd();
      if (VariantSetName.IsEmpty() || VariantSetName.Len() > 128) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("create_variant_set requires a non-empty variantSetName of at most 128 characters"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      FString AssetName = SanitizeAssetName(FPaths::GetBaseFilename(SafeAssetPath));
      if (AssetName.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("create_variant_set requires an asset name in assetPath"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      FString PythonAssetPath = SafeAssetPath;
      FString PythonPackagePath = FPaths::GetPath(SafeAssetPath);
      PythonAssetPath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      PythonPackagePath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      AssetName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      VariantSetName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      Code = FString::Printf(TEXT(
          "import unreal\n"
          "_asset_path = '%s'\n"
          "_lvs = unreal.EditorAssetLibrary.load_asset(_asset_path)\n"
          "if not _lvs:\n"
          "    _lvs = unreal.VariantManagerLibrary.create_level_variant_sets_asset('%s', '%s')\n"
          "if not _lvs:\n"
          "    raise RuntimeError('Level Variant Sets creation failed; enable VariantManagerContent')\n"
          "_name = '%s'\n"
          "_existing = _lvs.get_variant_set_by_name(_name)\n"
          "if _existing:\n"
          "    _variant_set = _existing\n"
          "else:\n"
          "    _variant_set = unreal.VariantSet(_lvs)\n"
          "    _variant_set.set_display_text(unreal.Text(_name))\n"
          "    _lvs.add_variant_set(_variant_set)\n"
          "unreal.EditorAssetLibrary.save_asset(_lvs.get_path_name())\n"
          "print(_lvs.get_path_name() + '|' + _variant_set.get_display_text().to_string())\n"),
          *PythonAssetPath, *AssetName, *PythonPackagePath, *VariantSetName);
    }

    if (Lower == TEXT("add_variant")) {
      FString AssetPath;
      FString VariantSetName;
      FString VariantName;
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
      Payload->TryGetStringField(TEXT("variantSetName"), VariantSetName);
      Payload->TryGetStringField(TEXT("variantName"), VariantName);
      const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
      if (SafeAssetPath.IsEmpty() || !SafeAssetPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase)) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("add_variant requires a valid /Game LevelVariantSets asset path"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      VariantSetName = VariantSetName.TrimStartAndEnd();
      VariantName = VariantName.TrimStartAndEnd();
      if (VariantSetName.IsEmpty() || VariantName.IsEmpty() || VariantSetName.Len() > 128 || VariantName.Len() > 128) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("add_variant requires non-empty variantSetName and variantName values of at most 128 characters"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      FString PythonAssetPath = SafeAssetPath;
      PythonAssetPath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      VariantSetName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      VariantName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      Code = FString::Printf(TEXT(
          "import unreal\n"
          "_lvs = unreal.EditorAssetLibrary.load_asset('%s')\n"
          "if not _lvs:\n"
          "    raise RuntimeError('Level Variant Sets asset not found')\n"
          "_variant_set = _lvs.get_variant_set_by_name('%s')\n"
          "if not _variant_set:\n"
          "    raise RuntimeError('Variant set not found')\n"
          "_name = '%s'\n"
          "_variant = _variant_set.get_variant_by_name(_name)\n"
          "if not _variant:\n"
          "    _variant = unreal.Variant(_variant_set)\n"
          "    _variant.set_display_text(unreal.Text(_name))\n"
          "    _variant_set.add_variant(_variant)\n"
          "unreal.EditorAssetLibrary.save_asset(_lvs.get_path_name())\n"
          "print(_lvs.get_path_name() + '|' + _variant_set.get_display_text().to_string() + '|' + _variant.get_display_text().to_string())\n"),
          *PythonAssetPath, *VariantSetName, *VariantName);
    }

    if (Lower == TEXT("configure_variant_properties")) {
      FString AssetPath;
      FString VariantSetName;
      FString VariantName;
      FString ActorName;
      FString PropertyPath;
      FString PropertyType;
      FString PropertyValue;
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
      Payload->TryGetStringField(TEXT("variantSetName"), VariantSetName);
      Payload->TryGetStringField(TEXT("variantName"), VariantName);
      Payload->TryGetStringField(TEXT("actorName"), ActorName);
      Payload->TryGetStringField(TEXT("propertyPath"), PropertyPath);
      Payload->TryGetStringField(TEXT("variantPropertyType"), PropertyType);
      Payload->TryGetStringField(TEXT("variantPropertyValue"), PropertyValue);
      const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
      if (SafeAssetPath.IsEmpty() || !SafeAssetPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase) ||
          VariantSetName.TrimStartAndEnd().IsEmpty() || VariantName.TrimStartAndEnd().IsEmpty() ||
          ActorName.TrimStartAndEnd().IsEmpty() || PropertyPath.TrimStartAndEnd().IsEmpty() || PropertyValue.Len() > 4096) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("configure_variant_properties requires valid assetPath, variantSetName, variantName, actorName, propertyPath, and variantPropertyValue"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      if (!PropertyType.Equals(TEXT("bool"), ESearchCase::IgnoreCase) &&
          !PropertyType.Equals(TEXT("int"), ESearchCase::IgnoreCase) &&
          !PropertyType.Equals(TEXT("float"), ESearchCase::IgnoreCase) &&
          !PropertyType.Equals(TEXT("string"), ESearchCase::IgnoreCase)) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("variantPropertyType must be bool, int, float, or string"), TEXT("INVALID_ARGUMENT"));
        return true;
      }
      FString PythonAssetPath = SafeAssetPath;
      PythonAssetPath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      VariantSetName = VariantSetName.TrimStartAndEnd();
      VariantName = VariantName.TrimStartAndEnd();
      ActorName = ActorName.TrimStartAndEnd();
      PropertyPath = PropertyPath.TrimStartAndEnd();
      PropertyType = PropertyType.ToLower();
      VariantSetName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      VariantName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      ActorName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      PropertyPath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      PropertyType.ReplaceInline(TEXT("'"), TEXT("\\'"));
      PropertyValue.ReplaceInline(TEXT("'"), TEXT("\\'"));
      Code = FString::Printf(TEXT(
          "import json\n"
          "import unreal\n"
          "_lvs = unreal.EditorAssetLibrary.load_asset('%s')\n"
          "if not _lvs:\n"
          "    raise RuntimeError('Level Variant Sets asset not found')\n"
          "_variant_set = _lvs.get_variant_set_by_name('%s')\n"
          "if not _variant_set:\n"
          "    raise RuntimeError('Variant set not found')\n"
          "_variant = _variant_set.get_variant_by_name('%s')\n"
          "if not _variant:\n"
          "    raise RuntimeError('Variant not found')\n"
          "_actor = next((a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label() == '%s'), None)\n"
          "if not _actor:\n"
          "    raise RuntimeError('Actor not found')\n"
          "_variant.add_actor_binding(_actor)\n"
          "_prop = _variant.capture_property(_actor, '%s')\n"
          "if not _prop:\n"
          "    raise RuntimeError('Property path could not be captured')\n"
          "_type = '%s'\n"
          "_value = '%s'\n"
          "if _type == 'bool':\n"
          "    unreal.VariantManagerLibrary.set_value_bool(_prop, _value.lower() == 'true')\n"
          "elif _type == 'int':\n"
          "    unreal.VariantManagerLibrary.set_value_int(_prop, int(_value))\n"
          "elif _type == 'float':\n"
          "    unreal.VariantManagerLibrary.set_value_float(_prop, float(_value))\n"
          "else:\n"
          "    unreal.VariantManagerLibrary.set_value_string(_prop, _value)\n"
          "unreal.EditorAssetLibrary.save_asset(_lvs.get_path_name())\n"
          "print(json.dumps({'actorName': _actor.get_actor_label(), 'propertyPath': '%s', 'propertyType': unreal.VariantManagerLibrary.get_property_type_string(_prop), 'variant': _variant.get_display_text().to_string()}, sort_keys=True))\n"),
          *PythonAssetPath, *VariantSetName, *VariantName, *ActorName, *PropertyPath, *PropertyType, *PropertyValue, *PropertyPath);
    }

    if (Lower == TEXT("set_variant_dependencies")) {
      FString AssetPath;
      FString VariantSetName;
      FString VariantName;
      FString DependencyVariantSetName;
      FString DependencyVariantName;
      bool bEnabled = true;
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
      Payload->TryGetStringField(TEXT("variantSetName"), VariantSetName);
      Payload->TryGetStringField(TEXT("variantName"), VariantName);
      Payload->TryGetStringField(TEXT("dependencyVariantSetName"), DependencyVariantSetName);
      Payload->TryGetStringField(TEXT("dependencyVariantName"), DependencyVariantName);
      Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
      const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
      VariantSetName = VariantSetName.TrimStartAndEnd();
      VariantName = VariantName.TrimStartAndEnd();
      DependencyVariantSetName = DependencyVariantSetName.TrimStartAndEnd();
      DependencyVariantName = DependencyVariantName.TrimStartAndEnd();
      if (SafeAssetPath.IsEmpty() || !SafeAssetPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase) ||
          VariantSetName.IsEmpty() || VariantName.IsEmpty() || DependencyVariantSetName.IsEmpty() || DependencyVariantName.IsEmpty() ||
          VariantSetName.Len() > 128 || VariantName.Len() > 128 || DependencyVariantSetName.Len() > 128 || DependencyVariantName.Len() > 128) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("set_variant_dependencies requires valid source and dependency asset/set/variant names"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      FString PythonAssetPath = SafeAssetPath;
      PythonAssetPath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      VariantSetName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      VariantName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      DependencyVariantSetName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      DependencyVariantName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      Code = FString::Printf(TEXT(
          "import unreal\n"
          "_lvs = unreal.EditorAssetLibrary.load_asset('%s')\n"
          "if not _lvs:\n"
          "    raise RuntimeError('Level Variant Sets asset not found')\n"
          "_source_set = _lvs.get_variant_set_by_name('%s')\n"
          "_source_variant = _source_set.get_variant_by_name('%s') if _source_set else None\n"
          "_dependency_set = _lvs.get_variant_set_by_name('%s')\n"
          "_dependency_variant = _dependency_set.get_variant_by_name('%s') if _dependency_set else None\n"
          "if not _source_variant or not _dependency_variant:\n"
          "    raise RuntimeError('Source or dependency variant was not found')\n"
          "if not _source_variant.is_valid_dependency(_dependency_variant):\n"
          "    raise RuntimeError('Dependency would create an invalid or cyclic relationship')\n"
          "_dependency = unreal.VariantDependency(_dependency_set, _dependency_variant, %s)\n"
          "unreal.VariantManagerLibrary.add_dependency(_source_variant, _dependency)\n"
          "unreal.EditorAssetLibrary.save_asset(_lvs.get_path_name())\n"
          "print(_source_variant.get_display_text().to_string() + '|' + _dependency_variant.get_display_text().to_string())\n"),
          *PythonAssetPath, *VariantSetName, *VariantName, *DependencyVariantSetName, *DependencyVariantName, bEnabled ? TEXT("True") : TEXT("False"));
    }

    if (Lower == TEXT("activate_variant")) {
      FString AssetPath;
      FString VariantSetName;
      FString VariantName;
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
      Payload->TryGetStringField(TEXT("variantSetName"), VariantSetName);
      Payload->TryGetStringField(TEXT("variantName"), VariantName);
      const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
      VariantSetName = VariantSetName.TrimStartAndEnd();
      VariantName = VariantName.TrimStartAndEnd();
      if (SafeAssetPath.IsEmpty() || !SafeAssetPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase) ||
          VariantSetName.IsEmpty() || VariantName.IsEmpty() || VariantSetName.Len() > 128 || VariantName.Len() > 128) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("activate_variant requires a valid /Game asset path and named variant set/variant"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      FString PythonAssetPath = SafeAssetPath;
      PythonAssetPath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      VariantSetName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      VariantName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      Code = FString::Printf(TEXT(
          "import unreal\n"
          "_lvs = unreal.EditorAssetLibrary.load_asset('%s')\n"
          "if not _lvs:\n"
          "    raise RuntimeError('Level Variant Sets asset not found')\n"
          "_variant_set = _lvs.get_variant_set_by_name('%s')\n"
          "_variant = _variant_set.get_variant_by_name('%s') if _variant_set else None\n"
          "if not _variant:\n"
          "    raise RuntimeError('Variant set or variant not found')\n"
          "_variant.switch_on()\n"
          "print(_variant.get_display_text().to_string())\n"),
          *PythonAssetPath, *VariantSetName, *VariantName);
    }

    if (Lower == TEXT("get_active_variants")) {
      FString AssetPath;
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
      const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
      if (SafeAssetPath.IsEmpty() || !SafeAssetPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase)) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("get_active_variants requires a valid /Game LevelVariantSets asset path"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      FString PythonAssetPath = SafeAssetPath;
      PythonAssetPath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      Code = FString::Printf(TEXT(
          "import json\n"
          "import unreal\n"
          "_lvs = unreal.EditorAssetLibrary.load_asset('%s')\n"
          "if not _lvs:\n"
          "    raise RuntimeError('Level Variant Sets asset not found')\n"
          "_sets = []\n"
          "for _set_index in range(_lvs.get_num_variant_sets()):\n"
          "    _set = _lvs.get_variant_set(_set_index)\n"
          "    _variants = []\n"
          "    for _variant_index in range(_set.get_num_variants()):\n"
          "        _variant = _set.get_variant(_variant_index)\n"
          "        _variants.append({'name': _variant.get_display_text().to_string(), 'active': _variant.is_active()})\n"
          "    _sets.append({'name': _set.get_display_text().to_string(), 'variants': _variants})\n"
          "print(json.dumps({'assetPath': _lvs.get_path_name(), 'variantSets': _sets}, sort_keys=True))\n"),
          *PythonAssetPath);
    }

    if (Lower == TEXT("capture_variant_thumbnail") || Lower == TEXT("set_variant_thumbnail")) {
      FString AssetPath;
      FString VariantSetName;
      FString VariantName;
      FString ThumbnailSource = Lower == TEXT("capture_variant_thumbnail") ? TEXT("editor_viewport") : TEXT("");
      FString ThumbnailPath;
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
      Payload->TryGetStringField(TEXT("variantSetName"), VariantSetName);
      Payload->TryGetStringField(TEXT("variantName"), VariantName);
      Payload->TryGetStringField(TEXT("thumbnailSource"), ThumbnailSource);
      Payload->TryGetStringField(TEXT("thumbnailPath"), ThumbnailPath);
      const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
      VariantSetName = VariantSetName.TrimStartAndEnd();
      VariantName = VariantName.TrimStartAndEnd();
      if (SafeAssetPath.IsEmpty() || !SafeAssetPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase) ||
          VariantSetName.IsEmpty() || VariantName.IsEmpty() || VariantSetName.Len() > 128 || VariantName.Len() > 128) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Variant thumbnail actions require a valid /Game asset path and named variant set/variant"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      ThumbnailSource = ThumbnailSource.TrimStartAndEnd().ToLower();
      if (ThumbnailSource != TEXT("editor_viewport") && ThumbnailSource != TEXT("file")) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("thumbnailSource must be editor_viewport or file"), TEXT("INVALID_ARGUMENT"));
        return true;
      }
      FString AbsoluteThumbnailPath;
      if (ThumbnailSource == TEXT("file")) {
        const FString SafeThumbnailPath = SanitizeProjectFilePath(ThumbnailPath);
        if (SafeThumbnailPath.IsEmpty() || !FPaths::FileExists(FPaths::ProjectDir() / SafeThumbnailPath)) {
          SendAutomationError(RequestingSocket, RequestId,
                              TEXT("thumbnailPath must be an existing project-confined image file"), TEXT("INVALID_ARGUMENT"));
          return true;
        }
        AbsoluteThumbnailPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / SafeThumbnailPath);
        AbsoluteThumbnailPath.ReplaceInline(TEXT("\\"), TEXT("/"));
      }
      FString PythonAssetPath = SafeAssetPath;
      PythonAssetPath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      VariantSetName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      VariantName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      AbsoluteThumbnailPath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      Code = FString::Printf(TEXT(
          "import unreal\n"
          "_lvs = unreal.EditorAssetLibrary.load_asset('%s')\n"
          "if not _lvs:\n"
          "    raise RuntimeError('Level Variant Sets asset not found')\n"
          "_variant_set = _lvs.get_variant_set_by_name('%s')\n"
          "_variant = _variant_set.get_variant_by_name('%s') if _variant_set else None\n"
          "if not _variant:\n"
          "    raise RuntimeError('Variant set or variant not found')\n"
          "if '%s' == 'file':\n"
          "    _variant.set_thumbnail_from_file('%s')\n"
          "else:\n"
          "    _variant.set_thumbnail_from_editor_viewport()\n"
          "unreal.EditorAssetLibrary.save_asset(_lvs.get_path_name())\n"
          "print(_variant.get_display_text().to_string())\n"),
          *PythonAssetPath, *VariantSetName, *VariantName, *ThumbnailSource, *AbsoluteThumbnailPath);
    }

    if (Lower == TEXT("export_variant_configuration")) {
      FString AssetPath;
      FString ExportPath;
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
      Payload->TryGetStringField(TEXT("variantExportPath"), ExportPath);
      const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
      const FString SafeExportPath = SanitizeProjectFilePath(ExportPath);
      if (SafeAssetPath.IsEmpty() || !SafeAssetPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase) ||
          SafeExportPath.IsEmpty() || !SafeExportPath.EndsWith(TEXT(".json"), ESearchCase::IgnoreCase)) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("export_variant_configuration requires a /Game asset path and project-confined .json variantExportPath"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      FString PythonAssetPath = SafeAssetPath;
      FString AbsoluteExportPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / SafeExportPath);
      PythonAssetPath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      AbsoluteExportPath.ReplaceInline(TEXT("\\"), TEXT("/"));
      AbsoluteExportPath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      Code = FString::Printf(TEXT(
          "import json\n"
          "import os\n"
          "import unreal\n"
          "_lvs = unreal.EditorAssetLibrary.load_asset('%s')\n"
          "if not _lvs:\n"
          "    raise RuntimeError('Level Variant Sets asset not found')\n"
          "_export = {'assetPath': _lvs.get_path_name(), 'variantSets': []}\n"
          "for _set_index in range(_lvs.get_num_variant_sets()):\n"
          "    _set = _lvs.get_variant_set(_set_index)\n"
          "    _set_data = {'name': _set.get_display_text().to_string(), 'variants': []}\n"
          "    for _variant_index in range(_set.get_num_variants()):\n"
          "        _variant = _set.get_variant(_variant_index)\n"
          "        _variant_data = {'name': _variant.get_display_text().to_string(), 'active': _variant.is_active(), 'actors': [], 'dependencies': _variant.get_num_dependencies()}\n"
          "        for _actor_index in range(_variant.get_num_actors()):\n"
          "            _actor = _variant.get_actor(_actor_index)\n"
          "            _properties = []\n"
          "            for _prop in _variant.get_captured_properties(_actor):\n"
          "                _properties.append({'type': _prop.get_property_type_string(), 'display': _prop.get_full_display_string()})\n"
          "            _variant_data['actors'].append({'name': _actor.get_actor_label(), 'properties': _properties})\n"
          "        _set_data['variants'].append(_variant_data)\n"
          "    _export['variantSets'].append(_set_data)\n"
          "os.makedirs(os.path.dirname('%s'), exist_ok=True)\n"
          "with open('%s', 'w', encoding='utf-8') as _file:\n"
          "    json.dump(_export, _file, indent=2, sort_keys=True)\n"
          "print('%s')\n"),
          *PythonAssetPath, *AbsoluteExportPath, *AbsoluteExportPath, *AbsoluteExportPath);
    }

    if (Lower == TEXT("add_geometry_to_collection") || Lower == TEXT("remove_geometry_from_collection")) {
      FString AssetPath;
      FString SourceAssetPath;
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
      Payload->TryGetStringField(TEXT("sourceAssetPath"), SourceAssetPath);
      const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
      const FString SafeSourceAssetPath = SanitizeProjectRelativePath(SourceAssetPath);
      if (SafeAssetPath.IsEmpty() || !SafeAssetPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase) ||
          SafeSourceAssetPath.IsEmpty() || !SafeSourceAssetPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase)) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Geometry Collection and source assets must be valid /Game paths"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      FString PythonAssetPath = SafeAssetPath;
      FString PythonSourceAssetPath = SafeSourceAssetPath;
      PythonAssetPath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      PythonSourceAssetPath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      if (Lower == TEXT("add_geometry_to_collection")) {
        Code = FString::Printf(TEXT(
            "import unreal\n"
            "_collection = unreal.load_asset('%s')\n"
            "if not _collection or not isinstance(_collection, unreal.GeometryCollection):\n"
            "    raise RuntimeError('Geometry Collection asset was not found')\n"
            "_source_asset = unreal.load_asset('%s')\n"
            "if not _source_asset:\n"
            "    raise RuntimeError('Source geometry asset was not found')\n"
            "_sources = list(_collection.get_editor_property('geometry_source') or [])\n"
            "_source = unreal.GeometryCollectionSource(source_geometry_object=unreal.SoftObjectPath(_source_asset.get_path_name()))\n"
            "_sources.append(_source)\n"
            "_collection.set_editor_property('geometry_source', _sources)\n"
            "unreal.EditorAssetLibrary.save_asset(_collection.get_path_name())\n"
            "print(_collection.get_path_name())\n"),
            *PythonAssetPath, *PythonSourceAssetPath);
      } else {
        Code = FString::Printf(TEXT(
            "import unreal\n"
            "_collection = unreal.load_asset('%s')\n"
            "if not _collection or not isinstance(_collection, unreal.GeometryCollection):\n"
            "    raise RuntimeError('Geometry Collection asset was not found')\n"
            "_target = '%s'\n"
            "_sources = list(_collection.get_editor_property('geometry_source') or [])\n"
            "_remaining = [s for s in _sources if str(s.get_editor_property('source_geometry_object')) != _target]\n"
            "if len(_remaining) == len(_sources):\n"
            "    raise RuntimeError('Source geometry asset is not present in the Geometry Collection')\n"
            "_collection.set_editor_property('geometry_source', _remaining)\n"
            "unreal.EditorAssetLibrary.save_asset(_collection.get_path_name())\n"
            "print(_collection.get_path_name())\n"),
            *PythonAssetPath, *PythonSourceAssetPath);
      }
    }

    if (Lower == TEXT("configure_geometry_collection")) {
      FString AssetPath;
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
      const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
      if (SafeAssetPath.IsEmpty() || !SafeAssetPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase)) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("configure_geometry_collection requires a valid /Game asset path"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }

      FString PythonAssetPath = SafeAssetPath;
      PythonAssetPath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      FString Updates;
      FString UpdatedNames;
      auto AddUpdate = [&Updates, &UpdatedNames](const TCHAR* PropertyName, const FString& PythonValue, const TCHAR* OutputName) {
        Updates += FString::Printf(TEXT("_updates['%s'] = %s\n"), PropertyName, *PythonValue);
        if (!UpdatedNames.IsEmpty()) UpdatedNames += TEXT(", ");
        UpdatedNames += FString::Printf(TEXT("'%s'"), OutputName);
      };

      double Mass = 0.0;
      bool bMassAsDensity = false;
      bool bEnableClustering = false;
      bool bEnableNanite = false;
      bool bSupportRayTracing = false;
      int32 MaxClusterLevel = 0;
      bool bRemoveOnMaxSleep = false;
      bool bMassSet = Payload->TryGetNumberField(TEXT("mass"), Mass);
      bool bMassAsDensitySet = Payload->TryGetBoolField(TEXT("massAsDensity"), bMassAsDensity);
      bool bEnableClusteringSet = Payload->TryGetBoolField(TEXT("enableClustering"), bEnableClustering);
      bool bEnableNaniteSet = Payload->TryGetBoolField(TEXT("enableNanite"), bEnableNanite);
      bool bSupportRayTracingSet = Payload->TryGetBoolField(TEXT("supportRayTracing"), bSupportRayTracing);
      bool bMaxClusterLevelSet = Payload->TryGetNumberField(TEXT("maxClusterLevel"), MaxClusterLevel);
      bool bRemoveOnMaxSleepSet = Payload->TryGetBoolField(TEXT("removeOnMaxSleep"), bRemoveOnMaxSleep);
      const TArray<TSharedPtr<FJsonValue>>* DamageThresholds = nullptr;
      bool bDamageThresholdsSet = Payload->TryGetArrayField(TEXT("damageThresholds"), DamageThresholds) && DamageThresholds && DamageThresholds->Num() > 0;
      if (!bMassSet && !bMassAsDensitySet && !bEnableClusteringSet && !bEnableNaniteSet && !bSupportRayTracingSet &&
          !bMaxClusterLevelSet && !bRemoveOnMaxSleepSet && !bDamageThresholdsSet) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("configure_geometry_collection requires at least one supported setting"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      if (bMassSet) AddUpdate(TEXT("mass"), LexToString(Mass), TEXT("mass"));
      if (bMassAsDensitySet) AddUpdate(TEXT("mass_as_density"), bMassAsDensity ? TEXT("True") : TEXT("False"), TEXT("massAsDensity"));
      if (bEnableClusteringSet) AddUpdate(TEXT("enable_clustering"), bEnableClustering ? TEXT("True") : TEXT("False"), TEXT("enableClustering"));
      if (bEnableNaniteSet) AddUpdate(TEXT("enable_nanite"), bEnableNanite ? TEXT("True") : TEXT("False"), TEXT("enableNanite"));
      if (bSupportRayTracingSet) AddUpdate(TEXT("support_ray_tracing"), bSupportRayTracing ? TEXT("True") : TEXT("False"), TEXT("supportRayTracing"));
      if (bMaxClusterLevelSet) AddUpdate(TEXT("max_cluster_level"), LexToString(MaxClusterLevel), TEXT("maxClusterLevel"));
      if (bRemoveOnMaxSleepSet) AddUpdate(TEXT("remove_on_max_sleep"), bRemoveOnMaxSleep ? TEXT("True") : TEXT("False"), TEXT("removeOnMaxSleep"));
      if (bDamageThresholdsSet) {
        FString Values = TEXT("[");
        for (int32 Index = 0; Index < DamageThresholds->Num(); ++Index) {
          double Threshold = 0.0;
          if (!(*DamageThresholds)[Index].IsValid() || !(*DamageThresholds)[Index]->TryGetNumber(Threshold) || Threshold < 0.0) {
            SendAutomationError(RequestingSocket, RequestId,
                                TEXT("damageThresholds must contain non-negative numbers"),
                                TEXT("INVALID_ARGUMENT"));
            return true;
          }
          if (Index > 0) Values += TEXT(", ");
          Values += LexToString(Threshold);
        }
        Values += TEXT("]");
        AddUpdate(TEXT("damage_threshold"), Values, TEXT("damageThresholds"));
      }
      Code = FString::Printf(TEXT(
          "import json\n"
          "import unreal\n"
          "_asset = unreal.load_asset('%s')\n"
          "if not _asset or not isinstance(_asset, unreal.GeometryCollection):\n"
          "    raise RuntimeError('Geometry Collection asset was not found')\n"
          "_updates = {}\n"
          "%s"
          "for _property, _value in _updates.items():\n"
          "    _asset.set_editor_property(_property, _value)\n"
          "unreal.EditorAssetLibrary.save_asset(_asset.get_path_name())\n"
          "print(json.dumps({'assetPath': _asset.get_path_name(), 'updatedProperties': [%s]}))\n"),
          *PythonAssetPath, *Updates, *UpdatedNames);
    }

    if (Lower == TEXT("inspect_geometry_collection")) {
      FString AssetPath;
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
      const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
      if (SafeAssetPath.IsEmpty() || !SafeAssetPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase)) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("inspect_geometry_collection requires a valid /Game asset path"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      FString PythonAssetPath = SafeAssetPath;
      PythonAssetPath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      Code = FString::Printf(TEXT(
          "import json\n"
          "import unreal\n"
          "_asset = unreal.load_asset('%s')\n"
          "if not _asset or not isinstance(_asset, unreal.GeometryCollection):\n"
          "    raise RuntimeError('Geometry Collection asset was not found')\n"
          "_result = {'assetPath': _asset.get_path_name(), 'classPath': _asset.get_class().get_path_name()}\n"
          "for _property in ('mass', 'mass_as_density', 'enable_clustering', 'enable_nanite', 'support_ray_tracing', 'damage_threshold', 'max_cluster_level', 'remove_on_max_sleep'):\n"
          "    try:\n"
          "        _result[_property] = _asset.get_editor_property(_property)\n"
          "    except Exception:\n"
          "        _result[_property] = None\n"
          "print(json.dumps(_result, default=str, sort_keys=True))\n"),
          *PythonAssetPath);
    }

    if (Lower == TEXT("configure_geometry_collection_component")) {
      FString ActorName;
      FString CollisionProfileName;
      bool bSimulatePhysics = false;
      bool bEnableDamageFromCollision = false;
      int32 ClusterGroupIndex = 0;
      int32 MaxSimulatedLevel = 0;
      double LinearEtherDrag = 0.0;
      Payload->TryGetStringField(TEXT("actorName"), ActorName);
      Payload->TryGetStringField(TEXT("collisionProfileName"), CollisionProfileName);
      const bool bSimulatePhysicsSet = Payload->TryGetBoolField(TEXT("simulatePhysics"), bSimulatePhysics);
      const bool bEnableDamageFromCollisionSet = Payload->TryGetBoolField(TEXT("enableDamageFromCollision"), bEnableDamageFromCollision);
      const bool bClusterGroupIndexSet = Payload->TryGetNumberField(TEXT("clusterGroupIndex"), ClusterGroupIndex);
      const bool bMaxSimulatedLevelSet = Payload->TryGetNumberField(TEXT("maxSimulatedLevel"), MaxSimulatedLevel);
      const bool bLinearEtherDragSet = Payload->TryGetNumberField(TEXT("linearEtherDrag"), LinearEtherDrag);
      const bool bCollisionProfileSet = !CollisionProfileName.TrimStartAndEnd().IsEmpty();
      if (ActorName.TrimStartAndEnd().IsEmpty() ||
          (!bSimulatePhysicsSet && !bEnableDamageFromCollisionSet && !bClusterGroupIndexSet &&
           !bMaxSimulatedLevelSet && !bLinearEtherDragSet && !bCollisionProfileSet)) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("configure_geometry_collection_component requires actorName and at least one supported setting"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      ActorName.TrimStartAndEndInline();
      CollisionProfileName.TrimStartAndEndInline();
      ActorName.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
      ActorName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      CollisionProfileName.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
      CollisionProfileName.ReplaceInline(TEXT("'"), TEXT("\\'"));
      FString Updates;
      if (bSimulatePhysicsSet) Updates += FString::Printf(TEXT("_updates['simulate_physics'] = %s\n"), bSimulatePhysics ? TEXT("True") : TEXT("False"));
      if (bEnableDamageFromCollisionSet) Updates += FString::Printf(TEXT("_updates['enable_damage_from_collision'] = %s\n"), bEnableDamageFromCollision ? TEXT("True") : TEXT("False"));
      if (bClusterGroupIndexSet) Updates += FString::Printf(TEXT("_updates['cluster_group_index'] = %s\n"), *LexToString(ClusterGroupIndex));
      if (bMaxSimulatedLevelSet) Updates += FString::Printf(TEXT("_updates['max_simulated_level'] = %s\n"), *LexToString(MaxSimulatedLevel));
      if (bLinearEtherDragSet) Updates += FString::Printf(TEXT("_updates['linear_ether_drag'] = %s\n"), *LexToString(LinearEtherDrag));
      const FString CollisionProfileCode = bCollisionProfileSet
          ? FString::Printf(TEXT("_component.set_collision_profile_name(unreal.Name('%s'))\n"), *CollisionProfileName)
          : FString();
      Code = FString::Printf(TEXT(
          "import json\n"
          "import unreal\n"
          "_actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()\n"
          "_actor = next((a for a in _actors if a.get_actor_label() == '%s'), None)\n"
          "if not _actor:\n"
          "    raise RuntimeError('Actor was not found')\n"
          "_component = _actor.get_component_by_class(unreal.GeometryCollectionComponent)\n"
          "if not _component:\n"
          "    raise RuntimeError('Actor has no Geometry Collection component')\n"
          "_updates = {}\n"
          "%s"
          "for _property, _value in _updates.items():\n"
          "    _component.set_editor_property(_property, _value)\n"
          "%s"
          "print(json.dumps({'actorName': _actor.get_actor_label(), 'updatedProperties': list(_updates.keys())}, sort_keys=True))\n"),
          *ActorName, *Updates, *CollisionProfileCode);
    }

    if (Lower == TEXT("register_python_command")) {
      FString CommandName;
      FString CommandSet;
      FString CommandContext;
      FString CommandLabel;
      FString CommandDescription;
      bool bOverrideExisting = false;
      Payload->TryGetStringField(TEXT("commandName"), CommandName);
      Payload->TryGetStringField(TEXT("commandSet"), CommandSet);
      Payload->TryGetStringField(TEXT("commandContext"), CommandContext);
      Payload->TryGetStringField(TEXT("commandLabel"), CommandLabel);
      Payload->TryGetStringField(TEXT("commandDescription"), CommandDescription);
      Payload->TryGetBoolField(TEXT("overrideExisting"), bOverrideExisting);
      if (CommandName.TrimStartAndEnd().IsEmpty() || CommandSet.TrimStartAndEnd().IsEmpty() ||
          CommandContext.TrimStartAndEnd().IsEmpty() || Code.TrimStartAndEnd().IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("register_python_command requires commandName, commandSet, commandContext, and code"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      if (CommandName.Len() > 128 || CommandSet.Len() > 128 || CommandContext.Len() > 128 ||
          CommandLabel.Len() > 256 || CommandDescription.Len() > 1024) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Python command metadata exceeds the supported length limits"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      auto EscapePythonString = [](FString& Value) {
        Value.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
        Value.ReplaceInline(TEXT("'"), TEXT("\\'"));
        Value.ReplaceInline(TEXT("\r"), TEXT("\\r"));
        Value.ReplaceInline(TEXT("\n"), TEXT("\\n"));
      };
      CommandName.TrimStartAndEndInline();
      CommandSet.TrimStartAndEndInline();
      CommandContext.TrimStartAndEndInline();
      CommandLabel.TrimStartAndEndInline();
      CommandDescription.TrimStartAndEndInline();
      EscapePythonString(CommandName);
      EscapePythonString(CommandSet);
      EscapePythonString(CommandContext);
      EscapePythonString(CommandLabel);
      EscapePythonString(CommandDescription);
      EscapePythonString(Code);
      Code = FString::Printf(TEXT(
          "import json\n"
          "import unreal\n"
          "_mcp_subsystem = unreal.get_engine_subsystem(unreal.UICommandsScriptingSubsystem)\n"
          "_mcp_set = unreal.Name('%s')\n"
          "if not _mcp_subsystem.is_command_set_registered(_mcp_set):\n"
          "    _mcp_subsystem.register_command_set(_mcp_set)\n"
          "_mcp_info = unreal.ScriptingCommandInfo(context_name=unreal.Name('%s'), set=_mcp_set, name=unreal.Name('%s'), label='%s', description='%s')\n"
          "_mcp_code = '%s'\n"
          "def _mcp_execute(_command_info):\n"
          "    exec(_mcp_code, globals(), globals())\n"
          "_mcp_delegate = unreal.ExecuteCommand(_mcp_execute)\n"
          "_mcp_registered = globals().setdefault('_mcp_registered_commands', {})\n"
          "_mcp_key = '%s::%s::%s'\n"
          "if not _mcp_subsystem.register_command(_mcp_info, _mcp_delegate, %s):\n"
          "    raise RuntimeError('Python command registration failed')\n"
          "_mcp_registered[_mcp_key] = (_mcp_info, _mcp_delegate)\n"
          "print(json.dumps({'commandName': '%s', 'commandSet': '%s', 'commandContext': '%s'}))\n"),
          *CommandSet, *CommandContext, *CommandName, *CommandLabel, *CommandDescription, *Code,
          *CommandContext, *CommandSet, *CommandName, bOverrideExisting ? TEXT("True") : TEXT("False"),
          *CommandName, *CommandSet, *CommandContext);
    }

    if (Lower == TEXT("unregister_python_command")) {
      FString CommandName;
      FString CommandSet;
      FString CommandContext;
      Payload->TryGetStringField(TEXT("commandName"), CommandName);
      Payload->TryGetStringField(TEXT("commandSet"), CommandSet);
      Payload->TryGetStringField(TEXT("commandContext"), CommandContext);
      if (CommandName.TrimStartAndEnd().IsEmpty() || CommandSet.TrimStartAndEnd().IsEmpty() || CommandContext.TrimStartAndEnd().IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("unregister_python_command requires commandName, commandSet, and commandContext"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      auto EscapePythonString = [](FString& Value) {
        Value.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
        Value.ReplaceInline(TEXT("'"), TEXT("\\'"));
      };
      CommandName.TrimStartAndEndInline();
      CommandSet.TrimStartAndEndInline();
      CommandContext.TrimStartAndEndInline();
      EscapePythonString(CommandName);
      EscapePythonString(CommandSet);
      EscapePythonString(CommandContext);
      Code = FString::Printf(TEXT(
          "import json\n"
          "import unreal\n"
          "_mcp_subsystem = unreal.get_engine_subsystem(unreal.UICommandsScriptingSubsystem)\n"
          "_mcp_key = '%s::%s::%s'\n"
          "_mcp_registered = globals().get('_mcp_registered_commands', {})\n"
          "_mcp_entry = _mcp_registered.get(_mcp_key)\n"
          "_mcp_info = _mcp_entry[0] if _mcp_entry else unreal.ScriptingCommandInfo(context_name=unreal.Name('%s'), set=unreal.Name('%s'), name=unreal.Name('%s'))\n"
          "if not _mcp_subsystem.unregister_command(_mcp_info):\n"
          "    raise RuntimeError('Python command was not registered or could not be removed')\n"
          "_mcp_registered.pop(_mcp_key, None)\n"
          "print(json.dumps({'commandName': '%s', 'commandSet': '%s', 'commandContext': '%s'}))\n"),
          *CommandContext, *CommandSet, *CommandName, *CommandContext, *CommandSet, *CommandName,
          *CommandName, *CommandSet, *CommandContext);
    }

    if (Lower == TEXT("run_editor_utility")) {
      FString AssetPath;
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
      const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
      if (SafeAssetPath.IsEmpty() || !SafeAssetPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase)) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("run_editor_utility requires a valid /Game asset path"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      FString PythonAssetPath = SafeAssetPath;
      PythonAssetPath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      Code = FString::Printf(TEXT(
          "import unreal\n"
          "_asset = unreal.load_asset('%s')\n"
          "if not _asset:\n"
          "    raise RuntimeError('Editor utility asset was not found')\n"
          "_subsystem = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)\n"
          "if not _subsystem.can_run(_asset):\n"
          "    raise RuntimeError('Editor utility asset cannot run')\n"
          "if not _subsystem.try_run(_asset):\n"
          "    raise RuntimeError('Editor utility execution failed')\n"
          "print(_asset.get_path_name())\n"),
          *PythonAssetPath);
    }

    if (Lower == TEXT("inspect_editor_utility")) {
      FString AssetPath;
      Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
      const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
      if (SafeAssetPath.IsEmpty() || !SafeAssetPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase)) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("inspect_editor_utility requires a valid /Game asset path"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      FString PythonAssetPath = SafeAssetPath;
      PythonAssetPath.ReplaceInline(TEXT("'"), TEXT("\\'"));
      Code = FString::Printf(TEXT(
          "import json\n"
          "import unreal\n"
          "_asset = unreal.load_asset('%s')\n"
          "if not _asset:\n"
          "    raise RuntimeError('Editor utility asset was not found')\n"
          "_subsystem = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)\n"
          "_result = {'assetPath': _asset.get_path_name(), 'classPath': _asset.get_class().get_path_name(), 'canRun': bool(_subsystem.can_run(_asset))}\n"
          "if isinstance(_asset, unreal.EditorUtilityWidgetBlueprint):\n"
          "    _tab_id = str(_subsystem.get_tab_id_from_blueprint(_asset))\n"
          "    _result['tabId'] = _tab_id\n"
          "    _result['tabOpen'] = bool(_subsystem.does_tab_exist(unreal.Name(_tab_id)))\n"
          "print(json.dumps(_result, sort_keys=True))\n"),
          *PythonAssetPath);
    }

    if (Lower == TEXT("configure_python_paths")) {
      const TArray<TSharedPtr<FJsonValue>>* PythonPaths = nullptr;
      if (!Payload->TryGetArrayField(TEXT("pythonPaths"), PythonPaths) || !PythonPaths || PythonPaths->Num() == 0 || PythonPaths->Num() > 64) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("configure_python_paths requires 1 to 64 pythonPaths entries"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }

      Code = TEXT("import sys\n");
      for (const TSharedPtr<FJsonValue>& PathValue : *PythonPaths) {
        FString PythonPath;
        if (!PathValue.IsValid() || !PathValue->TryGetString(PythonPath) || PythonPath.TrimStartAndEnd().IsEmpty() || PythonPath.Len() > 4096 || PythonPath.Contains(TEXT("\0"))) {
          SendAutomationError(RequestingSocket, RequestId,
                              TEXT("Each pythonPaths entry must be a non-empty path of at most 4096 characters"),
                              TEXT("INVALID_ARGUMENT"));
          return true;
        }
        PythonPath.TrimStartAndEndInline();
        PythonPath.ReplaceInline(TEXT("\\"), TEXT("/"));
        PythonPath.ReplaceInline(TEXT("'"), TEXT("\\'"));
        Code += FString::Printf(TEXT("_mcp_path = '%s'\nif _mcp_path not in sys.path:\n    sys.path.insert(0, _mcp_path)\n"), *PythonPath);
      }
    }

    const bool bHasCode = !Code.TrimStartAndEnd().IsEmpty();
    const bool bHasFile = !File.TrimStartAndEnd().IsEmpty();

    const bool bRequiresCode = Lower == TEXT("execute_python_script") || Lower == TEXT("execute_python_string");
    const bool bRequiresFile = Lower == TEXT("execute_python_file");
    if ((bRequiresCode && (!bHasCode || bHasFile)) || (bRequiresFile && (!bHasFile || bHasCode))) {
      SendAutomationError(RequestingSocket, RequestId,
                          bRequiresFile ? TEXT("execute_python_file requires file and does not accept code") : TEXT("execute_python_script/execute_python_string require code and do not accept file"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!bHasCode && !bHasFile) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("'code' or 'file' parameter is required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (bHasCode && bHasFile) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Provide either 'code' or 'file', not both"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Enforce maximum code size (1 MB)
    static const int32 MaxCodeSize = 1048576;
    if (Code.Len() > MaxCodeSize) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Python code exceeds maximum size (%d bytes)"), MaxCodeSize),
                          TEXT("CODE_TOO_LARGE"));
      return true;
    }

    // Temp paths — GUID in filenames for concurrency safety
    FString TempDir = FPaths::ProjectSavedDir() / TEXT("Temp/MCP_Python");
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*TempDir)) {
      PlatformFile.CreateDirectoryTree(*TempDir);
    }

    FString SafeId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FString ScriptPath = TempDir / FString::Printf(TEXT("mcp_exec_%s.py"), *SafeId);
    FString OutputPath = TempDir / FString::Printf(TEXT("output_%s.txt"), *SafeId);
    FString ErrorPath  = TempDir / FString::Printf(TEXT("error_%s.txt"), *SafeId);
    FString StatusPath = TempDir / FString::Printf(TEXT("status_%s.txt"), *SafeId);
    FString CodePath   = TempDir / FString::Printf(TEXT("code_%s.py"), *SafeId);

    // RAII-style scope guard for temp file cleanup
    struct FTempFileCleanup {
      TArray<FString> Paths;
      ~FTempFileCleanup() {
        IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
        for (const FString& P : Paths) {
          PF.DeleteFile(*P);
        }
      }
    } Cleanup;
    Cleanup.Paths.Add(ScriptPath);
    Cleanup.Paths.Add(OutputPath);
    Cleanup.Paths.Add(ErrorPath);
    Cleanup.Paths.Add(StatusPath);
    Cleanup.Paths.Add(CodePath);

    // Normalize paths for Python (forward slashes)
    auto NormalizePyPath = [](const FString& Path) -> FString {
      FString Normalized = Path.Replace(TEXT("\\"), TEXT("/"));
      // Paths are embedded in single-quoted Python raw strings below. Keep
      // project filenames containing apostrophes from breaking the wrapper.
      Normalized.ReplaceInline(TEXT("'"), TEXT("\\'"));
      return Normalized;
    };
    FString PyOutputPath = NormalizePyPath(OutputPath);
    FString PyErrorPath  = NormalizePyPath(ErrorPath);
    FString PyStatusPath = NormalizePyPath(StatusPath);

    // Build Python wrapper
    FString Wrapper;
    auto AppendPythonExec = [&Wrapper](const FString& PyScriptPath,
                                       const FString& PyScriptDir) {
      Wrapper += FString::Printf(TEXT("    _script_path = r'%s'\n"), *PyScriptPath);
      Wrapper += FString::Printf(TEXT("    _script_dir = r'%s'\n"), *PyScriptDir);
      Wrapper += TEXT("    _exec_globals = globals()\n");
      Wrapper += TEXT("    _exec_globals['__name__'] = '__main__'\n");
      Wrapper += TEXT("    _exec_globals['__file__'] = _script_path\n");
      Wrapper += TEXT("    _exec_globals['__package__'] = None\n");
      Wrapper += TEXT("    _exec_globals['__cached__'] = None\n");
      Wrapper += TEXT("    if _script_dir and _script_dir not in sys.path:\n");
      Wrapper += TEXT("        sys.path.insert(0, _script_dir)\n");
      Wrapper += FString::Printf(TEXT("    exec(compile(_user_code, r'%s', 'exec'), _exec_globals)\n"), *PyScriptPath);
    };
    Wrapper += TEXT("import sys\nimport traceback\n\n");
    Wrapper += FString::Printf(TEXT("_out = open(r'%s', 'w', encoding='utf-8')\n"), *PyOutputPath);
    Wrapper += FString::Printf(TEXT("_err = open(r'%s', 'w', encoding='utf-8')\n"), *PyErrorPath);
    Wrapper += TEXT("_old_out, _old_err = sys.stdout, sys.stderr\n");
    Wrapper += TEXT("sys.stdout, sys.stderr = _out, _err\n\n");
    Wrapper += TEXT("_success = True\n");
    Wrapper += TEXT("try:\n");

    if (bHasCode) {
      if (!FFileHelper::SaveStringToFile(Code, *CodePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Failed to write temp code file: %s"), *CodePath),
                            TEXT("FILE_WRITE_FAILED"));
        return true;
      }
      FString PyCodePath = NormalizePyPath(CodePath);
      FString PyCodeDir = NormalizePyPath(FPaths::GetPath(CodePath));
      Wrapper += FString::Printf(TEXT("    with open(r'%s', 'r', encoding='utf-8') as _f:\n"), *PyCodePath);
      Wrapper += TEXT("        _user_code = _f.read()\n");
      AppendPythonExec(PyCodePath, PyCodeDir);
    } else {
      // SECURITY: Sanitize file path to prevent directory traversal
      FString SafeFilePath = SanitizeProjectFilePath(File);
      if (SafeFilePath.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Invalid or unsafe file path: %s"), *File),
                            TEXT("SECURITY_VIOLATION"));
        return true;
      }

      // Resolve absolute path and verify it stays within project directory
      FString AbsoluteFilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / SafeFilePath);
      FPaths::NormalizeFilename(AbsoluteFilePath);

      FString NormalizedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
      FPaths::NormalizeDirectoryName(NormalizedProjectDir);
      if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
        NormalizedProjectDir += TEXT("/");
      }

      if (!AbsoluteFilePath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase)) {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("File path escapes project directory: %s"), *File),
                            TEXT("SECURITY_VIOLATION"));
        return true;
      }

      // Resolve symlinks and re-validate (prevents symlink escape attacks)
      FString ResolvedPath = FPlatformFileManager::Get().GetPlatformFile().ConvertToAbsolutePathForExternalAppForRead(*AbsoluteFilePath);
      if (!ResolvedPath.IsEmpty())
      {
        FPaths::NormalizeFilename(ResolvedPath);
        if (!ResolvedPath.StartsWith(NormalizedProjectDir, ESearchCase::IgnoreCase))
        {
          SendAutomationError(RequestingSocket, RequestId,
                              TEXT("Resolved file path escapes project directory (symlink detected)"),
                              TEXT("SECURITY_VIOLATION"));
          return true;
        }
        AbsoluteFilePath = ResolvedPath;
      }

      if (!FPaths::GetExtension(AbsoluteFilePath).Equals(TEXT("py"), ESearchCase::IgnoreCase)) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Python file execution requires a .py file"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }

      const int64 FileSize = IFileManager::Get().FileSize(*AbsoluteFilePath);
      if (FileSize < 0) {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Python file does not exist: %s"), *File),
                            TEXT("FILE_NOT_FOUND"));
        return true;
      }
      if (FileSize > MaxCodeSize) {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Python file exceeds maximum size (%d bytes)"), MaxCodeSize),
                            TEXT("CODE_TOO_LARGE"));
        return true;
      }

      // Use absolute path in Python wrapper (forward slashes)
      FString PyFilePath = NormalizePyPath(AbsoluteFilePath);
      FString PyFileDir = NormalizePyPath(FPaths::GetPath(AbsoluteFilePath));
      Wrapper += FString::Printf(TEXT("    with open(r'%s', 'r', encoding='utf-8') as _f:\n"), *PyFilePath);
      Wrapper += TEXT("        _user_code = _f.read()\n");
      AppendPythonExec(PyFilePath, PyFileDir);
    }

    Wrapper += TEXT("except:\n");
    Wrapper += TEXT("    traceback.print_exc()\n");
    Wrapper += TEXT("    _success = False\n");
    Wrapper += TEXT("finally:\n");
    Wrapper += TEXT("    sys.stdout, sys.stderr = _old_out, _old_err\n");
    Wrapper += TEXT("    _out.close()\n");
    Wrapper += TEXT("    _err.close()\n");
    Wrapper += FString::Printf(TEXT("    with open(r'%s', 'w') as _sf:\n"), *PyStatusPath);
    Wrapper += TEXT("        _sf.write('1' if _success else '0')\n");

    // Write wrapper to disk
    if (!FFileHelper::SaveStringToFile(Wrapper, *ScriptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Failed to write temp script: %s"), *ScriptPath),
                          TEXT("FILE_WRITE_FAILED"));
      return true;
    }

    SendProgressUpdate(RequestId, 0.0f, TEXT("Executing Python script"), true, CurrentRequestOrigin);

    IPythonScriptPlugin* PythonPlugin = FModuleManager::LoadModulePtr<IPythonScriptPlugin>(TEXT("PythonScriptPlugin"));
    if (!PythonPlugin) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Python Editor Script Plugin module is not loaded"),
                          TEXT("PYTHON_NOT_AVAILABLE"));
      return true;
    }
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
    if (!PythonPlugin->IsPythonInitialized()) {
      PythonPlugin->ForceEnablePythonAtRuntime();
    }
    if (!PythonPlugin->IsPythonInitialized()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Python Editor Script Plugin is not initialized yet"),
                          TEXT("PYTHON_NOT_AVAILABLE"));
      return true;
    }
#else
    // UE 5.0-5.5 IPythonScriptPlugin does not expose initialization helpers.
    // Loading the module and executing through ExecPythonCommandEx is the
    // compatible path for those versions.
#endif

    // Execute through PythonScriptPlugin directly. The console "py" command can
    // defer file loading on a fresh editor startup, racing temp-file cleanup.
    static constexpr double MaxPythonExecutionSeconds = 60.0;
    FPythonCommandEx PythonCommand;
    PythonCommand.Command = FString::Printf(TEXT("\"%s\""), *ScriptPath);
    PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
    PythonCommand.FileExecutionScope = EPythonFileExecutionScope::Private;
    PythonCommand.Flags |= EPythonCommandFlags::Unattended;
    double ExecStartTime = FPlatformTime::Seconds();
    bool bPythonCommandSucceeded = PythonPlugin->ExecPythonCommandEx(PythonCommand);
    double ExecElapsed = FPlatformTime::Seconds() - ExecStartTime;
    SendProgressUpdate(RequestId, 90.0f, TEXT("Collecting Python output"), true, CurrentRequestOrigin);
    if (ExecElapsed > MaxPythonExecutionSeconds) {
      UE_LOG(LogNebulaForgeBridgeSubsystem, Warning,
             TEXT("Python execution took %.1fs (exceeds %.1fs threshold). "
                  "Consider running long scripts via 'file' parameter in a separate process."),
             ExecElapsed, MaxPythonExecutionSeconds);
    }

    // Read results
    FString Output, Error, Status;
    FFileHelper::LoadFileToString(Output, *OutputPath);
    FFileHelper::LoadFileToString(Error, *ErrorPath);
    FFileHelper::LoadFileToString(Status, *StatusPath);
    if (!bPythonCommandSucceeded && Status.TrimStartAndEnd().IsEmpty()) {
      FString PythonError = PythonCommand.CommandResult;
      for (const FPythonLogOutputEntry& LogOutput : PythonCommand.LogOutput) {
        if (!PythonError.IsEmpty()) {
          PythonError += TEXT("\n");
        }
        PythonError += FString::Printf(TEXT("[%s] %s"), LexToString(LogOutput.Type), *LogOutput.Output);
      }
      if (!PythonError.IsEmpty()) {
        if (!Error.IsEmpty()) {
          Error += TEXT("\n");
        }
        Error += PythonError;
      }
    }

    // Cleanup happens automatically via FTempFileCleanup destructor

    bool bSuccess = bPythonCommandSucceeded && Status.TrimStartAndEnd().Equals(TEXT("1"));
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("output"), Output.TrimEnd());
    Result->SetStringField(TEXT("error"), Error.TrimEnd());

    SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
                           bSuccess ? TEXT("Python executed successfully") : TEXT("Python execution failed"),
                           Result, bSuccess ? FString() : TEXT("PYTHON_ERROR"));
    return true;
  }

  return false;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("System control actions require editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
