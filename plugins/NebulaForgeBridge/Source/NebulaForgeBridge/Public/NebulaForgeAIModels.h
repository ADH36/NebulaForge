// =============================================================================
// NebulaForgeAIModels.h
// =============================================================================
// Shared data model for the NebulaForge AI chat feature.
//
// These types are intentionally plain C++ (no UObject/Slate dependencies) so
// the conversation/provider/tool services stay testable without a running
// editor world. USTRUCT variants live in NebulaForgeAISettings.h for config
// persistence only.
//
// Editor-only feature: nothing in this header may be referenced from
// packaged/cooked runtime targets.
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** Provider families supported by the chat UI. */
enum class ENebulaAIProviderKind : uint8
{
    /** Official OpenAI endpoint. Defaults to the Responses protocol. */
    OpenAI = 0,
    /** Generic OpenAI-compatible server using Chat Completions. */
    OpenAICompatible = 1,
    /** OpenAI Codex models routed through the Responses API. */
    CodexResponses = 2
};

/** Wire protocol used by a profile's adapter. */
enum class ENebulaAIProtocol : uint8
{
    ChatCompletions = 0,
    Responses = 1
};

/** Chat roles used by the normalized message history. */
enum class ENebulaAIChatRole : uint8
{
    System = 0,
    User = 1,
    Assistant = 2,
    Tool = 3
};

/** Per-message interaction mode. Ask/Plan are read-only by default. */
enum class ENebulaAIInteractionMode : uint8
{
    Ask = 0,
    Plan = 1,
    Act = 2
};

/** Risk classification used by the approval UX. */
enum class ENebulaAIToolRisk : uint8
{
    ReadOnly = 0,
    Reversible = 1,
    Mutating = 2,
    Destructive = 3,
    ExternalProcess = 4
};

/** High-level chat/connection state surfaced to the UI. */
enum class ENebulaAIChatState : uint8
{
    NoProvider = 0,
    MissingSecret = 1,
    TestingConnection = 2,
    Ready = 3,
    Streaming = 4,
    WaitingForApproval = 5,
    ToolExecuting = 6,
    Cancelled = 7,
    Error = 8
};

/** A single tool invocation attached to an assistant message. */
struct FNebulaAIToolCall
{
    FString Id;
    FString Name;
    /** Raw JSON arguments exactly as provided by the model. */
    FString ArgumentsJson;
    /** Human-readable summary of arguments for the approval UI. */
    FString ArgumentsSummary;
    FString ResultJson;
    bool bSucceeded = false;
    double DurationSeconds = 0.0;
    /** One of: pending, approved, denied, executed, failed, skipped. */
    FString ApprovalState;
};

/** One normalized conversation message. */
struct FNebulaAIMessage
{
    FString Id;
    int64 Sequence = 0;
    ENebulaAIChatRole Role = ENebulaAIChatRole::User;
    FString Content;
    /** For Role==Tool: the tool call id this result answers. */
    FString ToolCallId;
    /** Safe, non-chain-of-thought status/reasoning summary. */
    FString StatusSummary;
    TArray<FNebulaAIToolCall> ToolCalls;
    /** Set for error cards. */
    FString ErrorCode;
    FString ErrorMessage;
    FDateTime TimestampUtc = FDateTime::UtcNow();
    ENebulaAIInteractionMode Mode = ENebulaAIInteractionMode::Ask;
};

/** Request tuning options resolved from profile + conversation overrides. */
struct FNebulaAIRequestOptions
{
    float Temperature = -1.0f;          // < 0 = provider default
    int32 MaxOutputTokens = 0;          // 0 = provider default
    bool bStream = true;
    FString ReasoningEffort;            // empty = unsupported/omitted
    float TimeoutSeconds = 120.0f;
    int32 ContextWindowWarningTokens = 0;
};

/** Tool definition exposed to the model. */
struct FNebulaAIToolDefinition
{
    FString Name;
    FString Description;
    /** JSON schema object for the tool parameters. */
    TSharedPtr<FJsonObject> ParametersSchema;
    ENebulaAIToolRisk Risk = ENebulaAIToolRisk::ReadOnly;
};

/** Token usage reported by the provider (when available). */
struct FNebulaAIUsage
{
    int32 PromptTokens = 0;
    int32 CompletionTokens = 0;
    int32 TotalTokens = 0;
    bool bValid = false;
};

/** Structured provider error. */
struct FNebulaAIError
{
    FString Code;
    FString Message;
    int32 HttpStatus = 0;
    /** Rate limited / timeout / unavailable classification helpers. */
    bool bRateLimited() const { return HttpStatus == 429 || Code == TEXT("rate_limit_exceeded"); }
    bool bTimeout() const { return Code == TEXT("TIMEOUT"); }
    bool bUnreachable() const { return HttpStatus == 0 || HttpStatus >= 502; }
};

/** Fully normalized provider request. */
struct FNebulaAIRequest
{
    FString ConversationId;
    FString Model;
    FString SystemInstructions;
    TArray<FNebulaAIMessage> Messages;
    TArray<FNebulaAIToolDefinition> Tools;
    FNebulaAIRequestOptions Options;
};

/** Normalized streaming event emitted by provider adapters. */
enum class ENebulaAIEventType : uint8
{
    TextDelta = 0,
    StatusDelta = 1,
    ToolCallStarted = 2,
    ToolCallDelta = 3,
    ToolCallCompleted = 4,
    Usage = 5,
    Completed = 6,
    Failed = 7,
    Cancelled = 8
};

struct FNebulaAIEvent
{
    ENebulaAIEventType Type = ENebulaAIEventType::TextDelta;
    FString ConversationId;
    FString RequestId;
    FString TextDelta;
    FString StatusDelta;
    FNebulaAIToolCall ToolCall;
    FNebulaAIUsage Usage;
    FNebulaAIError Error;
    FString FinishReason;
    FString ProviderRequestId;
    double LatencySeconds = 0.0;
};

/** Context chips gathered from the editor before a send. */
struct FNebulaAIContextChip
{
    FString Id;
    FString Label;
    FString PayloadJson;
    int32 EstimatedTokens = 0;
};

/** Conversation metadata (messages are owned by the conversation service). */
struct FNebulaAIConversationMeta
{
    FString Id;
    FString Title;
    FDateTime CreatedUtc;
    FDateTime LastActivityUtc;
    FString ProviderProfileId;
    FString Model;
    int64 NextSequence = 1;
    bool bPinned = false;
};

/** Diagnostics for the most recent provider request. */
struct FNebulaAIRequestDiagnostics
{
    FString RequestId;
    FString ProviderRequestId;
    int32 HttpStatus = 0;
    double LatencySeconds = 0.0;
    FNebulaAIUsage Usage;
    FString LastError;
    FDateTime TimestampUtc = FDateTime::UtcNow();
};

/** Convert a provider kind to a stable config string. */
NEBULAFORGEBRIDGE_API const TCHAR* NebulaAIProviderKindToString(ENebulaAIProviderKind Kind);
NEBULAFORGEBRIDGE_API ENebulaAIProviderKind NebulaAIProviderKindFromString(const FString& InValue, ENebulaAIProviderKind Fallback);
NEBULAFORGEBRIDGE_API const TCHAR* NebulaAIProtocolToString(ENebulaAIProtocol Protocol);
NEBULAFORGEBRIDGE_API ENebulaAIProtocol NebulaAIProtocolFromString(const FString& InValue, ENebulaAIProtocol Fallback);
NEBULAFORGEBRIDGE_API const TCHAR* NebulaAIToolRiskToString(ENebulaAIToolRisk Risk);
NEBULAFORGEBRIDGE_API FString NebulaAIInteractionModeToString(ENebulaAIInteractionMode Mode);
