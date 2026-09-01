// =============================================================================
// NebulaForgeBridge_SequenceHandlers.cpp
// =============================================================================
// Sequencer & Timeline Handlers for NebulaForge Bridge
//
// HANDLERS IMPLEMENTED:
// ---------------------
// Section 1: Level Sequence
//   - create_level_sequence        : Create new ULevelSequence asset
//   - open_level_sequence          : Open sequence in editor
//   - save_level_sequence          : Save sequence asset
//
// Section 2: Track Management
//   - add_track                    : Add track to sequence
//   - remove_track                 : Remove track from sequence
//   - get_tracks                   : List all tracks
//
// Section 3: Keyframe Operations
//   - add_key                      : Add keyframe at time
//   - remove_key                   : Remove keyframe
//   - set_key_time                 : Move keyframe to new time
//   - set_key_value                : Set keyframe value
//
// Section 4: Binding
//   - add_binding                  : Add object binding
//   - remove_binding               : Remove object binding
//   - get_bindings                 : List all bindings
//
// VERSION COMPATIBILITY:
// ----------------------
// UE 5.0: Uses GetMasterTracks() for MovieScene
// UE 5.1+: Uses GetTracks() for MovieScene
// - MCP_GET_MOVIESCENE_TRACKS macro handles compatibility
//
// Copyright (c) 2024 NebulaForge Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first
#include "McpHandlerUtils.h"

#include "Modules/ModuleManager.h"  // Required for FModuleManager::IsModuleLoaded() runtime checks
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "LevelSequence.h"
#include "NebulaForgeBridgeGlobals.h"
#include "NebulaForgeBridgeHelpers.h"
#include "NebulaForgeBridgeSubsystem.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieSceneSection.h"
#include "MovieSceneSequence.h"
#include "MovieSceneTrack.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Sections/MovieSceneSubSection.h"
#if __has_include("Tracks/MovieSceneCinematicShotTrack.h") && __has_include("Sections/MovieSceneCinematicShotSection.h")
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#define MCP_HAS_CINEMATIC_SHOT_SECTION 1
#else
#define MCP_HAS_CINEMATIC_SHOT_SECTION 0
#endif
#include "UObject/UObjectIterator.h"

#if __has_include("Tracks/MovieSceneCameraShakeTrack.h") && __has_include("Camera/CameraShakeBase.h")
#include "Tracks/MovieSceneCameraShakeTrack.h"
#include "Camera/CameraShakeBase.h"
#define MCP_HAS_CAMERA_SHAKE_TRACK 1
#else
#define MCP_HAS_CAMERA_SHAKE_TRACK 0
#endif

// UE 5.0 compatibility: GetTracks() was introduced in UE 5.1, use GetMasterTracks() in 5.0
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_GET_MOVIESCENE_TRACKS(MovieScene) (MovieScene)->GetTracks()
#define MCP_GET_BINDING_TRACKS(Binding) (Binding).GetTracks()
#else
#define MCP_GET_MOVIESCENE_TRACKS(MovieScene) (MovieScene)->GetMasterTracks()
#define MCP_GET_BINDING_TRACKS(Binding) (Binding).GetTracks()
#endif

#if WITH_EDITOR
#include "Editor.h"
#include "EditorAssetLibrary.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#define MCP_HAS_EDITOR_ACTOR_SUBSYSTEM 1
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#define MCP_HAS_EDITOR_ACTOR_SUBSYSTEM 1
#else
#define MCP_HAS_EDITOR_ACTOR_SUBSYSTEM 0
#endif

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Editor/EditorEngine.h"
#include "Engine/Selection.h"
#include "Factories/Factory.h"
#include "IAssetTools.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Subsystems/AssetEditorSubsystem.h"

#if __has_include("MoviePipelineQueueEngineSubsystem.h") && __has_include("MoviePipelineOutputSetting.h") && __has_include("MoviePipelineImageSequenceOutput.h")
#include "MoviePipelineQueueEngineSubsystem.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelineConfigBase.h"
#include "MoviePipelinePrimaryConfig.h"
#include "MoviePipelineOutputSetting.h"
#if __has_include("MoviePipelineAntiAliasingSetting.h")
#include "MoviePipelineAntiAliasingSetting.h"
#define MCP_HAS_MRQ_AA 1
#else
#define MCP_HAS_MRQ_AA 0
#endif
#if __has_include("MoviePipelineBurnInSetting.h") && __has_include("MoviePipelineBurnInWidget.h")
#include "MoviePipelineBurnInSetting.h"
#include "MoviePipelineBurnInWidget.h"
#define MCP_HAS_MRQ_BURN_IN 1
#else
#define MCP_HAS_MRQ_BURN_IN 0
#endif
#if __has_include("MoviePipelineObjectIdPass.h")
#include "MoviePipelineObjectIdPass.h"
#define MCP_HAS_MRQ_OBJECT_ID 1
#else
#define MCP_HAS_MRQ_OBJECT_ID 0
#endif
#include "MoviePipelineImageSequenceOutput.h"
#if __has_include("MoviePipelineEXROutput.h") && __has_include("Imath/ImathBox.h")
#include "MoviePipelineEXROutput.h"
#define MCP_HAS_MRQ_EXR 1
#else
#define MCP_HAS_MRQ_EXR 0
#endif
#include "MoviePipelineExecutor.h"
#define MCP_HAS_MRQ 1
#else
#define MCP_HAS_MRQ 0
#endif

#if WITH_EDITOR && MCP_HAS_MRQ
void UNebulaForgeBridgeSubsystem::BeginMovieRenderQueueTracking(
    const FString &JobId, UMoviePipelineExecutorBase *Executor) {
  ActiveMRQJobId = JobId;
  LastMRQJobId = JobId;
  LastMRQError.Empty();
  bLastMRQCompleted = false;
  bLastMRQSuccess = false;
  if (!Executor) {
    return;
  }

  const TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
  Executor->OnExecutorErrored().AddLambda(
      [WeakThis, JobId](UMoviePipelineExecutorBase *, UMoviePipeline *,
                        bool bIsFatal, FText ErrorText) {
        if (UNebulaForgeBridgeSubsystem *Owner = WeakThis.Get()) {
          if (Owner->ActiveMRQJobId == JobId && bIsFatal) {
            Owner->LastMRQError = ErrorText.ToString();
          }
        }
      });
  Executor->OnExecutorFinished().AddLambda(
      [WeakThis, JobId](UMoviePipelineExecutorBase *, bool bSuccess) {
        if (UNebulaForgeBridgeSubsystem *Owner = WeakThis.Get()) {
          if (Owner->ActiveMRQJobId == JobId) {
            Owner->LastMRQJobId = JobId;
            Owner->bLastMRQCompleted = true;
            Owner->bLastMRQSuccess = bSuccess && Owner->LastMRQError.IsEmpty();
            Owner->ActiveMRQJobId.Empty();
          }
        }
      });
}

TSharedPtr<FJsonObject> UNebulaForgeBridgeSubsystem::GetMovieRenderQueueTrackedStatus(
    const FString &RequestedJobId) const {
  const FString JobId = !ActiveMRQJobId.IsEmpty() ? ActiveMRQJobId : LastMRQJobId;
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("known"), !JobId.IsEmpty() &&
                                      (RequestedJobId.IsEmpty() || RequestedJobId == JobId));
  Result->SetStringField(TEXT("jobId"), JobId);
  Result->SetStringField(TEXT("mrqJobId"), JobId);
  if (bLastMRQCompleted) {
    Result->SetStringField(TEXT("status"), bLastMRQSuccess ? TEXT("completed") : TEXT("failed"));
    Result->SetBoolField(TEXT("completed"), true);
    Result->SetBoolField(TEXT("success"), bLastMRQSuccess);
    if (!LastMRQError.IsEmpty()) Result->SetStringField(TEXT("error"), LastMRQError);
  } else {
    Result->SetStringField(TEXT("status"), ActiveMRQJobId.IsEmpty() ? TEXT("idle") : TEXT("rendering"));
    Result->SetBoolField(TEXT("completed"), false);
    Result->SetBoolField(TEXT("success"), false);
  }
  return Result;
}
#endif

// Header checks removed causing issues with private headers

// LevelSequenceEditorSubsystem is only available in UE 5.1+
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "LevelSequenceEditorSubsystem.h"
#define MCP_HAS_LEVELSEQUENCE_EDITOR_SUBSYSTEM 1
#else
#define MCP_HAS_LEVELSEQUENCE_EDITOR_SUBSYSTEM 0
#endif

#if __has_include("ILevelSequenceEditorToolkit.h")
#include "ILevelSequenceEditorToolkit.h"
#endif

#if __has_include("ISequencer.h")
#include "ISequencer.h"
#include "MovieSceneSequencePlayer.h"
#endif

#if __has_include("Tracks/MovieSceneFloatTrack.h")
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieSceneFloatTrack.h"

#endif

#if __has_include("Tracks/MovieSceneBoolTrack.h")
#include "Sections/MovieSceneBoolSection.h"
#include "Tracks/MovieSceneBoolTrack.h"

#endif

#if __has_include("Tracks/MovieScene3DTransformTrack.h")
#include "Tracks/MovieScene3DTransformTrack.h"
#endif

#include "Tracks/MovieSceneAudioTrack.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Tracks/MovieSceneMaterialTrack.h"
#include "Tracks/MovieSceneCustomPrimitiveDataTrack.h"
#if __has_include("MovieScene/MovieSceneNiagaraSystemTrack.h") && __has_include("MovieScene/MovieSceneNiagaraSystemSpawnSection.h")
#include "MovieScene/MovieSceneNiagaraSystemTrack.h"
#include "MovieScene/MovieSceneNiagaraSystemSpawnSection.h"
#define MCP_HAS_NIAGARA_SEQUENCE_TRACK 1
#else
#define MCP_HAS_NIAGARA_SEQUENCE_TRACK 0
#endif
#if __has_include("MovieScene/Parameters/MovieSceneNiagaraFloatParameterTrack.h") && __has_include("NiagaraVariable.h") && __has_include("NiagaraTypes.h")
#include "MovieScene/Parameters/MovieSceneNiagaraFloatParameterTrack.h"
#include "NiagaraVariable.h"
#include "NiagaraTypes.h"
#define MCP_HAS_NIAGARA_FLOAT_PARAMETER_TRACK 1
#else
#define MCP_HAS_NIAGARA_FLOAT_PARAMETER_TRACK 0
#endif
#if __has_include("MovieScene/Parameters/MovieSceneNiagaraIntegerParameterTrack.h") && __has_include("Channels/MovieSceneIntegerChannel.h") && __has_include("NiagaraVariable.h") && __has_include("NiagaraTypes.h")
#include "MovieScene/Parameters/MovieSceneNiagaraIntegerParameterTrack.h"
#include "Channels/MovieSceneIntegerChannel.h"
#define MCP_HAS_NIAGARA_INTEGER_PARAMETER_TRACK 1
#else
#define MCP_HAS_NIAGARA_INTEGER_PARAMETER_TRACK 0
#endif
#if __has_include("MovieScene/Parameters/MovieSceneNiagaraBoolParameterTrack.h") && __has_include("Channels/MovieSceneBoolChannel.h") && __has_include("NiagaraVariable.h") && __has_include("NiagaraTypes.h")
#include "MovieScene/Parameters/MovieSceneNiagaraBoolParameterTrack.h"
#include "Channels/MovieSceneBoolChannel.h"
#define MCP_HAS_NIAGARA_BOOL_PARAMETER_TRACK 1
#else
#define MCP_HAS_NIAGARA_BOOL_PARAMETER_TRACK 0
#endif
#if __has_include("MovieScene/Parameters/MovieSceneNiagaraVectorParameterTrack.h") && __has_include("Channels/MovieSceneFloatChannel.h") && __has_include("NiagaraVariable.h") && __has_include("NiagaraTypes.h")
#include "MovieScene/Parameters/MovieSceneNiagaraVectorParameterTrack.h"
#define MCP_HAS_NIAGARA_VECTOR_PARAMETER_TRACK 1
#else
#define MCP_HAS_NIAGARA_VECTOR_PARAMETER_TRACK 0
#endif
#include "Tracks/MovieSceneEventTrack.h"
#include "Sound/SoundBase.h"

#if __has_include("Sections/MovieScene3DTransformSection.h")
#include "Sections/MovieScene3DTransformSection.h"
#endif
#if __has_include("Channels/MovieSceneDoubleChannel.h")
#include "Channels/MovieSceneDoubleChannel.h"
#endif
#if __has_include("Channels/MovieSceneChannelProxy.h")
#include "Channels/MovieSceneChannelProxy.h"
#endif

// Optional components check
#if __has_include("ScopedTransaction.h")
#include "ScopedTransaction.h"
#elif __has_include("Misc/ScopedTransaction.h")
#include "Misc/ScopedTransaction.h"
#else
#define MCP_NO_SCOPED_TRANSACTION 1
#endif
#if __has_include("Camera/CameraActor.h")
#include "Camera/CameraActor.h"
#endif
#endif

FString UNebulaForgeBridgeSubsystem::ResolveSequencePath(
    const TSharedPtr<FJsonObject> &Payload) {
  FString Path;
  if (Payload.IsValid() && Payload->TryGetStringField(TEXT("path"), Path) &&
      !Path.IsEmpty()) {
#if WITH_EDITOR
    // Check existence first to avoid error log spam
    if (UEditorAssetLibrary::DoesAssetExist(Path)) {
      UObject *Obj = UEditorAssetLibrary::LoadAsset(Path);
      if (Obj) {
        return Obj->GetPathName();
      }
    }
#endif
    return Path;
  }
  if (!GCurrentSequencePath.IsEmpty())
    return GCurrentSequencePath;
  return FString();
}

TSharedPtr<FJsonObject>
UNebulaForgeBridgeSubsystem::EnsureSequenceEntry(const FString &SeqPath) {
  if (SeqPath.IsEmpty())
    return nullptr;
  if (TSharedPtr<FJsonObject> *Found = GSequenceRegistry.Find(SeqPath))
    return *Found;
  TSharedPtr<FJsonObject> NewObj = McpHandlerUtils::CreateResultObject();
  NewObj->SetStringField(TEXT("sequencePath"), SeqPath);
  GSequenceRegistry.Add(SeqPath, NewObj);
  return NewObj;
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceCreate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  // Runtime check: Verify LevelSequenceEditor module is loaded
  // This handles the case where headers were available at compile time
  // but the plugin is not enabled in the target project at runtime
  if (!FModuleManager::Get().IsModuleLoaded(TEXT("LevelSequenceEditor")))
  {
      if (!FModuleManager::Get().ModuleExists(TEXT("LevelSequenceEditor")) ||
          !FModuleManager::Get().LoadModule(TEXT("LevelSequenceEditor")))
      {
          SendAutomationError(Socket, RequestId,
              TEXT("LevelSequenceEditor plugin is not enabled in this project. Enable the Level Sequence Editor plugin to use Sequencer features."),
              TEXT("LEVELSEQUENCEEDITOR_PLUGIN_NOT_ENABLED"));
          return true;
      }
  }

  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString Name;
  LocalPayload->TryGetStringField(TEXT("name"), Name);
  FString Path;
  LocalPayload->TryGetStringField(TEXT("path"), Path);
  if (Name.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_create requires name"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString FullPath = Path.IsEmpty()
                         ? FString::Printf(TEXT("/Game/%s"), *Name)
                         : FString::Printf(TEXT("%s/%s"), *Path, *Name);

  FString DestFolder = Path.IsEmpty() ? TEXT("/Game") : Path;
  if (DestFolder.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
    DestFolder = FString::Printf(TEXT("/Game%s"), *DestFolder.RightChop(8));
  }

  TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakSubsystem(this);
  FString RequestIdArg = RequestId;

  // Execute on Game Thread
  UNebulaForgeBridgeSubsystem *Subsystem = this;
  UE_LOG(LogNebulaForgeBridgeSubsystem, Warning,
         TEXT("HandleSequenceCreate: Handing RequestID=%s Path=%s"),
         *RequestIdArg, *FullPath);

// Check existence first to avoid error log spam
  if (UEditorAssetLibrary::DoesAssetExist(FullPath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    VerifyAssetExists(Resp, FullPath);
    UE_LOG(LogNebulaForgeBridgeSubsystem, Warning,
           TEXT("HandleSequenceCreate: Sequence exists, sending response for "
                "RequestID=%s"),
           *RequestIdArg);
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                      TEXT("Sequence already exists"), Resp,
                                      FString());
    return true;
  }

  // Dynamic factory lookup
  UClass *FactoryClass = FindObject<UClass>(
      nullptr, TEXT("/Script/LevelSequenceEditor.LevelSequenceFactoryNew"));
  if (!FactoryClass)
    FactoryClass = LoadClass<UClass>(
        nullptr, TEXT("/Script/LevelSequenceEditor.LevelSequenceFactoryNew"));

  if (FactoryClass) {
    UFactory *Factory =
        NewObject<UFactory>(GetTransientPackage(), FactoryClass);
    FAssetToolsModule &AssetToolsModule =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools"));
    UObject *NewObj = AssetToolsModule.Get().CreateAsset(
        Name, DestFolder, ULevelSequence::StaticClass(), Factory);
if (NewObj) {
      if (!McpSafeAssetSave(NewObj)) {
        Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                          TEXT("Sequence created but save failed"), nullptr,
                                          TEXT("SAVE_FAILED"));
        return true;
      }
      GCurrentSequencePath = FullPath;
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      McpHandlerUtils::AddVerification(Resp, NewObj);
      UE_LOG(LogNebulaForgeBridgeSubsystem, Warning,
             TEXT("HandleSequenceCreate: Created sequence, sending response "
                  "for RequestID=%s"),
             *RequestIdArg);
      Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                        TEXT("Sequence created"), Resp,
                                        FString());
    } else {
      UE_LOG(
          LogNebulaForgeBridgeSubsystem, Error,
          TEXT("HandleSequenceCreate: Failed to create asset for RequestID=%s"),
          *RequestIdArg);
      Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                        TEXT("Failed to create sequence asset"),
                                        nullptr, TEXT("CREATE_ASSET_FAILED"));
    }
  } else {
    UE_LOG(LogNebulaForgeBridgeSubsystem, Error,
           TEXT("HandleSequenceCreate: Factory not found for RequestID=%s"),
           *RequestIdArg);
    Subsystem->SendAutomationResponse(
        Socket, RequestIdArg, false,
        TEXT("LevelSequenceFactoryNew class not found (Module not loaded?)"),
        nullptr, TEXT("FACTORY_NOT_AVAILABLE"));
  }
  return true;

#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_create requires editor build"), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceSetDisplayRate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_set_display_rate requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    if (UMovieScene *MovieScene = LevelSeq->GetMovieScene()) {
      FString FrameRateStr;
      double FrameRateVal = 0.0;

      FFrameRate NewRate;
      bool bRateFound = false;

      if (LocalPayload->TryGetStringField(TEXT("frameRate"), FrameRateStr)) {
        // Parse "30fps", "24000/1001", etc.
        // Simple parsing for standard rates
        if (FrameRateStr.EndsWith(TEXT("fps"))) {
          FrameRateStr.RemoveFromEnd(TEXT("fps"));
          NewRate = FFrameRate(FCString::Atoi(*FrameRateStr), 1);
          bRateFound = true;
        } else if (FrameRateStr.Contains(TEXT("/"))) {
          // Rational
          FString NumStr, DenomStr;
          if (FrameRateStr.Split(TEXT("/"), &NumStr, &DenomStr)) {
            NewRate =
                FFrameRate(FCString::Atoi(*NumStr), FCString::Atoi(*DenomStr));
            bRateFound = true;
          }
        } else {
          // Decimal string?
          if (FrameRateStr.IsNumeric()) {
            NewRate = FFrameRate(FCString::Atoi(*FrameRateStr), 1);
            bRateFound = true;
          }
        }
      } else if (LocalPayload->TryGetNumberField(TEXT("frameRate"),
                                                 FrameRateVal)) {
        NewRate = FFrameRate(FMath::RoundToInt(FrameRateVal), 1);
        bRateFound = true;
      }

if (bRateFound) {
        MovieScene->SetDisplayRate(NewRate);
        MovieScene->Modify();

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("displayRate"),
                             NewRate.ToPrettyText().ToString());
        McpHandlerUtils::AddVerification(Resp, LevelSeq);
        SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Display rate set"), Resp, FString());
        return true;
      }

      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Invalid frameRate format"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Invalid sequence type"), nullptr,
                         TEXT("INVALID_SEQUENCE"));
  return true;
#else
  SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("sequence_set_display_rate requires editor build"), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceSetProperties(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_set_properties requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakSubsystem(this);
  FString RequestIdArg = RequestId;

  // Capture simple types. For JsonObject, we need to capture the data, not the
  // pointer if we want to be safe, but since we parsed it above, we should
  // capture the parsed values. Parsing logic happens above. We'll capture the
  // parsed variables. But wait, the parsing logic in the original code is
  // INSIDE the block I'm replacing (lines 176-185). I need to include the
  // parsing inside the Async task or move it out. I'll move the parsing INSIDE
  // the Async task, but I need to capture LocalPayload. LocalPayload is a
  // SharedPtr, so it's safe to capture.

  UNebulaForgeBridgeSubsystem *Subsystem = this;
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                      TEXT("Sequence not found"), nullptr,
                                      TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    if (UMovieScene *MovieScene = LevelSeq->GetMovieScene()) {
      bool bModified = false;
      double FrameRateValue = 0.0;
      double LengthInFramesValue = 0.0;
      double PlaybackStartValue = 0.0;
      double PlaybackEndValue = 0.0;

      const bool bHasFrameRate =
          LocalPayload->TryGetNumberField(TEXT("frameRate"), FrameRateValue);
      const bool bHasLengthInFrames = LocalPayload->TryGetNumberField(
          TEXT("lengthInFrames"), LengthInFramesValue);
      const bool bHasPlaybackStart = LocalPayload->TryGetNumberField(
          TEXT("playbackStart"), PlaybackStartValue);
      const bool bHasPlaybackEnd = LocalPayload->TryGetNumberField(
          TEXT("playbackEnd"), PlaybackEndValue);

      if (bHasFrameRate) {
        if (FrameRateValue <= 0.0) {
          Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                            TEXT("frameRate must be > 0"),
                                            nullptr, TEXT("INVALID_ARGUMENT"));
          return true;
        }
        const int32 Rounded =
            FMath::Clamp<int32>(FMath::RoundToInt(FrameRateValue), 1, 960);
        FFrameRate CurrentRate = MovieScene->GetDisplayRate();
        FFrameRate NewRate(Rounded, 1);
        if (NewRate != CurrentRate) {
          MovieScene->SetDisplayRate(NewRate);
          bModified = true;
        }
      }

      if (bHasPlaybackStart || bHasPlaybackEnd || bHasLengthInFrames) {
        TRange<FFrameNumber> ExistingRange = MovieScene->GetPlaybackRange();
        FFrameNumber StartFrame = ExistingRange.GetLowerBoundValue();
        FFrameNumber EndFrame = ExistingRange.GetUpperBoundValue();

        if (bHasPlaybackStart)
          StartFrame = FFrameNumber(static_cast<int32>(PlaybackStartValue));
        if (bHasPlaybackEnd)
          EndFrame = FFrameNumber(static_cast<int32>(PlaybackEndValue));
        else if (bHasLengthInFrames)
          EndFrame =
              StartFrame +
              FMath::Max<int32>(0, static_cast<int32>(LengthInFramesValue));

        if (EndFrame < StartFrame)
          EndFrame = StartFrame;
        MovieScene->SetPlaybackRange(
            TRange<FFrameNumber>(StartFrame, EndFrame));
        bModified = true;
      }

      if (bModified)
        MovieScene->Modify();

      FFrameRate FR = MovieScene->GetDisplayRate();
      TSharedPtr<FJsonObject> FrameRateObj = McpHandlerUtils::CreateResultObject();
      FrameRateObj->SetNumberField(TEXT("numerator"), FR.Numerator);
      FrameRateObj->SetNumberField(TEXT("denominator"), FR.Denominator);
      Resp->SetObjectField(TEXT("frameRate"), FrameRateObj);

      TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
      const double Start =
          static_cast<double>(Range.GetLowerBoundValue().Value);
      const double End = static_cast<double>(Range.GetUpperBoundValue().Value);
      Resp->SetNumberField(TEXT("playbackStart"), Start);
      Resp->SetNumberField(TEXT("playbackEnd"), End);
      Resp->SetNumberField(TEXT("duration"), End - Start);
      Resp->SetBoolField(TEXT("applied"), bModified);

      Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                        TEXT("properties updated"), Resp,
                                        FString());
      return true;
    }
  }
  Resp->SetObjectField(TEXT("frameRate"), McpHandlerUtils::CreateResultObject());
  Resp->SetNumberField(TEXT("playbackStart"), 0.0);
  Resp->SetNumberField(TEXT("playbackEnd"), 0.0);
  Resp->SetNumberField(TEXT("duration"), 0.0);
  Resp->SetBoolField(TEXT("applied"), false);
  Subsystem->SendAutomationResponse(
      Socket, RequestIdArg, false,
      TEXT("sequence_set_properties is not available in this editor build or "
           "for this sequence type"),
      Resp, TEXT("NOT_IMPLEMENTED"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_set_properties requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceOpen(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_open requires a sequence path"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakSubsystem(this);
  FString RequestIdArg = RequestId;
  UNebulaForgeBridgeSubsystem *Subsystem = this;
  UE_LOG(LogNebulaForgeBridgeSubsystem, Warning,
         TEXT("HandleSequenceOpen: Opening sequence %s for RequestID=%s"),
         *SeqPath, *RequestIdArg);
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                      TEXT("Sequence not found"), nullptr,
                                      TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if MCP_HAS_LEVELSEQUENCE_EDITOR_SUBSYSTEM
  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    if (GEditor) {
      if (ULevelSequenceEditorSubsystem *LSES =
              GEditor->GetEditorSubsystem<ULevelSequenceEditorSubsystem>()) {
        if (UAssetEditorSubsystem *AssetEditorSS =
                GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()) {
          AssetEditorSS->OpenEditorForAsset(LevelSeq);
          Resp->SetStringField(TEXT("sequencePath"), SeqPath);
          Resp->SetStringField(TEXT("message"), TEXT("Sequence opened"));
          UE_LOG(LogNebulaForgeBridgeSubsystem, Warning,
                 TEXT("HandleSequenceOpen: Successfully opened in LSES, "
                      "sending response for RequestID=%s"),
                 *RequestIdArg);
          Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                            TEXT("Sequence opened"), Resp,
                                            FString());
          return true;
        }
      }
    }
  }
#endif

  if (GEditor) {
    if (UAssetEditorSubsystem *AssetEditorSS =
            GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()) {
      AssetEditorSS->OpenEditorForAsset(SeqObj);
    }
  }
  Resp->SetStringField(TEXT("sequencePath"), SeqPath);
  Resp->SetStringField(TEXT("message"), TEXT("Sequence opened (asset editor)"));
  UE_LOG(LogNebulaForgeBridgeSubsystem, Warning,
         TEXT("HandleSequenceOpen: Opened via AssetEditorSS, sending response "
              "for RequestID=%s"),
         *RequestIdArg);
  Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                    TEXT("Sequence opened"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_open requires editor build."), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceAddCamera(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_add_camera requires a sequence path"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if MCP_HAS_EDITOR_ACTOR_SUBSYSTEM
  if (GEditor) {
    UClass *CameraClass = ACameraActor::StaticClass();
    AActor *Spawned = SpawnActorInActiveWorld<AActor>(
        CameraClass, FVector::ZeroVector, FRotator::ZeroRotator,
        TEXT("SequenceCamera"));
    if (Spawned) {
      // Fix for Issue #6: Auto-bind the camera to the sequence
      if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
        if (UMovieScene *MovieScene = LevelSeq->GetMovieScene()) {
          FGuid BindingGuid = MovieScene->AddPossessable(
              Spawned->GetActorLabel(), Spawned->GetClass());
          if (MovieScene->FindPossessable(BindingGuid)) {
            MovieScene->Modify();
            Resp->SetStringField(TEXT("bindingGuid"), BindingGuid.ToString());
          }
        }
      }

      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("actorLabel"), Spawned->GetActorLabel());
      SendAutomationResponse(Socket, RequestId, true,
                             TEXT("Camera actor spawned and bound to sequence"),
                             Resp, FString());
      return true;
    }
  }
  SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to add camera"),
                         nullptr, TEXT("ADD_CAMERA_FAILED"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("UEditorActorSubsystem not available"), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_add_camera requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequencePlay(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No sequence selected or path provided"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakSubsystem(this);
  FString RequestIdArg = RequestId;
  UNebulaForgeBridgeSubsystem *Subsystem = this;
  ULevelSequence *LevelSeq =
      Cast<ULevelSequence>(UEditorAssetLibrary::LoadAsset(SeqPath));
  if (LevelSeq) {
    if (ULevelSequenceEditorBlueprintLibrary::OpenLevelSequence(LevelSeq)) {
      ULevelSequenceEditorBlueprintLibrary::Play();
      Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                        TEXT("Sequence playing"), nullptr);
      return true;
    }
  }
  Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                    TEXT("Failed to open or play sequence"),
                                    nullptr, TEXT("EXECUTION_ERROR"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_play requires editor build."), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceAddActor(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString ActorName;
  LocalPayload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_add_actor requires a sequence path"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  // Reuse multi-actor binding logic for a single actor by forwarding to
  // HandleSequenceAddActors with a one-element actorNames array and the
  // resolved sequence path. This ensures real LevelSequence bindings are
  // applied when supported by the editor build.
  TSharedPtr<FJsonObject> ForwardPayload = McpHandlerUtils::CreateResultObject();
  ForwardPayload->SetStringField(TEXT("path"), SeqPath);
  TArray<TSharedPtr<FJsonValue>> NamesArray;
  NamesArray.Add(MakeShared<FJsonValueString>(ActorName));
  ForwardPayload->SetArrayField(TEXT("actorNames"), NamesArray);

  return HandleSequenceAddActors(RequestId, ForwardPayload, Socket);
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_add_actor requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceAddActors(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
  LocalPayload->TryGetArrayField(TEXT("actorNames"), Arr);
  if (!Arr || Arr->Num() == 0) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("actorNames required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_add_actors requires a sequence path"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  TArray<FString> Names;
  Names.Reserve(Arr->Num());
  for (const TSharedPtr<FJsonValue> &V : *Arr) {
    if (V.IsValid() && V->Type == EJson::String)
      Names.Add(V->AsString());
  }

  TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakSubsystem(this);
  FString RequestIdArg = RequestId;
  UNebulaForgeBridgeSubsystem *Subsystem = this;
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                      TEXT("Sequence not found"), nullptr,
                                      TEXT("INVALID_SEQUENCE"));
    return true;
  }
  if (!GEditor) {
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                      TEXT("Editor not available"), nullptr,
                                      TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

#if MCP_HAS_EDITOR_ACTOR_SUBSYSTEM
  if (UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
    TArray<TSharedPtr<FJsonValue>> Results;
    Results.Reserve(Names.Num());
    for (const FString &Name : Names) {
      TSharedPtr<FJsonObject> Item = McpHandlerUtils::CreateResultObject();
      Item->SetStringField(TEXT("name"), Name);
      // Use robust actor lookup that checks label, name, and UAID
      AActor *Found = Subsystem->FindActorByName(Name);

      if (!Found) {
        Item->SetBoolField(TEXT("success"), false);
        Item->SetStringField(TEXT("error"), TEXT("Actor not found"));
      } else {
        if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
          UMovieScene *MovieScene = LevelSeq->GetMovieScene();
          if (MovieScene) {
            FGuid BindingGuid = MovieScene->AddPossessable(
                Found->GetActorLabel(), Found->GetClass());
            if (MovieScene->FindPossessable(BindingGuid)) {
              Item->SetBoolField(TEXT("success"), true);
              Item->SetStringField(TEXT("bindingGuid"), BindingGuid.ToString());
              MovieScene->Modify();
            } else {
              Item->SetBoolField(TEXT("success"), false);
              Item->SetStringField(
                  TEXT("error"), TEXT("Failed to create possessable binding"));
            }
          } else {
            Item->SetBoolField(TEXT("success"), false);
            Item->SetStringField(TEXT("error"),
                                 TEXT("Sequence has no MovieScene"));
          }
        } else {
          Item->SetBoolField(TEXT("success"), false);
          Item->SetStringField(TEXT("error"),
                               TEXT("Sequence object is not a LevelSequence"));
        }
      }
      Results.Add(MakeShared<FJsonValueObject>(Item));
    }
    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    Out->SetArrayField(TEXT("results"), Results);
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                      TEXT("Actors processed"), Out, FString());
    return true;
  }
  Subsystem->SendAutomationResponse(
      Socket, RequestIdArg, false, TEXT("EditorActorSubsystem not available"),
      nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
  return true;
#else
  Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                    TEXT("UEditorActorSubsystem not available"),
                                    nullptr, TEXT("NOT_AVAILABLE"));
#endif
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_add_actors requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceAddSpawnable(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString ClassName;
  LocalPayload->TryGetStringField(TEXT("className"), ClassName);
  if (ClassName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("className required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_add_spawnable_from_class requires a sequence path"),
        nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  UClass *ResolvedClass = nullptr;
  if (ClassName.StartsWith(TEXT("/")) || ClassName.Contains(TEXT("/"))) {
    if (UObject *Loaded = UEditorAssetLibrary::LoadAsset(ClassName)) {
      if (UBlueprint *BP = Cast<UBlueprint>(Loaded))
        ResolvedClass = BP->GeneratedClass;
      else if (UClass *C = Cast<UClass>(Loaded))
        ResolvedClass = C;
    }
  }
  if (!ResolvedClass)
    ResolvedClass = ResolveClassByName(ClassName);
  if (!ResolvedClass) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Class not found"),
                           nullptr, TEXT("CLASS_NOT_FOUND"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    UMovieScene *MovieScene = LevelSeq->GetMovieScene();
    if (MovieScene) {
      UObject *DefaultObject = ResolvedClass->GetDefaultObject();
      if (DefaultObject) {
        FGuid BindingGuid = MovieScene->AddSpawnable(ClassName, *DefaultObject);
        if (MovieScene->FindSpawnable(BindingGuid)) {
          MovieScene->Modify();
          TSharedPtr<FJsonObject> SpawnableResp = McpHandlerUtils::CreateResultObject();
          SpawnableResp->SetBoolField(TEXT("success"), true);
          SpawnableResp->SetStringField(TEXT("className"), ClassName);
          SpawnableResp->SetStringField(TEXT("bindingGuid"),
                                        BindingGuid.ToString());
          SendAutomationResponse(Socket, RequestId, true,
                                 TEXT("Spawnable added to sequence"),
                                 SpawnableResp, FString());
          return true;
        }
      }
    }
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create spawnable binding"), nullptr,
                           TEXT("SPAWNABLE_CREATION_FAILED"));
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Sequence object is not a LevelSequence"),
                         nullptr, TEXT("INVALID_SEQUENCE_TYPE"));
  return true;
#else
  SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("sequence_add_spawnable_from_class requires editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceRemoveActors(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
  LocalPayload->TryGetArrayField(TEXT("actorNames"), Arr);
  if (!Arr || Arr->Num() == 0) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("actorNames required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_remove_actors requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

#if MCP_HAS_EDITOR_ACTOR_SUBSYSTEM
  if (UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
    TArray<TSharedPtr<FJsonValue>> Removed;
    int32 RemovedCount = 0;
    for (const TSharedPtr<FJsonValue> &V : *Arr) {
      if (!V.IsValid() || V->Type != EJson::String)
        continue;
      FString Name = V->AsString();
      TSharedPtr<FJsonObject> Item = McpHandlerUtils::CreateResultObject();
      Item->SetStringField(TEXT("name"), Name);

      if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
        UMovieScene *MovieScene = LevelSeq->GetMovieScene();
        if (MovieScene) {
          bool bRemoved = false;
          for (const FMovieSceneBinding &Binding :
               const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
            FString BindingName;
            if (FMovieScenePossessable *Possessable =
                    MovieScene->FindPossessable(Binding.GetObjectGuid())) {
              BindingName = Possessable->GetName();
            } else if (FMovieSceneSpawnable *Spawnable =
                           MovieScene->FindSpawnable(Binding.GetObjectGuid())) {
              BindingName = Spawnable->GetName();
            }

            if (BindingName.Equals(Name, ESearchCase::IgnoreCase)) {
              MovieScene->RemovePossessable(Binding.GetObjectGuid());
              MovieScene->Modify();
              bRemoved = true;
              break;
            }
          }
          if (bRemoved) {
            Item->SetBoolField(TEXT("success"), true);
            Item->SetStringField(TEXT("status"), TEXT("Actor removed"));
            RemovedCount++;
          } else {
            Item->SetBoolField(TEXT("success"), false);
            Item->SetStringField(TEXT("error"),
                                 TEXT("Actor not found in sequence bindings"));
          }
        } else {
          Item->SetBoolField(TEXT("success"), false);
          Item->SetStringField(TEXT("error"),
                               TEXT("Sequence has no MovieScene"));
        }
      } else {
        Item->SetBoolField(TEXT("success"), false);
        Item->SetStringField(TEXT("error"),
                             TEXT("Sequence object is not a LevelSequence"));
      }
      Removed.Add(MakeShared<FJsonValueObject>(Item));
    }
    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    Out->SetArrayField(TEXT("removedActors"), Removed);
    Out->SetNumberField(TEXT("bindingsProcessed"), RemovedCount);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Actors processed for removal"), Out,
                           FString());
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("EditorActorSubsystem not available"), nullptr,
                         TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("UEditorActorSubsystem not available"), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_remove_actors requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceGetBindings(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_get_bindings requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    if (UMovieScene *MovieScene = LevelSeq->GetMovieScene()) {
      TArray<TSharedPtr<FJsonValue>> BindingsArray;
      for (const FMovieSceneBinding &B :
           const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
        TSharedPtr<FJsonObject> Bobj = McpHandlerUtils::CreateResultObject();
        Bobj->SetStringField(TEXT("id"), B.GetObjectGuid().ToString());

        FString BindingName;
        if (FMovieScenePossessable *Possessable =
                MovieScene->FindPossessable(B.GetObjectGuid())) {
          BindingName = Possessable->GetName();
        } else if (FMovieSceneSpawnable *Spawnable =
                       MovieScene->FindSpawnable(B.GetObjectGuid())) {
          BindingName = Spawnable->GetName();
        }

        Bobj->SetStringField(TEXT("name"), BindingName);
        BindingsArray.Add(MakeShared<FJsonValueObject>(Bobj));
      }
      Resp->SetArrayField(TEXT("bindings"), BindingsArray);
      SendAutomationResponse(Socket, RequestId, true, TEXT("bindings listed"),
                             Resp, FString());
      return true;
    }
  }
  Resp->SetArrayField(TEXT("bindings"), TArray<TSharedPtr<FJsonValue>>());
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("bindings listed (empty)"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_get_bindings requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceGetProperties(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_get_properties requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    if (UMovieScene *MovieScene = LevelSeq->GetMovieScene()) {
      FFrameRate FR = MovieScene->GetDisplayRate();
      TSharedPtr<FJsonObject> FrameRateObj = McpHandlerUtils::CreateResultObject();
      FrameRateObj->SetNumberField(TEXT("numerator"), FR.Numerator);
      FrameRateObj->SetNumberField(TEXT("denominator"), FR.Denominator);
      Resp->SetObjectField(TEXT("frameRate"), FrameRateObj);
      TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
      const double Start =
          static_cast<double>(Range.GetLowerBoundValue().Value);
      const double End = static_cast<double>(Range.GetUpperBoundValue().Value);
      Resp->SetNumberField(TEXT("playbackStart"), Start);
      Resp->SetNumberField(TEXT("playbackEnd"), End);
      Resp->SetNumberField(TEXT("duration"), End - Start);
      SendAutomationResponse(Socket, RequestId, true,
                             TEXT("properties retrieved"), Resp, FString());
      return true;
    }
  }
  Resp->SetObjectField(TEXT("frameRate"), McpHandlerUtils::CreateResultObject());
  Resp->SetNumberField(TEXT("playbackStart"), 0.0);
  Resp->SetNumberField(TEXT("playbackEnd"), 0.0);
  Resp->SetNumberField(TEXT("duration"), 0.0);
  SendAutomationResponse(Socket, RequestId, true, TEXT("properties retrieved"),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_get_properties requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceSetPlaybackSpeed(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  double Speed = 1.0;
  LocalPayload->TryGetNumberField(TEXT("speed"), Speed);
  if (Speed <= 0.0) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid speed (must be > 0)"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_set_playback_speed requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakSubsystem(this);
  FString RequestIdArg = RequestId; // Capture

  // Execute on Game Thread
  UNebulaForgeBridgeSubsystem *Subsystem = this;
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                      TEXT("Sequence not found"), nullptr,
                                      TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (GEditor) {
    if (UAssetEditorSubsystem *AssetEditorSS =
            GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()) {
      IAssetEditorInstance *Editor =
          AssetEditorSS->FindEditorForAsset(SeqObj, false);
      // ILevelSequenceEditorToolkit inherits from IAssetEditorInstance.
      // Use static_cast since we verified the toolkit type through FindEditorForAsset.
      // Note: dynamic_cast doesn't work with /GR- compiler flag in UE 5.7+
      if (ILevelSequenceEditorToolkit *LSEditor =
              static_cast<ILevelSequenceEditorToolkit *>(Editor)) {
		if (LSEditor->GetSequencer().IsValid()) {
				LSEditor->GetSequencer()->SetPlaybackSpeed(static_cast<float>(Speed));
          Subsystem->SendAutomationResponse(
              Socket, RequestIdArg, true,
              FString::Printf(TEXT("Playback speed set to %.2f"), Speed),
              nullptr);
          return true;
        } else {
          UE_LOG(LogNebulaForgeBridgeSubsystem, Error,
                 TEXT("HandleSequenceSetPlaybackSpeed: Sequencer invalid for "
                      "asset %s"),
                 *SeqObj->GetName());
        }
      }
    }
  }

  Subsystem->SendAutomationResponse(
      Socket, RequestIdArg, false,
      TEXT("Sequence editor not open or interface unavailable"), nullptr,
      TEXT("EDITOR_NOT_OPEN"));
  return true;
#else
  SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("sequence_set_playback_speed requires editor build."), nullptr,
      TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequencePause(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_pause requires a sequence path"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }
#if WITH_EDITOR
  TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakSubsystem(this);
  FString RequestIdArg = RequestId;
  UNebulaForgeBridgeSubsystem *Subsystem = this;

  ULevelSequence *LevelSeq =
      Cast<ULevelSequence>(UEditorAssetLibrary::LoadAsset(SeqPath));
  if (LevelSeq) {
    // Ensure it's the active one
    if (ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence() ==
        LevelSeq) {
      ULevelSequenceEditorBlueprintLibrary::Pause();
      Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                        TEXT("Sequence paused"), nullptr);
      return true;
    }
  }
  Subsystem->SendAutomationResponse(
      Socket, RequestIdArg, false,
      TEXT("Sequence not currently open in editor"), nullptr,
      TEXT("EXECUTION_ERROR"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_pause requires editor build."), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceStop(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_stop requires a sequence path"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }
#if WITH_EDITOR
  TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakSubsystem(this);
  FString RequestIdArg = RequestId;
  UNebulaForgeBridgeSubsystem *Subsystem = this;

  ULevelSequence *LevelSeq =
      Cast<ULevelSequence>(UEditorAssetLibrary::LoadAsset(SeqPath));
  if (LevelSeq) {
    if (ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence() ==
        LevelSeq) {
      ULevelSequenceEditorBlueprintLibrary::Pause();

      FMovieSceneSequencePlaybackParams PlaybackParams;
      PlaybackParams.Frame = FFrameTime(0);
      PlaybackParams.UpdateMethod = EUpdatePositionMethod::Scrub;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
      // UE 5.4+: SetGlobalPosition available
      ULevelSequenceEditorBlueprintLibrary::SetGlobalPosition(PlaybackParams);
#else
      // UE 5.0-5.3 fallback - use SetCurrentTime (takes int32 frame number)
      ULevelSequenceEditorBlueprintLibrary::SetCurrentTime(0);
#endif

      Subsystem->SendAutomationResponse(
          Socket, RequestIdArg, true, TEXT("Sequence stopped (reset to start)"),
          nullptr);
      return true;
    }
  }
  Subsystem->SendAutomationResponse(
      Socket, RequestIdArg, false,
      TEXT("Sequence not currently open in editor"), nullptr,
      TEXT("EXECUTION_ERROR"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_stop requires editor build."), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceList(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> SequencesArray;

  // Use Asset Registry to find all LevelSequence assets, not string matching
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(ULevelSequence::StaticClass()->GetClassPathName());
#else
  // UE 5.0: Use ClassNames instead of ClassPaths
  Filter.ClassNames.Add(ULevelSequence::StaticClass()->GetFName());
#endif
  Filter.bRecursiveClasses = true;
  Filter.bRecursivePaths = true;
  Filter.PackagePaths.Add(FName("/Game"));

  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);

  for (const FAssetData &Asset : AssetList) {
    TSharedPtr<FJsonObject> SeqObj = McpHandlerUtils::CreateResultObject();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    SeqObj->SetStringField(TEXT("path"), Asset.GetObjectPathString());
#else
    // UE 5.0: Construct object path manually
    SeqObj->SetStringField(TEXT("path"), FString::Printf(TEXT("%s.%s"), *Asset.PackageName.ToString(), *Asset.AssetName.ToString()));
#endif
    SeqObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
    SequencesArray.Add(MakeShared<FJsonValueObject>(SeqObj));
  }

  Resp->SetArrayField(TEXT("sequences"), SequencesArray);
  Resp->SetNumberField(TEXT("count"), SequencesArray.Num());
  SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Found %d sequences"), SequencesArray.Num()), Resp,
      FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_list requires editor build."), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceDuplicate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SourcePath;
  LocalPayload->TryGetStringField(TEXT("path"), SourcePath);
  FString DestinationPath;
  LocalPayload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
  if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_duplicate requires path and destinationPath"), nullptr,
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Auto-resolve relative destination path (if just a name is provided)
  if (!DestinationPath.IsEmpty() && !DestinationPath.StartsWith(TEXT("/"))) {
    FString ParentPath = FPaths::GetPath(SourcePath);
    DestinationPath =
        FString::Printf(TEXT("%s/%s"), *ParentPath, *DestinationPath);
  }

#if WITH_EDITOR
  UObject *SourceSeq = UEditorAssetLibrary::LoadAsset(SourcePath);
  if (!SourceSeq) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source sequence not found: %s"), *SourcePath),
        nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }
  UObject *DuplicatedSeq =
      UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath);
  if (DuplicatedSeq) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("sourcePath"), SourcePath);
    Resp->SetStringField(TEXT("destinationPath"), DestinationPath);
    Resp->SetStringField(TEXT("duplicatedPath"), DuplicatedSeq->GetPathName());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Sequence duplicated successfully"), Resp,
                           FString());
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Failed to duplicate sequence"), nullptr,
                         TEXT("OPERATION_FAILED"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_duplicate requires editor build."),
                         nullptr, TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceRename(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString Path;
  LocalPayload->TryGetStringField(TEXT("path"), Path);
  FString NewName;
  LocalPayload->TryGetStringField(TEXT("newName"), NewName);
  if (Path.IsEmpty() || NewName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_rename requires path and newName"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Auto-resolve relative new name to full path
  if (!NewName.IsEmpty() && !NewName.StartsWith(TEXT("/"))) {
    FString ParentPath = FPaths::GetPath(Path);
    NewName = FString::Printf(TEXT("%s/%s"), *ParentPath, *NewName);
  }

#if WITH_EDITOR
  if (UEditorAssetLibrary::RenameAsset(Path, NewName)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("oldPath"), Path);
    Resp->SetStringField(TEXT("newName"), NewName);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Sequence renamed successfully"), Resp,
                           FString());
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Failed to rename sequence"), nullptr,
                         TEXT("OPERATION_FAILED"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_rename requires editor build."),
                         nullptr, TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceDelete(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString Path;
  LocalPayload->TryGetStringField(TEXT("path"), Path);
  if (Path.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_delete requires path"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
#if WITH_EDITOR
  if (!UEditorAssetLibrary::DoesAssetExist(Path)) {
    // Idempotent success - if it's already gone, good.
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("deletedPath"), Path);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Sequence deleted (or did not exist)"), Resp,
                           FString());
    return true;
  }

  if (UEditorAssetLibrary::DeleteAsset(Path)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("deletedPath"), Path);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Sequence deleted successfully"), Resp,
                           FString());
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Failed to delete sequence"), nullptr,
                         TEXT("OPERATION_FAILED"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_delete requires editor build."),
                         nullptr, TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceGetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_get_metadata requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }
#if WITH_EDITOR
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("path"), SeqPath);
  Resp->SetStringField(TEXT("name"), SeqObj->GetName());
  Resp->SetStringField(TEXT("class"), SeqObj->GetClass()->GetName());
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Sequence metadata retrieved"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_get_metadata requires editor build."),
                         nullptr, TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceAddKeyframe(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_add_keyframe requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

  FString BindingIdStr;
  LocalPayload->TryGetStringField(TEXT("bindingId"), BindingIdStr);
  FString ActorName;
  LocalPayload->TryGetStringField(TEXT("actorName"), ActorName);
  FString PropertyName;
  LocalPayload->TryGetStringField(TEXT("property"), PropertyName);

  if (BindingIdStr.IsEmpty() && ActorName.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Either bindingId or actorName must be provided. bindingId is the "
             "GUID from add_actor/get_bindings. actorName is the label of an "
             "actor already bound to the sequence. Example: {\"actorName\": "
             "\"MySphere\", \"property\": \"Location\", \"frame\": 0, "
             "\"value\": {\"x\":0,\"y\":0,\"z\":0}}"),
        nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  double Frame = 0.0;
  if (!LocalPayload->TryGetNumberField(TEXT("frame"), Frame)) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("frame number is required. Example: "
                                "{\"frame\": 30} for keyframe at frame 30"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  auto NormalizeTransformPropertyAlias = [&](const TCHAR *Alias,
                                             const TCHAR *TransformField) {
    if (!PropertyName.Equals(Alias, ESearchCase::IgnoreCase)) {
      return false;
    }

    const TSharedPtr<FJsonObject> *ValueObj = nullptr;
    if (LocalPayload->TryGetObjectField(TEXT("value"), ValueObj) &&
        ValueObj && ValueObj->IsValid()) {
      TSharedPtr<FJsonObject> TransformValue = MakeShared<FJsonObject>();
      TransformValue->SetObjectField(TransformField, *ValueObj);
      LocalPayload->SetObjectField(TEXT("value"), TransformValue);
    }

    PropertyName = TEXT("Transform");
    return true;
  };

  NormalizeTransformPropertyAlias(TEXT("Location"), TEXT("location")) ||
      NormalizeTransformPropertyAlias(TEXT("Translation"), TEXT("location")) ||
      NormalizeTransformPropertyAlias(TEXT("Rotation"), TEXT("rotation")) ||
      NormalizeTransformPropertyAlias(TEXT("Scale"), TEXT("scale"));

#if WITH_EDITOR
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    UMovieScene *MovieScene = LevelSeq->GetMovieScene();
    if (MovieScene) {
      FGuid BindingGuid;
      if (!BindingIdStr.IsEmpty()) {
        FGuid::Parse(BindingIdStr, BindingGuid);
      } else if (!ActorName.IsEmpty()) {
        for (const FMovieSceneBinding &Binding :
             const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
          FString BindingName;
          if (FMovieScenePossessable *Possessable =
                  MovieScene->FindPossessable(Binding.GetObjectGuid())) {
            BindingName = Possessable->GetName();
          } else if (FMovieSceneSpawnable *Spawnable =
                         MovieScene->FindSpawnable(Binding.GetObjectGuid())) {
            BindingName = Spawnable->GetName();
          }

          if (BindingName.Equals(ActorName, ESearchCase::IgnoreCase)) {
            BindingGuid = Binding.GetObjectGuid();
            break;
          }
        }
      }

      if (!BindingGuid.IsValid()) {
        FString Target = !BindingIdStr.IsEmpty() ? BindingIdStr : ActorName;
        SendAutomationResponse(
            Socket, RequestId, false,
            FString::Printf(TEXT("Binding not found for '%s'. Ensure actor is "
                                 "bound to sequence."),
                            *Target),
            nullptr, TEXT("BINDING_NOT_FOUND"));
        return true;
      }

      FMovieSceneBinding *Binding = MovieScene->FindBinding(BindingGuid);
      if (!Binding) {
        SendAutomationResponse(Socket, RequestId, false,
                               TEXT("Binding object not found in sequence"),
                               nullptr, TEXT("BINDING_NOT_FOUND"));
        return true;
      }

      if (PropertyName.Equals(TEXT("Transform"), ESearchCase::IgnoreCase)) {
        UMovieScene3DTransformTrack *Track =
            MovieScene->FindTrack<UMovieScene3DTransformTrack>(
                BindingGuid, FName("Transform"));
        if (!Track) {
          Track =
              MovieScene->AddTrack<UMovieScene3DTransformTrack>(BindingGuid);
        }

        if (Track) {
          bool bSectionAdded = false;
          UMovieScene3DTransformSection *Section =
              Cast<UMovieScene3DTransformSection>(
                  Track->FindOrAddSection(0, bSectionAdded));
          if (Section) {
            FFrameRate TickResolution = MovieScene->GetTickResolution();
            FFrameRate DisplayRate = MovieScene->GetDisplayRate();
            FFrameNumber FrameNum = FFrameNumber(static_cast<int32>(Frame));
            FFrameNumber TickFrame =
                FFrameRate::TransformTime(FFrameTime(FrameNum), DisplayRate,
                                          TickResolution)
                    .FloorToFrame();

            bool bModified = false;
            const TSharedPtr<FJsonObject> *ValueObj = nullptr;

            FMovieSceneChannelProxy &Proxy = Section->GetChannelProxy();
            TArrayView<FMovieSceneDoubleChannel *> Channels =
                Proxy.GetChannels<FMovieSceneDoubleChannel>();

            if (LocalPayload->TryGetObjectField(TEXT("value"), ValueObj) &&
                ValueObj && Channels.Num() >= 9) {
              const TSharedPtr<FJsonObject> *LocObj = nullptr;
              if ((*ValueObj)->TryGetObjectField(TEXT("location"), LocObj)) {
                double X, Y, Z;
                if ((*LocObj)->TryGetNumberField(TEXT("x"), X)) {
                  Channels[0]->GetData().AddKey(TickFrame,
                                                FMovieSceneDoubleValue(X));
                  bModified = true;
                }
                if ((*LocObj)->TryGetNumberField(TEXT("y"), Y)) {
                  Channels[1]->GetData().AddKey(TickFrame,
                                                FMovieSceneDoubleValue(Y));
                  bModified = true;
                }
                if ((*LocObj)->TryGetNumberField(TEXT("z"), Z)) {
                  Channels[2]->GetData().AddKey(TickFrame,
                                                FMovieSceneDoubleValue(Z));
                  bModified = true;
                }
              }

              const TSharedPtr<FJsonObject> *RotObj = nullptr;
              if ((*ValueObj)->TryGetObjectField(TEXT("rotation"), RotObj)) {
                double P, Yaw, R;
                // UMovieScene3DTransformSection channel layout (standard UE order):
                // Channels 0-2: Location (X, Y, Z)
                // Channels 3-5: Rotation (Roll, Pitch, Yaw) - corresponds to FRotator order
                // Channels 6-8: Scale (X, Y, Z)
                if ((*RotObj)->TryGetNumberField(TEXT("roll"), R)) {
                  Channels[3]->GetData().AddKey(TickFrame,
                                                FMovieSceneDoubleValue(R));
                  bModified = true;
                }
                if ((*RotObj)->TryGetNumberField(TEXT("pitch"), P)) {
                  Channels[4]->GetData().AddKey(TickFrame,
                                                FMovieSceneDoubleValue(P));
                  bModified = true;
                }
                if ((*RotObj)->TryGetNumberField(TEXT("yaw"), Yaw)) {
                  Channels[5]->GetData().AddKey(TickFrame,
                                                FMovieSceneDoubleValue(Yaw));
                  bModified = true;
                }
              }

              const TSharedPtr<FJsonObject> *ScaleObj = nullptr;
              if ((*ValueObj)->TryGetObjectField(TEXT("scale"), ScaleObj)) {
                double X, Y, Z;
                if ((*ScaleObj)->TryGetNumberField(TEXT("x"), X)) {
                  Channels[6]->GetData().AddKey(TickFrame,
                                                FMovieSceneDoubleValue(X));
                  bModified = true;
                }
                if ((*ScaleObj)->TryGetNumberField(TEXT("y"), Y)) {
                  Channels[7]->GetData().AddKey(TickFrame,
                                                FMovieSceneDoubleValue(Y));
                  bModified = true;
                }
                if ((*ScaleObj)->TryGetNumberField(TEXT("z"), Z)) {
                  Channels[8]->GetData().AddKey(TickFrame,
                                                FMovieSceneDoubleValue(Z));
                  bModified = true;
                }
              }
            }

            if (bModified) {
              MovieScene->Modify();
              SendAutomationResponse(Socket, RequestId, true,
                                     TEXT("Keyframe added"), nullptr,
                                     FString());
              return true;
            }
          }
        }
      } else {
        // Try generic property tracks
        const TSharedPtr<FJsonValue> Val =
            LocalPayload->TryGetField(TEXT("value"));
        if (Val.IsValid() && Val->Type == EJson::Number) {
          UMovieSceneFloatTrack *Track =
              MovieScene->FindTrack<UMovieSceneFloatTrack>(
                  BindingGuid, FName(*PropertyName));
          if (!Track) {
            Track = MovieScene->AddTrack<UMovieSceneFloatTrack>(BindingGuid);
            if (Track)
              Track->SetPropertyNameAndPath(FName(*PropertyName), PropertyName);
          }
          if (Track) {
            bool bSectionAdded = false;
            UMovieSceneFloatSection *Section = Cast<UMovieSceneFloatSection>(
                Track->FindOrAddSection(0, bSectionAdded));
            if (Section) {
              FFrameRate TickResolution = MovieScene->GetTickResolution();
              FFrameRate DisplayRate = MovieScene->GetDisplayRate();
              FFrameNumber FrameNum = FFrameNumber(static_cast<int32>(Frame));
              FFrameNumber TickFrame =
                  FFrameRate::TransformTime(FFrameTime(FrameNum), DisplayRate,
                                            TickResolution)
                      .GetFrame();

              FMovieSceneFloatChannel *Channel =
                  Section->GetChannelProxy()
                      .GetChannel<FMovieSceneFloatChannel>(0);
              if (Channel) {
                Channel->GetData().UpdateOrAddKey(
                    TickFrame, FMovieSceneFloatValue((float)Val->AsNumber()));
                MovieScene->Modify();
                SendAutomationResponse(Socket, RequestId, true,
                                       TEXT("Float Keyframe added"), nullptr);
                return true;
              }
            }
          }
        } else if (Val.IsValid() && Val->Type == EJson::Boolean) {
          UMovieSceneBoolTrack *Track =
              MovieScene->FindTrack<UMovieSceneBoolTrack>(BindingGuid,
                                                          FName(*PropertyName));
          if (!Track) {
            Track = MovieScene->AddTrack<UMovieSceneBoolTrack>(BindingGuid);
            if (Track)
              Track->SetPropertyNameAndPath(FName(*PropertyName), PropertyName);
          }
          if (Track) {
            bool bSectionAdded = false;
            UMovieSceneBoolSection *Section = Cast<UMovieSceneBoolSection>(
                Track->FindOrAddSection(0, bSectionAdded));
            if (Section) {
              FFrameRate TickResolution = MovieScene->GetTickResolution();
              FFrameRate DisplayRate = MovieScene->GetDisplayRate();
              FFrameNumber FrameNum = FFrameNumber(static_cast<int32>(Frame));
              FFrameNumber TickFrame =
                  FFrameRate::TransformTime(FFrameTime(FrameNum), DisplayRate,
                                            TickResolution)
                      .GetFrame();

              FMovieSceneBoolChannel *Channel =
                  Section->GetChannelProxy().GetChannel<FMovieSceneBoolChannel>(
                      0);
              if (Channel) {
                Channel->GetData().UpdateOrAddKey(TickFrame, Val->AsBool());
                MovieScene->Modify();
                SendAutomationResponse(Socket, RequestId, true,
                                       TEXT("Bool Keyframe added"), nullptr);
                return true;
              }
            }
          }
        }
      }

      SendAutomationResponse(
          Socket, RequestId, false,
          TEXT("Unsupported property or failed to create track"), nullptr,
          TEXT("UNSUPPORTED_PROPERTY"));
      return true;
    }
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Sequence object is not a LevelSequence"),
                         nullptr, TEXT("INVALID_SEQUENCE_TYPE"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_add_keyframe requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceAddSection(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SeqPath = ResolveSequencePath(Payload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_add_section requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

  FString TrackName;
  Payload->TryGetStringField(TEXT("trackName"), TrackName);
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  double StartFrame = 0.0, EndFrame = 100.0;
  Payload->TryGetNumberField(TEXT("startFrame"), StartFrame);
  Payload->TryGetNumberField(TEXT("endFrame"), EndFrame);

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (!Sequence || !Sequence->GetMovieScene()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  UMovieScene *MovieScene = Sequence->GetMovieScene();

  // Find the track - either from master tracks or from actor binding
  UMovieSceneTrack *Track = nullptr;

  // First check master tracks
  for (UMovieSceneTrack *MasterTrack : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (MasterTrack &&
        (MasterTrack->GetName().Contains(TrackName) ||
         MasterTrack->GetDisplayName().ToString().Contains(TrackName))) {
      Track = MasterTrack;
      break;
    }
  }

  // If not found in master tracks, check bindings
  // Search all bindings if ActorName is empty, or filter by ActorName if
  // provided
  if (!Track) {
    for (const FMovieSceneBinding &Binding :
         const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
      FString BindingName;
      if (FMovieScenePossessable *Possessable =
              MovieScene->FindPossessable(Binding.GetObjectGuid())) {
        BindingName = Possessable->GetName();
      } else if (FMovieSceneSpawnable *Spawnable =
                     MovieScene->FindSpawnable(Binding.GetObjectGuid())) {
        BindingName = Spawnable->GetName();
      }

      // If ActorName is provided, filter by it; otherwise search all bindings
      if (ActorName.IsEmpty() || BindingName.Contains(ActorName)) {
        for (UMovieSceneTrack *BindingTrack : MCP_GET_BINDING_TRACKS(Binding)) {
          if (BindingTrack &&
              (BindingTrack->GetName().Contains(TrackName) ||
               BindingTrack->GetDisplayName().ToString().Contains(TrackName))) {
            Track = BindingTrack;
            break;
          }
        }
        if (Track)
          break;
      }
    }
  }

  if (!Track) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Track not found"),
                           nullptr, TEXT("TRACK_NOT_FOUND"));
    return true;
  }

  // Create the section
  UMovieSceneSection *NewSection = Track->CreateNewSection();
  if (NewSection) {
    FFrameRate TickResolution = MovieScene->GetTickResolution();
    FFrameNumber Start((int32)FMath::RoundToInt(StartFrame));
    FFrameNumber End((int32)FMath::RoundToInt(EndFrame));
    NewSection->SetRange(TRange<FFrameNumber>(Start, End));
    Track->AddSection(*NewSection);
    MovieScene->Modify();

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("trackName"), Track->GetName());
    Resp->SetNumberField(TEXT("startFrame"), StartFrame);
    Resp->SetNumberField(TEXT("endFrame"), EndFrame);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Section added to track"), Resp);
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create section"), nullptr,
                           TEXT("SECTION_CREATION_FAILED"));
  }
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_add_section requires editor build"),
                         nullptr, TEXT("EDITOR_ONLY"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceSetTickResolution(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ResolutionStr;
  Payload->TryGetStringField(TEXT("resolution"), ResolutionStr);

  FString SeqPath = ResolveSequencePath(Payload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("path required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (Sequence && Sequence->GetMovieScene()) {
    // Get current tick resolution as fallback
    FFrameRate TickResolution = Sequence->GetMovieScene()->GetTickResolution();

    // Parse user-provided resolution string
    if (!ResolutionStr.IsEmpty()) {
      if (ResolutionStr.Contains(TEXT("24000")))
        TickResolution = FFrameRate(24000, 1);
      else if (ResolutionStr.Contains(TEXT("60000")))
        TickResolution = FFrameRate(60000, 1);
      else if (ResolutionStr.Contains(TEXT("/"))) {
        // Parse rational format "numerator/denominator"
        FString NumStr, DenomStr;
        if (ResolutionStr.Split(TEXT("/"), &NumStr, &DenomStr)) {
          int32 Num = FCString::Atoi(*NumStr);
          int32 Denom = FCString::Atoi(*DenomStr);
          if (Num > 0 && Denom > 0) {
            TickResolution = FFrameRate(Num, Denom);
          }
        }
      }
      else if (ResolutionStr.IsNumeric()) {
        // Parse simple integer format
        int32 Num = FCString::Atoi(*ResolutionStr);
        if (Num > 0) {
          TickResolution = FFrameRate(Num, 1);
        }
      }
      else {
        // Unrecognized format - log a warning but continue with current resolution
        UE_LOG(LogNebulaForgeBridgeSubsystem, Warning,
               TEXT("HandleSequenceSetTickResolution: Unrecognized resolution format '%s'. Using current resolution."),
               *ResolutionStr);
      }
    }

    Sequence->GetMovieScene()->SetTickResolutionDirectly(TickResolution);
    Sequence->GetMovieScene()->Modify();
    SendAutomationResponse(Socket, RequestId, true, TEXT("Tick resolution set"),
                           nullptr);
  } else {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("NOT_FOUND"));
  }
  return true;
#else
  return false;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceSetViewRange(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  double Start = 0;
  double End = 10;
  Payload->TryGetNumberField(TEXT("start"), Start);
  Payload->TryGetNumberField(TEXT("end"), End);
  FString SeqPath = ResolveSequencePath(Payload);

  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("path required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (Sequence && Sequence->GetMovieScene()) {
    Sequence->GetMovieScene()->SetViewRange(Start, End);
    Sequence->GetMovieScene()->Modify();
    SendAutomationResponse(Socket, RequestId, true, TEXT("View range set"),
                           nullptr);
  } else {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("NOT_FOUND"));
  }
  return true;
#else
  return false;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceSetTrackMuted(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SeqPath = ResolveSequencePath(Payload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence path required"), nullptr,
                           TEXT("INVALID_SEQUENCE"));
    return true;
  }

  FString TrackName;
  Payload->TryGetStringField(TEXT("trackName"), TrackName);
  bool bMuted = true;
  Payload->TryGetBoolField(TEXT("muted"), bMuted);

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (!Sequence || !Sequence->GetMovieScene()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  UMovieScene *MovieScene = Sequence->GetMovieScene();
  UMovieSceneTrack *Track = nullptr;

  // Search master tracks and binding tracks
  for (UMovieSceneTrack *MasterTrack : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (MasterTrack && MasterTrack->GetName().Contains(TrackName)) {
      Track = MasterTrack;
      break;
    }
  }

  if (!Track) {
    for (const FMovieSceneBinding &Binding :
         const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
      for (UMovieSceneTrack *BindingTrack : MCP_GET_BINDING_TRACKS(Binding)) {
        if (BindingTrack && BindingTrack->GetName().Contains(TrackName)) {
          Track = BindingTrack;
          break;
        }
      }
      if (Track)
        break;
    }
  }

  if (!Track) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Track not found"),
                           nullptr, TEXT("TRACK_NOT_FOUND"));
    return true;
  }

  // Set muted state via EvalOptions
  Track->SetEvalDisabled(bMuted);
  MovieScene->Modify();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("trackName"), Track->GetName());
  Resp->SetBoolField(TEXT("muted"), bMuted);
  SendAutomationResponse(Socket, RequestId, true,
                         bMuted ? TEXT("Track muted") : TEXT("Track unmuted"),
                         Resp);
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_set_track_muted requires editor build"),
                         nullptr, TEXT("EDITOR_ONLY"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceSetTrackSolo(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  // Note: UE doesn't have a direct "solo" property on tracks, but we can
  // simulate it by muting all other tracks
  FString SeqPath = ResolveSequencePath(Payload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence path required"), nullptr,
                           TEXT("INVALID_SEQUENCE"));
    return true;
  }

  FString TrackName;
  Payload->TryGetStringField(TEXT("trackName"), TrackName);
  bool bSolo = true;
  Payload->TryGetBoolField(TEXT("solo"), bSolo);

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (!Sequence || !Sequence->GetMovieScene()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  UMovieScene *MovieScene = Sequence->GetMovieScene();
  UMovieSceneTrack *SoloTrack = nullptr;

  // Find the track to solo
  TArray<UMovieSceneTrack *> AllTracks;
  for (UMovieSceneTrack *Track : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (Track) {
      AllTracks.Add(Track);
      if (Track->GetName().Contains(TrackName)) {
        SoloTrack = Track;
      }
    }
  }

  for (const FMovieSceneBinding &Binding :
       const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
    for (UMovieSceneTrack *Track : MCP_GET_BINDING_TRACKS(Binding)) {
      if (Track) {
        AllTracks.Add(Track);
        if (Track->GetName().Contains(TrackName)) {
          SoloTrack = Track;
        }
      }
    }
  }

  if (!SoloTrack) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Track not found"),
                           nullptr, TEXT("TRACK_NOT_FOUND"));
    return true;
  }

  int32 AffectedTrackCount = 0;
  int32 DisabledOtherTrackCount = 0;

  // If enabling solo, disable evaluation on every other track; if disabling, re-enable all tracks.
  for (UMovieSceneTrack *Track : AllTracks) {
    if (!Track) {
      continue;
    }
    if (bSolo) {
      const bool bDisableTrack = Track != SoloTrack;
      Track->SetEvalDisabled(bDisableTrack);
      if (bDisableTrack) {
        ++DisabledOtherTrackCount;
      }
    } else {
      Track->SetEvalDisabled(false);
    }
    ++AffectedTrackCount;
  }
  MovieScene->Modify();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("trackName"), SoloTrack->GetName());
  Resp->SetBoolField(TEXT("solo"), bSolo);
  Resp->SetNumberField(TEXT("affectedTrackCount"), AffectedTrackCount);
  Resp->SetNumberField(TEXT("disabledOtherTrackCount"), DisabledOtherTrackCount);
  SendAutomationResponse(
      Socket, RequestId, true,
      bSolo ? TEXT("Track solo enabled by disabling evaluation on other tracks") : TEXT("Solo disabled; all tracks evaluation-enabled"), Resp);
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_set_track_solo requires editor build"),
                         nullptr, TEXT("EDITOR_ONLY"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceSetTrackLocked(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SeqPath = ResolveSequencePath(Payload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence path required"), nullptr,
                           TEXT("INVALID_SEQUENCE"));
    return true;
  }

  FString TrackName;
  Payload->TryGetStringField(TEXT("trackName"), TrackName);
  bool bLocked = true;
  Payload->TryGetBoolField(TEXT("locked"), bLocked);

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (!Sequence || !Sequence->GetMovieScene()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  UMovieScene *MovieScene = Sequence->GetMovieScene();
  UMovieSceneTrack *Track = nullptr;

  // Search for track
  for (UMovieSceneTrack *MasterTrack : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (MasterTrack && MasterTrack->GetName().Contains(TrackName)) {
      Track = MasterTrack;
      break;
    }
  }

  if (!Track) {
    for (const FMovieSceneBinding &Binding :
         const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
      for (UMovieSceneTrack *BindingTrack : MCP_GET_BINDING_TRACKS(Binding)) {
        if (BindingTrack && BindingTrack->GetName().Contains(TrackName)) {
          Track = BindingTrack;
          break;
        }
      }
      if (Track)
        break;
    }
  }

  if (!Track) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Track not found"),
                           nullptr, TEXT("TRACK_NOT_FOUND"));
    return true;
  }

  // Lock all sections in the track
  for (UMovieSceneSection *Section : Track->GetAllSections()) {
    if (Section) {
      Section->SetIsLocked(bLocked);
    }
  }
  MovieScene->Modify();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("trackName"), Track->GetName());
  Resp->SetBoolField(TEXT("locked"), bLocked);
  SendAutomationResponse(
      Socket, RequestId, true,
      bLocked ? TEXT("Track locked") : TEXT("Track unlocked"), Resp);
  return true;
#else
  SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("sequence_set_track_locked requires editor build"), nullptr,
      TEXT("EDITOR_ONLY"));
  return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleSequenceRemoveTrack(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SeqPath = ResolveSequencePath(Payload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence path required"), nullptr,
                           TEXT("INVALID_SEQUENCE"));
    return true;
  }

  FString TrackName;
  Payload->TryGetStringField(TEXT("trackName"), TrackName);

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (!Sequence || !Sequence->GetMovieScene()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  UMovieScene *MovieScene = Sequence->GetMovieScene();
  bool bRemoved = false;
  FString RemovedTrackName;

  // Try to remove from master tracks first
  for (UMovieSceneTrack *Track : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (Track && Track->GetName().Contains(TrackName)) {
      RemovedTrackName = Track->GetName();
      MovieScene->RemoveTrack(*Track);
      bRemoved = true;
      break;
    }
  }

  // If not found, try binding tracks
  if (!bRemoved) {
    for (const FMovieSceneBinding &Binding :
         const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
      for (UMovieSceneTrack *Track : MCP_GET_BINDING_TRACKS(Binding)) {
        if (Track && Track->GetName().Contains(TrackName)) {
          RemovedTrackName = Track->GetName();
          MovieScene->RemoveTrack(*Track);
          bRemoved = true;
          break;
        }
      }
      if (bRemoved)
        break;
    }
  }

  if (bRemoved) {
    MovieScene->Modify();
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("trackName"), RemovedTrackName);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Track removed"),
                           Resp);
  } else {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Track not found"),
                           nullptr, TEXT("TRACK_NOT_FOUND"));
  }
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_remove_track requires editor build"),
                         nullptr, TEXT("EDITOR_ONLY"));
  return true;
#endif
}

#if WITH_EDITOR && MCP_HAS_MRQ
static bool HandleMovieRenderQueueAction(
    UNebulaForgeBridgeSubsystem* Owner,
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  UMoviePipelineQueueEngineSubsystem *Subsystem =
      GEngine ? GEngine->GetEngineSubsystem<UMoviePipelineQueueEngineSubsystem>() : nullptr;
  if (!Subsystem) {
    Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Movie Render Queue is unavailable"), nullptr,
                           TEXT("MRQ_NOT_AVAILABLE"));
    return true;
  }

  if (Action == TEXT("get_mrq_status")) {
    FString RequestedJobId;
    if (Payload.IsValid()) Payload->TryGetStringField(TEXT("mrqJobId"), RequestedJobId);
    if (RequestedJobId.IsEmpty() && Payload.IsValid()) Payload->TryGetStringField(TEXT("jobId"), RequestedJobId);
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("isRendering"), Subsystem->IsRendering());
    Response->SetStringField(TEXT("status"),
                             Subsystem->IsRendering() ? TEXT("rendering") : TEXT("idle"));
    if (UMoviePipelineExecutorBase *Executor = Subsystem->GetActiveExecutor()) {
      Response->SetStringField(TEXT("executorClass"), Executor->GetClass()->GetPathName());
    }
    const TSharedPtr<FJsonObject> TrackedStatus = Owner->GetMovieRenderQueueTrackedStatus(RequestedJobId);
    if (TrackedStatus.IsValid()) {
      for (const TPair<FString, TSharedPtr<FJsonValue>> &Field : TrackedStatus->Values) {
        if (Field.Value.IsValid()) Response->SetField(Field.Key, Field.Value);
      }
      bool bKnown = true;
      TrackedStatus->TryGetBoolField(TEXT("known"), bKnown);
      if (!bKnown && !RequestedJobId.IsEmpty()) {
        Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
            TEXT("Movie Render Queue job was not found"), Response,
            TEXT("MRQ_JOB_NOT_FOUND"));
        return true;
      }
    }
    Owner->SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Movie Render Queue status"), Response);
    return true;
  }

  if (Action == TEXT("cancel_mrq")) {
    if (UMoviePipelineExecutorBase *Executor = Subsystem->GetActiveExecutor()) {
      Executor->CancelAllJobs();
      Owner->SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Movie Render Queue cancellation requested"), nullptr);
    } else {
      Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No active Movie Render Queue render"), nullptr,
                             TEXT("MRQ_NOT_RENDERING"));
    }
    return true;
  }

  FString SequencePath;
  if (!Payload.IsValid() || !Payload->TryGetStringField(TEXT("path"), SequencePath) ||
      SequencePath.IsEmpty()) {
    Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("render_sequence_mrq requires a sequence path"), nullptr,
                           TEXT("INVALID_SEQUENCE"));
    return true;
  }
  FString OutputPath;
  if (!Payload->TryGetStringField(TEXT("outputPath"), OutputPath) || OutputPath.IsEmpty() ||
      OutputPath.Contains(TEXT("..")) || OutputPath.StartsWith(TEXT("/")) ||
      OutputPath.Contains(TEXT(":"))) {
    Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("outputPath must be a project-relative directory"), nullptr,
                           TEXT("INVALID_OUTPUT_PATH"));
    return true;
  }
  if (Subsystem->IsRendering()) {
    Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Movie Render Queue is already rendering"), nullptr,
                           TEXT("MRQ_BUSY"));
    return true;
  }

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
  if (!Sequence) {
    Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Level sequence not found"), nullptr,
                           TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  UMoviePipelineExecutorJob *Job = Subsystem->AllocateJob(Sequence);
  if (!Job || !Job->GetConfiguration()) {
    Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Unable to allocate Movie Render Queue job"), nullptr,
                           TEXT("MRQ_JOB_ALLOCATION_FAILED"));
    return true;
  }

  FString MrqPresetPath;
  if (Payload->TryGetStringField(TEXT("mrqPresetPath"), MrqPresetPath) && !MrqPresetPath.IsEmpty()) {
    if (!IsValidAssetPath(MrqPresetPath)) {
      Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
          TEXT("mrqPresetPath must be a valid Unreal asset path"), nullptr,
          TEXT("INVALID_MRQ_PRESET_PATH"));
      return true;
    }
    UMoviePipelinePrimaryConfig *Preset =
        Cast<UMoviePipelinePrimaryConfig>(UEditorAssetLibrary::LoadAsset(MrqPresetPath));
    if (!Preset) {
      Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
          TEXT("Movie Render Queue preset was not found or is not a UMoviePipelinePrimaryConfig"), nullptr,
          TEXT("MRQ_PRESET_INVALID"));
      return true;
    }
    // Copy into the job-owned transient configuration. The preset asset remains
    // immutable and is never passed directly to the active renderer.
    Job->GetConfiguration()->CopyFrom(Preset);
    Job->SetPresetOrigin(Preset);
  }
  UMoviePipelineOutputSetting *OutputSetting =
      Cast<UMoviePipelineOutputSetting>(Job->GetConfiguration()->FindOrAddSettingByClass(
          UMoviePipelineOutputSetting::StaticClass()));
  if (!OutputSetting) {
    Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Unable to configure Movie Render Queue output"), nullptr,
                           TEXT("MRQ_OUTPUT_CONFIGURATION_FAILED"));
    return true;
  }
  OutputSetting->OutputDirectory.Path = FPaths::Combine(FPaths::ProjectDir(), OutputPath);

  // Apply optional deterministic render settings.  Keep these values on the
  // transient MRQ job so the source sequence is never modified as a side
  // effect of rendering.
  FString ResolutionText;
  int32 OutputWidth = 0;
  int32 OutputHeight = 0;
  if (Payload->TryGetStringField(TEXT("resolution"), ResolutionText) && !ResolutionText.IsEmpty()) {
    ResolutionText.ReplaceInline(TEXT("X"), TEXT("x"));
    ResolutionText.ReplaceInline(TEXT("*"), TEXT("x"));
    TArray<FString> ResolutionParts;
    ResolutionText.ParseIntoArray(ResolutionParts, TEXT("x"), true);
    if (ResolutionParts.Num() == 2) {
      OutputWidth = FCString::Atoi(*ResolutionParts[0]);
      OutputHeight = FCString::Atoi(*ResolutionParts[1]);
    }
    if (OutputWidth < 1 || OutputHeight < 1 || OutputWidth > 16384 || OutputHeight > 16384) {
      Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
          TEXT("resolution must be WIDTHxHEIGHT with values between 1 and 16384"), nullptr,
          TEXT("INVALID_RESOLUTION"));
      return true;
    }
    OutputSetting->OutputResolution = FIntPoint(OutputWidth, OutputHeight);
  }

  bool bHasStartFrame = false;
  bool bHasEndFrame = false;
  double StartFrameValue = 0.0;
  double EndFrameValue = 0.0;
  bHasStartFrame = Payload->TryGetNumberField(TEXT("startFrame"), StartFrameValue);
  bHasEndFrame = Payload->TryGetNumberField(TEXT("endFrame"), EndFrameValue);
  if (bHasStartFrame != bHasEndFrame ||
      (bHasStartFrame && (!FMath::IsFinite(StartFrameValue) || !FMath::IsFinite(EndFrameValue) ||
                          StartFrameValue < -1000000.0 || EndFrameValue > 1000000.0 ||
                          StartFrameValue > EndFrameValue))) {
    Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("startFrame and endFrame must be supplied together with startFrame <= endFrame and values between -1000000 and 1000000"), nullptr,
        TEXT("INVALID_FRAME_RANGE"));
    return true;
  }
  if (bHasStartFrame) {
    OutputSetting->bUseCustomPlaybackRange = true;
    OutputSetting->CustomStartFrame = FMath::RoundToInt(StartFrameValue);
    OutputSetting->CustomEndFrame = FMath::RoundToInt(EndFrameValue);
  }

  double SpatialSampleValue = 0.0;
  double TemporalSampleValue = 0.0;
  const bool bHasSpatialSamples = Payload->TryGetNumberField(TEXT("spatialSampleCount"), SpatialSampleValue);
  const bool bHasTemporalSamples = Payload->TryGetNumberField(TEXT("temporalSampleCount"), TemporalSampleValue);
  if (bHasSpatialSamples || bHasTemporalSamples) {
#if MCP_HAS_MRQ_AA
    if ((bHasSpatialSamples && (!FMath::IsFinite(SpatialSampleValue) || SpatialSampleValue < 1.0 || SpatialSampleValue > 256.0 || SpatialSampleValue != FMath::RoundToDouble(SpatialSampleValue))) ||
        (bHasTemporalSamples && (!FMath::IsFinite(TemporalSampleValue) || TemporalSampleValue < 1.0 || TemporalSampleValue > 256.0 || TemporalSampleValue != FMath::RoundToDouble(TemporalSampleValue)))) {
      Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
          TEXT("spatialSampleCount and temporalSampleCount must be integers between 1 and 256"), nullptr,
          TEXT("INVALID_ANTI_ALIASING"));
      return true;
    }
    UMoviePipelineAntiAliasingSetting *AntiAliasing =
        Cast<UMoviePipelineAntiAliasingSetting>(Job->GetConfiguration()->FindOrAddSettingByClass(
            UMoviePipelineAntiAliasingSetting::StaticClass()));
    if (!AntiAliasing) {
      Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
          TEXT("Unable to configure Movie Render Queue anti-aliasing"), nullptr,
          TEXT("MRQ_AA_CONFIGURATION_FAILED"));
      return true;
    }
    if (bHasSpatialSamples) AntiAliasing->SpatialSampleCount = FMath::RoundToInt(SpatialSampleValue);
    if (bHasTemporalSamples) AntiAliasing->TemporalSampleCount = FMath::RoundToInt(TemporalSampleValue);
#else
    Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("Movie Render Queue anti-aliasing is unavailable in this build"), nullptr,
        TEXT("MRQ_AA_NOT_AVAILABLE"));
    return true;
#endif
  }
  const bool bHasBurnInClass = Payload->HasField(TEXT("burnInClass"));
  const bool bHasCompositeBurnIn = Payload->HasField(TEXT("compositeBurnIn"));
  if (bHasBurnInClass || bHasCompositeBurnIn) {
#if MCP_HAS_MRQ_BURN_IN
    UMoviePipelineBurnInSetting *BurnIn =
        Cast<UMoviePipelineBurnInSetting>(Job->GetConfiguration()->FindOrAddSettingByClass(
            UMoviePipelineBurnInSetting::StaticClass()));
    if (!BurnIn) {
      Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
          TEXT("Unable to configure Movie Render Queue burn-in"), nullptr,
          TEXT("MRQ_BURN_IN_CONFIGURATION_FAILED"));
      return true;
    }
    if (bHasBurnInClass) {
      FString BurnInClassPath;
      Payload->TryGetStringField(TEXT("burnInClass"), BurnInClassPath);
      BurnInClassPath.TrimStartAndEndInline();
      if (BurnInClassPath.IsEmpty() || BurnInClassPath.Len() > 512) {
        Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
            TEXT("burnInClass must be a non-empty class path of at most 512 characters"), nullptr,
            TEXT("INVALID_BURN_IN_CLASS"));
        return true;
      }
      UClass *BurnInWidgetClass = LoadClass<UMoviePipelineBurnInWidget>(nullptr, *BurnInClassPath);
      if (!BurnInWidgetClass) {
        Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
            TEXT("burnInClass must resolve to a UMoviePipelineBurnInWidget class"), nullptr,
            TEXT("BURN_IN_CLASS_NOT_FOUND"));
        return true;
      }
      BurnIn->BurnInClass = FSoftClassPath(BurnInClassPath);
    }
    if (bHasCompositeBurnIn) {
      bool bComposite = false;
      Payload->TryGetBoolField(TEXT("compositeBurnIn"), bComposite);
      BurnIn->bCompositeOntoFinalImage = bComposite;
    }
#else
    Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("Movie Render Queue burn-in is unavailable in this build"), nullptr,
        TEXT("MRQ_BURN_IN_NOT_AVAILABLE"));
    return true;
#endif
  }
  FString OutputFormat = TEXT("png");
  Payload->TryGetStringField(TEXT("outputFormat"), OutputFormat);
  OutputFormat.TrimStartAndEndInline();
  OutputFormat.ToLowerInline();
  UClass* ImageOutputClass = UMoviePipelineImageSequenceOutput_PNG::StaticClass();
  if (OutputFormat == TEXT("jpg") || OutputFormat == TEXT("jpeg")) {
    ImageOutputClass = UMoviePipelineImageSequenceOutput_JPG::StaticClass();
    OutputFormat = TEXT("jpg");
  } else if (OutputFormat == TEXT("bmp")) {
    ImageOutputClass = UMoviePipelineImageSequenceOutput_BMP::StaticClass();
  } else if (OutputFormat == TEXT("exr")) {
#if MCP_HAS_MRQ_EXR
    ImageOutputClass = UMoviePipelineImageSequenceOutput_EXR::StaticClass();
#else
    Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("EXR output is unavailable because the MRQ render-passes module is not present"), nullptr,
        TEXT("MRQ_FORMAT_NOT_AVAILABLE"));
    return true;
#endif
  } else if (OutputFormat != TEXT("png")) {
    Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("outputFormat must be one of png, jpg, jpeg, bmp, or exr"), nullptr,
        TEXT("INVALID_OUTPUT_FORMAT"));
    return true;
  }
  FString RenderPass = TEXT("beauty");
  Payload->TryGetStringField(TEXT("renderPass"), RenderPass);
  RenderPass.TrimStartAndEndInline();
  RenderPass.ToLowerInline();
  if (RenderPass == TEXT("object_id")) {
#if MCP_HAS_MRQ_OBJECT_ID
    if (OutputFormat != TEXT("exr")) {
      Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
          TEXT("object_id render pass requires EXR output"), nullptr,
          TEXT("OBJECT_ID_REQUIRES_EXR"));
      return true;
    }
    Job->GetConfiguration()->FindOrAddSettingByClass(UMoviePipelineObjectIdRenderPass::StaticClass());
#else
    Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("object_id render pass requires the Movie Render Queue Additional Render Passes plugin"), nullptr,
        TEXT("OBJECT_ID_NOT_AVAILABLE"));
    return true;
#endif
  } else if (RenderPass != TEXT("beauty")) {
    Owner->SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("renderPass must be beauty or object_id; depth, normal, and motion-vector outputs require project-specific deferred-pass materials"), nullptr,
        TEXT("UNSUPPORTED_RENDER_PASS"));
    return true;
  }
  Job->GetConfiguration()->FindOrAddSettingByClass(ImageOutputClass);
  Subsystem->RenderJob(Job);
  Owner->BeginMovieRenderQueueTracking(Job->GetName(), Subsystem->GetActiveExecutor());

  TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
  Response->SetStringField(TEXT("jobId"), Job->GetName());
  Response->SetStringField(TEXT("mrqJobId"), Job->GetName());
  Response->SetStringField(TEXT("sequencePath"), SequencePath);
  Response->SetStringField(TEXT("outputPath"), OutputPath);
  if (!MrqPresetPath.IsEmpty()) Response->SetStringField(TEXT("mrqPresetPath"), MrqPresetPath);
  Response->SetStringField(TEXT("outputFormat"), OutputFormat);
  Response->SetBoolField(TEXT("customResolution"), OutputWidth > 0 && OutputHeight > 0);
  if (OutputWidth > 0 && OutputHeight > 0) {
    Response->SetNumberField(TEXT("outputWidth"), OutputWidth);
    Response->SetNumberField(TEXT("outputHeight"), OutputHeight);
  }
  Response->SetBoolField(TEXT("customFrameRange"), bHasStartFrame);
  if (bHasStartFrame) {
    Response->SetNumberField(TEXT("startFrame"), OutputSetting->CustomStartFrame);
    Response->SetNumberField(TEXT("endFrame"), OutputSetting->CustomEndFrame);
  }
  Response->SetStringField(TEXT("status"), TEXT("queued"));
  Owner->SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Movie Render Queue job submitted"), Response);
  return true;
}
#endif

bool UNebulaForgeBridgeSubsystem::HandleSequenceAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  // Also handle manage_sequence which acts as a dispatcher for sub-actions
  if (!Lower.StartsWith(TEXT("sequence_")) &&
      !Lower.Equals(TEXT("manage_sequence")) &&
      !Lower.Equals(TEXT("render_sequence_queue")))
    return false;

  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  if (Lower.Equals(TEXT("render_sequence_queue"))) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Render queue orchestration is owned by the TypeScript MCP host"),
                        TEXT("HOST_ONLY"));
    return true;
  }
  if (Lower.Equals(TEXT("manage_sequence"))) {
    FString RequestedSubAction;
    LocalPayload->TryGetStringField(TEXT("subAction"), RequestedSubAction);
    if (RequestedSubAction.IsEmpty()) LocalPayload->TryGetStringField(TEXT("action"), RequestedSubAction);
    if (RequestedSubAction.Equals(TEXT("render_sequence_queue"), ESearchCase::IgnoreCase)) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Render queue orchestration is owned by the TypeScript MCP host"),
                          TEXT("HOST_ONLY"));
      return true;
    }
  }
  FString EffectiveAction = Lower;

  // If generic manage_sequence, extract the sub-action to determine behavior
  if (Lower.Equals(TEXT("manage_sequence"))) {
    FString Sub;
    if (LocalPayload->TryGetStringField(TEXT("subAction"), Sub) &&
        !Sub.IsEmpty()) {
      EffectiveAction = Sub.ToLower();
      // If subAction is just "create", map to "sequence_create" for consistency
      if (EffectiveAction == TEXT("create") ||
          EffectiveAction == TEXT("create_master_sequence"))
        EffectiveAction = TEXT("sequence_create");
      else if (EffectiveAction == TEXT("create_cine_camera_actor"))
        EffectiveAction = TEXT("sequence_add_camera");
      else if (EffectiveAction == TEXT("add_shot_track") ||
               EffectiveAction == TEXT("add_audio_track") ||
               EffectiveAction == TEXT("add_material_parameter_track") ||
               EffectiveAction == TEXT("add_material_color_track") ||
               EffectiveAction == TEXT("add_custom_primitive_data_track") ||
               EffectiveAction == TEXT("add_niagara_system_track") ||
               EffectiveAction == TEXT("create_niagara_float_parameter_track") ||
               EffectiveAction == TEXT("add_camera_cut_track") ||
               EffectiveAction == TEXT("add_camera_shake_track") ||
               EffectiveAction == TEXT("add_fade_track") ||
               EffectiveAction == TEXT("add_level_visibility_track") ||
               EffectiveAction == TEXT("add_skeletal_animation_track") ||
               EffectiveAction == TEXT("add_transform_track") ||
               EffectiveAction == TEXT("add_event_track") ||
               EffectiveAction == TEXT("add_property_track"))
      {
        FString TrackType;
        if (EffectiveAction == TEXT("add_shot_track")) TrackType = TEXT("CinematicShot");
        else if (EffectiveAction == TEXT("add_audio_track")) TrackType = TEXT("Audio");
        else if (EffectiveAction == TEXT("add_material_parameter_track")) TrackType = TEXT("Material");
        else if (EffectiveAction == TEXT("add_material_color_track")) TrackType = TEXT("Material");
        else if (EffectiveAction == TEXT("add_custom_primitive_data_track")) TrackType = TEXT("CustomPrimitiveData");
        else if (EffectiveAction == TEXT("add_niagara_system_track")) TrackType = TEXT("NiagaraSystem");
        else if (EffectiveAction == TEXT("create_niagara_float_parameter_track")) TrackType = TEXT("NiagaraFloatParameter");
        else if (EffectiveAction == TEXT("add_camera_cut_track")) TrackType = TEXT("CameraCut");
        else if (EffectiveAction == TEXT("add_camera_shake_track")) TrackType = TEXT("CameraShake");
        else if (EffectiveAction == TEXT("add_fade_track")) TrackType = TEXT("Fade");
        else if (EffectiveAction == TEXT("add_level_visibility_track")) TrackType = TEXT("LevelVisibility");
        else if (EffectiveAction == TEXT("add_skeletal_animation_track")) TrackType = TEXT("SkeletalAnimation");
        else if (EffectiveAction == TEXT("add_transform_track")) TrackType = TEXT("3DTransform");
        else if (EffectiveAction == TEXT("add_event_track")) TrackType = TEXT("Event");
        else TrackType = TEXT("Property");
        LocalPayload->SetStringField(TEXT("trackType"), TrackType);
        if (EffectiveAction == TEXT("add_camera_shake_track"))
          EffectiveAction = TEXT("sequence_add_camera_shake_track");
        else if (EffectiveAction == TEXT("add_audio_track"))
          EffectiveAction = TEXT("sequence_add_audio_track");
        else if (EffectiveAction == TEXT("add_material_parameter_track"))
          EffectiveAction = TEXT("sequence_add_material_parameter_track");
        else if (EffectiveAction == TEXT("add_material_color_track"))
          EffectiveAction = TEXT("sequence_add_material_color_track");
        else if (EffectiveAction == TEXT("add_custom_primitive_data_track"))
          EffectiveAction = TEXT("sequence_add_custom_primitive_data_track");
        else if (EffectiveAction == TEXT("add_niagara_system_track"))
          EffectiveAction = TEXT("sequence_add_niagara_system_track");
        else if (EffectiveAction == TEXT("create_niagara_float_parameter_track"))
          EffectiveAction = TEXT("sequence_create_niagara_float_parameter_track");
        else if (EffectiveAction == TEXT("add_niagara_float_parameter_key"))
          EffectiveAction = TEXT("sequence_add_niagara_float_parameter_key");
        else if (EffectiveAction == TEXT("inspect_niagara_parameter_track"))
          EffectiveAction = TEXT("sequence_inspect_niagara_parameter_track");
        else
          EffectiveAction = TEXT("sequence_add_track");
      }
      else if (!EffectiveAction.StartsWith(TEXT("sequence_")))
        EffectiveAction = TEXT("sequence_") + EffectiveAction;
    }
  }

  if (EffectiveAction == TEXT("render_sequence_mrq") ||
      EffectiveAction == TEXT("get_mrq_status") ||
      EffectiveAction == TEXT("cancel_mrq")) {
#if WITH_EDITOR && MCP_HAS_MRQ
    return HandleMovieRenderQueueAction(this, RequestId, EffectiveAction, LocalPayload,
                                        RequestingSocket);
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Movie Render Queue is unavailable in this build"), nullptr,
                           TEXT("MRQ_NOT_AVAILABLE"));
    return true;
#endif
  }

  if (EffectiveAction == TEXT("sequence_create"))
    return HandleSequenceCreate(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_display_rate"))
    return HandleSequenceSetDisplayRate(RequestId, LocalPayload,
                                        RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_properties"))
    return HandleSequenceSetProperties(RequestId, LocalPayload,
                                       RequestingSocket);
  if (EffectiveAction == TEXT("sequence_open"))
    return HandleSequenceOpen(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_add_camera"))
    return HandleSequenceAddCamera(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_play"))
    return HandleSequencePlay(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_add_actor"))
    return HandleSequenceAddActor(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_add_actors"))
    return HandleSequenceAddActors(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_add_spawnable_from_class"))
    return HandleSequenceAddSpawnable(RequestId, LocalPayload,
                                      RequestingSocket);
  if (EffectiveAction == TEXT("sequence_remove_actors"))
    return HandleSequenceRemoveActors(RequestId, LocalPayload,
                                      RequestingSocket);
  if (EffectiveAction == TEXT("sequence_get_bindings"))
    return HandleSequenceGetBindings(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_get_properties"))
    return HandleSequenceGetProperties(RequestId, LocalPayload,
                                       RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_playback_speed"))
    return HandleSequenceSetPlaybackSpeed(RequestId, LocalPayload,
                                          RequestingSocket);
  if (EffectiveAction == TEXT("sequence_pause"))
    return HandleSequencePause(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_stop"))
    return HandleSequenceStop(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_list"))
    return HandleSequenceList(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_duplicate"))
    return HandleSequenceDuplicate(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_rename"))
    return HandleSequenceRename(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_delete"))
    return HandleSequenceDelete(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_get_metadata"))
    return HandleSequenceGetMetadata(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_add_keyframe"))
    return HandleSequenceAddKeyframe(RequestId, LocalPayload, RequestingSocket);

  // New handlers
  if (EffectiveAction == TEXT("sequence_add_section"))
    return HandleSequenceAddSection(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_tick_resolution"))
    return HandleSequenceSetTickResolution(RequestId, LocalPayload,
                                           RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_view_range"))
    return HandleSequenceSetViewRange(RequestId, LocalPayload,
                                      RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_track_muted"))
    return HandleSequenceSetTrackMuted(RequestId, LocalPayload,
                                       RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_track_solo"))
    return HandleSequenceSetTrackSolo(RequestId, LocalPayload,
                                      RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_track_locked"))
    return HandleSequenceSetTrackLocked(RequestId, LocalPayload,
                                        RequestingSocket);
  if (EffectiveAction == TEXT("sequence_remove_track"))
    return HandleSequenceRemoveTrack(RequestId, LocalPayload, RequestingSocket);

  if (EffectiveAction == TEXT("sequence_list_track_types")) {
    // Discovery: list available track types
    TArray<TSharedPtr<FJsonValue>> Types;
    // Add common shortcuts first
    Types.Add(MakeShared<FJsonValueString>(TEXT("transform")));
    Types.Add(MakeShared<FJsonValueString>(TEXT("3dtransform")));
    Types.Add(MakeShared<FJsonValueString>(TEXT("audio")));
    Types.Add(MakeShared<FJsonValueString>(TEXT("event")));

    // Discover all UMovieSceneTrack subclasses via reflection
    TSet<FString> AddedNames;
    AddedNames.Add(TEXT("transform"));
    AddedNames.Add(TEXT("3dtransform"));
    AddedNames.Add(TEXT("audio"));
    AddedNames.Add(TEXT("event"));

    for (TObjectIterator<UClass> It; It; ++It) {
      if (It->IsChildOf(UMovieSceneTrack::StaticClass()) &&
          !It->HasAnyClassFlags(CLASS_Abstract) &&
          !AddedNames.Contains(It->GetName())) {
        Types.Add(MakeShared<FJsonValueString>(It->GetName()));
        AddedNames.Add(It->GetName());
      }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetArrayField(TEXT("types"), Types);
    Resp->SetNumberField(TEXT("count"), Types.Num());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Available track types"), Resp);
    return true;
  }

  if (EffectiveAction == TEXT("sequence_add_track")) {
    // add_track action: Add a track to a binding in a level sequence
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("sequence_add_track requires a sequence path"), nullptr,
          TEXT("INVALID_SEQUENCE"));
      return true;
    }

    ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
    if (!Sequence) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Level sequence not found"), nullptr,
                             TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }

    UMovieScene *MovieScene = Sequence->GetMovieScene();
    if (!MovieScene) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("MovieScene not available"), nullptr,
                             TEXT("MOVIESCENE_UNAVAILABLE"));
      return true;
    }

    FString TrackType;
    LocalPayload->TryGetStringField(TEXT("trackType"), TrackType);
    if (TrackType.IsEmpty()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("trackType required (e.g., Transform, Animation, Audio, Event)"),
          nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString TrackName;
    LocalPayload->TryGetStringField(TEXT("trackName"), TrackName);

    FString ActorName;
    LocalPayload->TryGetStringField(TEXT("actorName"), ActorName);

    // Find or use master track if no actor specified
    FGuid BindingGuid;
    if (!ActorName.IsEmpty()) {
      // Find binding by actor name
      // Use const interface to avoid deprecation warning
      const UMovieScene *ConstMovieScene = MovieScene;
      for (const FMovieSceneBinding &Binding : ConstMovieScene->GetBindings()) {
        FString BindingName;
        if (FMovieScenePossessable *Possessable =
                MovieScene->FindPossessable(Binding.GetObjectGuid())) {
          BindingName = Possessable->GetName();
        } else if (FMovieSceneSpawnable *Spawnable =
                       MovieScene->FindSpawnable(Binding.GetObjectGuid())) {
          BindingName = Spawnable->GetName();
        }

        if (BindingName.Contains(ActorName)) {
          BindingGuid = Binding.GetObjectGuid();
          break;
        }
      }
      if (!BindingGuid.IsValid()) {
        SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Binding not found for actor: %s"),
                            *ActorName),
            nullptr, TEXT("BINDING_NOT_FOUND"));
        return true;
      }
    }

    // Add the track
    UMovieSceneTrack *NewTrack = nullptr;

    // Dynamic resolution with heuristics
    UClass *TrackClass = ResolveUClass(TrackType);

    // Try with common prefixes
    if (!TrackClass) {
      TrackClass = ResolveUClass(
          FString::Printf(TEXT("UMovieScene%sTrack"), *TrackType));
    }
    if (!TrackClass) {
      TrackClass =
          ResolveUClass(FString::Printf(TEXT("MovieScene%sTrack"), *TrackType));
    }
    // Try simple "U" prefix
    if (!TrackClass) {
      TrackClass = ResolveUClass(FString::Printf(TEXT("U%s"), *TrackType));
    }

    // Validate it's actually a track class
    if (TrackClass && TrackClass->IsChildOf(UMovieSceneTrack::StaticClass())) {
      if (BindingGuid.IsValid()) {
        NewTrack = MovieScene->AddTrack(TrackClass, BindingGuid);
      } else {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        NewTrack = MovieScene->AddTrack(TrackClass);
#else
        // UE 5.0: AddTrack always requires a binding GUID
        SendAutomationError(
            RequestingSocket, RequestId,
            TEXT("Adding tracks without binding is not supported in UE 5.0. Please provide an actor or object binding."),
            TEXT("NOT_SUPPORTED"));
        return true;
#endif
      }
    } else if (TrackClass) {
      // Found a class but it's not a track
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Class '%s' is not a UMovieSceneTrack"),
                          *TrackClass->GetName()),
          TEXT("INVALID_CLASS_TYPE"));
      return true;
    }

    if (NewTrack) {
      Sequence->MarkPackageDirty();

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("sequencePath"), SeqPath);
      Resp->SetStringField(TEXT("trackType"), TrackType);
      Resp->SetStringField(TEXT("trackName"),
                           TrackName.IsEmpty() ? TrackType : TrackName);
      if (!ActorName.IsEmpty()) {
        Resp->SetStringField(TEXT("actorName"), ActorName);
        Resp->SetStringField(TEXT("bindingGuid"), BindingGuid.ToString());
      }
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Track added successfully"), Resp, FString());
    } else {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Failed to add track of type: %s"), *TrackType),
          nullptr, TEXT("TRACK_CREATION_FAILED"));
    }
    return true;
  }

  if (EffectiveAction == TEXT("sequence_add_audio_track")) {
    const FString SeqPath = ResolveSequencePath(LocalPayload);
    FString SoundPath;
    int32 Frame = 0;
    int32 DurationFrames = 1;
    int32 RowIndex = 0;
    if (SeqPath.IsEmpty() || !LocalPayload->TryGetStringField(TEXT("soundPath"), SoundPath) || SoundPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("path and soundPath are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    LocalPayload->TryGetNumberField(TEXT("frame"), Frame);
    LocalPayload->TryGetNumberField(TEXT("durationFrames"), DurationFrames);
    LocalPayload->TryGetNumberField(TEXT("rowIndex"), RowIndex);
    if (DurationFrames < 1 || DurationFrames > 10000000 || RowIndex < 0 || RowIndex > 100000) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("durationFrames must be positive and rowIndex must be non-negative"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
    USoundBase* Sound = LoadObject<USoundBase>(nullptr, *SoundPath);
    if (!Sequence || !Sequence->GetMovieScene()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Level sequence not found"), TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }
    if (!Sound) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Sound asset not found"), TEXT("SOUND_NOT_FOUND"));
      return true;
    }
    UMovieSceneAudioTrack* AudioTrack = nullptr;
    for (UMovieSceneTrack* Track : MCP_GET_MOVIESCENE_TRACKS(Sequence->GetMovieScene())) {
      AudioTrack = Cast<UMovieSceneAudioTrack>(Track);
      if (AudioTrack) break;
    }
    if (!AudioTrack) AudioTrack = Cast<UMovieSceneAudioTrack>(Sequence->GetMovieScene()->AddTrack(UMovieSceneAudioTrack::StaticClass()));
    if (!AudioTrack) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to create audio track"), TEXT("TRACK_CREATION_FAILED"));
      return true;
    }
    UMovieSceneAudioSection* AudioSection = Cast<UMovieSceneAudioSection>(AudioTrack->AddNewSoundOnRow(Sound, FFrameNumber(Frame), RowIndex));
    if (!AudioSection) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to create audio section"), TEXT("SECTION_CREATION_FAILED"));
      return true;
    }
    AudioSection->SetRange(TRange<FFrameNumber>(FFrameNumber(Frame), FFrameNumber(Frame + DurationFrames)));
    bool bLooping = false;
    bool bPlayUntilFinished = false;
    LocalPayload->TryGetBoolField(TEXT("looping"), bLooping);
    LocalPayload->TryGetBoolField(TEXT("playUntilFinished"), bPlayUntilFinished);
    AudioSection->SetRepeating(bLooping);
    AudioSection->SetPlayUntilFinished(bPlayUntilFinished);
    Sequence->MarkPackageDirty();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("sequencePath"), SeqPath);
    Result->SetStringField(TEXT("soundPath"), Sound->GetPathName());
    Result->SetNumberField(TEXT("frame"), Frame);
    Result->SetNumberField(TEXT("durationFrames"), DurationFrames);
    Result->SetNumberField(TEXT("rowIndex"), RowIndex);
    Result->SetBoolField(TEXT("looping"), bLooping);
    Result->SetBoolField(TEXT("playUntilFinished"), bPlayUntilFinished);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Audio track section added"), Result);
    return true;
  }

  if (EffectiveAction == TEXT("sequence_inspect_niagara_parameter_track")) {
#if MCP_HAS_NIAGARA_FLOAT_PARAMETER_TRACK
    const FString SeqPath = ResolveSequencePath(LocalPayload);
    FString ActorName;
    if (SeqPath.IsEmpty() || !LocalPayload->TryGetStringField(TEXT("actorName"), ActorName) || ActorName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("path and actorName are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
    if (!Sequence || !Sequence->GetMovieScene()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Level sequence not found"), TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }
    FGuid BindingGuid;
    for (const FMovieSceneBinding& Binding : Sequence->GetMovieScene()->GetBindings()) {
      FString BindingName;
      if (const FMovieScenePossessable* Possessable = Sequence->GetMovieScene()->FindPossessable(Binding.GetObjectGuid())) BindingName = Possessable->GetName();
      else if (const FMovieSceneSpawnable* Spawnable = Sequence->GetMovieScene()->FindSpawnable(Binding.GetObjectGuid())) BindingName = Spawnable->GetName();
      if (BindingName.Equals(ActorName, ESearchCase::IgnoreCase) || BindingName.Contains(ActorName)) { BindingGuid = Binding.GetObjectGuid(); break; }
    }
    if (!BindingGuid.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Binding not found for actor"), TEXT("BINDING_NOT_FOUND"));
      return true;
    }
    TArray<TSharedPtr<FJsonValue>> Tracks;
    for (UMovieSceneTrack* Track : Sequence->GetMovieScene()->FindTracks(UMovieSceneNiagaraParameterTrack::StaticClass(), BindingGuid)) {
      UMovieSceneNiagaraParameterTrack* ParameterTrack = Cast<UMovieSceneNiagaraParameterTrack>(Track);
      if (!ParameterTrack) continue;
      TSharedPtr<FJsonObject> TrackJson = McpHandlerUtils::CreateResultObject();
      TrackJson->SetStringField(TEXT("trackClass"), ParameterTrack->GetClass()->GetName());
      TrackJson->SetStringField(TEXT("parameterName"), ParameterTrack->GetParameter().GetName().ToString());
      TrackJson->SetStringField(TEXT("parameterType"), ParameterTrack->GetParameter().GetType().GetName());
      TrackJson->SetNumberField(TEXT("sectionCount"), ParameterTrack->GetAllSections().Num());
      Tracks.Add(MakeShared<FJsonValueObject>(TrackJson));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("sequencePath"), SeqPath);
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetArrayField(TEXT("tracks"), Tracks);
    Result->SetNumberField(TEXT("count"), Tracks.Num());
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara parameter tracks inspected"), Result);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Niagara parameter track API is unavailable in this engine build"), TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  if (EffectiveAction == TEXT("sequence_add_niagara_vector_parameter_key")) {
#if MCP_HAS_NIAGARA_VECTOR_PARAMETER_TRACK
    const FString SeqPath = ResolveSequencePath(LocalPayload);
    FString ActorName, ParameterName;
    int32 Frame = 0, RowIndex = 0;
    double X = 0.0, Y = 0.0, Z = 0.0;
    if (SeqPath.IsEmpty() || !LocalPayload->TryGetStringField(TEXT("actorName"), ActorName) || ActorName.IsEmpty() ||
        !LocalPayload->TryGetStringField(TEXT("parameterName"), ParameterName) || ParameterName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("path, actorName, and parameterName are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    LocalPayload->TryGetNumberField(TEXT("frame"), Frame);
    LocalPayload->TryGetNumberField(TEXT("rowIndex"), RowIndex);
    LocalPayload->TryGetNumberField(TEXT("vectorX"), X);
    LocalPayload->TryGetNumberField(TEXT("vectorY"), Y);
    LocalPayload->TryGetNumberField(TEXT("vectorZ"), Z);
    if (Frame < -1000000000 || Frame > 1000000000 || RowIndex < 0 || RowIndex > 100000 || !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("frame, rowIndex, or vector channels are outside the supported range"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
    if (!Sequence || !Sequence->GetMovieScene()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Level sequence not found"), TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }
    FGuid BindingGuid;
    for (const FMovieSceneBinding& Binding : Sequence->GetMovieScene()->GetBindings()) {
      FString BindingName;
      if (const FMovieScenePossessable* Possessable = Sequence->GetMovieScene()->FindPossessable(Binding.GetObjectGuid())) BindingName = Possessable->GetName();
      else if (const FMovieSceneSpawnable* Spawnable = Sequence->GetMovieScene()->FindSpawnable(Binding.GetObjectGuid())) BindingName = Spawnable->GetName();
      if (BindingName.Equals(ActorName, ESearchCase::IgnoreCase) || BindingName.Contains(ActorName)) { BindingGuid = Binding.GetObjectGuid(); break; }
    }
    if (!BindingGuid.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Binding not found for actor"), TEXT("BINDING_NOT_FOUND"));
      return true;
    }
    UMovieSceneNiagaraVectorParameterTrack* ParameterTrack = nullptr;
    for (UMovieSceneTrack* Track : Sequence->GetMovieScene()->FindTracks(UMovieSceneNiagaraVectorParameterTrack::StaticClass(), BindingGuid)) {
      ParameterTrack = Cast<UMovieSceneNiagaraVectorParameterTrack>(Track);
      if (ParameterTrack) break;
    }
    if (!ParameterTrack) ParameterTrack = Cast<UMovieSceneNiagaraVectorParameterTrack>(Sequence->GetMovieScene()->AddTrack(UMovieSceneNiagaraVectorParameterTrack::StaticClass(), BindingGuid));
    if (!ParameterTrack) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to create Niagara vector parameter track"), TEXT("TRACK_CREATION_FAILED"));
      return true;
    }
    ParameterTrack->SetChannelsUsed(3);
    ParameterTrack->SetParameter(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), FName(*ParameterName)));
    UMovieSceneSection* Section = ParameterTrack->GetAllSections().Num() > 0 ? ParameterTrack->GetAllSections()[0] : ParameterTrack->CreateNewSection();
    if (!Section) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to create Niagara vector parameter section"), TEXT("SECTION_CREATION_FAILED"));
      return true;
    }
    if (ParameterTrack->GetAllSections().Num() == 0) ParameterTrack->AddSection(*Section);
    FMovieSceneFloatChannel* XChannel = Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0);
    FMovieSceneFloatChannel* YChannel = Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(1);
    FMovieSceneFloatChannel* ZChannel = Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(2);
    if (!XChannel || !YChannel || !ZChannel) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Niagara vector parameter section does not expose three float channels"), TEXT("CHANNEL_UNAVAILABLE"));
      return true;
    }
    XChannel->AddLinearKey(FFrameNumber(Frame), static_cast<float>(X));
    YChannel->AddLinearKey(FFrameNumber(Frame), static_cast<float>(Y));
    ZChannel->AddLinearKey(FFrameNumber(Frame), static_cast<float>(Z));
    Sequence->MarkPackageDirty();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("sequencePath"), SeqPath);
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("parameterName"), ParameterName);
    Result->SetNumberField(TEXT("frame"), Frame);
    Result->SetNumberField(TEXT("vectorX"), X);
    Result->SetNumberField(TEXT("vectorY"), Y);
    Result->SetNumberField(TEXT("vectorZ"), Z);
    Result->SetNumberField(TEXT("rowIndex"), RowIndex);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara vector parameter key added"), Result);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Niagara vector parameter track API is unavailable in this engine build"), TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  if (EffectiveAction == TEXT("sequence_add_niagara_bool_parameter_key")) {
#if MCP_HAS_NIAGARA_BOOL_PARAMETER_TRACK
    const FString SeqPath = ResolveSequencePath(LocalPayload);
    FString ActorName, ParameterName;
    int32 Frame = 0, RowIndex = 0;
    bool Value = false;
    if (SeqPath.IsEmpty() || !LocalPayload->TryGetStringField(TEXT("actorName"), ActorName) || ActorName.IsEmpty() ||
        !LocalPayload->TryGetStringField(TEXT("parameterName"), ParameterName) || ParameterName.IsEmpty() ||
        !LocalPayload->TryGetBoolField(TEXT("value"), Value)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("path, actorName, parameterName, and boolean value are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    LocalPayload->TryGetNumberField(TEXT("frame"), Frame);
    LocalPayload->TryGetNumberField(TEXT("rowIndex"), RowIndex);
    if (Frame < -1000000000 || Frame > 1000000000 || RowIndex < 0 || RowIndex > 100000) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("frame or rowIndex is outside the supported range"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
    if (!Sequence || !Sequence->GetMovieScene()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Level sequence not found"), TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }
    FGuid BindingGuid;
    for (const FMovieSceneBinding& Binding : Sequence->GetMovieScene()->GetBindings()) {
      FString BindingName;
      if (const FMovieScenePossessable* Possessable = Sequence->GetMovieScene()->FindPossessable(Binding.GetObjectGuid())) BindingName = Possessable->GetName();
      else if (const FMovieSceneSpawnable* Spawnable = Sequence->GetMovieScene()->FindSpawnable(Binding.GetObjectGuid())) BindingName = Spawnable->GetName();
      if (BindingName.Equals(ActorName, ESearchCase::IgnoreCase) || BindingName.Contains(ActorName)) { BindingGuid = Binding.GetObjectGuid(); break; }
    }
    if (!BindingGuid.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Binding not found for actor"), TEXT("BINDING_NOT_FOUND"));
      return true;
    }
    UMovieSceneNiagaraBoolParameterTrack* ParameterTrack = nullptr;
    for (UMovieSceneTrack* Track : Sequence->GetMovieScene()->FindTracks(UMovieSceneNiagaraBoolParameterTrack::StaticClass(), BindingGuid)) {
      ParameterTrack = Cast<UMovieSceneNiagaraBoolParameterTrack>(Track);
      if (ParameterTrack) break;
    }
    if (!ParameterTrack) ParameterTrack = Cast<UMovieSceneNiagaraBoolParameterTrack>(Sequence->GetMovieScene()->AddTrack(UMovieSceneNiagaraBoolParameterTrack::StaticClass(), BindingGuid));
    if (!ParameterTrack) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to create Niagara bool parameter track"), TEXT("TRACK_CREATION_FAILED"));
      return true;
    }
    ParameterTrack->SetParameter(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), FName(*ParameterName)));
    UMovieSceneSection* Section = ParameterTrack->GetAllSections().Num() > 0 ? ParameterTrack->GetAllSections()[0] : ParameterTrack->CreateNewSection();
    if (!Section) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to create Niagara bool parameter section"), TEXT("SECTION_CREATION_FAILED"));
      return true;
    }
    if (ParameterTrack->GetAllSections().Num() == 0) ParameterTrack->AddSection(*Section);
    FMovieSceneBoolChannel* Channel = Section->GetChannelProxy().GetChannel<FMovieSceneBoolChannel>(0);
    if (!Channel) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Niagara bool parameter section has no bool channel"), TEXT("CHANNEL_UNAVAILABLE"));
      return true;
    }
    TArray<FFrameNumber> Times{FFrameNumber(Frame)};
    TArray<bool> Values{Value};
    Channel->AddKeys(Times, Values);
    Sequence->MarkPackageDirty();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("sequencePath"), SeqPath);
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("parameterName"), ParameterName);
    Result->SetNumberField(TEXT("frame"), Frame);
    Result->SetBoolField(TEXT("value"), Value);
    Result->SetNumberField(TEXT("rowIndex"), RowIndex);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara bool parameter key added"), Result);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Niagara bool parameter track API is unavailable in this engine build"), TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  if (EffectiveAction == TEXT("sequence_add_niagara_integer_parameter_key")) {
#if MCP_HAS_NIAGARA_INTEGER_PARAMETER_TRACK
    const FString SeqPath = ResolveSequencePath(LocalPayload);
    FString ActorName, ParameterName;
    int32 Frame = 0, RowIndex = 0, Value = 0;
    if (SeqPath.IsEmpty() || !LocalPayload->TryGetStringField(TEXT("actorName"), ActorName) || ActorName.IsEmpty() ||
        !LocalPayload->TryGetStringField(TEXT("parameterName"), ParameterName) || ParameterName.IsEmpty() ||
        !LocalPayload->TryGetNumberField(TEXT("value"), Value)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("path, actorName, parameterName, and integer value are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    LocalPayload->TryGetNumberField(TEXT("frame"), Frame);
    LocalPayload->TryGetNumberField(TEXT("rowIndex"), RowIndex);
    if (Frame < -1000000000 || Frame > 1000000000 || RowIndex < 0 || RowIndex > 100000) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("frame or rowIndex is outside the supported range"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
    if (!Sequence || !Sequence->GetMovieScene()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Level sequence not found"), TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }
    FGuid BindingGuid;
    for (const FMovieSceneBinding& Binding : Sequence->GetMovieScene()->GetBindings()) {
      FString BindingName;
      if (const FMovieScenePossessable* Possessable = Sequence->GetMovieScene()->FindPossessable(Binding.GetObjectGuid())) BindingName = Possessable->GetName();
      else if (const FMovieSceneSpawnable* Spawnable = Sequence->GetMovieScene()->FindSpawnable(Binding.GetObjectGuid())) BindingName = Spawnable->GetName();
      if (BindingName.Equals(ActorName, ESearchCase::IgnoreCase) || BindingName.Contains(ActorName)) { BindingGuid = Binding.GetObjectGuid(); break; }
    }
    if (!BindingGuid.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Binding not found for actor"), TEXT("BINDING_NOT_FOUND"));
      return true;
    }
    UMovieSceneNiagaraIntegerParameterTrack* ParameterTrack = nullptr;
    for (UMovieSceneTrack* Track : Sequence->GetMovieScene()->FindTracks(UMovieSceneNiagaraIntegerParameterTrack::StaticClass(), BindingGuid)) {
      ParameterTrack = Cast<UMovieSceneNiagaraIntegerParameterTrack>(Track);
      if (ParameterTrack) break;
    }
    if (!ParameterTrack) ParameterTrack = Cast<UMovieSceneNiagaraIntegerParameterTrack>(Sequence->GetMovieScene()->AddTrack(UMovieSceneNiagaraIntegerParameterTrack::StaticClass(), BindingGuid));
    if (!ParameterTrack) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to create Niagara integer parameter track"), TEXT("TRACK_CREATION_FAILED"));
      return true;
    }
    ParameterTrack->SetParameter(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), FName(*ParameterName)));
    UMovieSceneSection* Section = ParameterTrack->GetAllSections().Num() > 0 ? ParameterTrack->GetAllSections()[0] : ParameterTrack->CreateNewSection();
    if (!Section) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to create Niagara integer parameter section"), TEXT("SECTION_CREATION_FAILED"));
      return true;
    }
    if (ParameterTrack->GetAllSections().Num() == 0) ParameterTrack->AddSection(*Section);
    FMovieSceneIntegerChannel* Channel = Section->GetChannelProxy().GetChannel<FMovieSceneIntegerChannel>(0);
    if (!Channel) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Niagara integer parameter section has no integer channel"), TEXT("CHANNEL_UNAVAILABLE"));
      return true;
    }
    TArray<FFrameNumber> Times{FFrameNumber(Frame)};
    TArray<int32> Values{Value};
    Channel->AddKeys(Times, Values);
    Sequence->MarkPackageDirty();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("sequencePath"), SeqPath);
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("parameterName"), ParameterName);
    Result->SetNumberField(TEXT("frame"), Frame);
    Result->SetNumberField(TEXT("value"), Value);
    Result->SetNumberField(TEXT("rowIndex"), RowIndex);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara integer parameter key added"), Result);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Niagara integer parameter track API is unavailable in this engine build"), TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  if (EffectiveAction == TEXT("sequence_add_niagara_float_parameter_key")) {
#if MCP_HAS_NIAGARA_FLOAT_PARAMETER_TRACK
    const FString SeqPath = ResolveSequencePath(LocalPayload);
    FString ActorName, ParameterName;
    int32 Frame = 0, RowIndex = 0;
    double Value = 0.0;
    if (SeqPath.IsEmpty() || !LocalPayload->TryGetStringField(TEXT("actorName"), ActorName) || ActorName.IsEmpty() ||
        !LocalPayload->TryGetStringField(TEXT("parameterName"), ParameterName) || ParameterName.IsEmpty() ||
        !LocalPayload->TryGetNumberField(TEXT("value"), Value)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("path, actorName, parameterName, and value are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    LocalPayload->TryGetNumberField(TEXT("frame"), Frame);
    LocalPayload->TryGetNumberField(TEXT("rowIndex"), RowIndex);
    if (Frame < -1000000000 || Frame > 1000000000 || RowIndex < 0 || RowIndex > 100000 || !FMath::IsFinite(Value)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("frame, rowIndex, or value is outside the supported range"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
    if (!Sequence || !Sequence->GetMovieScene()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Level sequence not found"), TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }
    FGuid BindingGuid;
    for (const FMovieSceneBinding& Binding : Sequence->GetMovieScene()->GetBindings()) {
      FString BindingName;
      if (const FMovieScenePossessable* Possessable = Sequence->GetMovieScene()->FindPossessable(Binding.GetObjectGuid())) BindingName = Possessable->GetName();
      else if (const FMovieSceneSpawnable* Spawnable = Sequence->GetMovieScene()->FindSpawnable(Binding.GetObjectGuid())) BindingName = Spawnable->GetName();
      if (BindingName.Equals(ActorName, ESearchCase::IgnoreCase) || BindingName.Contains(ActorName)) { BindingGuid = Binding.GetObjectGuid(); break; }
    }
    if (!BindingGuid.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Binding not found for actor"), TEXT("BINDING_NOT_FOUND"));
      return true;
    }
    UMovieSceneNiagaraFloatParameterTrack* ParameterTrack = nullptr;
    for (UMovieSceneTrack* Track : Sequence->GetMovieScene()->FindTracks(UMovieSceneNiagaraFloatParameterTrack::StaticClass(), BindingGuid)) {
      ParameterTrack = Cast<UMovieSceneNiagaraFloatParameterTrack>(Track);
      if (ParameterTrack) break;
    }
    if (!ParameterTrack) ParameterTrack = Cast<UMovieSceneNiagaraFloatParameterTrack>(Sequence->GetMovieScene()->AddTrack(UMovieSceneNiagaraFloatParameterTrack::StaticClass(), BindingGuid));
    if (!ParameterTrack) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to create Niagara float parameter track"), TEXT("TRACK_CREATION_FAILED"));
      return true;
    }
    ParameterTrack->SetParameter(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), FName(*ParameterName)));
    UMovieSceneSection* Section = ParameterTrack->GetAllSections().Num() > 0 ? ParameterTrack->GetAllSections()[0] : ParameterTrack->CreateNewSection();
    if (!Section) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to create Niagara parameter section"), TEXT("SECTION_CREATION_FAILED"));
      return true;
    }
    if (ParameterTrack->GetAllSections().Num() == 0) ParameterTrack->AddSection(*Section);
    FMovieSceneFloatChannel* Channel = Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0);
    if (!Channel) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Niagara float parameter section has no float channel"), TEXT("CHANNEL_UNAVAILABLE"));
      return true;
    }
    Channel->AddLinearKey(FFrameNumber(Frame), static_cast<float>(Value));
    Sequence->MarkPackageDirty();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("sequencePath"), SeqPath);
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("parameterName"), ParameterName);
    Result->SetNumberField(TEXT("frame"), Frame);
    Result->SetNumberField(TEXT("value"), Value);
    Result->SetNumberField(TEXT("rowIndex"), RowIndex);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara float parameter key added"), Result);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Niagara float parameter track API is unavailable in this engine build"), TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  if (EffectiveAction == TEXT("sequence_create_niagara_float_parameter_track")) {
#if MCP_HAS_NIAGARA_FLOAT_PARAMETER_TRACK
    const FString SeqPath = ResolveSequencePath(LocalPayload);
    FString ActorName;
    FString ParameterName;
    if (SeqPath.IsEmpty() || !LocalPayload->TryGetStringField(TEXT("actorName"), ActorName) || ActorName.IsEmpty() ||
        !LocalPayload->TryGetStringField(TEXT("parameterName"), ParameterName) || ParameterName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("path, actorName, and parameterName are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
    if (!Sequence || !Sequence->GetMovieScene()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Level sequence not found"), TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }
    FGuid BindingGuid;
    for (const FMovieSceneBinding& Binding : Sequence->GetMovieScene()->GetBindings()) {
      FString BindingName;
      if (const FMovieScenePossessable* Possessable = Sequence->GetMovieScene()->FindPossessable(Binding.GetObjectGuid())) BindingName = Possessable->GetName();
      else if (const FMovieSceneSpawnable* Spawnable = Sequence->GetMovieScene()->FindSpawnable(Binding.GetObjectGuid())) BindingName = Spawnable->GetName();
      if (BindingName.Equals(ActorName, ESearchCase::IgnoreCase) || BindingName.Contains(ActorName)) { BindingGuid = Binding.GetObjectGuid(); break; }
    }
    if (!BindingGuid.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Binding not found for actor"), TEXT("BINDING_NOT_FOUND"));
      return true;
    }
    UMovieSceneNiagaraFloatParameterTrack* ParameterTrack = Cast<UMovieSceneNiagaraFloatParameterTrack>(
        Sequence->GetMovieScene()->AddTrack(UMovieSceneNiagaraFloatParameterTrack::StaticClass(), BindingGuid));
    if (!ParameterTrack) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to create Niagara float parameter track"), TEXT("TRACK_CREATION_FAILED"));
      return true;
    }
    ParameterTrack->SetParameter(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), FName(*ParameterName)));
    UMovieSceneSection* Section = ParameterTrack->CreateNewSection();
    if (Section) ParameterTrack->AddSection(*Section);
    Sequence->MarkPackageDirty();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("sequencePath"), SeqPath);
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("parameterName"), ParameterName);
    Result->SetStringField(TEXT("parameterType"), TEXT("float"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara float parameter track created"), Result);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Niagara float parameter track API is unavailable in this engine build"), TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  if (EffectiveAction == TEXT("sequence_add_niagara_system_track")) {
#if MCP_HAS_NIAGARA_SEQUENCE_TRACK
    const FString SeqPath = ResolveSequencePath(LocalPayload);
    FString ActorName;
    int32 StartFrame = 0;
    int32 EndFrame = 1;
    if (SeqPath.IsEmpty() || !LocalPayload->TryGetStringField(TEXT("actorName"), ActorName) || ActorName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("path and actorName are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    LocalPayload->TryGetNumberField(TEXT("startFrame"), StartFrame);
    LocalPayload->TryGetNumberField(TEXT("endFrame"), EndFrame);
    if (EndFrame <= StartFrame) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("endFrame must be greater than startFrame"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
    if (!Sequence || !Sequence->GetMovieScene()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Level sequence not found"), TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }
    FGuid BindingGuid;
    for (const FMovieSceneBinding& Binding : Sequence->GetMovieScene()->GetBindings()) {
      FString BindingName;
      if (const FMovieScenePossessable* Possessable = Sequence->GetMovieScene()->FindPossessable(Binding.GetObjectGuid())) BindingName = Possessable->GetName();
      else if (const FMovieSceneSpawnable* Spawnable = Sequence->GetMovieScene()->FindSpawnable(Binding.GetObjectGuid())) BindingName = Spawnable->GetName();
      if (BindingName.Equals(ActorName, ESearchCase::IgnoreCase) || BindingName.Contains(ActorName)) { BindingGuid = Binding.GetObjectGuid(); break; }
    }
    if (!BindingGuid.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Binding not found for actor"), TEXT("BINDING_NOT_FOUND"));
      return true;
    }
    UMovieSceneNiagaraSystemTrack* NiagaraTrack = Cast<UMovieSceneNiagaraSystemTrack>(Sequence->GetMovieScene()->AddTrack(UMovieSceneNiagaraSystemTrack::StaticClass(), BindingGuid));
    if (!NiagaraTrack) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to create Niagara system track"), TEXT("TRACK_CREATION_FAILED"));
      return true;
    }
    UMovieSceneNiagaraSystemSpawnSection* Section = Cast<UMovieSceneNiagaraSystemSpawnSection>(NiagaraTrack->CreateNewSection());
    if (!Section) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to create Niagara spawn section"), TEXT("SECTION_CREATION_FAILED"));
      return true;
    }
    Section->SetRange(TRange<FFrameNumber>(FFrameNumber(StartFrame), FFrameNumber(EndFrame)));
    NiagaraTrack->AddSection(*Section);
    Sequence->MarkPackageDirty();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("sequencePath"), SeqPath);
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("startFrame"), StartFrame);
    Result->SetNumberField(TEXT("endFrame"), EndFrame);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Niagara system track added"), Result);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Niagara Sequencer API is unavailable in this engine build"), TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  if (EffectiveAction == TEXT("sequence_add_custom_primitive_data_track")) {
    const FString SeqPath = ResolveSequencePath(LocalPayload);
    FString ActorName;
    int32 Frame = 0, RowIndex = 0, DataIndex = -1;
    double Value = 0.0;
    if (SeqPath.IsEmpty() || !LocalPayload->TryGetStringField(TEXT("actorName"), ActorName) || ActorName.IsEmpty() ||
        !LocalPayload->TryGetNumberField(TEXT("customPrimitiveDataIndex"), DataIndex) ||
        !LocalPayload->TryGetNumberField(TEXT("value"), Value)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("path, actorName, customPrimitiveDataIndex, and value are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    LocalPayload->TryGetNumberField(TEXT("frame"), Frame);
    LocalPayload->TryGetNumberField(TEXT("rowIndex"), RowIndex);
    if (DataIndex < 0 || DataIndex > 255 || Frame < -1000000000 || Frame > 1000000000 || RowIndex < 0 || RowIndex > 100000 || !FMath::IsFinite(Value)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("customPrimitiveDataIndex, frame, rowIndex, or value is outside the supported range"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
    if (!Sequence || !Sequence->GetMovieScene()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Level sequence not found"), TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }
    FGuid BindingGuid;
    for (const FMovieSceneBinding& Binding : Sequence->GetMovieScene()->GetBindings()) {
      FString BindingName;
      if (const FMovieScenePossessable* Possessable = Sequence->GetMovieScene()->FindPossessable(Binding.GetObjectGuid())) BindingName = Possessable->GetName();
      else if (const FMovieSceneSpawnable* Spawnable = Sequence->GetMovieScene()->FindSpawnable(Binding.GetObjectGuid())) BindingName = Spawnable->GetName();
      if (BindingName.Equals(ActorName, ESearchCase::IgnoreCase) || BindingName.Contains(ActorName)) { BindingGuid = Binding.GetObjectGuid(); break; }
    }
    if (!BindingGuid.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Binding not found for actor"), TEXT("BINDING_NOT_FOUND"));
      return true;
    }
    UMovieSceneCustomPrimitiveDataTrack* DataTrack = nullptr;
    for (UMovieSceneTrack* Track : Sequence->GetMovieScene()->FindTracks(UMovieSceneCustomPrimitiveDataTrack::StaticClass(), BindingGuid)) {
      DataTrack = Cast<UMovieSceneCustomPrimitiveDataTrack>(Track);
      if (DataTrack) break;
    }
    if (!DataTrack) DataTrack = Cast<UMovieSceneCustomPrimitiveDataTrack>(Sequence->GetMovieScene()->AddTrack(UMovieSceneCustomPrimitiveDataTrack::StaticClass(), BindingGuid));
    if (!DataTrack) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to create custom primitive data track"), TEXT("TRACK_CREATION_FAILED"));
      return true;
    }
    DataTrack->AddScalarParameterKey(static_cast<uint8>(DataIndex), FFrameNumber(Frame), RowIndex, static_cast<float>(Value), RCIM_Linear);
    Sequence->MarkPackageDirty();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("sequencePath"), SeqPath);
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("customPrimitiveDataIndex"), DataIndex);
    Result->SetNumberField(TEXT("frame"), Frame);
    Result->SetNumberField(TEXT("value"), Value);
    Result->SetNumberField(TEXT("rowIndex"), RowIndex);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Custom primitive data key added"), Result);
    return true;
  }

  if (EffectiveAction == TEXT("sequence_add_material_parameter_track") ||
      EffectiveAction == TEXT("sequence_add_material_color_track")) {
    const FString SeqPath = ResolveSequencePath(LocalPayload);
    FString ActorName;
    FString ParameterName;
    int32 Frame = 0;
    int32 RowIndex = 0;
    double Value = 0.0;
    double ColorR = 0.0, ColorG = 0.0, ColorB = 0.0, ColorA = 1.0;
    if (SeqPath.IsEmpty() || !LocalPayload->TryGetStringField(TEXT("actorName"), ActorName) || ActorName.IsEmpty() ||
        !LocalPayload->TryGetStringField(TEXT("parameterName"), ParameterName) || ParameterName.IsEmpty() ||
        (EffectiveAction == TEXT("sequence_add_material_parameter_track") && !LocalPayload->TryGetNumberField(TEXT("value"), Value))) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("path, actorName, parameterName, and value are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    LocalPayload->TryGetNumberField(TEXT("frame"), Frame);
    LocalPayload->TryGetNumberField(TEXT("rowIndex"), RowIndex);
    if (EffectiveAction == TEXT("sequence_add_material_color_track")) {
      LocalPayload->TryGetNumberField(TEXT("colorR"), ColorR);
      LocalPayload->TryGetNumberField(TEXT("colorG"), ColorG);
      LocalPayload->TryGetNumberField(TEXT("colorB"), ColorB);
      LocalPayload->TryGetNumberField(TEXT("colorA"), ColorA);
      if (!FMath::IsFinite(ColorR) || !FMath::IsFinite(ColorG) || !FMath::IsFinite(ColorB) || !FMath::IsFinite(ColorA)) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("color channels must be finite numbers"), TEXT("INVALID_ARGUMENT"));
        return true;
      }
    }
    if (Frame < -1000000000 || Frame > 1000000000 || RowIndex < 0 || RowIndex > 100000 || !FMath::IsFinite(Value)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("frame, rowIndex, and value are outside the supported range"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
    if (!Sequence || !Sequence->GetMovieScene()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Level sequence not found"), TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }
    FGuid BindingGuid;
    for (const FMovieSceneBinding& Binding : Sequence->GetMovieScene()->GetBindings()) {
      FString BindingName;
      if (const FMovieScenePossessable* Possessable = Sequence->GetMovieScene()->FindPossessable(Binding.GetObjectGuid())) BindingName = Possessable->GetName();
      else if (const FMovieSceneSpawnable* Spawnable = Sequence->GetMovieScene()->FindSpawnable(Binding.GetObjectGuid())) BindingName = Spawnable->GetName();
      if (BindingName.Equals(ActorName, ESearchCase::IgnoreCase) || BindingName.Contains(ActorName)) { BindingGuid = Binding.GetObjectGuid(); break; }
    }
    if (!BindingGuid.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Binding not found for actor"), TEXT("BINDING_NOT_FOUND"));
      return true;
    }
    UMovieSceneMaterialTrack* MaterialTrack = nullptr;
    for (UMovieSceneTrack* Track : Sequence->GetMovieScene()->FindTracks(UMovieSceneMaterialTrack::StaticClass(), BindingGuid)) {
      MaterialTrack = Cast<UMovieSceneMaterialTrack>(Track);
      if (MaterialTrack) break;
    }
    if (!MaterialTrack) MaterialTrack = Cast<UMovieSceneMaterialTrack>(Sequence->GetMovieScene()->AddTrack(UMovieSceneComponentMaterialTrack::StaticClass(), BindingGuid));
    if (!MaterialTrack) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to create component material track"), TEXT("TRACK_CREATION_FAILED"));
      return true;
    }
    if (EffectiveAction == TEXT("sequence_add_material_color_track")) {
      MaterialTrack->AddColorParameterKey(FName(*ParameterName), FFrameNumber(Frame), RowIndex,
          FLinearColor(static_cast<float>(ColorR), static_cast<float>(ColorG), static_cast<float>(ColorB), static_cast<float>(ColorA)), RCIM_Linear);
    } else {
      MaterialTrack->AddScalarParameterKey(FName(*ParameterName), FFrameNumber(Frame), RowIndex, static_cast<float>(Value), RCIM_Linear);
    }
    Sequence->MarkPackageDirty();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("sequencePath"), SeqPath);
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("parameterName"), ParameterName);
    Result->SetNumberField(TEXT("frame"), Frame);
    Result->SetNumberField(TEXT("value"), Value);
    if (EffectiveAction == TEXT("sequence_add_material_color_track")) {
      Result->SetNumberField(TEXT("colorR"), ColorR);
      Result->SetNumberField(TEXT("colorG"), ColorG);
      Result->SetNumberField(TEXT("colorB"), ColorB);
      Result->SetNumberField(TEXT("colorA"), ColorA);
    }
    Result->SetNumberField(TEXT("rowIndex"), RowIndex);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Material parameter key added"), Result);
    return true;
  }

  if (EffectiveAction == TEXT("sequence_inspect_shot_settings")) {
#if MCP_HAS_CINEMATIC_SHOT_SECTION
    const FString SeqPath = ResolveSequencePath(LocalPayload);
    ULevelSequence* Sequence = SeqPath.IsEmpty() ? nullptr : LoadObject<ULevelSequence>(nullptr, *SeqPath);
    if (!Sequence || !Sequence->GetMovieScene()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Level sequence not found"), TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }
    int32 ShotIndex = 0;
    LocalPayload->TryGetNumberField(TEXT("shotIndex"), ShotIndex);
    if (ShotIndex < 0 || ShotIndex > 100000) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("shotIndex must be a non-negative integer"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    UMovieSceneCinematicShotSection* ShotSection = nullptr;
    for (UMovieSceneTrack* Track : MCP_GET_MOVIESCENE_TRACKS(Sequence->GetMovieScene())) {
      UMovieSceneCinematicShotTrack* ShotTrack = Cast<UMovieSceneCinematicShotTrack>(Track);
      if (!ShotTrack || !ShotTrack->GetAllSections().IsValidIndex(ShotIndex)) continue;
      ShotSection = Cast<UMovieSceneCinematicShotSection>(ShotTrack->GetAllSections()[ShotIndex]);
      if (ShotSection) break;
    }
    if (!ShotSection) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Cinematic shot section not found"), TEXT("SHOT_NOT_FOUND"));
      return true;
    }
    const TRange<FFrameNumber> Range = ShotSection->GetRange();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("sequencePath"), Sequence->GetPathName());
    Result->SetNumberField(TEXT("shotIndex"), ShotIndex);
    Result->SetStringField(TEXT("shotDisplayName"), ShotSection->GetShotDisplayName());
    Result->SetNumberField(TEXT("thumbnailReferenceOffset"), ShotSection->GetThumbnailReferenceOffset());
    Result->SetBoolField(TEXT("hasStartFrame"), Range.HasLowerBound());
    Result->SetBoolField(TEXT("hasEndFrame"), Range.HasUpperBound());
    if (Range.HasLowerBound()) Result->SetNumberField(TEXT("startFrame"), Range.GetLowerBoundValue().Value);
    if (Range.HasUpperBound()) Result->SetNumberField(TEXT("endFrame"), Range.GetUpperBoundValue().Value);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Cinematic shot settings inspected"), Result);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Cinematic shot section API is unavailable in this engine build"), TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  if (EffectiveAction == TEXT("sequence_configure_shot_settings")) {
#if MCP_HAS_CINEMATIC_SHOT_SECTION
    const FString SeqPath = ResolveSequencePath(LocalPayload);
    ULevelSequence* Sequence = SeqPath.IsEmpty() ? nullptr : LoadObject<ULevelSequence>(nullptr, *SeqPath);
    if (!Sequence || !Sequence->GetMovieScene()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Level sequence not found"), TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }
    int32 ShotIndex = 0;
    LocalPayload->TryGetNumberField(TEXT("shotIndex"), ShotIndex);
    if (ShotIndex < 0 || ShotIndex > 100000) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("shotIndex must be a non-negative integer"), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMovieSceneCinematicShotSection* ShotSection = nullptr;
    for (UMovieSceneTrack* Track : MCP_GET_MOVIESCENE_TRACKS(Sequence->GetMovieScene())) {
      UMovieSceneCinematicShotTrack* ShotTrack = Cast<UMovieSceneCinematicShotTrack>(Track);
      if (!ShotTrack || !ShotTrack->GetAllSections().IsValidIndex(ShotIndex)) continue;
      ShotSection = Cast<UMovieSceneCinematicShotSection>(ShotTrack->GetAllSections()[ShotIndex]);
      if (ShotSection) break;
    }
    if (!ShotSection) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Cinematic shot section not found"), TEXT("SHOT_NOT_FOUND"));
      return true;
    }

    FString DisplayName;
    if (LocalPayload->TryGetStringField(TEXT("shotDisplayName"), DisplayName)) {
      ShotSection->SetShotDisplayName(DisplayName);
    }
    double ThumbnailOffset = 0.0;
    if (LocalPayload->TryGetNumberField(TEXT("thumbnailReferenceOffset"), ThumbnailOffset)) {
      if (!FMath::IsFinite(ThumbnailOffset)) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("thumbnailReferenceOffset must be finite"), TEXT("INVALID_ARGUMENT"));
        return true;
      }
      ShotSection->SetThumbnailReferenceOffset(static_cast<float>(ThumbnailOffset));
    }
    int32 StartFrame = 0;
    int32 EndFrame = 0;
    const bool bHasStart = LocalPayload->TryGetNumberField(TEXT("startFrame"), StartFrame);
    const bool bHasEnd = LocalPayload->TryGetNumberField(TEXT("endFrame"), EndFrame);
    if (bHasStart != bHasEnd || (bHasStart && EndFrame <= StartFrame)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("startFrame and endFrame must be provided together with endFrame greater than startFrame"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (bHasStart) {
      ShotSection->SetRange(TRange<FFrameNumber>(FFrameNumber(StartFrame), FFrameNumber(EndFrame)));
    }

    Sequence->MarkPackageDirty();
    const bool bPersisted = McpSafeAssetSave(Sequence);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("sequencePath"), Sequence->GetPathName());
    Result->SetNumberField(TEXT("shotIndex"), ShotIndex);
    Result->SetStringField(TEXT("shotDisplayName"), ShotSection->GetShotDisplayName());
    Result->SetNumberField(TEXT("thumbnailReferenceOffset"), ShotSection->GetThumbnailReferenceOffset());
    Result->SetBoolField(TEXT("persisted"), bPersisted);
    SendAutomationResponse(RequestingSocket, RequestId, bPersisted,
                           bPersisted ? TEXT("Cinematic shot settings configured") : TEXT("Cinematic shot settings changed but save failed"),
                           Result, bPersisted ? FString() : TEXT("SAVE_FAILED"));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Cinematic shot section API is unavailable in this engine build"), TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  if (EffectiveAction == TEXT("sequence_add_subsequence")) {
    const FString ParentPath = ResolveSequencePath(LocalPayload);
    FString ChildPath;
    LocalPayload->TryGetStringField(TEXT("subsequencePath"), ChildPath);
    if (ChildPath.IsEmpty()) LocalPayload->TryGetStringField(TEXT("childSequencePath"), ChildPath);
    if (ParentPath.IsEmpty() || ChildPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
          TEXT("path and subsequencePath are required for add_subsequence"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (ParentPath.Equals(ChildPath, ESearchCase::IgnoreCase)) {
      SendAutomationError(RequestingSocket, RequestId,
          TEXT("A sequence cannot contain itself as a subsequence"), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    ULevelSequence* ParentSequence = LoadObject<ULevelSequence>(nullptr, *ParentPath);
    UMovieSceneSequence* ChildSequence = LoadObject<UMovieSceneSequence>(nullptr, *ChildPath);
    if (!ParentSequence) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Parent level sequence not found"), TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }
    if (!ChildSequence) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Subsequence asset not found"), TEXT("SUBSEQUENCE_NOT_FOUND"));
      return true;
    }
    UMovieScene* MovieScene = ParentSequence->GetMovieScene();
    if (!MovieScene) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Parent MovieScene not available"), TEXT("MOVIESCENE_UNAVAILABLE"));
      return true;
    }
    int32 StartFrame = 0;
    LocalPayload->TryGetNumberField(TEXT("startFrame"), StartFrame);
    int32 DurationFrames = 0;
    if (!LocalPayload->TryGetNumberField(TEXT("durationFrames"), DurationFrames) || DurationFrames <= 0 || DurationFrames > 10000000) {
      SendAutomationError(RequestingSocket, RequestId,
          TEXT("durationFrames must be between 1 and 10000000"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    int32 RowIndex = INDEX_NONE;
    LocalPayload->TryGetNumberField(TEXT("rowIndex"), RowIndex);
    UMovieSceneSubTrack* SubTrack = nullptr;
    for (UMovieSceneTrack* Track : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
      if (UMovieSceneSubTrack* Candidate = Cast<UMovieSceneSubTrack>(Track)) {
        SubTrack = Candidate;
        break;
      }
    }
    if (!SubTrack) SubTrack = MovieScene->AddTrack<UMovieSceneSubTrack>();
    if (!SubTrack) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to create a subsequence track"), TEXT("TRACK_CREATE_FAILED"));
      return true;
    }
    if (SubTrack->ContainsSequence(*ChildSequence)) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Subsequence is already present in the parent sequence"), TEXT("DUPLICATE_SUBSEQUENCE"));
      return true;
    }
    UMovieSceneSubSection* Section = RowIndex >= 0
        ? SubTrack->AddSequenceOnRow(ChildSequence, FFrameNumber(StartFrame), DurationFrames, RowIndex)
        : SubTrack->AddSequence(ChildSequence, FFrameNumber(StartFrame), DurationFrames);
    if (!Section) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Unable to create subsequence section"), TEXT("SECTION_CREATE_FAILED"));
      return true;
    }
    ParentSequence->MarkPackageDirty();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("parentSequencePath"), ParentSequence->GetPathName());
    Result->SetStringField(TEXT("subsequencePath"), ChildSequence->GetPathName());
    Result->SetNumberField(TEXT("startFrame"), StartFrame);
    Result->SetNumberField(TEXT("durationFrames"), DurationFrames);
    if (RowIndex >= 0) Result->SetNumberField(TEXT("rowIndex"), RowIndex);
    Result->SetBoolField(TEXT("persisted"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Subsequence section added"), Result);
    return true;
  }

  if (EffectiveAction == TEXT("sequence_add_camera_shake_track")) {
#if MCP_HAS_CAMERA_SHAKE_TRACK
    const FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("add_camera_shake_track requires a sequence path"), nullptr,
                             TEXT("INVALID_SEQUENCE"));
      return true;
    }
    ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
    UMovieScene *MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
    FString ShakeClassPath;
    LocalPayload->TryGetStringField(TEXT("shakeClass"), ShakeClassPath);
    double FrameValue = 0.0;
    if (!MovieScene || ShakeClassPath.IsEmpty() ||
        !LocalPayload->TryGetNumberField(TEXT("frame"), FrameValue) ||
        !FMath::IsFinite(FrameValue) || !FMath::IsNearlyEqual(FrameValue, FMath::RoundToDouble(FrameValue))) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("add_camera_shake_track requires an existing sequence, shakeClass, and an integer frame"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }
    UClass *ShakeClass = ResolveUClass(ShakeClassPath);
    if (!ShakeClass || !ShakeClass->IsChildOf(UCameraShakeBase::StaticClass())) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("shakeClass must resolve to a UCameraShakeBase subclass"), nullptr,
                             TEXT("INVALID_CLASS_TYPE"));
      return true;
    }
    UMovieSceneCameraShakeTrack *Track = MovieScene->AddMasterTrack<UMovieSceneCameraShakeTrack>();
    if (!Track) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Unable to create camera shake track"), nullptr,
                             TEXT("TRACK_CREATE_FAILED"));
      return true;
    }
    Track->AddNewCameraShake(FFrameNumber(static_cast<int32>(FrameValue)), ShakeClass);
    Sequence->MarkPackageDirty();
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("sequencePath"), SeqPath);
    Resp->SetStringField(TEXT("trackType"), TEXT("CameraShake"));
    Resp->SetStringField(TEXT("shakeClass"), ShakeClass->GetPathName());
    Resp->SetNumberField(TEXT("frame"), FrameValue);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Camera shake track added successfully"), Resp);
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Camera shake tracks are unavailable in this build"), nullptr,
                           TEXT("CAMERA_SHAKE_NOT_AVAILABLE"));
    return true;
#endif
  }

  // sequence_list_tracks: List all tracks for a sequence binding
  if (EffectiveAction == TEXT("sequence_list_tracks")) {
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("sequence_list_tracks requires a sequence path"), nullptr,
          TEXT("INVALID_SEQUENCE"));
      return true;
    }

#if WITH_EDITOR
    ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
    if (!Sequence) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Level sequence not found"), nullptr,
                             TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }

    UMovieScene *MovieScene = Sequence->GetMovieScene();
    if (!MovieScene) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("MovieScene not available"), nullptr,
                             TEXT("MOVIESCENE_UNAVAILABLE"));
      return true;
    }

    TArray<TSharedPtr<FJsonValue>> TracksArray;

    // Get Tracks (formerly GetMasterTracks)
    for (UMovieSceneTrack *Track : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
      if (!Track)
        continue;
      TSharedPtr<FJsonObject> TrackObj = McpHandlerUtils::CreateResultObject();
      TrackObj->SetStringField(TEXT("trackName"), Track->GetName());
      TrackObj->SetStringField(TEXT("trackType"), Track->GetClass()->GetName());
      TrackObj->SetStringField(TEXT("displayName"),
                               Track->GetDisplayName().ToString());
      TrackObj->SetBoolField(TEXT("isMasterTrack"), true);
      TrackObj->SetNumberField(TEXT("sectionCount"),
                               Track->GetAllSections().Num());
      TracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
    }

    // Get tracks from bindings
    for (const FMovieSceneBinding &Binding :
         const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
      FString BindingName;
      if (FMovieScenePossessable *Possessable =
              MovieScene->FindPossessable(Binding.GetObjectGuid())) {
        BindingName = Possessable->GetName();
      } else if (FMovieSceneSpawnable *Spawnable =
                     MovieScene->FindSpawnable(Binding.GetObjectGuid())) {
        BindingName = Spawnable->GetName();
      }

      for (UMovieSceneTrack *Track : MCP_GET_BINDING_TRACKS(Binding)) {
        if (!Track)
          continue;
        TSharedPtr<FJsonObject> TrackObj = McpHandlerUtils::CreateResultObject();
        TrackObj->SetStringField(TEXT("trackName"), Track->GetName());
        TrackObj->SetStringField(TEXT("trackType"),
                                 Track->GetClass()->GetName());
        TrackObj->SetStringField(TEXT("displayName"),
                                 Track->GetDisplayName().ToString());
        TrackObj->SetBoolField(TEXT("isMasterTrack"), false);
        TrackObj->SetStringField(TEXT("bindingName"), BindingName);
        TrackObj->SetStringField(TEXT("bindingGuid"),
                                 Binding.GetObjectGuid().ToString());
        TrackObj->SetNumberField(TEXT("sectionCount"),
                                 Track->GetAllSections().Num());
        TracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
      }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetArrayField(TEXT("tracks"), TracksArray);
    Resp->SetNumberField(TEXT("trackCount"), TracksArray.Num());
    Resp->SetStringField(TEXT("sequencePath"), SeqPath);
    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Found %d tracks"), TracksArray.Num()), Resp,
        FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("sequence_list_tracks requires editor build"),
                           nullptr, TEXT("EDITOR_ONLY"));
    return true;
#endif
  }

  // sequence_set_work_range: Set the work range of a sequence
  if (EffectiveAction == TEXT("sequence_set_work_range")) {
    FString SeqPath = ResolveSequencePath(LocalPayload);
    if (SeqPath.IsEmpty()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("sequence_set_work_range requires a sequence path"), nullptr,
          TEXT("INVALID_SEQUENCE"));
      return true;
    }

#if WITH_EDITOR
    ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
    if (!Sequence) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Level sequence not found"), nullptr,
                             TEXT("SEQUENCE_NOT_FOUND"));
      return true;
    }

    UMovieScene *MovieScene = Sequence->GetMovieScene();
    if (!MovieScene) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("MovieScene not available"), nullptr,
                             TEXT("MOVIESCENE_UNAVAILABLE"));
      return true;
    }

    double Start = 0.0, End = 0.0;
    LocalPayload->TryGetNumberField(TEXT("start"), Start);
    LocalPayload->TryGetNumberField(TEXT("end"), End);

    FFrameRate TickResolution = MovieScene->GetTickResolution();
    // Round to int32 for FFrameNumber constructor
    FFrameNumber StartFrame(
        (int32)FMath::RoundToInt(Start * TickResolution.AsDecimal()));
    FFrameNumber EndFrame(
        (int32)FMath::RoundToInt(End * TickResolution.AsDecimal()));

    // SetWorkingRange expects seconds (double)
    MovieScene->SetWorkingRange(Start, End);
    MovieScene->Modify();

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetNumberField(TEXT("startFrame"), StartFrame.Value);
    Resp->SetNumberField(TEXT("endFrame"), EndFrame.Value);
    Resp->SetStringField(TEXT("sequencePath"), SeqPath);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Work range set successfully"), Resp,
                           FString());
    return true;
#else
    SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("sequence_set_work_range requires editor build"), nullptr,
        TEXT("EDITOR_ONLY"));
    return true;
#endif
  }

  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      FString::Printf(TEXT("Sequence action not implemented by plugin: %s"),
                      *Action),
      nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
}
