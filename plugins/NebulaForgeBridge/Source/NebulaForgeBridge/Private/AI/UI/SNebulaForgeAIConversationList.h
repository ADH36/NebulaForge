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
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

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
    TSharedRef<ITableRow> GenerateRow(
        TSharedPtr<FNebulaAIConversationMeta> Item, const TSharedRef<STableViewBase>& OwnerTable) const;

    void OnSelectionChanged(TSharedPtr<FNebulaAIConversationMeta> Item, ESelectInfo::Type SelectInfo);
    FReply OnNewChatClicked();
    FReply OnDeleteClicked(const FNebulaAIConversationMeta& Item);

    TArray<TSharedPtr<FNebulaAIConversationMeta>> Items;
    TSharedPtr<SListView<TSharedPtr<FNebulaAIConversationMeta>>> ListView;
    FString ActiveConversationId;
    FOnNebulaAIConversationSelected OnConversationSelected;
};
