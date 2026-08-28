// =============================================================================
// NebulaForgeOpenAIChatCompletionsAdapter.h
// =============================================================================
// OpenAI-compatible /chat/completions transport (plan section 5.2).
//
// Conservative compatibility baseline: bearer auth, messages/model/stream,
// token limits, temperature, and function/tool calling when supported.
// SSE parsing tolerates [DONE], tool-call chunk accumulation, and both
// max_tokens / max_completion_tokens conventions.
// =============================================================================

#pragma once

#include "AI/NebulaForgeAISseParser.h"
#include "AI/Providers/NebulaForgeAIProviderAdapter.h"
#include "HAL/CriticalSection.h"

class FNebulaOpenAIChatCompletionsAdapter : public FNebulaAIHttpTransportBase
{
public:
    FNebulaOpenAIChatCompletionsAdapter(const FNebulaAIProviderProfile& InProfile, const FString& InApiKey);

    //~ INebulaAIProviderTransport
    virtual void SendRequest(const FNebulaAIRequest& Request, const FNebulaAISendContext& Context) override;
    virtual void CancelActive() override;
    virtual void DiscoverModels(TFunction<void(const TArray<FString>&)> OnModels) override;
    virtual void TestConnection(TFunction<void(bool bOk, const FString& Message)> OnResult) override;

private:
    /** Build the request body for the normalized request. */
    FString BuildRequestBody(const FNebulaAIRequest& Request) const;

    /**
     * Parse newly received bytes from the (possibly in-flight) response.
     * Shared by the progress delegate and the completion flush.
     */
    void ProcessStreamedContent(const FHttpRequestPtr& Request);

    /** Feed one SSE payload into the normalized event stream. */
    void HandleSsePayload(const FString& Data, const FNebulaAIRequest& Request, const FNebulaAISendContext& Context);

    /** Parse a complete (non-streamed) response body. */
    void HandleFullJsonBody(const FString& Body, const FNebulaAIRequest& Request, const FNebulaAISendContext& Context);

    /** Build messages array including system prompt and tool results. */
    static void AppendHistoryMessages(const FNebulaAIRequest& Request, TArray<TSharedPtr<FJsonValue>>& OutMessages);

    void EmitTerminal(const FNebulaAIEvent& Event, const FNebulaAISendContext& Context);

    FCriticalSection StateMutex;
    FHttpRequestPtr ActiveRequest;
    FNebulaAISseParser SseParser;
    /** Byte offset of already-parsed response content. */
    int32 ConsumedBytes = 0;
    bool bStreamingParsed = false;
    bool bTerminalEmitted = false;
    bool bCancelRequested = false;
    /** Accumulated tool calls keyed by provider chunk index. */
    TMap<int32, FNebulaAIToolCall> ToolCallAccumulator;
    TFunction<void(const FNebulaAIEvent&)> OnEvent;
    FString ConversationId;
    FString RequestId;
    double StartTimeSeconds = 0.0;
};
