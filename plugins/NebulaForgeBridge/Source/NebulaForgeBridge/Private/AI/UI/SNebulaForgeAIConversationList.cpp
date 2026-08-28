#include "AI/UI/SNebulaForgeAIConversationList.h"
#include "AI/NebulaForgeAIConversationService.h"
#include "AI/NebulaForgeAISecretStore.h"
#include "AI/NebulaForgeAIService.h"
#include "AI/NebulaForgeAISettings.h"
#include "EditorStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NebulaForgeAIConversationList"

void SNebulaForgeAIConversationList::Construct(const FArguments& InArgs)
{
    OnConversationSelected = InArgs._OnConversationSelected;
    ActiveConversationId = InArgs._ActiveConversationId;

    Refresh();

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f)
        [
            SNew(SButton)
            .Text(LOCTEXT("NewChat", "+ New chat"))
            .HAlign(HAlign_Center)
            .OnClicked_Raw(this, &SNebulaForgeAIConversationList::OnNewChatClicked)
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(FMargin(4.0f, 2.0f))
        [
            SAssignNew(ListView, SListView<TSharedPtr<FNebulaAIConversationMeta> >)
            .ListItemsSource(&Items)
            .OnGenerateRow(this, &SNebulaForgeAIConversationList::GenerateRow)
            .OnSelectionChanged(this, &SNebulaForgeAIConversationList::OnSelectionChanged)
            .SelectionMode(ESelectionMode::Single)
        ]
    ];
}

void SNebulaForgeAIConversationList::Refresh()
{
    Items.Reset();
    if (FNebulaForgeAIConversationService* Conversations = FNebulaForgeAIService::Get().Conversations())
    {
        for (const FNebulaAIConversationMeta& Meta : Conversations->GetConversationMetas())
        {
            Items.Add(MakeShared<FNebulaAIConversationMeta>(Meta));
        }
    }
    if (ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }
}

TSharedRef<ITableRow> SNebulaForgeAIConversationList::GenerateRow(
    TSharedPtr<FNebulaAIConversationMeta> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
    SNebulaForgeAIConversationList* MutableThis = const_cast<SNebulaForgeAIConversationList*>(this);
    return SNew(STableRow<TSharedPtr<FNebulaAIConversationMeta>>, OwnerTable)
        .Padding(2.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Item->Title))
                .ToolTipText(FText::FromString(
                    Item->Model.IsEmpty() ? Item->Title : (Item->Title + TEXT("  ·  ") + Item->Model)))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(2.0f, 0.0f))
            [
                SNew(SButton)
                .Text(LOCTEXT("Delete", "Delete"))
                .OnClicked(FOnClicked::CreateLambda([MutableThis, Item]()
                {
                    return MutableThis->OnDeleteClicked(*Item);
                }))
                .ToolTipText(LOCTEXT("DeleteTooltip", "Delete this conversation after confirmation."))
            ]
        ];
}

void SNebulaForgeAIConversationList::OnSelectionChanged(
    TSharedPtr<FNebulaAIConversationMeta> Item, ESelectInfo::Type SelectInfo)
{
    if (!Item.IsValid() || Item->Id.IsEmpty() || Item->Id == ActiveConversationId)
    {
        return;
    }
    ActiveConversationId = Item->Id;
    OnConversationSelected.ExecuteIfBound(Item->Id);
}

FReply SNebulaForgeAIConversationList::OnNewChatClicked()
{
    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    const FNebulaAIProviderProfile* Profile = Settings ? Settings->GetActiveProfile() : nullptr;
    const FString ConversationId = FNebulaForgeAIService::Get().Conversations()->CreateConversation(
        TEXT("New chat"),
        Profile ? Profile->Id : FString(),
        Profile ? Profile->DefaultModel : FString());
    ActiveConversationId = ConversationId;
    Refresh();
    OnConversationSelected.ExecuteIfBound(ConversationId);
    return FReply::Handled();
}

FReply SNebulaForgeAIConversationList::OnDeleteClicked(const FNebulaAIConversationMeta& Item)
{
    const FText ConfirmText = FText::Format(
        LOCTEXT("DeleteConfirm", "Delete conversation \"{0}\"? This cannot be undone."),
        FText::FromString(Item.Title));
    if (FSlateApplication::IsInitialized() &&
        FMessageDialog::Open(EAppMsgType::YesNo, ConfirmText) == EAppReturnType::No)
    {
        return FReply::Handled();
    }
    FNebulaForgeAIService::Get().Conversations()->DeleteConversation(Item.Id);
    if (ActiveConversationId == Item.Id)
    {
        ActiveConversationId.Reset();
    }
    Refresh();
    OnConversationSelected.ExecuteIfBound(ActiveConversationId);
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
