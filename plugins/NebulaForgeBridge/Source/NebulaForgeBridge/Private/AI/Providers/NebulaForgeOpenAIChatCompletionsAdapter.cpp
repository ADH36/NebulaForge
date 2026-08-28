#include "AI/Providers/NebulaForgeOpenAIChatCompletionsAdapter.h"
#include "AI/NebulaForgeAIDiagnostics.h"
#include "AI/NebulaForgeAISecretStore.h"
#include "Async/Async.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "HAL/PlatformTime.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FNebulaOpenAIChatCompletionsAdapter::FNebulaOpenAIChatCompletionsAdapter(
    const FNebulaAIProviderProfile& InProfile, const FString& InApiKey)
    : FNebulaAIHttpTransportBase(InProfile, InApiKey)
{
}

void FNebulaOpenAIChatCompletionsAdapter::SendRequest(const FNebulaAIRequest& Request, const FNebulaAISendContext& Context)
{
    {
        FScopeLock Lock(&StateMutex);
        if (ActiveRequest.IsValid())
        {
            FNebulaAIEvent Error;
            Error.Type = ENebulaAIEventType::Failed;
            Error.ConversationId = Request.ConversationId;
            Error.Error.Code = TEXT("REQUEST_IN_FLIGHT");
            Error.Error.Message = TEXT("A request is already active for this adapter.");
            Context.OnEvent(Error);
            return;
        }

        ActiveRequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        RequestId = ActiveRequestId;
        ConversationId = Request.ConversationId;
        OnEvent = Context.OnEvent;
        SseParser.Reset();
        ConsumedBytes = 0;
        bStreamingParsed = false;
        bTerminalEmitted = false;
        bCancelRequested = false;
        ToolCallAccumulator.Reset();
        StartTimeSeconds = FPlatformTime::Seconds();
    }

    const FString Url = BuildUrl(TEXT("/chat/completions"));
    const FString Body = BuildRequestBody(Request);
    FHttpRequestPtr HttpRequest = CreateConfiguredRequest(Url, TEXT("POST"), Body);
    {
        FScopeLock Lock(&StateMutex);
        ActiveRequest = HttpRequest;
    }

#if NEBULA_AI_HTTP_PROGRESS64
    HttpRequest->OnRequestProgress64().BindLambda(
        [this](FHttpRequestPtr Req, uint64 /*Sent*/, uint64 /*Received*/)
        {
            ProcessStreamedContent(Req);
        });
#else
    HttpRequest->OnRequestProgress().BindLambda(
        [this](FHttpRequestPtr Req, FHttpResponsePtr /*Response*/, int32 /*Sent*/, int32 /*Received*/)
        {
            ProcessStreamedContent(Req);
        });
#endif

    HttpRequest->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bSucceeded)
        {
            const FNebulaAISendContext Context{ OnEvent };

            // Flush any remaining streamed bytes.
            if (!bTerminalEmitted)
            {
                ProcessStreamedContent(Req);
            }

            {
                FScopeLock Lock(&StateMutex);
                ActiveRequest = nullptr;
            }

            if (bTerminalEmitted)
            {
                return;
            }

            if (bCancelRequested)
            {
                FNebulaAIEvent Cancelled;
                Cancelled.Type = ENebulaAIEventType::Cancelled;
                Cancelled.ConversationId = ConversationId;
                Cancelled.RequestId = RequestId;
                EmitTerminal(Cancelled, Context);
                return;
            }

            if (!bSucceeded || !Response.IsValid())
            {
                FNebulaAIEvent Failed;
                Failed.Type = ENebulaAIEventType::Failed;
                Failed.ConversationId = ConversationId;
                Failed.RequestId = RequestId;
                Failed.Error.Code = TEXT("TIMEOUT");
                Failed.Error.Message = TEXT("The provider request timed out or could not be completed.");
                if (Response.IsValid())
                {
                    Failed.Error = ParseErrorBody(Response->GetResponseCode(), Response->GetContentAsString());
                }
                Failed.LatencySeconds = FPlatformTime::Seconds() - StartTimeSeconds;
                LogTransportError(RequestId, Failed.Error.Message);
                EmitTerminal(Failed, Context);
                return;
            }

            const int32 Status = Response->GetResponseCode();
            if (Status < 200 || Status >= 300)
            {
                FNebulaAIEvent Failed;
                Failed.Type = ENebulaAIEventType::Failed;
                Failed.ConversationId = ConversationId;
                Failed.RequestId = RequestId;
                Failed.Error = ParseErrorBody(Status, Response->GetContentAsString());
                Failed.LatencySeconds = FPlatformTime::Seconds() - StartTimeSeconds;
                LogTransportError(RequestId, Failed.Error.Message);
                EmitTerminal(Failed, Context);
                return;
            }

            // Server returned a complete JSON body (stream not honored).
            const FString Body = Response->GetContentAsString();
            if (bStreamingParsed)
            {
                // SSE stream ended without [DONE]; treat as complete.
                FNebulaAIEvent Completed;
                Completed.Type = ENebulaAIEventType::Completed;
                Completed.ConversationId = ConversationId;
                Completed.RequestId = RequestId;
                Completed.ProviderRequestId = Response->GetHeader(TEXT("x-request-id"));
                Completed.LatencySeconds = FPlatformTime::Seconds() - StartTimeSeconds;
                EmitTerminal(Completed, Context);
            }
            else
            {
                FNebulaAIRequest Unused;
                HandleFullJsonBody(Body, Unused, Context);
            }
        });

    if (!HttpRequest->ProcessRequest())
    {
        FScopeLock Lock(&StateMutex);
        ActiveRequest = nullptr;
        FNebulaAIEvent Failed;
        Failed.Type = ENebulaAIEventType::Failed;
        Failed.ConversationId = Request.ConversationId;
        Failed.RequestId = RequestId;
        Failed.Error.Code = TEXT("REQUEST_FAILED");
        Failed.Error.Message = TEXT("Failed to start the HTTP request.");
        bTerminalEmitted = true;
        Context.OnEvent(Failed);
    }
}

void FNebulaOpenAIChatCompletionsAdapter::ProcessStreamedContent(const FHttpRequestPtr& Request)
{
    const FHttpResponsePtr Response = Request.IsValid() ? Request->GetResponse() : nullptr;
    if (!Response.IsValid())
    {
        return;
    }

    const TArray<uint8>& Content = Response->GetContent();
    int32 LocalConsumed = 0;
    {
        FScopeLock Lock(&StateMutex);
        if (bTerminalEmitted)
        {
            return;
        }
        LocalConsumed = ConsumedBytes;
    }
    if (Content.Num() <= LocalConsumed)
    {
        return;
    }

    FString Chunk(UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Content.GetData()) + LocalConsumed),
                  Content.Num() - LocalConsumed);
    {
        FScopeLock Lock(&StateMutex);
        ConsumedBytes = Content.Num();
        bStreamingParsed = true;
    }

    const FNebulaAISendContext Context{ OnEvent };
    for (const FNebulaAISseParser::FParsedEvent& Event : SseParser.Feed(Chunk))
    {
        if (bTerminalEmitted)
        {
            break;
        }
        if (Event.bDone)
        {
            FNebulaAIEvent Completed;
            Completed.Type = ENebulaAIEventType::Completed;
            Completed.ConversationId = ConversationId;
            Completed.RequestId = RequestId;
            Completed.LatencySeconds = FPlatformTime::Seconds() - StartTimeSeconds;
            EmitTerminal(Completed, Context);
            return;
        }
        if (!Event.Data.IsEmpty())
        {
            FNebulaAIRequest Unused;
            HandleSsePayload(Event.Data, Unused, Context);
        }
    }
}

void FNebulaOpenAIChatCompletionsAdapter::CancelActive()
{
    FScopeLock Lock(&StateMutex);
    if (ActiveRequest.IsValid())
    {
        bCancelRequested = true;
        ActiveRequest->CancelRequest();
    }
}

void FNebulaOpenAIChatCompletionsAdapter::DiscoverModels(TFunction<void(const TArray<FString>&)> OnModels)
{
    FHttpRequestPtr Request = CreateConfiguredRequest(BuildUrl(TEXT("/models")), TEXT("GET"), FString());
    Request->OnProcessRequestComplete().BindLambda(
        [this, OnModels](FHttpRequestPtr /*Req*/, FHttpResponsePtr Response, bool bSucceeded)
        {
            TArray<FString> Models;
            if (bSucceeded && Response.IsValid() && Response->GetResponseCode() >= 200 && Response->GetResponseCode() < 300)
            {
                TSharedPtr<FJsonObject> Root;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
                if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
                {
                    const TArray<TSharedPtr<FJsonValue>>* Data = nullptr;
                    if (Root->TryGetArrayField(TEXT("data"), Data))
                    {
                        for (const TSharedPtr<FJsonValue>& Item : *Data)
                        {
                            const TSharedPtr<FJsonObject>* ItemObj = nullptr;
                            if (Item.IsValid() && Item->TryGetObject(ItemObj) && ItemObj && ItemObj->IsValid())
                            {
                                FString Id = (*ItemObj)->GetStringField(TEXT("id"));
                                if (!Id.IsEmpty())
                                {
                                    Models.Add(MoveTemp(Id));
                                }
                            }
                        }
                    }
                }
            }
            AsyncTask(ENamedThreads::GameThread, [OnModels, Models]()
            {
                OnModels(Models);
            });
        });
    Request->ProcessRequest();
}

void FNebulaOpenAIChatCompletionsAdapter::TestConnection(TFunction<void(bool bOk, const FString& Message)> OnResult)
{
    FHttpRequestPtr Request = CreateConfiguredRequest(BuildUrl(TEXT("/models")), TEXT("GET"), FString());
    Request->OnProcessRequestComplete().BindLambda(
        [this, OnResult](FHttpRequestPtr /*Req*/, FHttpResponsePtr Response, bool bSucceeded)
        {
            bool bOk = false;
            FString Message;
            if (!bSucceeded || !Response.IsValid())
            {
                Message = TEXT("Request failed or timed out. Check the base URL and network access.");
            }
            else
            {
                const int32 Code = Response->GetResponseCode();
                if (Code >= 200 && Code < 300)
                {
                    bOk = true;
                    Message = TEXT("Connected successfully.");
                }
                else if (Code == 401 || Code == 403)
                {
                    Message = TEXT("Authentication failed. Check the API key and organization settings.");
                }
                else
                {
                    Message = FString::Printf(TEXT("Endpoint returned HTTP %d."), Code);
                }
            }
            AsyncTask(ENamedThreads::GameThread, [OnResult, bOk, Message]()
            {
                OnResult(bOk, Message);
            });
        });
    Request->ProcessRequest();
}

FString FNebulaOpenAIChatCompletionsAdapter::BuildRequestBody(const FNebulaAIRequest& Request) const
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("model"), Request.Model);
    Root->SetBoolField(TEXT("stream"), Request.Options.bStream);

    if (Request.Options.Temperature >= 0.0f)
    {
        Root->SetNumberField(TEXT("temperature"), Request.Options.Temperature);
    }
    if (Request.Options.MaxOutputTokens > 0)
    {
        // max_completion_tokens is the newer field; keep max_tokens as the
        // conservative default for generic compatible servers.
        if (Profile.bPreferMaxCompletionTokens)
        {
            Root->SetNumberField(TEXT("max_completion_tokens"), Request.Options.MaxOutputTokens);
        }
        else
        {
            Root->SetNumberField(TEXT("max_tokens"), Request.Options.MaxOutputTokens);
        }
    }

    TArray<TSharedPtr<FJsonValue>> Messages;
    if (!Request.SystemInstructions.IsEmpty())
    {
        TSharedRef<FJsonObject> SystemMsg = MakeShared<FJsonObject>();
        SystemMsg->SetStringField(TEXT("role"), TEXT("system"));
        SystemMsg->SetStringField(TEXT("content"), Request.SystemInstructions);
        Messages.Add(MakeShared<FJsonValueObject>(SystemMsg));
    }
    AppendHistoryMessages(Request, Messages);
    Root->SetArrayField(TEXT("messages"), Messages);

    if (!Request.Tools.IsEmpty())
    {
        TArray<TSharedPtr<FJsonValue>> Tools;
        for (const FNebulaAIToolDefinition& Tool : Request.Tools)
        {
            TSharedRef<FJsonObject> Function = MakeShared<FJsonObject>();
            Function->SetStringField(TEXT("name"), Tool.Name);
            Function->SetStringField(TEXT("description"), Tool.Description);
            if (Tool.ParametersSchema.IsValid())
            {
                Function->SetObjectField(TEXT("parameters"), Tool.ParametersSchema);
            }
            else
            {
                Function->SetObjectField(TEXT("parameters"), MakeShared<FJsonObject>());
            }
            TSharedRef<FJsonObject> ToolEntry = MakeShared<FJsonObject>();
            ToolEntry->SetStringField(TEXT("type"), TEXT("function"));
            ToolEntry->SetObjectField(TEXT("function"), Function);
            Tools.Add(MakeShared<FJsonValueObject>(ToolEntry));
        }
        Root->SetArrayField(TEXT("tools"), Tools);
    }

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Root, Writer);
    return Out;
}

void FNebulaOpenAIChatCompletionsAdapter::AppendHistoryMessages(
    const FNebulaAIRequest& Request, TArray<TSharedPtr<FJsonValue>>& OutMessages)
{
    for (const FNebulaAIMessage& Msg : Request.Messages)
    {
        switch (Msg.Role)
        {
        case ENebulaAIChatRole::System:
        {
            TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("role"), TEXT("system"));
            Entry->SetStringField(TEXT("content"), Msg.Content);
            OutMessages.Add(MakeShared<FJsonValueObject>(Entry));
            break;
        }
        case ENebulaAIChatRole::User:
        {
            TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("role"), TEXT("user"));
            Entry->SetStringField(TEXT("content"), Msg.Content);
            OutMessages.Add(MakeShared<FJsonValueObject>(Entry));
            break;
        }
        case ENebulaAIChatRole::Assistant:
        {
            TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("role"), TEXT("assistant"));
            Entry->SetStringField(TEXT("content"), Msg.Content);
            TArray<TSharedPtr<FJsonValue>> ToolJson;
            for (const FNebulaAIToolCall& Call : Msg.ToolCalls)
            {
                if (Call.Id.IsEmpty())
                {
                    continue;
                }
                // Replay every model-produced call; results are replayed as
                // tool-role messages keyed by tool_call_id below.
                TSharedRef<FJsonObject> Function = MakeShared<FJsonObject>();
                Function->SetStringField(TEXT("name"), Call.Name);
                Function->SetStringField(TEXT("arguments"), Call.ArgumentsJson);
                TSharedRef<FJsonObject> CallEntry = MakeShared<FJsonObject>();
                CallEntry->SetStringField(TEXT("id"), Call.Id);
                CallEntry->SetStringField(TEXT("type"), TEXT("function"));
                CallEntry->SetObjectField(TEXT("function"), Function);
                ToolJson.Add(MakeShared<FJsonValueObject>(CallEntry));
            }
            if (ToolJson.Num() > 0)
            {
                Entry->SetArrayField(TEXT("tool_calls"), ToolJson);
            }
            OutMessages.Add(MakeShared<FJsonValueObject>(Entry));
            break;
        }
        case ENebulaAIChatRole::Tool:
        {
            TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("role"), TEXT("tool"));
            Entry->SetStringField(TEXT("tool_call_id"), Msg.ToolCallId);
            Entry->SetStringField(TEXT("content"), Msg.Content);
            OutMessages.Add(MakeShared<FJsonValueObject>(Entry));
            break;
        }
        default:
            break;
        }
    }
}

void FNebulaOpenAIChatCompletionsAdapter::HandleSsePayload(
    const FString& Data, const FNebulaAIRequest& Request, const FNebulaAISendContext& Context)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        // Ignore keep-alive/malformed fragments; compatibility baseline.
        return;
    }

    const TSharedPtr<FJsonObject>* UsageObj = nullptr;
    if (Root->TryGetObjectField(TEXT("usage"), UsageObj) && UsageObj && UsageObj->IsValid())
    {
        FNebulaAIEvent Usage;
        Usage.Type = ENebulaAIEventType::Usage;
        Usage.ConversationId = ConversationId;
        Usage.RequestId = RequestId;
        Usage.Usage.PromptTokens = static_cast<int32>((*UsageObj)->GetNumberField(TEXT("prompt_tokens")));
        Usage.Usage.CompletionTokens = static_cast<int32>((*UsageObj)->GetNumberField(TEXT("completion_tokens")));
        Usage.Usage.TotalTokens = static_cast<int32>((*UsageObj)->GetNumberField(TEXT("total_tokens")));
        Usage.Usage.bValid = true;
        Context.OnEvent(Usage);
    }

    const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
    if (!Root->TryGetArrayField(TEXT("choices"), Choices) || !Choices || Choices->Num() == 0)
    {
        return;
    }

    const TSharedPtr<FJsonObject>* ChoiceObj = nullptr;
    if (!(*Choices)[0]->TryGetObject(ChoiceObj) || !ChoiceObj || !ChoiceObj->IsValid())
    {
        return;
    }

    FNebulaAIEvent Event;
    Event.ConversationId = ConversationId;
    Event.RequestId = RequestId;

    const TSharedPtr<FJsonObject>* Delta = nullptr;
    if ((*ChoiceObj)->TryGetObjectField(TEXT("delta"), Delta) && Delta && Delta->IsValid())
    {
        const TSharedPtr<FJsonValue> ContentField = (*Delta)->TryGetField(TEXT("content"));
        if (ContentField.IsValid() && !ContentField->IsNull())
        {
            const FString Text = ContentField->AsString();
            if (!Text.IsEmpty())
            {
                Event.Type = ENebulaAIEventType::TextDelta;
                Event.TextDelta = Text;
                Context.OnEvent(Event);
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* ToolChunks = nullptr;
        if ((*Delta)->TryGetArrayField(TEXT("tool_calls"), ToolChunks) && ToolChunks)
        {
            for (const TSharedPtr<FJsonValue>& ChunkValue : *ToolChunks)
            {
                const TSharedPtr<FJsonObject>* ChunkObj = nullptr;
                if (!ChunkValue.IsValid() || !ChunkValue->TryGetObject(ChunkObj) || !ChunkObj || !ChunkObj->IsValid())
                {
                    continue;
                }
                const int32 Index = static_cast<int32>((*ChunkObj)->GetNumberField(TEXT("index")));
                FNebulaAIToolCall& Call = ToolCallAccumulator.FindOrAdd(Index);
                const FString ChunkId = (*ChunkObj)->GetStringField(TEXT("id"));
                if (!ChunkId.IsEmpty())
                {
                    Call.Id = ChunkId;
                }
                const TSharedPtr<FJsonObject>* FnObj = nullptr;
                if ((*ChunkObj)->TryGetObjectField(TEXT("function"), FnObj) && FnObj && FnObj->IsValid())
                {
                    const FString Name = (*FnObj)->GetStringField(TEXT("name"));
                    if (!Name.IsEmpty())
                    {
                        Call.Name = Name;
                    }
                    Call.ArgumentsJson += (*FnObj)->GetStringField(TEXT("arguments"));
                }
            }
        }
    }

    const FString FinishReason = (*ChoiceObj)->GetStringField(TEXT("finish_reason"));
    if (!FinishReason.IsEmpty())
    {
        if (FinishReason == TEXT("tool_calls"))
        {
            for (TPair<int32, FNebulaAIToolCall>& Pair : ToolCallAccumulator)
            {
                FNebulaAIEvent ToolEvent;
                ToolEvent.Type = ENebulaAIEventType::ToolCallCompleted;
                ToolEvent.ConversationId = ConversationId;
                ToolEvent.RequestId = RequestId;
                ToolEvent.ToolCall = Pair.Value;
                ToolEvent.ToolCall.ApprovalState = TEXT("pending");
                Context.OnEvent(ToolEvent);
            }
            ToolCallAccumulator.Reset();
        }
        else
        {
            Event.Type = ENebulaAIEventType::Completed;
            Event.FinishReason = FinishReason;
            Event.LatencySeconds = FPlatformTime::Seconds() - StartTimeSeconds;
            EmitTerminal(Event, Context);
        }
    }
}

void FNebulaOpenAIChatCompletionsAdapter::HandleFullJsonBody(
    const FString& Body, const FNebulaAIRequest& Request, const FNebulaAISendContext& Context)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        FNebulaAIEvent Failed;
        Failed.Type = ENebulaAIEventType::Failed;
        Failed.ConversationId = ConversationId;
        Failed.RequestId = RequestId;
        Failed.Error.Code = TEXT("MALFORMED_RESPONSE");
        Failed.Error.Message = TEXT("Provider returned a malformed response body.");
        EmitTerminal(Failed, Context);
        return;
    }

    if (Root->HasField(TEXT("error")))
    {
        FNebulaAIEvent Failed;
        Failed.Type = ENebulaAIEventType::Failed;
        Failed.ConversationId = ConversationId;
        Failed.RequestId = RequestId;
        Failed.Error = ParseErrorBody(200, Body);
        EmitTerminal(Failed, Context);
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
    if (Root->TryGetArrayField(TEXT("choices"), Choices) && Choices && Choices->Num() > 0)
    {
        const TSharedPtr<FJsonObject>* ChoiceObj = nullptr;
        if ((*Choices)[0]->TryGetObject(ChoiceObj) && ChoiceObj && ChoiceObj->IsValid())
        {
            const TSharedPtr<FJsonObject>* Message = nullptr;
            if ((*ChoiceObj)->TryGetObjectField(TEXT("message"), Message) && Message && Message->IsValid())
            {
                const FString Content = (*Message)->GetStringField(TEXT("content"));
                if (!Content.IsEmpty())
                {
                    FNebulaAIEvent Delta;
                    Delta.Type = ENebulaAIEventType::TextDelta;
                    Delta.ConversationId = ConversationId;
                    Delta.RequestId = RequestId;
                    Delta.TextDelta = Content;
                    Context.OnEvent(Delta);
                }
                const TArray<TSharedPtr<FJsonValue>>* ToolCalls = nullptr;
                if ((*Message)->TryGetArrayField(TEXT("tool_calls"), ToolCalls) && ToolCalls)
                {
                    for (const TSharedPtr<FJsonValue>& Value : *ToolCalls)
                    {
                        const TSharedPtr<FJsonObject>* Entry = nullptr;
                        const TSharedPtr<FJsonObject>* Fn = nullptr;
                        if (Value.IsValid() && Value->TryGetObject(Entry) && Entry &&
                            (*Entry)->TryGetObjectField(TEXT("function"), Fn) && Fn)
                        {
                            FNebulaAIEvent ToolEvent;
                            ToolEvent.Type = ENebulaAIEventType::ToolCallCompleted;
                            ToolEvent.ConversationId = ConversationId;
                            ToolEvent.RequestId = RequestId;
                            ToolEvent.ToolCall.Id = (*Entry)->GetStringField(TEXT("id"));
                            ToolEvent.ToolCall.Name = (*Fn)->GetStringField(TEXT("name"));
                            ToolEvent.ToolCall.ArgumentsJson = (*Fn)->GetStringField(TEXT("arguments"));
                            ToolEvent.ToolCall.ApprovalState = TEXT("pending");
                            Context.OnEvent(ToolEvent);
                        }
                    }
                }
            }
        }
    }

    FNebulaAIEvent Completed;
    Completed.Type = ENebulaAIEventType::Completed;
    Completed.ConversationId = ConversationId;
    Completed.RequestId = RequestId;
    Completed.ProviderRequestId = Root->GetStringField(TEXT("id"));
    Completed.LatencySeconds = FPlatformTime::Seconds() - StartTimeSeconds;
    EmitTerminal(Completed, Context);
}

void FNebulaOpenAIChatCompletionsAdapter::EmitTerminal(const FNebulaAIEvent& Event, const FNebulaAISendContext& Context)
{
    {
        FScopeLock Lock(&StateMutex);
        if (bTerminalEmitted)
        {
            return;
        }
        bTerminalEmitted = true;
    }
    Context.OnEvent(Event);
}
