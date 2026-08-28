#include "AI/Providers/NebulaForgeAIProviderAdapter.h"
#include "AI/NebulaForgeAIDiagnostics.h"
#include "AI/NebulaForgeAISecretStore.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Guid.h"

FNebulaAIHttpTransportBase::FNebulaAIHttpTransportBase(const FNebulaAIProviderProfile& InProfile, const FString& InApiKey)
    : Profile(InProfile)
    , ApiKey(InApiKey)
{
}

FString FNebulaAIHttpTransportBase::NormalizeBaseUrl(const FString& BaseUrl)
{
    FString Url = BaseUrl;
    Url.TrimStartAndEnd();
    while (Url.EndsWith(TEXT("/")))
    {
        Url.LeftChopInline(1);
    }
    if (!Url.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase) &&
        !Url.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
    {
        Url = TEXT("https://") + Url;
    }
    return Url;
}

FString FNebulaAIHttpTransportBase::BuildUrl(const FString& EndpointSuffix) const
{
    FString Base = NormalizeBaseUrl(Profile.BaseUrl);
    // Append /v1 when the user provided a bare host (common for compatible
    // servers such as http://localhost:8000). Official URLs already end in /v1.
    if (!Base.EndsWith(TEXT("/v1"), ESearchCase::IgnoreCase))
    {
        Base += TEXT("/v1");
    }
    return Base + EndpointSuffix;
}

FHttpRequestPtr FNebulaAIHttpTransportBase::CreateConfiguredRequest(const FString& Url, const FString& Verb, const FString& BodyJson)
{
    FHttpRequestPtr Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Url);
    Request->SetVerb(Verb);
    if (!BodyJson.IsEmpty())
    {
        Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        Request->SetContentAsString(BodyJson);
    }
    ApplyHeaders(*Request);

    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    const float Timeout = Settings ? FMath::Max(5.0f, Settings->Model.RequestTimeoutSeconds) : 120.0f;
    Request->SetTimeout(Timeout);

    return Request;
}

void FNebulaAIHttpTransportBase::ApplyHeaders(class IHttpRequest& Request) const
{
    if (!ApiKey.IsEmpty())
    {
        Request.SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
    }
    if (!Profile.OrganizationId.IsEmpty())
    {
        Request.SetHeader(TEXT("OpenAI-Organization"), Profile.OrganizationId);
    }
    if (!Profile.ProjectId.IsEmpty())
    {
        Request.SetHeader(TEXT("OpenAI-Project"), Profile.ProjectId);
    }

    if (!Profile.CustomHeadersJson.IsEmpty())
    {
        TSharedPtr<FJsonObject> Headers;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Profile.CustomHeadersJson);
        if (FJsonSerializer::Deserialize(Reader, Headers) && Headers.IsValid())
        {
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Headers->Values)
            {
                const FString Value = Pair.Value->AsString();
                // Sensitive header names are applied but never logged.
                Request.SetHeader(Pair.Key, Value);
            }
        }
        else
        {
            UE_LOG(LogNebulaForgeAI, Warning,
                TEXT("Profile %s has malformed CustomHeadersJson; ignoring custom headers."),
                *Profile.DisplayName);
        }
    }
}

void FNebulaAIHttpTransportBase::LogTransportError(const FString& RequestId, const FString& Message) const
{
    UE_LOG(LogNebulaForgeAI, Error, TEXT("Provider transport error (%s / profile %s): %s"),
        *RequestId, *Profile.DisplayName, *FNebulaAIDiagnostics::RedactText(Message));
}

FNebulaAIError FNebulaAIHttpTransportBase::ParseErrorBody(int32 HttpStatus, const FString& Body)
{
    FNebulaAIError Error;
    Error.HttpStatus = HttpStatus;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
    if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
    {
        const TSharedPtr<FJsonObject>* ErrorObj = nullptr;
        if (Root->TryGetObjectField(TEXT("error"), ErrorObj) && ErrorObj && ErrorObj->IsValid())
        {
            Error.Code = (*ErrorObj)->GetStringField(TEXT("type"));
            if (Error.Code.IsEmpty())
            {
                Error.Code = (*ErrorObj)->GetStringField(TEXT("code"));
            }
            Error.Message = (*ErrorObj)->GetStringField(TEXT("message"));
        }
        else
        {
            Error.Message = Root->GetStringField(TEXT("message"));
            Error.Code = Root->GetStringField(TEXT("code"));
        }
    }
    if (Error.Message.IsEmpty())
    {
        Error.Message = FString::Printf(TEXT("HTTP %d"), HttpStatus);
    }
    if (Error.Code.IsEmpty())
    {
        Error.Code = FString::Printf(TEXT("HTTP_%d"), HttpStatus);
    }
    return Error;
}
