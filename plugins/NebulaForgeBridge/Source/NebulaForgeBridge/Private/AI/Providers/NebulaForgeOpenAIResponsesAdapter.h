// =============================================================================
// NebulaForgeOpenAIResponsesAdapter.h
// =============================================================================
// Official OpenAI Responses API transport (plan sections 5.3/5.4).
//
// Used by OpenAI (Responses protocol) and Codex profiles. Codex models such
// as GPT-5-Codex are agentic-coding models exposed through the Responses
// endpoint, so they must never be routed through Chat Completions.
//
// v1 support: text in/out, instructions, local-history continuation,
// function/tool calls, cancellation, timeout, usage/request-id display,
// store=false by default, reasoning effort passthrough when configured.
// =============================================================================

#pragma once

#include "AI/NebulaForgeAISseParser.h"
#include "AI/Providers/NebulaForgeAIProviderAdapter.h"
#include "HAL/CriticalSection.h"

class FNebulaOpenAIResponsesAdapter : public FNebulaAIHttpTransportBase
{
public:
    FNebulaOpenAIResponsesAdapter(const FNebulaAIProviderProfile& InProfile, const FString& InApiKey);

    //~ INebulaAIProviderTransport
    virtual void SendRequest(const FNebulaAIRequest& Request, const FNebulaAISendContext& Context) override;
    virtual void CancelActive() override;
    virtual void DiscoverModels(TFunction<void(const TArray<FString>&)> OnModels) override;
    virtual void TestConnection(TFunction<void(bool bOk, const FString& Message)> OnResult) override;

private:
    FString BuildRequestBody(const FNebulaAIRequest& Request) const;

    /**
     * Parse newly received bytes from the (possibly in-flight) response.
     * Shared by the progress delegate and the completion flush.
     */
    void ProcessStreamedContent(const FHttpRequestPtr& Request);

    /** Normalize one Responses SSE event into the internal event stream. */
    void HandleResponsesEvent(const FString& EventName, const FString& Data, const FNebulaAISendContext& Context);

    /** Parse a complete (non-streamed) Responses JSON body. */
    void HandleFullJsonBody(const FString& Body, const FNebulaAISendContext& Context);

    /** Extract usage from response.completed/completed payloads or root. */
    static void ExtractUsage(const TSharedPtr<FJsonObject>& Root, FNebulaAIUsage& OutUsage);

    void EmitTerminal(const FNebulaAIEvent& Event, const FNebulaAISendContext& Context);

    FCriticalSection StateMutex;
    FHttpRequestPtr ActiveRequest;
    FNebulaAISseParser SseParser;
    int32 ConsumedBytes = 0;
    bool bStreamingParsed = false;
    bool bTerminalEmitted = false;
    bool bCancelRequested = false;
    /** function_call item being accumulated; keyed by output index. */
    TMap<int32, FNebulaAIToolCall> PendingFunctionCalls;
    TFunction<void(const FNebulaAIEvent&)> OnEvent;
    FString ConversationId;
    FString RequestId;
    double StartTimeSeconds = 0.0;
};
