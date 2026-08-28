// =============================================================================
// NebulaForgeAIProviderAdapter.h
// =============================================================================
// Common provider transport interface plus a shared HTTP base class.
//
// The adapter owns HTTP, authentication, streaming, response normalization,
// and provider error mapping (plan section 4.1). Adapters emit normalized
// FNebulaAIEvent values through a callback that must be safe to call from
// any thread; the request coordinator marshals events to the game thread.
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IHttpRequest.h"
#include "NebulaForgeAIModels.h"
#include "NebulaForgeAISettings.h"
#include "Runtime/Launch/Resources/Version.h"

// UE 5.8 replaced IHttpRequest::OnRequestProgress (32-bit, response param)
// with OnRequestProgress64 (uint64, response via Request->GetResponse()).
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8
#define NEBULA_AI_HTTP_PROGRESS64 1
#else
#define NEBULA_AI_HTTP_PROGRESS64 0
#endif

/** Handle to an in-flight provider request. */
class INebulaAIProviderTransport;

struct FNebulaAISendContext
{
    /** Event callback; may be invoked from the HTTP thread. */
    TFunction<void(const FNebulaAIEvent&)> OnEvent;
};

class INebulaAIProviderTransport
{
public:
    virtual ~INebulaAIProviderTransport() = default;

    /**
     * Send a normalized request. Events (deltas, tool calls, usage,
     * completion, failure) are delivered through Context.OnEvent. Exactly one
     * terminal event (Completed/Failed/Cancelled) is emitted per call.
     */
    virtual void SendRequest(const FNebulaAIRequest& Request, const FNebulaAISendContext& Context) = 0;

    /** Cancel the active request; a Cancelled event is emitted if one is active. */
    virtual void CancelActive() = 0;

    /**
     * Best-effort model discovery. Invokes OnModels on the game thread with
     * discovered ids; OnModels receives an empty array on failure.
     */
    virtual void DiscoverModels(TFunction<void(const TArray<FString>&)> OnModels) = 0;

    /** Small request used by Test Connection; invokes OnResult on game thread. */
    virtual void TestConnection(TFunction<void(bool bOk, const FString& Message)> OnResult) = 0;
};

/**
 * Shared HTTP plumbing for OpenAI-style transports: URL join, bearer auth,
 * org/project headers, custom headers (with sensitive names blocked from
 * logs), timeouts, and SSE accumulation through the progress callback.
 */
class FNebulaAIHttpTransportBase : public INebulaAIProviderTransport
{
public:
    FNebulaAIHttpTransportBase(const FNebulaAIProviderProfile& InProfile, const FString& InApiKey);

protected:
    /** Ensure https scheme and no trailing slash on the stored base URL. */
    static FString NormalizeBaseUrl(const FString& BaseUrl);

    /** Build endpoint URL: base (+ /v1 when missing) + suffix. */
    FString BuildUrl(const FString& EndpointSuffix) const;

    /** Create a configured request (auth + headers + timeout). */
    FHttpRequestPtr CreateConfiguredRequest(const FString& Url, const FString& Verb, const FString& BodyJson);

    /** Apply profile headers; sensitive values are never logged. */
    void ApplyHeaders(class IHttpRequest& Request) const;

    /** Log a transport failure with redaction. */
    void LogTransportError(const FString& RequestId, const FString& Message) const;

    /** Parse a JSON error body into the normalized error struct. */
    static FNebulaAIError ParseErrorBody(int32 HttpStatus, const FString& Body);

    const FNebulaAIProviderProfile Profile;
    FString ApiKey;
    FString ActiveRequestId;
};
