#include "AI/Providers/NebulaForgeOpenAIResponsesAdapter.h"
#include "AI/NebulaForgeAIDiagnostics.h"
#include "AI/NebulaForgeAISecretStore.h"
#include "Async/Async.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Guid.h"
#include "Misc/PlatformTime.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FNebulaOpenAIResponsesAdapter::FNebulaOpenAIResponsesAdapter(
    const FNebulaAIProviderProfile& InProfile, const FString& InApiKey)
    : FNebulaAIHttpTransportBase(InProfile, InApiKey)
{
}

void FNebulaOpenAIResponsesAdapter::SendRequest(const FNebulaAIRequest& Request, const FNebulaAISendContext& Context)
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
        PendingFunctionCalls.Reset();
        StartTimeSeconds = FPlatformTime::Seconds();
    }

    const FString Url = BuildUrl(TEXT("/responses"));
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
                LogTransportError(RequestId, Failed.Error.Message);
                EmitTerminal(Failed, Context);
                return;
            }

            if (bStreamingParsed)
            {
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
                HandleFullJsonBody(Response->GetContentAsString(), Context);
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

void FNebulaOpenAIResponsesAdapter::ProcessStreamedContent(const FHttpRequestPtr& Request)
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
            HandleResponsesEvent(Event.EventName, Event.Data, Context);
        }
    }
}

void FNebulaOpenAIResponsesAdapter::CancelActive()
{
    FScopeLock Lock(&StateMutex);
    if (ActiveRequest.IsValid())
    {
        bCancelRequested = true;
        ActiveRequest->CancelRequest();
    }
}

void FNebulaOpenAIResponsesAdapter::DiscoverModels(TFunction<void(const TArray<FString>&)> OnModels)
{
    // The Responses endpoint shares model discovery with /v1/models.
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

void FNebulaOpenAIResponsesAdapter::TestConnection(TFunction<void(bool bOk, const FString& Message)> OnResult)
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
                    Message = TEXT("Authentication failed. Check the API key.");
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

FString FNebulaOpenAIResponsesAdapter::BuildRequestBody(const FNebulaAIRequest& Request) const
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("model"), Request.Model);
    if (!Request.SystemInstructions.IsEmpty())
    {
        Root->SetStringField(TEXT("instructions"), Request.SystemInstructions);
    }
    Root->SetBoolField(TEXT("stream"), Request.Options.bStream);
    // Privacy default: do not store responses provider-side unless the user
    // opts in later. Kept off unconditionally for v1 (plan section 5.3).
    Root->SetBoolField(TEXT("store"), false);

    if (Request.Options.MaxOutputTokens > 0)
    {
        Root->SetNumberField(TEXT("max_output_tokens"), Request.Options.MaxOutputTokens);
    }
    if (!Request.Options.ReasoningEffort.IsEmpty())
    {
        TSharedRef<FJsonObject> Reasoning = MakeShared<FJsonObject>();
        Reasoning->SetStringField(TEXT("effort"), Request.Options.ReasoningEffort.ToLower());
        Root->SetObjectField(TEXT("reasoning"), Reasoning);
    }

    // Local-history continuation: replay the conversation as input items.
    TArray<TSharedPtr<FJsonValue>> Input;
    for (const FNebulaAIMessage& Msg : Request.Messages)
    {
        switch (Msg.Role)
        {
        case ENebulaAIChatRole::User:
        {
            TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("role"), TEXT("user"));
            TSharedRef<FJsonObject> TextPart = MakeShared<FJsonObject>();
            TextPart->SetStringField(TEXT("type"), TEXT("input_text"));
            TextPart->SetStringField(TEXT("text"), Msg.Content);
            TArray<TSharedPtr<FJsonValue>> Parts;
            Parts.Add(MakeShared<FJsonValueObject>(TextPart));
            Item->SetArrayField(TEXT("content"), Parts);
            Input.Add(MakeShared<FJsonValueObject>(Item));
            break;
        }
        case ENebulaAIChatRole::Assistant:
        {
            if (!Msg.Content.IsEmpty())
            {
                TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
                Item->SetStringField(TEXT("type"), TEXT("message"));
                Item->SetStringField(TEXT("role"), TEXT("assistant"));
                TSharedRef<FJsonObject> TextPart = MakeShared<FJsonObject>();
                TextPart->SetStringField(TEXT("type"), TEXT("output_text"));
                TextPart->SetStringField(TEXT("text"), Msg.Content);
                TArray<TSharedPtr<FJsonValue>> Parts;
                Parts.Add(MakeShared<FJsonValueObject>(TextPart));
                Item->SetArrayField(TEXT("content"), Parts);
                Input.Add(MakeShared<FJsonValueObject>(Item));
            }
            for (const FNebulaAIToolCall& Call : Msg.ToolCalls)
            {
                if (Call.Id.IsEmpty())
                {
                    continue;
                }
                TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
                Item->SetStringField(TEXT("type"), TEXT("function_call"));
                Item->SetStringField(TEXT("call_id"), Call.Id);
                Item->SetStringField(TEXT("name"), Call.Name);
                Item->SetStringField(TEXT("arguments"), Call.ArgumentsJson);
                Input.Add(MakeShared<FJsonValueObject>(Item));
            }
            break;
        }
        case ENebulaAIChatRole::Tool:
        {
            TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("type"), TEXT("function_call_output"));
            Item->SetStringField(TEXT("call_id"), Msg.ToolCallId);
            Item->SetStringField(TEXT("output"), Msg.Content);
            Input.Add(MakeShared<FJsonValueObject>(Item));
            break;
        }
        case ENebulaAIChatRole::System:
        default:
            // System guidance is delivered via instructions, not input items.
            break;
        }
    }
    Root->SetArrayField(TEXT("input"), Input);

    if (!Request.Tools.IsEmpty())
    {
        TArray<TSharedPtr<FJsonValue>> Tools;
        for (const FNebulaAIToolDefinition& Tool : Request.Tools)
        {
            TSharedRef<FJsonObject> ToolEntry = MakeShared<FJsonObject>();
            ToolEntry->SetStringField(TEXT("type"), TEXT("function"));
            ToolEntry->SetStringField(TEXT("name"), Tool.Name);
            ToolEntry->SetStringField(TEXT("description"), Tool.Description);
            ToolEntry->SetObjectField(TEXT("parameters"),
                Tool.ParametersSchema.IsValid() ? Tool.ParametersSchema : MakeShared<FJsonObject>());
            Tools.Add(MakeShared<FJsonValueObject>(ToolEntry));
        }
        Root->SetArrayField(TEXT("tools"), Tools);
    }

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Root, Writer);
    return Out;
}

void FNebulaOpenAIResponsesAdapter::HandleResponsesEvent(
    const FString& EventName, const FString& Data, const FNebulaAISendContext& Context)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        return;
    }

    FNebulaAIEvent Event;
    Event.ConversationId = ConversationId;
    Event.RequestId = RequestId;

    if (EventName == TEXT("response.output_text.delta"))
    {
        Event.Type = ENebulaAIEventType::TextDelta;
        Event.TextDelta = Root->GetStringField(TEXT("delta"));
        Context.OnEvent(Event);
        return;
    }

    if (EventName == TEXT("response.reasoning_summary_text.delta"))
    {
        // Safe status summaries only; never expose chain-of-thought.
        Event.Type = ENebulaAIEventType::StatusDelta;
        Event.StatusDelta = TEXT("Reasoning...");
        Context.OnEvent(Event);
        return;
    }

    if (EventName == TEXT("response.output_item.added"))
    {
        const TSharedPtr<FJsonObject>* Item = nullptr;
        if (Root->TryGetObjectField(TEXT("item"), Item) && Item && Item->IsValid())
        {
            if ((*Item)->GetStringField(TEXT("type")) == TEXT("function_call"))
            {
                FNebulaAIToolCall Call;
                Call.Id = (*Item)->GetStringField(TEXT("call_id"));
                Call.Name = (*Item)->GetStringField(TEXT("name"));
                Call.ApprovalState = TEXT("pending");
                PendingFunctionCalls.Add(
                    static_cast<int32>(Root->GetNumberField(TEXT("output_index"))), MoveTemp(Call));
            }
        }
        return;
    }

    if (EventName == TEXT("response.function_call_arguments.delta"))
    {
        FNebulaAIToolCall* Call = PendingFunctionCalls.Find(
            static_cast<int32>(Root->GetNumberField(TEXT("output_index"))));
        if (Call)
        {
            Call->ArgumentsJson += Root->GetStringField(TEXT("delta"));
        }
        return;
    }

    if (EventName == TEXT("response.output_item.done"))
    {
        const TSharedPtr<FJsonObject>* Item = nullptr;
        if (Root->TryGetObjectField(TEXT("item"), Item) && Item && Item->IsValid() &&
            (*Item)->GetStringField(TEXT("type")) == TEXT("function_call"))
        {
            FNebulaAIToolCall Call;
            Call.Id = (*Item)->GetStringField(TEXT("call_id"));
            Call.Name = (*Item)->GetStringField(TEXT("name"));
            Call.ArgumentsJson = (*Item)->GetStringField(TEXT("arguments"));
            if (Call.ArgumentsJson.IsEmpty())
            {
                if (const FNebulaAIToolCall* Pending = PendingFunctionCalls.Find(
                        static_cast<int32>(Root->GetNumberField(TEXT("output_index")))))
                {
                    Call.ArgumentsJson = Pending->ArgumentsJson;
                }
            }
            Call.ApprovalState = TEXT("pending");
            Event.Type = ENebulaAIEventType::ToolCallCompleted;
            Event.ToolCall = MoveTemp(Call);
            Context.OnEvent(Event);
        }
        return;
    }

    if (EventName == TEXT("response.completed"))
    {
        const TSharedPtr<FJsonObject>* Response = nullptr;
        if (Root->TryGetObjectField(TEXT("response"), Response) && Response && Response->IsValid())
        {
            ExtractUsage(*Response, Event.Usage);
            Event.ProviderRequestId = (*Response)->GetStringField(TEXT("id"));
        }
        Event.Type = ENebulaAIEventType::Completed;
        Event.LatencySeconds = FPlatformTime::Seconds() - StartTimeSeconds;
        EmitTerminal(Event, Context);
        return;
    }

    if (EventName == TEXT("response.incomplete"))
    {
        Event.Type = ENebulaAIEventType::Completed;
        Event.FinishReason = TEXT("incomplete");
        Event.LatencySeconds = FPlatformTime::Seconds() - StartTimeSeconds;
        EmitTerminal(Event, Context);
        return;
    }

    if (EventName == TEXT("response.failed") || EventName == TEXT("error"))
    {
        Event.Type = ENebulaAIEventType::Failed;
        const TSharedPtr<FJsonObject>* Response = nullptr;
        if (Root->TryGetObjectField(TEXT("response"), Response) && Response && Response->IsValid())
        {
            const TSharedPtr<FJsonObject>* Err = nullptr;
            if ((*Response)->TryGetObjectField(TEXT("error"), Err) && Err && Err->IsValid())
            {
                Event.Error.Code = (*Err)->GetStringField(TEXT("code"));
                Event.Error.Message = (*Err)->GetStringField(TEXT("message"));
            }
        }
        if (Event.Error.Message.IsEmpty())
        {
            Event.Error.Code = Root->GetStringField(TEXT("code"));
            Event.Error.Message = Root->GetStringField(TEXT("message"));
        }
        if (Event.Error.Message.IsEmpty())
        {
            Event.Error.Code = TEXT("PROVIDER_ERROR");
            Event.Error.Message = FString::Printf(TEXT("Responses stream reported %s."), *EventName);
        }
        LogTransportError(RequestId, Event.Error.Message);
        EmitTerminal(Event, Context);
        return;
    }
}

void FNebulaOpenAIResponsesAdapter::HandleFullJsonBody(const FString& Body, const FNebulaAISendContext& Context)
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

    // Walk output items for text and function calls.
    const TArray<TSharedPtr<FJsonValue>>* Output = nullptr;
    if (Root->TryGetArrayField(TEXT("output"), Output) && Output)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Output)
        {
            const TSharedPtr<FJsonObject>* Item = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(Item) || !Item || !Item->IsValid())
            {
                continue;
            }
            const FString Type = (*Item)->GetStringField(TEXT("type"));
            if (Type == TEXT("message"))
            {
                const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
                if ((*Item)->TryGetArrayField(TEXT("content"), Content))
                {
                    for (const TSharedPtr<FJsonValue>& PartValue : *Content)
                    {
                        const TSharedPtr<FJsonObject>* Part = nullptr;
                        if (PartValue.IsValid() && PartValue->TryGetObject(Part) && Part &&
                            (*Part)->GetStringField(TEXT("type")) == TEXT("output_text"))
                        {
                            FNebulaAIEvent Delta;
                            Delta.Type = ENebulaAIEventType::TextDelta;
                            Delta.ConversationId = ConversationId;
                            Delta.RequestId = RequestId;
                            Delta.TextDelta = (*Part)->GetStringField(TEXT("text"));
                            Context.OnEvent(Delta);
                        }
                    }
                }
            }
            else if (Type == TEXT("function_call"))
            {
                FNebulaAIEvent ToolEvent;
                ToolEvent.Type = ENebulaAIEventType::ToolCallCompleted;
                ToolEvent.ConversationId = ConversationId;
                ToolEvent.RequestId = RequestId;
                ToolEvent.ToolCall.Id = (*Item)->GetStringField(TEXT("call_id"));
                ToolEvent.ToolCall.Name = (*Item)->GetStringField(TEXT("name"));
                ToolEvent.ToolCall.ArgumentsJson = (*Item)->GetStringField(TEXT("arguments"));
                ToolEvent.ToolCall.ApprovalState = TEXT("pending");
                Context.OnEvent(ToolEvent);
            }
        }
    }

    FNebulaAIEvent Completed;
    Completed.Type = ENebulaAIEventType::Completed;
    Completed.ConversationId = ConversationId;
    Completed.RequestId = RequestId;
    Completed.ProviderRequestId = Root->GetStringField(TEXT("id"));
    ExtractUsage(Root, Completed.Usage);
    Completed.LatencySeconds = FPlatformTime::Seconds() - StartTimeSeconds;
    EmitTerminal(Completed, Context);
}

void FNebulaOpenAIResponsesAdapter::ExtractUsage(const TSharedPtr<FJsonObject>& Root, FNebulaAIUsage& OutUsage)
{
    const TSharedPtr<FJsonObject>* Usage = nullptr;
    if (Root.IsValid() && Root->TryGetObjectField(TEXT("usage"), Usage) && Usage && Usage->IsValid())
    {
        OutUsage.PromptTokens = static_cast<int32>((*Usage)->GetNumberField(TEXT("input_tokens")));
        OutUsage.CompletionTokens = static_cast<int32>((*Usage)->GetNumberField(TEXT("output_tokens")));
        OutUsage.TotalTokens = OutUsage.PromptTokens + OutUsage.CompletionTokens;
        OutUsage.bValid = true;
    }
}

void FNebulaOpenAIResponsesAdapter::EmitTerminal(const FNebulaAIEvent& Event, const FNebulaAISendContext& Context)
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
