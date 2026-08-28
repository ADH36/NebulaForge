#include "AI/NebulaForgeAIRequestCoordinator.h"
#include "AI/NebulaForgeAIConversationService.h"
#include "AI/NebulaForgeAIDiagnostics.h"
#include "AI/NebulaForgeAISecretStore.h"
#include "AI/NebulaForgeAIService.h"
#include "AI/NebulaForgeAISettings.h"
#include "AI/NebulaForgeAIProviderService.h"
#include "AI/NebulaForgeAIToolGateway.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Misc/PlatformTime.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FNebulaForgeAIRequestCoordinator::FNebulaForgeAIRequestCoordinator() = default;

void FNebulaForgeAIRequestCoordinator::Tick(float /*DeltaTime*/)
{
    // Drain queued adapter events on the game thread.
    TArray<FQueuedEvent> Drained;
    {
        FScopeLock Lock(&EventQueueMutex);
        Drained = MoveTemp(EventQueue);
        EventQueue.Reset();
    }
    for (const FQueuedEvent& Queued : Drained)
    {
        HandleEvent(Queued.Event);
    }

    // Timeout watchdog. Approval waits pause the clock; the user decides,
    // and the gateway owns that wait.
    if (ActiveState.bActive && !ActiveState.bWaitingForApproval &&
        !FNebulaForgeAIService::Get().Tools()->HasPendingApprovals())
    {
        const double Elapsed = FPlatformTime::Seconds() - RequestStartTime;
        if (Elapsed > static_cast<double>(TimeoutSeconds))
        {
            if (Transport.IsValid())
            {
                Transport->CancelActive();
            }
            FNebulaAIEvent TimeoutEvent;
            TimeoutEvent.Type = ENebulaAIEventType::Failed;
            TimeoutEvent.ConversationId = ActiveState.ConversationId;
            TimeoutEvent.Error.Code = TEXT("TIMEOUT");
            TimeoutEvent.Error.Message =
                FString::Printf(TEXT("The request exceeded the %.0f second timeout and was cancelled."),
                                TimeoutSeconds);
            HandleEvent(TimeoutEvent);
        }
    }
}

void FNebulaForgeAIRequestCoordinator::SendUserMessage(
    const FString& ConversationId,
    const FString& UserText,
    ENebulaAIInteractionMode Mode,
    TFunction<void(const FString&)> OnSendFailed)
{
    if (ActiveState.bActive)
    {
        Async(EAsyncExecution::TaskGraphMainThread, [OnSendFailed]()
        {
            OnSendFailed(TEXT("A request is already in flight. Stop it before sending another message."));
        });
        return;
    }

    // Record the user message first; failures append an error card.
    FNebulaAIMessage UserMessage;
    UserMessage.Role = ENebulaAIChatRole::User;
    UserMessage.Content = UserText;
    UserMessage.Mode = Mode;
    UserMessage.TimestampUtc = FDateTime::UtcNow();
    FNebulaAIMessage& Stored =
        FNebulaForgeAIService::Get().Conversations()->AppendMessage(ConversationId, MoveTemp(UserMessage));
    OnMessageAdded.Broadcast(ConversationId, Stored);

    ContinueConversation(ConversationId);
}

void FNebulaForgeAIRequestCoordinator::Cancel(const FString& ConversationId)
{
    if (!ActiveState.bActive || ActiveState.ConversationId != ConversationId)
    {
        return;
    }

    FNebulaForgeAIService::Get().Tools()->CancelPendingApprovals(ConversationId);
    if (Transport.IsValid())
    {
        Transport->CancelActive();
    }
    else
    {
        FinishActive(TEXT("Cancelled"));
    }
}

bool FNebulaForgeAIRequestCoordinator::IsBusy(const FString& ConversationId) const
{
    return ActiveState.bActive && ActiveState.ConversationId == ConversationId;
}

void FNebulaForgeAIRequestCoordinator::ContinueConversation(const FString& ConversationId)
{
    FString Error;
    FNebulaAIRequest Request;
    if (!BuildRequest(ConversationId, Request, Error))
    {
        FNebulaAIMessage ErrorCard;
        ErrorCard.Role = ENebulaAIChatRole::Assistant;
        ErrorCard.ErrorCode = TEXT("SEND_FAILED");
        ErrorCard.ErrorMessage = Error;
        ErrorCard.TimestampUtc = FDateTime::UtcNow();
        FNebulaAIMessage& Stored =
            FNebulaForgeAIService::Get().Conversations()->AppendMessage(ConversationId, MoveTemp(ErrorCard));
        OnMessageAdded.Broadcast(ConversationId, Stored);
        FinishActive(TEXT("Error"));
        return;
    }

    Aggregation = FStreamAggregation();
    ActiveState.bActive = true;
    ActiveState.bWaitingForApproval = false;
    ActiveState.ConversationId = ConversationId;
    ActiveState.StatusText = TEXT("Streaming...");
    ActiveState.LastDiagnostics = FNebulaAIRequestDiagnostics();
    RequestStartTime = FPlatformTime::Seconds();
    TimeoutSeconds = Request.Options.TimeoutSeconds;
    bTimeoutEmitted = false;
    OnStateChanged.Broadcast();

    FNebulaAISendContext Context;
    Context.OnEvent = [this](const FNebulaAIEvent& Event)
    {
        // May be called from the HTTP thread; marshal through the queue.
        FScopeLock Lock(&EventQueueMutex);
        EventQueue.Add(FQueuedEvent{ Event });
    };

    Transport->SendRequest(Request, Context);
}

bool FNebulaForgeAIRequestCoordinator::BuildRequest(
    const FString& ConversationId, FNebulaAIRequest& OutRequest, FString& OutError) const
{
    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    const FNebulaAIProviderProfile* Profile = Settings ? Settings->GetActiveProfile() : nullptr;
    if (!Settings || !Profile)
    {
        OutError = TEXT("No provider profile is configured. Open Settings to add one and test the connection.");
        return false;
    }

    FString TransportError;
    Transport = FNebulaForgeAIProviderService::CreateTransport(Profile->Id, TransportError);
    if (!Transport.IsValid())
    {
        // Clear the dangling shared pointer state on failure.
        OutError = TransportError;
        return false;
    }

    TSharedPtr<FNebulaForgeAIConversationService::FConversation> Conversation =
        FNebulaForgeAIService::Get().Conversations()->FindConversation(ConversationId);
    if (!Conversation.IsValid())
    {
        OutError = TEXT("The conversation no longer exists.");
        return false;
    }

    OutRequest.ConversationId = ConversationId;
    OutRequest.Model = Profile->DefaultModel;
    OutRequest.SystemInstructions = Settings->Model.SystemInstructions.IsEmpty()
        ? FNebulaForgeAIProviderService::GetDefaultSystemInstructions(
              static_cast<ENebulaAIProviderKind>(static_cast<uint8>(Profile->Kind)))
        : Settings->Model.SystemInstructions;
    OutRequest.Messages = Conversation->Messages;
    OutRequest.Tools = FNebulaForgeAIService::Get().Tools()->GetToolCatalog();

    OutRequest.Options.Temperature = Settings->Model.Temperature;
    OutRequest.Options.MaxOutputTokens = Settings->Model.MaxOutputTokens;
    OutRequest.Options.bStream = Settings->Model.bStream;
    OutRequest.Options.TimeoutSeconds = Settings->Model.RequestTimeoutSeconds;
    OutRequest.Options.ContextWindowWarningTokens = Settings->Model.ContextWindowWarningTokens;
    if (FNebulaForgeAIProviderService::SupportsReasoningEffort(
            static_cast<ENebulaAIProviderKind>(static_cast<uint8>(Profile->Kind))))
    {
        OutRequest.Options.ReasoningEffort = Settings->Model.ReasoningEffort;
    }
    return true;
}

void FNebulaForgeAIRequestCoordinator::HandleEvent(const FNebulaAIEvent& Event)
{
    if (!ActiveState.bActive && Event.Type != ENebulaAIEventType::Failed)
    {
        return;
    }

    switch (Event.Type)
    {
    case ENebulaAIEventType::TextDelta:
    case ENebulaAIEventType::StatusDelta:
    case ENebulaAIEventType::ToolCallStarted:
    case ENebulaAIEventType::ToolCallDelta:
    case ENebulaAIEventType::ToolCallCompleted:
    case ENebulaAIEventType::Usage:
        ApplyEventToConversation(Event);
        break;

    case ENebulaAIEventType::Completed:
        ApplyEventToConversation(Event);
        if (Aggregation.ToolCalls.Num() > 0)
        {
            // Tool round-trip: stay active and execute queued calls; the
            // final completion re-issues the provider request with results.
            StartToolExecutionQueue(Event.ConversationId);
        }
        else
        {
            FinishActive(TEXT("Ready"));
        }
        break;

    case ENebulaAIEventType::Cancelled:
        ApplyEventToConversation(Event);
        FinishActive(TEXT("Cancelled"));
        break;

    case ENebulaAIEventType::Failed:
        ApplyEventToConversation(Event);
        FinishActive(TEXT("Error"));
        break;

    default:
        break;
    }
}

void FNebulaForgeAIRequestCoordinator::ApplyEventToConversation(const FNebulaAIEvent& Event)
{
    FNebulaForgeAIConversationService& Conversations = *FNebulaForgeAIService::Get().Conversations();
    TSharedPtr<FNebulaForgeAIConversationService::FConversation> Conversation =
        Conversations.FindConversation(Event.ConversationId);
    if (!Conversation.IsValid())
    {
        return;
    }

    const bool bNewAssistantMessage = Aggregation.AssistantMessageId.IsEmpty();

    switch (Event.Type)
    {
    case ENebulaAIEventType::TextDelta:
        Aggregation.Text += Event.TextDelta;
        break;
    case ENebulaAIEventType::StatusDelta:
        Aggregation.StatusSummary = Event.StatusDelta;
        break;
    case ENebulaAIEventType::ToolCallCompleted:
        Aggregation.ToolCalls.Add(Event.ToolCall);
        break;
    case ENebulaAIEventType::Usage:
        ActiveState.LastDiagnostics.Usage = Event.Usage;
        ActiveState.LastDiagnostics.LatencySeconds = Event.LatencySeconds;
        ActiveState.LastDiagnostics.TimestampUtc = FDateTime::UtcNow();
        break;
    case ENebulaAIEventType::Completed:
    case ENebulaAIEventType::Cancelled:
    case ENebulaAIEventType::Failed:
        // Terminal bookkeeping handled below.
        break;
    default:
        break;
    }

    // Persist/emit the aggregated assistant message.
    if (bNewAssistantMessage)
    {
        FNebulaAIMessage Assistant;
        Assistant.Role = ENebulaAIChatRole::Assistant;
        Assistant.Content = Aggregation.Text;
        Assistant.StatusSummary = Aggregation.StatusSummary;
        Assistant.ToolCalls = Aggregation.ToolCalls;
        Assistant.TimestampUtc = FDateTime::UtcNow();
        // Match the mode of the last user message for UI display.
        for (int32 Index = Conversation->Messages.Num() - 1; Index >= 0; --Index)
        {
            if (Conversation->Messages[Index].Role == ENebulaAIChatRole::User)
            {
                Assistant.Mode = Conversation->Messages[Index].Mode;
                break;
            }
        }
        FNebulaAIMessage& Stored = Conversations.AppendMessage(Event.ConversationId, Assistant);
        Aggregation.AssistantMessageId = Stored.Id;
        if (Aggregation.Text.IsEmpty() && Aggregation.ToolCalls.Num() == 0 &&
            Aggregation.StatusSummary.IsEmpty())
        {
            // Skip notifying for the empty placeholder; first real update
            // will refresh it.
            OnMessageAdded.Broadcast(Event.ConversationId, Stored);
            return;
        }
        OnMessageAdded.Broadcast(Event.ConversationId, Stored);
    }
    else
    {
        // Update the existing aggregated message in place (no full rewrite).
        for (FNebulaAIMessage& Message : Conversation->Messages)
        {
            if (Message.Id == Aggregation.AssistantMessageId)
            {
                Message.Content = Aggregation.Text;
                Message.StatusSummary = Aggregation.StatusSummary;
                Message.ToolCalls = Aggregation.ToolCalls;
                OnMessageUpdated.Broadcast(Event.ConversationId, Message);
                break;
            }
        }
    }

    // Terminal states: attach error info / trigger tool execution.
    if (Event.Type == ENebulaAIEventType::Failed || Event.Type == ENebulaAIEventType::Cancelled)
    {
        for (FNebulaAIMessage& Message : Conversation->Messages)
        {
            if (Message.Id == Aggregation.AssistantMessageId)
            {
                if (Event.Type == ENebulaAIEventType::Failed)
                {
                    Message.ErrorCode = Event.Error.Code;
                    Message.ErrorMessage = Event.Error.Message;
                }
                else
                {
                    Message.StatusSummary = TEXT("Cancelled by user.");
                }
                OnMessageUpdated.Broadcast(Event.ConversationId, Message);
                break;
            }
        }
    }
}

void FNebulaForgeAIRequestCoordinator::StartToolExecutionQueue(const FString& ConversationId)
{
    PendingToolExecutions = Aggregation.ToolCalls;
    ActiveState.StatusText = TEXT("Tool calls pending...");
    ActiveState.bWaitingForApproval = true;
    OnStateChanged.Broadcast();
    ExecuteNextQueuedTool(ConversationId);
}

void FNebulaForgeAIRequestCoordinator::ExecuteNextQueuedTool(const FString& ConversationId)
{
    if (PendingToolExecutions.Num() == 0)
    {
        ActiveState.bWaitingForApproval = false;
        ActiveState.StatusText = TEXT("Streaming...");
        ContinueConversation(ConversationId);
        return;
    }

    const FNebulaAIToolCall Call = PendingToolExecutions[0];
    PendingToolExecutions.RemoveAt(0);
    ExecuteToolCall(ConversationId, Call);
}

void FNebulaForgeAIRequestCoordinator::ExecuteToolCall(
    const FString& ConversationId, const FNebulaAIToolCall& Call)
{
    ActiveState.StatusText = FString::Printf(TEXT("Executing tool: %s"), *Call.Name);
    ActiveState.bWaitingForApproval = false;
    OnStateChanged.Broadcast();

    TSharedPtr<FJsonObject> Arguments;
    if (!Call.ArgumentsJson.IsEmpty())
    {
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Call.ArgumentsJson);
        TSharedPtr<FJsonObject> Parsed;
        if (FJsonSerializer::Deserialize(Reader, Parsed))
        {
            Arguments = Parsed;
        }
    }
    if (!Arguments.IsValid())
    {
        Arguments = MakeShared<FJsonObject>();
    }

    FNebulaForgeAIToolGateway& Gateway = *FNebulaForgeAIService::Get().Tools();
    Gateway.RequestToolExecution(ConversationId, Call.Name, Arguments,
        [this, ConversationId, Call](const FNebulaForgeAIToolGateway::FToolResult& Result)
        {
            // Record the tool result as a Tool message (game thread).
            const FString ResultContent = Result.bSucceeded
                ? Result.ResultJson
                : (Result.ErrorSummary.IsEmpty() ? TEXT("Tool failed.") : Result.ErrorSummary);
            FNebulaAIMessage ToolMessage;
            ToolMessage.Role = ENebulaAIChatRole::Tool;
            ToolMessage.ToolCallId = Call.Id;
            ToolMessage.Content = ResultContent;
            ToolMessage.TimestampUtc = FDateTime::UtcNow();
            FNebulaAIMessage& Stored =
                FNebulaForgeAIService::Get().Conversations()->AppendMessage(ConversationId, MoveTemp(ToolMessage));
            OnMessageAdded.Broadcast(ConversationId, Stored);

            // Stamp the result on the assistant's tool call entry as well.
            TSharedPtr<FNebulaForgeAIConversationService::FConversation> Conversation =
                FNebulaForgeAIService::Get().Conversations()->FindConversation(ConversationId);
            if (Conversation.IsValid())
            {
                for (FNebulaAIMessage& Message : Conversation->Messages)
                {
                    if (Message.Id == Aggregation.AssistantMessageId)
                    {
                        for (FNebulaAIToolCall& StoredCall : Message.ToolCalls)
                        {
                            if (StoredCall.Id == Call.Id)
                            {
                                StoredCall.ResultJson = ResultContent;
                                StoredCall.bSucceeded = Result.bSucceeded;
                                StoredCall.DurationSeconds = Result.DurationSeconds;
                                StoredCall.ApprovalState = Result.bSucceeded ? TEXT("executed") : TEXT("failed");
                                break;
                            }
                        }
                        OnMessageUpdated.Broadcast(ConversationId, Message);
                        break;
                    }
                }
            }

            // Continue with any remaining queued tool calls, then re-issue
            // the provider request with all tool results attached.
            ExecuteNextQueuedTool(ConversationId);
        });
}

void FNebulaForgeAIRequestCoordinator::FinishActive(const TCHAR* Status)
{
    ActiveState.bActive = false;
    ActiveState.bWaitingForApproval = false;
    ActiveState.StatusText = Status;
    Aggregation = FStreamAggregation();
    PendingToolExecutions.Reset();
    Transport.Reset();
    OnStateChanged.Broadcast();
}
