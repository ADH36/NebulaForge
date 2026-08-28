#include "AI/UI/SNebulaForgeAIMessageView.h"
#include "AI/NebulaForgeAIConversationService.h"
#include "AI/NebulaForgeAISecretStore.h"
#include "AI/NebulaForgeAIService.h"
#include "AI/UI/NebulaForgeAIStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NebulaForgeAIMessageView"

void SNebulaForgeAIMessageView::Construct(const FArguments& InArgs)
{
    ConversationId = InArgs._ConversationId;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FNebulaForgeAIStyle::GetBubbleBrush())
        .Padding(4.0f)
        [
            SAssignNew(ScrollBox, SScrollBox)
        ]
    ];

    RefreshAll();
}

void SNebulaForgeAIMessageView::SetConversation(const FString& InConversationId)
{
    if (ConversationId != InConversationId)
    {
        ConversationId = InConversationId;
        RefreshAll();
    }
}

void SNebulaForgeAIMessageView::RefreshMessage(const FString& MessageId)
{
    // Streaming updates: rebuild the row content in place to avoid
    // reconstructing the whole timeline per token batch.
    TSharedPtr<SWidget>* Existing = MessageRows.Find(MessageId);
    if (Existing && Existing->IsValid())
    {
        RefreshAll();
    }
}

void SNebulaForgeAIMessageView::RefreshAll()
{
    if (!ScrollBox.IsValid())
    {
        return;
    }
    ScrollBox->ClearChildren();
    MessageRows.Reset();

    FNebulaForgeAIConversationService* Conversations = FNebulaForgeAIService::Get().Conversations();
    TSharedPtr<FNebulaForgeAIConversationService::FConversation> Conversation =
        Conversations ? Conversations->FindConversation(ConversationId) : nullptr;
    if (!Conversation.IsValid())
    {
        ScrollBox->AddSlot()
        [
            SNew(STextBlock)
            .Text(LOCTEXT("EmptyConversation",
                "No messages yet. Ask about your project, attach context chips, and send."))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ];
        return;
    }

    for (const FNebulaAIMessage& Message : Conversation->Messages)
    {
        TSharedRef<SWidget> Row = BuildMessageRow(Message);
        MessageRows.Add(Message.Id, Row);
        ScrollBox->AddSlot()
        [
            Row
        ];
    }
    ScrollToBottom();
}

void SNebulaForgeAIMessageView::ScrollToBottom()
{
    if (ScrollBox.IsValid())
    {
        ScrollBox->ScrollToEnd();
    }
}

TSharedRef<SWidget> SNebulaForgeAIMessageView::BuildMessageRow(const FNebulaAIMessage& Message) const
{
    FSlateColor BubbleColor;
    FString RoleLabel;
    switch (Message.Role)
    {
    case ENebulaAIChatRole::User:
        BubbleColor = FNebulaForgeAIStyle::UserBubbleColor();
        RoleLabel = TEXT("You");
        break;
    case ENebulaAIChatRole::Assistant:
        BubbleColor = Message.ErrorCode.IsEmpty()
            ? FNebulaForgeAIStyle::AssistantBubbleColor()
            : FNebulaForgeAIStyle::ErrorBubbleColor();
        RoleLabel = TEXT("Assistant");
        break;
    case ENebulaAIChatRole::Tool:
        BubbleColor = FNebulaForgeAIStyle::ToolBubbleColor();
        RoleLabel = TEXT("Tool");
        break;
    case ENebulaAIChatRole::System:
    default:
        BubbleColor = FNebulaForgeAIStyle::AssistantBubbleColor();
        RoleLabel = TEXT("System");
        break;
    }

    return SNew(SBorder)
        .BorderImage(FNebulaForgeAIStyle::GetBubbleBrush())
        .BorderBackgroundColor(BubbleColor)
        .Padding(6.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(RoleLabel + TEXT("  ·  ") +
                    Message.TimestampUtc.ToString(TEXT("%H:%M"))))
                .Font(FAppStyle::GetFontStyle("TinyText"))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                BuildMessageContent(Message)
            ]
        ];
}

TSharedRef<SWidget> SNebulaForgeAIMessageView::BuildMessageContent(const FNebulaAIMessage& Message) const
{
    const FString CopyText = Message.Content;

    TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

    if (Message.Role == ENebulaAIChatRole::Assistant && !Message.StatusSummary.IsEmpty())
    {
        Content->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(Message.StatusSummary))
            .Font(FAppStyle::GetFontStyle("SmallText"))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ];
    }

    if (!Message.Content.IsEmpty())
    {
        Content->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(Message.Content))
            .AutoWrapText(true)
            .SetFont(FAppStyle::GetFontStyle(Message.Role == ENebulaAIChatRole::User
                ? "BoldText"
                : "NormalText"))
        ];
    }

    if (Message.Role == ENebulaAIChatRole::Tool)
    {
        // Tool results stay compact with a copy affordance.
        Content->AddSlot()
        .AutoHeight()
        [
            SNew(SButton)
            .Text(LOCTEXT("CopyResult", "Copy"))
            .OnClicked(FOnClicked::CreateLambda([CopyText]()
            {
                if (FSlateApplication::IsInitialized())
                {
                    FSlateApplication::Get().CopyToClipboard(FText::FromString(CopyText));
                }
                return FReply::Handled();
            }))
        ];
    }

    for (const FNebulaAIToolCall& Call : Message.ToolCalls)
    {
        const FString Summary = FString::Printf(TEXT("%s  (%s)  %s  [%s]"),
            *Call.Name,
            *Call.ApprovalState,
            *Call.ArgumentsSummary,
            *Call.ResultJson.Left(200));
        Content->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(Summary))
            .Font(FAppStyle::GetFontStyle("Monospace"))
            .AutoWrapText(true)
            .ColorAndOpacity(FNebulaForgeAIStyle::ToolBubbleColor())
        ];
    }

    if (!Message.ErrorMessage.IsEmpty())
    {
        TSharedRef<SHorizontalBox> ErrorBox = SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Message.ErrorCode + TEXT(": ") + Message.ErrorMessage))
                .AutoWrapText(true)
                .ColorAndOpacity(FNebulaForgeAIStyle::ErrorColor())
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(LOCTEXT("CopyError", "Copy diagnostic"))
                .OnClicked(FOnClicked::CreateLambda([Message]()
                {
                    if (FSlateApplication::IsInitialized())
                    {
                        const FString Diagnostic = FString::Printf(
                            TEXT("Error: %s\nMessage: %s"), *Message.ErrorCode, *Message.ErrorMessage);
                        FSlateApplication::Get().CopyToClipboard(FText::FromString(Diagnostic));
                    }
                    return FReply::Handled();
                }))
            ];
        Content->AddSlot()
        .AutoHeight()
        [
            ErrorBox
        ];
    }

    return Content;
}

#undef LOCTEXT_NAMESPACE
