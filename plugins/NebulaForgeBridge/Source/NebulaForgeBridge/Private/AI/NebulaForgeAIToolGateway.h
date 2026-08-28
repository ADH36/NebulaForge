// =============================================================================
// NebulaForgeAIToolGateway.h
// =============================================================================
// Permission checks, approval state, and dispatch for AI-requested Unreal
// operations (plan section 6.2/6.3).
//
// Approved calls are executed exclusively through the existing subsystem
// registry path (ProcessAutomationRequest / handler map), never by calling
// handler implementation functions directly. Dangerous capabilities stay
// disabled unless explicitly enabled in Settings.
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "NebulaForgeAIModels.h"

class FNebulaForgeAIToolGateway
{
public:
    /** A tool proposal awaiting an approval decision. */
    struct FPendingApproval
    {
        FString CallId;
        FString ConversationId;
        FString ToolName;
        ENebulaAIToolRisk Risk = ENebulaAIToolRisk::ReadOnly;
        FString ArgumentsSummary;
        TSharedPtr<FJsonObject> Arguments;
    };

    /** Approval decisions the UI can return. */
    enum class EApprovalDecision : uint8
    {
        ApproveOnce = 0,
        AllowForConversation = 1,
        Deny = 2
    };

    /** Result of an executed tool call. */
    struct FToolResult
    {
        bool bSucceeded = false;
        FString ResultJson;
        FString ErrorSummary;
        double DurationSeconds = 0.0;
    };

    /** Called when a tool requires an interactive approval decision. */
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnApprovalRequested, const FPendingApproval&);
    FOnApprovalRequested OnApprovalRequested;

    FNebulaForgeAIToolGateway();

    /** Tool catalog filtered by current permissions; exposed to the model. */
    TArray<FNebulaAIToolDefinition> GetToolCatalog() const;

    /** Find a catalog entry by tool name. */
    bool FindToolDefinition(const FString& ToolName, FNebulaAIToolDefinition& OutDefinition) const;

    /**
     * Begin processing a tool call from the model. Read-only tools execute
     * immediately (when permitted). Others emit OnApprovalRequested.
     * OnComplete is invoked on the game thread.
     */
    void RequestToolExecution(
        const FString& ConversationId,
        const FString& ToolName,
        const TSharedPtr<FJsonObject>& Arguments,
        TFunction<void(const FToolResult&)> OnComplete);

    /** Resolve a pending approval; executes or skips the tool. */
    void ResolveApproval(const FString& CallId, EApprovalDecision Decision);

    /** Drop stale approvals (conversation deleted, request cancelled). */
    void CancelPendingApprovals(const FString& ConversationId);

    /** True when any approval is outstanding (pauses the request timeout). */
    bool HasPendingApprovals() const { return PendingApprovals.Num() > 0; }

    static FText RiskToDisplayText(ENebulaAIToolRisk Risk);

private:
    struct FApprovalState : public FPendingApproval
    {
        TFunction<void(const FToolResult&)> OnComplete;
    };

    /** Catalog entry pairing the AI-facing definition with its dispatch target. */
    struct FCatalogEntry
    {
        FNebulaAIToolDefinition Definition;
        /** Canonical parent tool action, e.g. "control_actor". */
        FString SubsystemAction;
        /** Sub-action passed in the payload, e.g. "spawn". */
        FString SubAction;
    };

    /** Check settings-based permission for a tool's risk class. */
    bool IsToolPermitted(const FString& ToolName, ENebulaAIToolRisk Risk, FString& OutDenialReason) const;

    /** Execute through the subsystem registry path. */
    void ExecuteThroughRegistry(const FString& ToolName, const TSharedPtr<FJsonObject>& Arguments,
        TFunction<void(const FToolResult&)> OnComplete);

    /** Curated catalog entries (name -> entry). */
    TMap<FString, FCatalogEntry> Catalog;

    TMap<FString, FApprovalState> PendingApprovals;
    int32 NextCallId = 1;
};
