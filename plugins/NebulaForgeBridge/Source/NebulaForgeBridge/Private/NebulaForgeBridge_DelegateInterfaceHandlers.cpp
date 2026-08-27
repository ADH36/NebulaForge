// =============================================================================
// NebulaForgeBridge_DelegateInterfaceHandlers.cpp
// =============================================================================
// Phase 34.8: Blueprint delegates/event dispatchers and reflected interfaces.

#include "NebulaForgeBridgeGlobals.h"
#include "NebulaForgeBridgeHelpers.h"
#include "NebulaForgeBridgeSubsystem.h"
#include "McpPropertyReflection.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/ScriptDelegates.h"
#include "UObject/Interface.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#endif

#if WITH_EDITOR
namespace
{

FString GetDelegateName(const TSharedPtr<FJsonObject>& Payload)
{
  FString Name;
  if (Payload.IsValid())
  {
    Payload->TryGetStringField(TEXT("delegateName"), Name);
    if (Name.IsEmpty()) Payload->TryGetStringField(TEXT("name"), Name);
  }
  return Name.TrimStartAndEnd();
}

FString GetInterfaceFunctionName(const TSharedPtr<FJsonObject>& Payload)
{
  FString Name;
  if (Payload.IsValid())
  {
    Payload->TryGetStringField(TEXT("interfaceFunction"), Name);
    if (Name.IsEmpty()) Payload->TryGetStringField(TEXT("interfaceFunctionName"), Name);
    if (Name.IsEmpty()) Payload->TryGetStringField(TEXT("functionName"), Name);
  }
  return Name.TrimStartAndEnd();
}

UObject* ResolveLoadedObject(const FString& ObjectPath)
{
  if (ObjectPath.IsEmpty()) return nullptr;
  if (UObject* Existing = FindObject<UObject>(nullptr, *ObjectPath)) return Existing;
  return LoadObject<UObject>(nullptr, *ObjectPath);
}

UBlueprint* ResolveBlueprint(const FString& BlueprintPath)
{
  if (BlueprintPath.IsEmpty()) return nullptr;
  FString ResolvedPath;
  FString LoadError;
  return LoadBlueprintAsset(BlueprintPath, ResolvedPath, LoadError);
}

UClass* ResolveInterfaceClass(const FString& InterfacePath)
{
  if (InterfacePath.IsEmpty()) return nullptr;

  if (UClass* Class = ResolveClassByName(InterfacePath))
  {
    return Class->IsChildOf(UInterface::StaticClass()) ? Class : nullptr;
  }

  if (UBlueprint* InterfaceBlueprint = ResolveBlueprint(InterfacePath))
  {
    UClass* GeneratedClass = InterfaceBlueprint->GeneratedClass;
    return GeneratedClass && GeneratedClass->IsChildOf(UInterface::StaticClass())
               ? GeneratedClass
               : nullptr;
  }
  return nullptr;
}

UFunction* GetDelegateSignature(FProperty* Property)
{
  if (FDelegateProperty* Single = CastField<FDelegateProperty>(Property))
  {
    return Single->SignatureFunction;
  }
  if (FMulticastDelegateProperty* Multi = CastField<FMulticastDelegateProperty>(Property))
  {
    return Multi->SignatureFunction;
  }
  return nullptr;
}

void AddFunctionSignature(UFunction* Function, TSharedPtr<FJsonObject>& Result)
{
  if (!Function) return;

  Result->SetStringField(TEXT("signatureFunction"), Function->GetPathName());
  TArray<TSharedPtr<FJsonValue>> Parameters;
  for (TFieldIterator<FProperty> It(Function); It; ++It)
  {
    FProperty* Property = *It;
    if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm)) continue;
    TSharedPtr<FJsonObject> Parameter = McpHandlerUtils::CreateResultObject();
    Parameter->SetStringField(TEXT("name"), Property->GetName());
    Parameter->SetStringField(TEXT("type"), McpPropertyReflection::GetPropertyTypeName(Property));
    Parameter->SetBoolField(TEXT("returnValue"), Property->HasAnyPropertyFlags(CPF_ReturnParm));
    Parameter->SetBoolField(TEXT("outParameter"), Property->HasAnyPropertyFlags(CPF_OutParm));
    Parameters.Add(MakeShared<FJsonValueObject>(Parameter));
  }
  Result->SetArrayField(TEXT("parameters"), Parameters);
}

bool BuildInvocationParameters(UFunction* Function,
                               const TSharedPtr<FJsonObject>& Values,
                               TArray<uint8>& Storage,
                               FString& OutError)
{
  OutError.Empty();
  if (!Function) return true;

  Storage.SetNumZeroed(Function->ParmsSize);
  if (Function->ParmsSize > 0) Function->InitializeStruct(Storage.GetData());

  for (TFieldIterator<FProperty> It(Function); It; ++It)
  {
    FProperty* Property = *It;
    if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm) ||
        Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
    {
      continue;
    }

    const TSharedPtr<FJsonValue>* Value =
        Values.IsValid() ? Values->Values.Find(Property->GetName()) : nullptr;
    if (!Value || !Value->IsValid())
    {
      OutError = FString::Printf(TEXT("Missing parameter value: %s"), *Property->GetName());
      if (Function->ParmsSize > 0) Function->DestroyStruct(Storage.GetData());
      return false;
    }

    FString PropertyError;
    if (!McpPropertyReflection::ApplyJsonValueToProperty(
            Storage.GetData(), Property, *Value, PropertyError))
    {
      OutError = FString::Printf(TEXT("Parameter %s: %s"), *Property->GetName(), *PropertyError);
      if (Function->ParmsSize > 0) Function->DestroyStruct(Storage.GetData());
      return false;
    }
  }
  return true;
}

void AddInvocationReturn(UFunction* Function, TArray<uint8>& Storage,
                         TSharedPtr<FJsonObject>& Result)
{
  if (!Function || Function->ParmsSize == 0) return;
  for (TFieldIterator<FProperty> It(Function); It; ++It)
  {
    FProperty* Property = *It;
    if (!Property || !Property->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
    if (TSharedPtr<FJsonValue> Value =
            McpPropertyReflection::ExportPropertyToJsonValue(Storage.GetData(), Property))
    {
      Result->SetField(TEXT("returnValue"), Value);
    }
  }
}

void AddDelegateState(UObject* Object, FProperty* Property,
                      TSharedPtr<FJsonObject>& Result)
{
  if (!Object || !Property) return;

  Result->SetStringField(TEXT("delegateName"), Property->GetName());
  Result->SetStringField(TEXT("delegateType"),
                          CastField<FDelegateProperty>(Property) ? TEXT("single") : TEXT("multicast"));
  if (UFunction* Signature = GetDelegateSignature(Property)) AddFunctionSignature(Signature, Result);

  if (FDelegateProperty* Single = CastField<FDelegateProperty>(Property))
  {
    FScriptDelegate* Delegate = Single->ContainerPtrToValuePtr<FScriptDelegate>(Object);
    Result->SetBoolField(TEXT("bound"), Delegate && Delegate->IsBound());
    if (Delegate && Delegate->IsBound())
    {
      if (UObject* BoundObject = Delegate->GetUObject())
      {
        Result->SetStringField(TEXT("boundObject"), BoundObject->GetPathName());
      }
      Result->SetStringField(TEXT("boundFunction"), Delegate->GetFunctionName().ToString());
    }
    return;
  }

  if (FMulticastDelegateProperty* Multi = CastField<FMulticastDelegateProperty>(Property))
  {
    void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
    const FMulticastScriptDelegate* Delegate = Multi->GetMulticastDelegate(ValuePtr);
    TArray<TSharedPtr<FJsonValue>> BoundObjects;
    if (Delegate)
    {
      for (UObject* BoundObject : Delegate->GetAllObjects())
      {
        if (BoundObject) BoundObjects.Add(MakeShared<FJsonValueString>(BoundObject->GetPathName()));
      }
      Result->SetBoolField(TEXT("bound"), Delegate->IsBound());
      Result->SetStringField(TEXT("delegateState"), Delegate->ToString());
    }
    Result->SetArrayField(TEXT("boundObjects"), BoundObjects);
    Result->SetNumberField(TEXT("bindingCount"), BoundObjects.Num());
  }
}

bool AddBlueprintDelegateVariable(UBlueprint* Blueprint, const FString& Name,
                                  const FName& PinCategory, bool bSave,
                                  TSharedPtr<FJsonObject>& Result,
                                  FString& OutError)
{
  if (!Blueprint || Name.IsEmpty())
  {
    OutError = TEXT("blueprintPath and delegateName are required");
    return false;
  }

  for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
  {
    if (Variable.VarName == FName(*Name))
    {
      Result->SetBoolField(TEXT("created"), false);
      Result->SetBoolField(TEXT("alreadyExisted"), true);
      Result->SetStringField(TEXT("delegateName"), Name);
      Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
      return true;
    }
  }

  FEdGraphPinType DelegateType;
  DelegateType.PinCategory = PinCategory;
  FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*Name), DelegateType);
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
  if (bSave) McpSafeAssetSave(Blueprint);

  Result->SetBoolField(TEXT("created"), true);
  Result->SetBoolField(TEXT("alreadyExisted"), false);
  Result->SetStringField(TEXT("delegateName"), Name);
  Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
  Result->SetStringField(TEXT("delegateType"),
                         PinCategory == UEdGraphSchema_K2::PC_Delegate ? TEXT("single") : TEXT("multicast"));
  return true;
}

} // namespace
#endif

bool UNebulaForgeBridgeSubsystem::HandleDelegateInterfaceAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
  if (!Payload.IsValid())
  {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Delegate/interface payload missing"),
                         TEXT("INVALID_PAYLOAD"));
    return true;
  }

  auto SendInvalid = [&](const FString& Message) {
    SendAutomationResponse(RequestingSocket, RequestId, false, Message, nullptr,
                            TEXT("INVALID_ARGUMENT"));
  };
  auto GetString = [&](const TCHAR* Field) {
    FString Value;
    Payload->TryGetStringField(Field, Value);
    return Value.TrimStartAndEnd();
  };
  auto ShouldSave = [&]() {
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("saveAsset"), bSave);
    return bSave;
  };

  if (Action == TEXT("create_event_dispatcher") || Action == TEXT("create_delegate"))
  {
    const FString BlueprintPath = GetString(TEXT("blueprintPath"));
    const FString Name = GetDelegateName(Payload);
    if (BlueprintPath.IsEmpty() || Name.IsEmpty())
    {
      SendInvalid(TEXT("blueprintPath and delegateName are required."));
      return true;
    }
    UBlueprint* Blueprint = ResolveBlueprint(BlueprintPath);
    if (!Blueprint)
    {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint asset was not found"),
                           TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    FString Kind = GetString(TEXT("delegateKind")).ToLower();
    if (Action == TEXT("create_event_dispatcher")) Kind = TEXT("event_dispatcher");
    if (Kind.IsEmpty()) Kind = TEXT("multicast");
    if (Kind != TEXT("single") && Kind != TEXT("multicast") && Kind != TEXT("event_dispatcher"))
    {
      SendInvalid(TEXT("delegateKind must be single, multicast, or event_dispatcher."));
      return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString Error;
    const FName PinCategory = Kind == TEXT("single")
                                   ? UEdGraphSchema_K2::PC_Delegate
                                   : UEdGraphSchema_K2::PC_MCDelegate;
    if (!AddBlueprintDelegateVariable(Blueprint, Name, PinCategory, ShouldSave(), Result, Error))
    {
      SendInvalid(Error);
      return true;
    }
    Result->SetStringField(TEXT("kind"), Kind);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           Action == TEXT("create_event_dispatcher")
                               ? TEXT("Event dispatcher created")
                               : TEXT("Delegate variable created"),
                           Result, FString());
    return true;
  }

  if (Action == TEXT("create_blueprint_interface"))
  {
    const FString Name = GetString(TEXT("name"));
    FString Folder = GetString(TEXT("folder"));
    if (Folder.IsEmpty()) Folder = TEXT("/Game/Interfaces");
    if (Name.IsEmpty()) { SendInvalid(TEXT("name is required.")); return true; }

    FString PackageName;
    FString PathError;
    if (!ValidateAssetCreationPath(Folder, Name, PackageName, PathError))
    {
      SendAutomationError(RequestingSocket, RequestId, PathError, TEXT("INVALID_PATH"));
      return true;
    }
    if (LoadObject<UBlueprint>(nullptr, *(PackageName + TEXT(".") + SanitizeAssetName(Name))))
    {
      SendInvalid(TEXT("An asset with that interface name already exists."));
      return true;
    }

    UPackage* Package = CreatePackage(*PackageName);
    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    if (!Package || !Factory)
    {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create interface package"),
                           TEXT("PACKAGE_CREATE_FAILED"));
      return true;
    }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Factory->BlueprintType = BPTYPE_Interface;
#endif
    Factory->ParentClass = UInterface::StaticClass();
    UBlueprint* InterfaceBlueprint = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*SanitizeAssetName(Name)),
                                  RF_Public | RF_Standalone, nullptr, GWarn));
    if (!InterfaceBlueprint)
    {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create interface Blueprint"),
                           TEXT("BLUEPRINT_CREATE_FAILED"));
      return true;
    }
    InterfaceBlueprint->BlueprintType = BPTYPE_Interface;
    FAssetRegistryModule::AssetCreated(InterfaceBlueprint);
    McpSafeAssetSave(InterfaceBlueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("interfacePath"), InterfaceBlueprint->GetPathName());
    Result->SetBoolField(TEXT("created"), true);
    Result->SetBoolField(TEXT("isInterface"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Blueprint interface created"), Result, FString());
    return true;
  }

  if (Action == TEXT("add_interface_function"))
  {
    const FString BlueprintPath = GetString(TEXT("blueprintPath"));
    const FString FunctionName = GetInterfaceFunctionName(Payload);
    UBlueprint* Blueprint = ResolveBlueprint(BlueprintPath);
    if (!Blueprint || FunctionName.IsEmpty())
    {
      SendInvalid(TEXT("blueprintPath and interfaceFunction are required."));
      return true;
    }
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
      if (Graph && Graph->GetFName() == FName(*FunctionName))
      {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetBoolField(TEXT("created"), false);
        Result->SetBoolField(TEXT("alreadyExisted"), true);
        Result->SetStringField(TEXT("functionName"), FunctionName);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Interface function already exists"), Result, FString());
        return true;
      }
    }
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint, FName(*FunctionName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    if (!Graph)
    {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create interface function graph"),
                           TEXT("FUNCTION_GRAPH_CREATE_FAILED"));
      return true;
    }
    FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, Graph, false,
                                                       static_cast<UFunction*>(nullptr));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    if (ShouldSave()) McpSafeAssetSave(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("created"), true);
    Result->SetStringField(TEXT("functionName"), FunctionName);
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Interface function added"), Result, FString());
    return true;
  }

  if (Action == TEXT("implement_interface"))
  {
    const FString BlueprintPath = GetString(TEXT("blueprintPath"));
    FString InterfacePath = GetString(TEXT("interfacePath"));
    if (InterfacePath.IsEmpty()) InterfacePath = GetString(TEXT("interfaceClass"));
    UBlueprint* Blueprint = ResolveBlueprint(BlueprintPath);
    UClass* InterfaceClass = ResolveInterfaceClass(InterfacePath);
    if (!Blueprint || !InterfaceClass)
    {
      SendInvalid(TEXT("blueprintPath and a valid interfacePath/interfaceClass are required."));
      return true;
    }
    if (Blueprint->GeneratedClass && Blueprint->GeneratedClass->ImplementsInterface(InterfaceClass))
    {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetBoolField(TEXT("implemented"), true);
      Result->SetBoolField(TEXT("alreadyImplemented"), true);
      Result->SetStringField(TEXT("interfaceClass"), InterfaceClass->GetPathName());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Blueprint already implements interface"), Result, FString());
      return true;
    }
    if (!FBlueprintEditorUtils::ImplementNewInterface(Blueprint, InterfaceClass->GetFName()))
    {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint interface implementation failed"),
                           TEXT("INTERFACE_IMPLEMENT_FAILED"));
      return true;
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    if (ShouldSave()) McpSafeAssetSave(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("implemented"), true);
    Result->SetBoolField(TEXT("alreadyImplemented"), false);
    Result->SetStringField(TEXT("interfaceClass"), InterfaceClass->GetPathName());
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Blueprint interface implemented"), Result, FString());
    return true;
  }

  if (Action == TEXT("get_interface_info"))
  {
    FString TargetPath = GetString(TEXT("targetObject"));
    FString BlueprintPath = GetString(TEXT("blueprintPath"));
    UObject* Target = TargetPath.IsEmpty() ? nullptr : ResolveLoadedObject(TargetPath);
    UClass* TargetClass = Target ? Target->GetClass() : nullptr;
    if (!TargetClass && !BlueprintPath.IsEmpty())
    {
      if (UBlueprint* Blueprint = ResolveBlueprint(BlueprintPath)) TargetClass = Blueprint->GeneratedClass;
    }
    FString InterfacePath = GetString(TEXT("interfacePath"));
    if (InterfacePath.IsEmpty()) InterfacePath = GetString(TEXT("interfaceClass"));
    UClass* FilterInterface = InterfacePath.IsEmpty() ? nullptr : ResolveInterfaceClass(InterfacePath);
    UClass* ClassToInspect = TargetClass ? TargetClass : FilterInterface;
    if (!ClassToInspect)
    {
      SendInvalid(TEXT("targetObject, blueprintPath, or interfacePath is required."));
      return true;
    }

    TArray<TSharedPtr<FJsonValue>> Interfaces;
    if (TargetClass)
    {
      for (const FImplementedInterface& Implemented : TargetClass->Interfaces)
      {
        UClass* InterfaceClass = Implemented.Class;
        if (!InterfaceClass || (FilterInterface && InterfaceClass != FilterInterface)) continue;
        TSharedPtr<FJsonObject> Item = McpHandlerUtils::CreateResultObject();
        Item->SetStringField(TEXT("interfaceClass"), InterfaceClass->GetPathName());
        TArray<TSharedPtr<FJsonValue>> Functions;
        for (TFieldIterator<UFunction> It(InterfaceClass); It; ++It)
        {
          if (*It) Functions.Add(MakeShared<FJsonValueString>((*It)->GetName()));
        }
        Item->SetArrayField(TEXT("functions"), Functions);
        Interfaces.Add(MakeShared<FJsonValueObject>(Item));
      }
    }
    else if (FilterInterface)
    {
      TSharedPtr<FJsonObject> Item = McpHandlerUtils::CreateResultObject();
      Item->SetStringField(TEXT("interfaceClass"), FilterInterface->GetPathName());
      TArray<TSharedPtr<FJsonValue>> Functions;
      for (TFieldIterator<UFunction> It(FilterInterface); It; ++It)
      {
        if (*It) Functions.Add(MakeShared<FJsonValueString>((*It)->GetName()));
      }
      Item->SetArrayField(TEXT("functions"), Functions);
      Interfaces.Add(MakeShared<FJsonValueObject>(Item));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("interfaces"), Interfaces);
    Result->SetNumberField(TEXT("count"), Interfaces.Num());
    Result->SetBoolField(TEXT("implementsInterface"), FilterInterface && TargetClass &&
                                                        TargetClass->ImplementsInterface(FilterInterface));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Interface information inspected"), Result, FString());
    return true;
  }

  if (Action == TEXT("call_interface_function"))
  {
    const FString TargetPath = GetString(TEXT("targetObject"));
    FString InterfacePath = GetString(TEXT("interfacePath"));
    if (InterfacePath.IsEmpty()) InterfacePath = GetString(TEXT("interfaceClass"));
    const FString FunctionName = GetInterfaceFunctionName(Payload);
    UObject* Target = ResolveLoadedObject(TargetPath);
    UClass* InterfaceClass = ResolveInterfaceClass(InterfacePath);
    if (!Target || !InterfaceClass || FunctionName.IsEmpty())
    {
      SendInvalid(TEXT("targetObject, interfacePath/interfaceClass, and interfaceFunction are required."));
      return true;
    }
    if (!Target->GetClass()->ImplementsInterface(InterfaceClass))
    {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Target does not implement the requested interface"),
                           TEXT("INTERFACE_NOT_IMPLEMENTED"));
      return true;
    }
    UFunction* Function = Target->FindFunction(FName(*FunctionName));
    if (!Function) Function = InterfaceClass->FindFunctionByName(FName(*FunctionName));
    if (!Function)
    {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Interface function was not found"),
                           TEXT("INTERFACE_FUNCTION_NOT_FOUND"));
      return true;
    }
    TArray<uint8> Storage;
    FString ParameterError;
    const TSharedPtr<FJsonObject>* Values = nullptr;
    Payload->TryGetObjectField(TEXT("parameterValues"), Values);
    if (!BuildInvocationParameters(Function, Values ? *Values : nullptr, Storage, ParameterError))
    {
      SendInvalid(ParameterError);
      return true;
    }
    Target->ProcessEvent(Function, Function->ParmsSize > 0 ? Storage.GetData() : nullptr);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("targetObject"), Target->GetPathName());
    Result->SetStringField(TEXT("interfaceClass"), InterfaceClass->GetPathName());
    Result->SetStringField(TEXT("interfaceFunction"), FunctionName);
    AddInvocationReturn(Function, Storage, Result);
    if (Function->ParmsSize > 0) Function->DestroyStruct(Storage.GetData());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Interface function called"), Result, FString());
    return true;
  }

  const FString DelegatePath = GetString(TEXT("delegateObject"));
  const FString DelegateName = GetDelegateName(Payload);
  UObject* DelegateObject = ResolveLoadedObject(DelegatePath);
  if (DelegatePath.IsEmpty())
  {
    SendInvalid(TEXT("delegateObject is required."));
    return true;
  }
  if (!DelegateObject)
  {
    SendAutomationError(RequestingSocket, RequestId, TEXT("delegateObject must reference a loaded UObject"),
                         TEXT("DELEGATE_OBJECT_NOT_FOUND"));
    return true;
  }

  if (Action == TEXT("list_delegate_bindings") && DelegateName.IsEmpty())
  {
    TArray<TSharedPtr<FJsonValue>> Delegates;
    for (TFieldIterator<FProperty> It(DelegateObject->GetClass()); It; ++It)
    {
      FProperty* Candidate = *It;
      if (!Candidate || (!CastField<FDelegateProperty>(Candidate) &&
                         !CastField<FMulticastDelegateProperty>(Candidate)))
      {
        continue;
      }
      TSharedPtr<FJsonObject> Item = McpHandlerUtils::CreateResultObject();
      AddDelegateState(DelegateObject, Candidate, Item);
      Delegates.Add(MakeShared<FJsonValueObject>(Item));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("delegateObject"), DelegateObject->GetPathName());
    Result->SetArrayField(TEXT("delegates"), Delegates);
    Result->SetNumberField(TEXT("count"), Delegates.Num());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Delegate bindings listed"), Result, FString());
    return true;
  }
  if (DelegateName.IsEmpty())
  {
    SendInvalid(TEXT("delegateName is required for this delegate operation."));
    return true;
  }
  FProperty* Property = DelegateObject->GetClass()->FindPropertyByName(FName(*DelegateName));
  if (!Property || (!CastField<FDelegateProperty>(Property) &&
                    !CastField<FMulticastDelegateProperty>(Property)))
  {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Delegate property was not found"),
                         TEXT("DELEGATE_NOT_FOUND"));
    return true;
  }

  if (Action == TEXT("inspect_delegate") || Action == TEXT("list_delegate_bindings"))
  {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    AddDelegateState(DelegateObject, Property, Result);
    Result->SetStringField(TEXT("delegateObject"), DelegateObject->GetPathName());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Delegate bindings inspected"), Result, FString());
    return true;
  }

  if (Action == TEXT("bind_to_event") || Action == TEXT("bind_delegate") ||
      Action == TEXT("unbind_from_event"))
  {
    const FString TargetPath = GetString(TEXT("targetObject"));
    const FString FunctionName = GetString(TEXT("callbackFunction")).IsEmpty()
                                      ? GetString(TEXT("functionName"))
                                      : GetString(TEXT("callbackFunction"));
    UObject* Target = ResolveLoadedObject(TargetPath);
    if (!Target || FunctionName.IsEmpty())
    {
      SendInvalid(TEXT("targetObject and callbackFunction are required."));
      return true;
    }
    UFunction* Callback = Target->FindFunction(FName(*FunctionName));
    if (!Callback)
    {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Callback UFunction was not found on targetObject"),
                           TEXT("CALLBACK_FUNCTION_NOT_FOUND"));
      return true;
    }

    FScriptDelegate ScriptDelegate;
    ScriptDelegate.BindUFunction(Target, FName(*FunctionName));
    const bool bUnbind = Action == TEXT("unbind_from_event");
    if (FDelegateProperty* Single = CastField<FDelegateProperty>(Property))
    {
      FScriptDelegate* Delegate = Single->ContainerPtrToValuePtr<FScriptDelegate>(DelegateObject);
      if (bUnbind) Delegate->Unbind();
      else Delegate->BindUFunction(Target, FName(*FunctionName));
    }
    else if (FMulticastDelegateProperty* Multi = CastField<FMulticastDelegateProperty>(Property))
    {
      void* ValuePtr = Property->ContainerPtrToValuePtr<void>(DelegateObject);
      if (bUnbind) Multi->RemoveDelegate(ScriptDelegate, DelegateObject, ValuePtr);
      else Multi->AddDelegate(ScriptDelegate, DelegateObject, ValuePtr);
    }
    DelegateObject->Modify();
    DelegateObject->MarkPackageDirty();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("delegateObject"), DelegateObject->GetPathName());
    Result->SetStringField(TEXT("delegateName"), DelegateName);
    Result->SetStringField(TEXT("targetObject"), Target->GetPathName());
    Result->SetStringField(TEXT("callbackFunction"), FunctionName);
    Result->SetBoolField(TEXT("bound"), !bUnbind);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           bUnbind ? TEXT("Delegate binding removed") : TEXT("Delegate binding added"),
                           Result, FString());
    return true;
  }

  if (Action == TEXT("broadcast_event"))
  {
    FMulticastDelegateProperty* Multi = CastField<FMulticastDelegateProperty>(Property);
    if (!Multi)
    {
      SendAutomationError(RequestingSocket, RequestId, TEXT("broadcast_event requires a multicast delegate"),
                           TEXT("DELEGATE_NOT_MULTICAST"));
      return true;
    }
    UFunction* Signature = Multi->SignatureFunction;
    TArray<uint8> Storage;
    FString ParameterError;
    const TSharedPtr<FJsonObject>* Values = nullptr;
    Payload->TryGetObjectField(TEXT("parameterValues"), Values);
    if (!BuildInvocationParameters(Signature, Values ? *Values : nullptr, Storage, ParameterError))
    {
      SendInvalid(ParameterError);
      return true;
    }
    void* ValuePtr = Property->ContainerPtrToValuePtr<void>(DelegateObject);
    const FMulticastScriptDelegate* Delegate = Multi->GetMulticastDelegate(ValuePtr);
    if (Delegate) Delegate->ProcessMulticastDelegate(Signature && Signature->ParmsSize > 0 ? Storage.GetData() : nullptr);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("delegateObject"), DelegateObject->GetPathName());
    Result->SetStringField(TEXT("delegateName"), DelegateName);
    Result->SetBoolField(TEXT("broadcast"), Delegate != nullptr);
    Result->SetNumberField(TEXT("bindingCount"), Delegate ? Delegate->GetAllObjects().Num() : 0);
    if (Signature && Signature->ParmsSize > 0) Signature->DestroyStruct(Storage.GetData());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Event dispatcher broadcast"), Result, FString());
    return true;
  }

  return false;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}
