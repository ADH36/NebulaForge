#include "AI/NebulaForgeAIConversationService.h"
#include "AI/NebulaForgeAIDiagnostics.h"
#include "AI/NebulaForgeAISecretStore.h"
#include "AI/NebulaForgeAISettings.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FNebulaForgeAIConversationService::FNebulaForgeAIConversationService() = default;

void FNebulaForgeAIConversationService::Initialize()
{
    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    if (!Settings || !Settings->Privacy.bStoreConversationsLocally)
    {
        return;
    }
    LoadPersisted();
    BroadcastChanged();
}

void FNebulaForgeAIConversationService::Shutdown()
{
    // Flush any dirty conversations when storage is enabled.
    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    if (!Settings || !Settings->Privacy.bStoreConversationsLocally)
    {
        return;
    }
    for (const TPair<FString, TSharedPtr<FConversation>>& Pair : Conversations)
    {
        if (Pair.Value.IsValid())
        {
            SaveConversation(*Pair.Value);
        }
    }
}

FString FNebulaForgeAIConversationService::CreateConversation(
    const FString& Title, const FString& ProviderProfileId, const FString& Model)
{
    TSharedPtr<FConversation> Conversation = MakeShared<FConversation>();
    Conversation->Meta.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    Conversation->Meta.Title = Title.IsEmpty() ? TEXT("New chat") : Title;
    Conversation->Meta.CreatedUtc = FDateTime::UtcNow();
    Conversation->Meta.LastActivityUtc = Conversation->Meta.CreatedUtc;
    Conversation->Meta.ProviderProfileId = ProviderProfileId;
    Conversation->Meta.Model = Model;
    Conversation->Meta.NextSequence = 1;

    const FString Id = Conversation->Meta.Id;
    Conversations.Add(Id, Conversation);
    Order.Insert(Id, 0);

    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    if (Settings && Settings->Privacy.bStoreConversationsLocally)
    {
        SaveConversation(*Conversation);
    }
    BroadcastChanged();
    return Id;
}

bool FNebulaForgeAIConversationService::DeleteConversation(const FString& ConversationId)
{
    if (!Conversations.Remove(ConversationId))
    {
        return false;
    }
    Order.Remove(ConversationId);
    IFileManager::Get().Delete(*GetConversationFilePath(ConversationId), false, true);
    BroadcastChanged();
    return true;
}

void FNebulaForgeAIConversationService::DeleteAll()
{
    const FString Dir = GetConversationDir();
    Conversations.Reset();
    Order.Reset();
    if (IFileManager::Get().DirectoryExists(*Dir))
    {
        TArray<FString> Files;
        IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.json")), true, false);
        for (const FString& File : Files)
        {
            IFileManager::Get().Delete(*(Dir / File), false, true);
        }
    }
    BroadcastChanged();
}

TArray<FNebulaAIConversationMeta> FNebulaForgeAIConversationService::GetConversationMetas() const
{
    TArray<FNebulaAIConversationMeta> Metas;
    Metas.Reserve(Order.Num());
    for (const FString& Id : Order)
    {
        const TSharedPtr<FConversation>* Found = Conversations.Find(Id);
        if (Found && Found->IsValid())
        {
            Metas.Add((*Found)->Meta);
        }
    }
    return Metas;
}

TSharedPtr<FNebulaForgeAIConversationService::FConversation>
FNebulaForgeAIConversationService::FindConversation(const FString& ConversationId) const
{
    const TSharedPtr<FConversation>* Found = Conversations.Find(ConversationId);
    return Found ? *Found : nullptr;
}

FNebulaAIMessage& FNebulaForgeAIConversationService::AppendMessage(
    const FString& ConversationId, FNebulaAIMessage Message)
{
    static FNebulaAIMessage InvalidMessage;
    TSharedPtr<FConversation> Conversation = FindConversation(ConversationId);
    if (!Conversation.IsValid())
    {
        InvalidMessage = FNebulaAIMessage();
        return InvalidMessage;
    }

    Message.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    Message.Sequence = Conversation->Meta.NextSequence++;
    Conversation->Messages.Add(Message);
    Conversation->Meta.LastActivityUtc = FDateTime::UtcNow();

    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    if (Settings && Settings->Privacy.bStoreConversationsLocally)
    {
        SaveConversation(*Conversation);
    }
    BroadcastChanged();
    return Conversation->Messages.Last();
}

bool FNebulaForgeAIConversationService::UpdateMessage(
    const FString& ConversationId, const FNebulaAIMessage& Message)
{
    TSharedPtr<FConversation> Conversation = FindConversation(ConversationId);
    if (!Conversation.IsValid())
    {
        return false;
    }
    for (FNebulaAIMessage& Existing : Conversation->Messages)
    {
        if (Existing.Id == Message.Id)
        {
            Existing = Message;
            return true;
        }
    }
    return false;
}

void FNebulaForgeAIConversationService::RenameConversation(const FString& ConversationId, const FString& NewTitle)
{
    TSharedPtr<FConversation> Conversation = FindConversation(ConversationId);
    if (!Conversation.IsValid())
    {
        return;
    }
    Conversation->Meta.Title = NewTitle;
    Conversation->Meta.LastActivityUtc = FDateTime::UtcNow();
    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    if (Settings && Settings->Privacy.bStoreConversationsLocally)
    {
        SaveConversation(*Conversation);
    }
    BroadcastChanged();
}

void FNebulaForgeAIConversationService::Touch(const FString& ConversationId)
{
    Order.Remove(ConversationId);
    Order.Insert(ConversationId, 0);
    TSharedPtr<FConversation> Conversation = FindConversation(ConversationId);
    if (Conversation.IsValid())
    {
        Conversation->Meta.LastActivityUtc = FDateTime::UtcNow();
    }
    BroadcastChanged();
}

void FNebulaForgeAIConversationService::AllowToolForConversation(const FString& ConversationId, const FString& ToolName)
{
    TSharedPtr<FConversation> Conversation = FindConversation(ConversationId);
    if (Conversation.IsValid())
    {
        Conversation->ConversationAllowedTools.Add(ToolName);
    }
}

bool FNebulaForgeAIConversationService::IsToolAllowedForConversation(
    const FString& ConversationId, const FString& ToolName) const
{
    TSharedPtr<FConversation> Conversation = FindConversation(ConversationId);
    return Conversation.IsValid() && Conversation->ConversationAllowedTools.Contains(ToolName);
}

bool FNebulaForgeAIConversationService::ExportConversationMarkdown(
    const FString& ConversationId, const FString& OutputFilePath) const
{
    TSharedPtr<FConversation> Conversation = FindConversation(ConversationId);
    if (!Conversation.IsValid())
    {
        return false;
    }

    FString Out;
    Out += FString::Printf(TEXT("# %s\n\n"), *Conversation->Meta.Title);
    Out += FString::Printf(TEXT("Exported: %s\n\n---\n\n"),
        *Conversation->Meta.LastActivityUtc.ToString(TEXT("%Y-%m-%d %H:%M:%S")));

    for (const FNebulaAIMessage& Msg : Conversation->Messages)
    {
        const TCHAR* RoleLabel = TEXT("System");
        switch (Msg.Role)
        {
        case ENebulaAIChatRole::User: RoleLabel = TEXT("User"); break;
        case ENebulaAIChatRole::Assistant: RoleLabel = TEXT("Assistant"); break;
        case ENebulaAIChatRole::Tool: RoleLabel = TEXT("Tool result"); break;
        default: break;
        }
        Out += FString::Printf(TEXT("## %s\n\n"), RoleLabel);
        if (!Msg.Content.IsEmpty())
        {
            Out += FNebulaAIDiagnostics::RedactText(Msg.Content) + TEXT("\n\n");
        }
        for (const FNebulaAIToolCall& Call : Msg.ToolCalls)
        {
            Out += FString::Printf(TEXT("- Tool `%s` (%s): %s\n"),
                *Call.Name, *Call.ApprovalState,
                *FNebulaAIDiagnostics::RedactText(Call.ResultJson.Left(400)));
        }
        if (!Msg.ErrorMessage.IsEmpty())
        {
            Out += FString::Printf(TEXT("> Error [%s]: %s\n\n"),
                *Msg.ErrorCode, *FNebulaAIDiagnostics::RedactText(Msg.ErrorMessage));
        }
    }

    return FFileHelper::SaveStringToFile(Out, *OutputFilePath);
}

FString FNebulaForgeAIConversationService::GetConversationDir() const
{
    return FPaths::ProjectSavedDir() / TEXT("NebulaForgeAI") / TEXT("Conversations");
}

FString FNebulaForgeAIConversationService::GetConversationFilePath(const FString& ConversationId) const
{
    return GetConversationDir() / (ConversationId + TEXT(".json"));
}

void FNebulaForgeAIConversationService::SaveConversation(const FConversation& Conversation) const
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("id"), Conversation.Meta.Id);
    Root->SetStringField(TEXT("title"), Conversation.Meta.Title);
    Root->SetStringField(TEXT("createdUtc"), Conversation.Meta.CreatedUtc.ToString());
    Root->SetStringField(TEXT("lastActivityUtc"), Conversation.Meta.LastActivityUtc.ToString());
    Root->SetStringField(TEXT("providerProfileId"), Conversation.Meta.ProviderProfileId);
    Root->SetStringField(TEXT("model"), Conversation.Meta.Model);
    Root->SetNumberField(TEXT("nextSequence"), static_cast<double>(Conversation.Meta.NextSequence));

    TArray<TSharedPtr<FJsonValue>> Messages;
    for (const FNebulaAIMessage& Msg : Conversation.Messages)
    {
        Messages.Add(MakeShared<FJsonValueObject>(MessageToJson(Msg)));
    }
    Root->SetArrayField(TEXT("messages"), Messages);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Root, Writer);

    const FString Path = GetConversationFilePath(Conversation.Meta.Id);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    FFileHelper::SaveStringToFile(Out, *Path);
}

void FNebulaForgeAIConversationService::LoadPersisted()
{
    const FString Dir = GetConversationDir();
    if (!IFileManager::Get().DirectoryExists(*Dir))
    {
        return;
    }

    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.json")), true, false);
    for (const FString& File : Files)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *(Dir / File)))
        {
            continue;
        }

        TSharedPtr<FJsonObject> Root;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
        {
            UE_LOG(LogNebulaForgeAI, Warning, TEXT("Skipping malformed conversation file %s."), *File);
            continue;
        }

        TSharedPtr<FConversation> Conversation = MakeShared<FConversation>();
        Conversation->Meta.Id = Root->GetStringField(TEXT("id"));
        Conversation->Meta.Title = Root->GetStringField(TEXT("title"));
        FDateTime::Parse(Root->GetStringField(TEXT("createdUtc")), Conversation->Meta.CreatedUtc);
        FDateTime::Parse(Root->GetStringField(TEXT("lastActivityUtc")), Conversation->Meta.LastActivityUtc);
        Conversation->Meta.ProviderProfileId = Root->GetStringField(TEXT("providerProfileId"));
        Conversation->Meta.Model = Root->GetStringField(TEXT("model"));
        Conversation->Meta.NextSequence = static_cast<int64>(Root->GetNumberField(TEXT("nextSequence")));

        const TArray<TSharedPtr<FJsonValue>>* Messages = nullptr;
        if (Root->TryGetArrayField(TEXT("messages"), Messages))
        {
            for (const TSharedPtr<FJsonValue>& Value : *Messages)
            {
                const TSharedPtr<FJsonObject>* Obj = nullptr;
                if (Value.IsValid() && Value->TryGetObject(Obj) && Obj)
                {
                    Conversation->Messages.Add(MessageFromJson(*Obj));
                }
            }
        }

        if (!Conversation->Meta.Id.IsEmpty())
        {
            Conversations.Add(Conversation->Meta.Id, Conversation);
            Order.Add(Conversation->Meta.Id);
        }
    }

    // Most recent first.
    Order.Sort([this](const FString& A, const FString& B)
    {
        const TSharedPtr<FConversation>* ConvA = Conversations.Find(A);
        const TSharedPtr<FConversation>* ConvB = Conversations.Find(B);
        const FDateTime TimeA = ConvA && ConvA->IsValid() ? (*ConvA)->Meta.LastActivityUtc : FDateTime::MinValue();
        const FDateTime TimeB = ConvB && ConvB->IsValid() ? (*ConvB)->Meta.LastActivityUtc : FDateTime::MinValue();
        return TimeA > TimeB;
    });
}

FNebulaAIMessage FNebulaForgeAIConversationService::MessageFromJson(const TSharedPtr<FJsonObject>& Obj)
{
    FNebulaAIMessage Msg;
    Msg.Id = Obj->GetStringField(TEXT("id"));
    Msg.Sequence = static_cast<int64>(Obj->GetNumberField(TEXT("sequence")));
    Msg.Role = static_cast<ENebulaAIChatRole>(static_cast<uint8>(Obj->GetNumberField(TEXT("role"))));
    Msg.Content = Obj->GetStringField(TEXT("content"));
    Msg.ToolCallId = Obj->GetStringField(TEXT("toolCallId"));
    Msg.StatusSummary = Obj->GetStringField(TEXT("statusSummary"));
    Msg.ErrorCode = Obj->GetStringField(TEXT("errorCode"));
    Msg.ErrorMessage = Obj->GetStringField(TEXT("errorMessage"));
    Msg.Mode = static_cast<ENebulaAIInteractionMode>(static_cast<uint8>(Obj->GetNumberField(TEXT("mode"))));
    Msg.TimestampUtc = FDateTime::FromUnixTimestamp(
        static_cast<int64>(Obj->GetNumberField(TEXT("timestampUnix"))));

    const TArray<TSharedPtr<FJsonValue>>* Calls = nullptr;
    if (Obj->TryGetArrayField(TEXT("toolCalls"), Calls))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Calls)
        {
            const TSharedPtr<FJsonObject>* CallObj = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(CallObj) || !CallObj)
            {
                continue;
            }
            FNebulaAIToolCall Call;
            Call.Id = (*CallObj)->GetStringField(TEXT("id"));
            Call.Name = (*CallObj)->GetStringField(TEXT("name"));
            Call.ArgumentsJson = (*CallObj)->GetStringField(TEXT("arguments"));
            Call.ResultJson = (*CallObj)->GetStringField(TEXT("result"));
            Call.bSucceeded = (*CallObj)->GetBoolField(TEXT("succeeded"));
            Call.DurationSeconds = (*CallObj)->GetNumberField(TEXT("duration"));
            Call.ApprovalState = (*CallObj)->GetStringField(TEXT("approval"));
            Msg.ToolCalls.Add(MoveTemp(Call));
        }
    }
    return Msg;
}

TSharedRef<FJsonObject> FNebulaForgeAIConversationService::MessageToJson(const FNebulaAIMessage& Msg)
{
    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("id"), Msg.Id);
    Obj->SetNumberField(TEXT("sequence"), static_cast<double>(Msg.Sequence));
    Obj->SetNumberField(TEXT("role"), static_cast<double>(static_cast<uint8>(Msg.Role)));
    Obj->SetStringField(TEXT("content"), Msg.Content);
    Obj->SetStringField(TEXT("toolCallId"), Msg.ToolCallId);
    Obj->SetStringField(TEXT("statusSummary"), Msg.StatusSummary);
    Obj->SetStringField(TEXT("errorCode"), Msg.ErrorCode);
    Obj->SetStringField(TEXT("errorMessage"), Msg.ErrorMessage);
    Obj->SetNumberField(TEXT("mode"), static_cast<double>(static_cast<uint8>(Msg.Mode)));
    Obj->SetNumberField(TEXT("timestampUnix"),
        static_cast<double>(Msg.TimestampUtc.ToUnixTimestamp()));

    TArray<TSharedPtr<FJsonValue>> Calls;
    for (const FNebulaAIToolCall& Call : Msg.ToolCalls)
    {
        TSharedRef<FJsonObject> CallObj = MakeShared<FJsonObject>();
        CallObj->SetStringField(TEXT("id"), Call.Id);
        CallObj->SetStringField(TEXT("name"), Call.Name);
        CallObj->SetStringField(TEXT("arguments"), Call.ArgumentsJson);
        CallObj->SetStringField(TEXT("result"), Call.ResultJson);
        CallObj->SetBoolField(TEXT("succeeded"), Call.bSucceeded);
        CallObj->SetNumberField(TEXT("duration"), Call.DurationSeconds);
        CallObj->SetStringField(TEXT("approval"), Call.ApprovalState);
        Calls.Add(MakeShared<FJsonValueObject>(CallObj));
    }
    Obj->SetArrayField(TEXT("toolCalls"), Calls);
    return Obj;
}

void FNebulaForgeAIConversationService::BroadcastChanged()
{
    OnChanged.Broadcast();
}
