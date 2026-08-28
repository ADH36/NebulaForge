#include "AI/NebulaForgeAIToolGateway.h"
#include "AI/NebulaForgeAIDiagnostics.h"
#include "AI/NebulaForgeAISecretStore.h"
#include "NebulaForgeAIService.h"
#include "AI/NebulaForgeAIConversationService.h"
#include "AI/NebulaForgeAISettings.h"
#include "Editor.h"
#include "HAL/PlatformTime.h"
#include "NebulaForgeBridgeSubsystem.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    TSharedPtr<FJsonObject> MakeSchema(TArray<TPair<FString, FString>>&& Properties, TArray<FString>&& Required)
    {
        TSharedRef<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));
        TSharedRef<FJsonObject> Props = MakeShared<FJsonObject>();
        for (TPair<FString, FString>& Prop : Properties)
        {
            TSharedRef<FJsonObject> PropObj = MakeShared<FJsonObject>();
            PropObj->SetStringField(TEXT("type"), Prop.Value);
            Props->SetObjectField(Prop.Key, PropObj);
        }
        Schema->SetObjectField(TEXT("properties"), Props);
        if (Required.Num() > 0)
        {
            TArray<TSharedPtr<FJsonValue>> RequiredJson;
            for (const FString& Req : Required)
            {
                RequiredJson.Add(MakeShared<FJsonValueString>(Req));
            }
            Schema->SetArrayField(TEXT("required"), RequiredJson);
        }
        return Schema;
    }

    FString SummarizeArguments(const TSharedPtr<FJsonObject>& Arguments)
    {
        if (!Arguments.IsValid() || Arguments->Values.Num() == 0)
        {
            return TEXT("(no arguments)");
        }
        TArray<FString> Parts;
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Arguments->Values)
        {
            FString Value = Pair.Value.IsValid() ? Pair.Value->AsString() : FString();
            if (Value.Len() > 60)
            {
                Value = Value.Left(60) + TEXT("...");
            }
            Parts.Add(FString::Printf(TEXT("%s=%s"), *Pair.Key, *Value));
        }
        return FString::Join(Parts, TEXT(", "));
    }
} // namespace

FNebulaForgeAIToolGateway::FNebulaForgeAIToolGateway()
{
    auto AddEntry = [this](const TCHAR* Name, const TCHAR* Description, ENebulaAIToolRisk Risk,
                           TSharedPtr<FJsonObject> Schema, const TCHAR* SubsystemAction,
                           const TCHAR* SubAction)
    {
        FNebulaAIToolDefinition Def;
        Def.Name = Name;
        Def.Description = Description;
        Def.ParametersSchema = MoveTemp(Schema);
        Def.Risk = Risk;
        // Store the dispatch target in the description-sibling fields via a
        // side map entry below.
        FCatalogEntry Entry;
        Entry.Definition = MoveTemp(Def);
        Entry.SubsystemAction = SubsystemAction;
        Entry.SubAction = SubAction;
        Catalog.Add(Name, MoveTemp(Entry));
    };

    // ---- Read-only catalog (plan: read project/level/selection, query
    // assets, inspect logs) -----------------------------------------------
    AddEntry(TEXT("read_project_info"),
        TEXT("Read project name, engine version, and high-level project settings."),
        ENebulaAIToolRisk::ReadOnly, MakeSchema({}, {}), TEXT("inspect"), TEXT("get_project_settings"));

    AddEntry(TEXT("get_level_details"),
        TEXT("Get the current level/map details including name and actor counts."),
        ENebulaAIToolRisk::ReadOnly, MakeSchema({}, {}), TEXT("manage_level"), TEXT("get_current_level"));

    AddEntry(TEXT("list_actors"),
        TEXT("List actors in the current level with names and classes."),
        ENebulaAIToolRisk::ReadOnly,
        MakeSchema({{TEXT("filter"), TEXT("string")}, {TEXT("limit"), TEXT("number")}}, {}),
        TEXT("control_actor"), TEXT("list"));

    AddEntry(TEXT("find_actors_by_name"),
        TEXT("Find actors in the current level by name fragment."),
        ENebulaAIToolRisk::ReadOnly,
        MakeSchema({{TEXT("name"), TEXT("string")}}, {TEXT("name")}),
        TEXT("control_actor"), TEXT("find_by_name"));

    AddEntry(TEXT("get_scene_stats"),
        TEXT("Get scene statistics for the current world."),
        ENebulaAIToolRisk::ReadOnly, MakeSchema({}, {}), TEXT("inspect"), TEXT("get_scene_stats"));

    AddEntry(TEXT("search_assets"),
        TEXT("Search project assets by query and optional path filter."),
        ENebulaAIToolRisk::ReadOnly,
        MakeSchema({{TEXT("query"), TEXT("string")}, {TEXT("path"), TEXT("string")}}, {TEXT("query")}),
        TEXT("manage_asset"), TEXT("search_assets"));

    AddEntry(TEXT("get_asset_dependencies"),
        TEXT("Get the dependency graph for an asset path."),
        ENebulaAIToolRisk::ReadOnly,
        MakeSchema({{TEXT("assetPath"), TEXT("string")}}, {TEXT("assetPath")}),
        TEXT("manage_asset"), TEXT("get_dependencies"));

    AddEntry(TEXT("read_output_log"),
        TEXT("Read recent output log entries (redacted)."),
        ENebulaAIToolRisk::ReadOnly,
        MakeSchema({{TEXT("limit"), TEXT("number")}}, {}),
        TEXT("manage_logs"), TEXT("read"));

    // ---- Mutating catalog (disabled unless Settings allow) ---------------
    AddEntry(TEXT("spawn_actor"),
        TEXT("Spawn an actor in the current level. Requires approval."),
        ENebulaAIToolRisk::Mutating,
        MakeSchema({{TEXT("classPath"), TEXT("string")}, {TEXT("name"), TEXT("string")},
                    {TEXT("location"), TEXT("object")}, {TEXT("rotation"), TEXT("object")}},
                   {TEXT("classPath")}),
        TEXT("control_actor"), TEXT("spawn"));

    AddEntry(TEXT("set_actor_transform"),
        TEXT("Set an actor's world transform. Requires approval."),
        ENebulaAIToolRisk::Mutating,
        MakeSchema({{TEXT("actorName"), TEXT("string")}, {TEXT("location"), TEXT("object")}},
                   {TEXT("actorName")}),
        TEXT("control_actor"), TEXT("set_transform"));

    AddEntry(TEXT("set_component_property"),
        TEXT("Set a property on an actor component. Requires approval."),
        ENebulaAIToolRisk::Mutating,
        MakeSchema({{TEXT("actorName"), TEXT("string")}, {TEXT("componentName"), TEXT("string")},
                    {TEXT("propertyName"), TEXT("string")}, {TEXT("value"), TEXT("object")}},
                   {TEXT("actorName"), TEXT("componentName"), TEXT("propertyName")}),
        TEXT("control_actor"), TEXT("set_component_property"));

    AddEntry(TEXT("delete_actor"),
        TEXT("Delete an actor from the level. Destructive; requires explicit approval."),
        ENebulaAIToolRisk::Destructive,
        MakeSchema({{TEXT("actorName"), TEXT("string")}}, {TEXT("actorName")}),
        TEXT("control_actor"), TEXT("delete"));

    AddEntry(TEXT("save_all_assets"),
        TEXT("Save all dirty packages through the safe-save path. Requires approval."),
        ENebulaAIToolRisk::Mutating, MakeSchema({}, {}),
        TEXT("control_editor"), TEXT("save_all"));

    // ---- Dangerous catalog (disabled unless explicitly enabled) ---------
    AddEntry(TEXT("execute_console_command"),
        TEXT("Execute an Unreal console command. Disabled unless enabled in Settings."),
        ENebulaAIToolRisk::ExternalProcess,
        MakeSchema({{TEXT("command"), TEXT("string")}}, {TEXT("command")}),
        TEXT("system_control"), TEXT("console_command"));

    AddEntry(TEXT("execute_python"),
        TEXT("Execute a Python snippet in the editor. Disabled unless enabled in Settings."),
        ENebulaAIToolRisk::ExternalProcess,
        MakeSchema({{TEXT("code"), TEXT("string")}}, {TEXT("code")}),
        TEXT("system_control"), TEXT("execute_python"));
}

TArray<FNebulaAIToolDefinition> FNebulaForgeAIToolGateway::GetToolCatalog() const
{
    TArray<FNebulaAIToolDefinition> Tools;
    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    if (!Settings || !Settings->Permissions.bProposeToolCalls)
    {
        return Tools;
    }

    for (const TPair<FString, FCatalogEntry>& Pair : Catalog)
    {
        FString DenialReason;
        if (IsToolPermitted(Pair.Key, Pair.Value.Definition.Risk, DenialReason))
        {
            Tools.Add(Pair.Value.Definition);
        }
    }
    return Tools;
}

bool FNebulaForgeAIToolGateway::FindToolDefinition(const FString& ToolName, FNebulaAIToolDefinition& OutDefinition) const
{
    const FCatalogEntry* Entry = Catalog.Find(ToolName);
    if (Entry)
    {
        OutDefinition = Entry->Definition;
        return true;
    }
    return false;
}

void FNebulaForgeAIToolGateway::RequestToolExecution(
    const FString& ConversationId,
    const FString& ToolName,
    const TSharedPtr<FJsonObject>& Arguments,
    TFunction<void(const FToolResult&)> OnComplete)
{
    FNebulaAIToolDefinition Def;
    if (!FindToolDefinition(ToolName, Def))
    {
        FToolResult Result;
        Result.ErrorSummary = FString::Printf(TEXT("Unknown tool '%s'."), *ToolName);
        OnComplete(Result);
        return;
    }

    FString DenialReason;
    if (!IsToolPermitted(ToolName, Def.Risk, DenialReason))
    {
        FToolResult Result;
        Result.ErrorSummary = DenialReason;
        OnComplete(Result);
        return;
    }

    const FNebulaForgeAIService& Service = FNebulaForgeAIService::Get();
    const bool bConversationAllowed =
        Service.Conversations()->IsToolAllowedForConversation(ConversationId, ToolName);
    const bool bNeedsApproval = Def.Risk != ENebulaAIToolRisk::ReadOnly && !bConversationAllowed;
    if (!bNeedsApproval)
    {
        ExecuteThroughRegistry(ToolName, Arguments, OnComplete);
        return;
    }

    FApprovalState State;
    State.CallId = FString::Printf(TEXT("ai-tool-%d"), NextCallId++);
    State.ConversationId = ConversationId;
    State.ToolName = ToolName;
    State.Risk = Def.Risk;
    State.ArgumentsSummary = SummarizeArguments(Arguments);
    State.Arguments = Arguments;
    State.OnComplete = OnComplete;

    const FString CallId = State.CallId;
    PendingApprovals.Add(CallId, MoveTemp(State));
    if (const FApprovalState* Pending = PendingApprovals.Find(CallId))
    {
        OnApprovalRequested.Broadcast(*Pending);
    }
}

void FNebulaForgeAIToolGateway::ResolveApproval(const FString& CallId, EApprovalDecision Decision)
{
    FApprovalState State;
    if (!PendingApprovals.RemoveAndCopyValue(CallId, State))
    {
        return;
    }

    switch (Decision)
    {
    case EApprovalDecision::ApproveOnce:
        ExecuteThroughRegistry(State.ToolName, State.Arguments, State.OnComplete);
        break;
    case EApprovalDecision::AllowForConversation:
        FNebulaForgeAIService::Get().Conversations()->AllowToolForConversation(State.ConversationId, State.ToolName);
        ExecuteThroughRegistry(State.ToolName, State.Arguments, State.OnComplete);
        break;
    case EApprovalDecision::Deny:
    default:
        {
            FToolResult Result;
            Result.ErrorSummary = TEXT("The user denied this tool call.");
            State.OnComplete(Result);
            break;
        }
    }
}

void FNebulaForgeAIToolGateway::CancelPendingApprovals(const FString& ConversationId)
{
    for (TMap<FString, FApprovalState>::TIterator It(PendingApprovals); It; ++It)
    {
        if (It->Value.ConversationId == ConversationId)
        {
            FToolResult Result;
            Result.ErrorSummary = TEXT("Approval was cancelled (conversation closed or request stopped).");
            It->Value.OnComplete(Result);
            It.RemoveCurrent();
        }
    }
}

FText FNebulaForgeAIToolGateway::RiskToDisplayText(ENebulaAIToolRisk Risk)
{
    switch (Risk)
    {
    case ENebulaAIToolRisk::Reversible:
        return NSLOCTEXT("NebulaForgeAI", "RiskReversible", "Reversible");
    case ENebulaAIToolRisk::Mutating:
        return NSLOCTEXT("NebulaForgeAI", "RiskMutating", "Mutating");
    case ENebulaAIToolRisk::Destructive:
        return NSLOCTEXT("NebulaForgeAI", "RiskDestructive", "Destructive");
    case ENebulaAIToolRisk::ExternalProcess:
        return NSLOCTEXT("NebulaForgeAI", "RiskExternalProcess", "External process");
    case ENebulaAIToolRisk::ReadOnly:
    default:
        return NSLOCTEXT("NebulaForgeAI", "RiskReadOnly", "Read-only");
    }
}

bool FNebulaForgeAIToolGateway::IsToolPermitted(
    const FString& ToolName, ENebulaAIToolRisk Risk, FString& OutDenialReason) const
{
    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    if (!Settings)
    {
        OutDenialReason = TEXT("Settings are unavailable.");
        return false;
    }
    const FNebulaAIPermissionSettings& Permissions = Settings->Permissions;
    if (!Permissions.bProposeToolCalls)
    {
        OutDenialReason = TEXT("Tool calls are disabled in Settings.");
        return false;
    }

    auto Deny = [&OutDenialReason](const TCHAR* Reason)
    {
        OutDenialReason = Reason;
        return false;
    };

    switch (Risk)
    {
    case ENebulaAIToolRisk::ReadOnly:
        if (ToolName == TEXT("read_project_info") && !Permissions.bReadProjectMetadata)
        {
            return Deny(TEXT("Reading project metadata is disabled in Settings."));
        }
        if ((ToolName == TEXT("list_actors") || ToolName == TEXT("find_actors_by_name")) &&
            !Permissions.bReadSelection)
        {
            return Deny(TEXT("Reading the current selection is disabled in Settings."));
        }
        if ((ToolName == TEXT("search_assets") || ToolName == TEXT("get_asset_dependencies")) &&
            !Permissions.bReadAssetsAndBlueprints)
        {
            return Deny(TEXT("Reading assets is disabled in Settings."));
        }
        if (ToolName == TEXT("read_output_log") && !Permissions.bReadOutputLog)
        {
            return Deny(TEXT("Reading the output log is disabled in Settings."));
        }
        return true;

    case ENebulaAIToolRisk::Reversible:
        return Permissions.bExecuteApprovedNonDestructiveTools
            ? true
            : Deny(TEXT("Non-destructive tool execution is disabled in Settings."));

    case ENebulaAIToolRisk::Mutating:
        if (!Permissions.bExecuteApprovedMutatingTools)
        {
            return Deny(TEXT("Mutating tool execution is disabled in Settings."));
        }
        if (ToolName == TEXT("save_all_assets") && !Permissions.bWriteFilesOrAssets)
        {
            return Deny(TEXT("Writing files or assets is disabled in Settings."));
        }
        return true;

    case ENebulaAIToolRisk::Destructive:
        return Permissions.bExecuteApprovedMutatingTools && Permissions.bDangerousCapabilitiesAcknowledged
            ? true
            : Deny(TEXT("Destructive tools require explicit Settings enablement plus acknowledgement."));

    case ENebulaAIToolRisk::ExternalProcess:
        if (ToolName == TEXT("execute_console_command") && !Permissions.bExecuteConsoleCommands)
        {
            return Deny(TEXT("Console command execution is disabled in Settings."));
        }
        if (ToolName == TEXT("execute_python") && !Permissions.bExecutePython)
        {
            return Deny(TEXT("Python execution is disabled in Settings."));
        }
        return true;

    default:
        return Deny(TEXT("Unsupported tool risk class."));
    }
}

void FNebulaForgeAIToolGateway::ExecuteThroughRegistry(
    const FString& ToolName, const TSharedPtr<FJsonObject>& Arguments,
    TFunction<void(const FToolResult&)> OnComplete)
{
    // Dispatch exclusively through the existing subsystem registry path
    // (ProcessAutomationRequest + handler map). The gateway never calls
    // handler implementations directly.
    UNebulaForgeBridgeSubsystem* Subsystem = GEditor
        ? GEditor->GetEditorSubsystem<UNebulaForgeBridgeSubsystem>()
        : nullptr;
    if (!Subsystem)
    {
        FToolResult Result;
        Result.ErrorSummary = TEXT("The automation bridge subsystem is unavailable.");
        OnComplete(Result);
        return;
    }

    const FCatalogEntry* Entry = Catalog.Find(ToolName);
    if (!Entry)
    {
        FToolResult Result;
        Result.ErrorSummary = FString::Printf(TEXT("Unknown tool '%s'."), *ToolName);
        OnComplete(Result);
        return;
    }

    // Build the canonical MCP payload: {action: <sub-action>, ...args}.
    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    if (Arguments.IsValid())
    {
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Arguments->Values)
        {
            Payload->SetField(Pair.Key, Pair.Value);
        }
    }
    Payload->SetStringField(TEXT("action"), Entry->SubAction);

    const double StartTime = FPlatformTime::Seconds();
    Subsystem->ExecuteLocalAutomationRequest(Entry->SubsystemAction, Payload, 120.0f,
        [OnComplete, StartTime](bool bSuccess, const FString& Message, const TSharedPtr<FJsonObject>& Result)
        {
            FToolResult ToolResult;
            ToolResult.bSucceeded = bSuccess;
            ToolResult.DurationSeconds = FPlatformTime::Seconds() - StartTime;

            FString ResultJson;
            if (Result.IsValid())
            {
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultJson);
                FJsonSerializer::Serialize(Result, Writer);
            }
            if (ResultJson.IsEmpty())
            {
                ResultJson = Message;
            }
            ToolResult.ResultJson = MoveTemp(ResultJson);
            if (!bSuccess)
            {
                ToolResult.ErrorSummary = Message;
            }
            OnComplete(ToolResult);
        });
}
