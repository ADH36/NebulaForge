// =============================================================================
// NebulaForgeAIConversationService.h
// =============================================================================
// Local conversation lifecycle and persistence (plan sections 7.1/7.3).
//
// Conversations are stored as JSON under <Project>/Saved/NebulaForgeAI/
// Conversations/. Persistence can be disabled per project/user; deletion is
// always available. Change notification is delivered on the game thread.
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "NebulaForgeAIModels.h"

class FNebulaForgeAIConversationService
{
public:
    struct FConversation
    {
        FNebulaAIConversationMeta Meta;
        TArray<FNebulaAIMessage> Messages;
        /** Tool names approved for the rest of this conversation. */
        TSet<FString> ConversationAllowedTools;
    };

    /** Multi-cast change notification fired on the game thread. */
    DECLARE_MULTICAST_DELEGATE(FOnChanged);
    FOnChanged OnChanged;

    FNebulaForgeAIConversationService();

    /** Load persisted conversations when local storage is enabled. */
    void Initialize();
    void Shutdown();

    /** Create a new conversation; returns its id. */
    FString CreateConversation(const FString& Title, const FString& ProviderProfileId, const FString& Model);

    /** Delete a conversation (memory + disk). */
    bool DeleteConversation(const FString& ConversationId);

    /** Delete every conversation. */
    void DeleteAll();

    /** Sorted-by-recent list of conversation metadata. */
    TArray<FNebulaAIConversationMeta> GetConversationMetas() const;

    /** Pointer to the live conversation or nullptr. */
    TSharedPtr<FConversation> FindConversation(const FString& ConversationId) const;

    /** Append a message and bump the sequence counter. */
    FNebulaAIMessage& AppendMessage(const FString& ConversationId, FNebulaAIMessage Message);

    /** Update an existing message in place (streaming deltas). */
    bool UpdateMessage(const FString& ConversationId, const FNebulaAIMessage& Message);

    /** Rename a conversation. */
    void RenameConversation(const FString& ConversationId, const FString& NewTitle);

    /** Touch the conversation's activity stamp and notify listeners. */
    void Touch(const FString& ConversationId);

    /** Approve a tool name for the remainder of a conversation. */
    void AllowToolForConversation(const FString& ConversationId, const FString& ToolName);

    bool IsToolAllowedForConversation(const FString& ConversationId, const FString& ToolName) const;

    /** Export one conversation to a redacted Markdown transcript file. */
    bool ExportConversationMarkdown(const FString& ConversationId, const FString& OutputFilePath) const;

private:
    FString GetConversationDir() const;
    FString GetConversationFilePath(const FString& ConversationId) const;
    void SaveConversation(const FConversation& Conversation) const;
    void LoadPersisted();
    static FNebulaAIMessage MessageFromJson(const TSharedPtr<FJsonObject>& Obj);
    static TSharedRef<FJsonObject> MessageToJson(const FNebulaAIMessage& Msg);
    void BroadcastChanged();

    TMap<FString, TSharedPtr<FConversation>> Conversations;
    /** Most-recent-first ordering maintained by Touch(). */
    TArray<FString> Order;
    FString ActiveConversationId;
};
