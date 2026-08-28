// =============================================================================
// NebulaForgeAIRequestCoordinator.h
// =============================================================================
// Cancellation, streaming, retry, timeout, and concurrency for provider
// requests (plan section 4.2).
//
// Events emitted by adapters (possibly off the game thread) are queued and
// drained on the game thread by the coordinator ticker, so Slate is never
// touched from HTTP callbacks. Retry never replays an approved mutation
// without a fresh approval decision.
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "NebulaForgeAIModels.h"
#include "AI/Providers/NebulaForgeAIProviderAdapter.h"

class FNebulaForgeAIRequestCoordinator
{
public:
    /** State of the active request for UI display. */
    struct FActiveState
    {
        bool bActive = false;
        bool bWaitingForApproval = false;
        FString ConversationId;
        FString StatusText;
        FNebulaAIRequestDiagnostics LastDiagnostics;
    };

    DECLARE_MULTICAST_DELEGATE(FOnStateChanged);
    FOnStateChanged OnStateChanged;

    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMessageUpdated, const FString& /*ConversationId*/, const FNebulaAIMessage& /*Message*/);
    FOnMessageUpdated OnMessageUpdated;

    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMessageAdded, const FString& /*ConversationId*/, const FNebulaAIMessage& /*Message*/);
    FOnMessageAdded OnMessageAdded;

    FNebulaForgeAIRequestCoordinator();

    /** Drain queued adapter events; call from a game-thread ticker. */
    void Tick(float DeltaTime);

    /**
     * Send a user message (context chips are already part of the text).
     * Invokes OnSendFailed(message) on the game thread when the request
     * cannot start.
     */
    void SendUserMessage(
        const FString& ConversationId,
        const FString& UserText,
        ENebulaAIInteractionMode Mode,
        TFunction<void(const FString&)> OnSendFailed);

    /** Cancel the active request for a conversation. */
    void Cancel(const FString& ConversationId);

    /** True when a request is active for the conversation. */
    bool IsBusy(const FString& ConversationId) const;

    const FActiveState& GetActiveState() const { return ActiveState; }

private:
    struct FQueuedEvent
    {
        FNebulaAIEvent Event;
    };

    /** Build and start the next provider request for the conversation. */
    void ContinueConversation(const FString& ConversationId);

    /** Convert conversation history into a normalized request. */
    bool BuildRequest(const FString& ConversationId, FNebulaAIRequest& OutRequest, FString& OutError);

    /** Handle one drained event on the game thread. */
    void HandleEvent(const FNebulaAIEvent& Event);

    /** Append/update messages based on a drained event; notifies UI. */
    void ApplyEventToConversation(const FNebulaAIEvent& Event);

    /** Execute a proposed tool call through the gateway (approval-gated). */
    void ExecuteToolCall(const FString& ConversationId, const FNebulaAIToolCall& Call);

    /** Mark the active request finished and notify. */
    void FinishActive(const TCHAR* Status);

    /** Aggregate assistant streaming state for the active request. */
    struct FStreamAggregation
    {
        FString AssistantMessageId;
        FString Text;
        FString StatusSummary;
        TArray<FNebulaAIToolCall> ToolCalls;
    };

    /** Execute queued tool calls from the completed assistant turn. */
    void StartToolExecutionQueue(const FString& ConversationId);
    void ExecuteNextQueuedTool(const FString& ConversationId);

    TSharedPtr<INebulaAIProviderTransport> Transport;
    FCriticalSection EventQueueMutex;
    TArray<FQueuedEvent> EventQueue;
    FActiveState ActiveState;
    FStreamAggregation Aggregation;
    /** Tool calls awaiting sequential execution for the active turn. */
    TArray<FNebulaAIToolCall> PendingToolExecutions;
    double RequestStartTime = 0.0;
    float TimeoutSeconds = 120.0f;
    bool bTimeoutEmitted = false;
};
