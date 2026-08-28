// =============================================================================
// SNebulaForgeAIConversationList.h
// =============================================================================
// Conversation sidebar: new chat, list sorted by recent activity, rename,
// delete with confirmation (plan section 3.2).
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "NebulaForgeAIModels.h"
#include "Widgets/SCompoundWidget.h"

class SListView;

DECLARE_DELEGATE_OneParam(FOnNebulaAIConversationSelected, const FString& /*ConversationId*/);

class SNebulaForgeAIConversationList : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SNebulaForgeAIConversationList) {}
    SLATE_EVENT(FOnNebulaAIConversationSelected, OnConversationSelected)
    SLATE_ARGUMENT(FString, ActiveConversationId)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Refresh from the conversation service (game thread). */
    void Refresh();

private:
    TSharedRef<class STableRow<FNebulaAIConversationMeta>> GenerateRow(
        FNebulaAIConversationMeta Item, const TSharedRef<STableViewBase>& OwnerTable);

    void OnSelectionChanged(FNebulaAIConversationMeta Item, ESelectInfo::Type SelectInfo);
    FReply OnNewChatClicked();
    FReply OnDeleteClicked(const FNebulaAIConversationMeta& Item);

    TArray<FNebulaAIConversationMeta> Items;
    TSharedPtr<SListView<FNebulaAIConversationMeta>> ListView;
    FString ActiveConversationId;
    FOnNebulaAIConversationSelected OnConversationSelected;
};
