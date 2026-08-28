#include "AI/UI/SNebulaForgeAIToolApproval.h"
#include "AI/NebulaForgeAISecretStore.h"
#include "AI/NebulaForgeAIService.h"
#include "AI/UI/NebulaForgeAIStyle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NebulaForgeAIToolApproval"

void SNebulaForgeAIToolApproval::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FNebulaForgeAIStyle::GetBubbleBrush())
        .BorderBackgroundColor(FNebulaForgeAIStyle::ToolBubbleColor())
        .Visibility_Raw(this, &SNebulaForgeAIToolApproval::GetBannerVisibility)
        .Padding(6.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text_Raw(this, &SNebulaForgeAIToolApproval::GetTitleText)
                .Font(FAppStyle::GetFontStyle("NormalTextBold"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 2.0f)
            [
                SNew(STextBlock)
                .Text_Raw(this, &SNebulaForgeAIToolApproval::GetDetailText)
                .AutoWrapText(true)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f, 0.0f, 0.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("AllowOnce", "Allow once"))
                    .OnClicked_Raw(this, &SNebulaForgeAIToolApproval::OnAllowOnce)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("AllowConversation", "Allow for this conversation"))
                    .OnClicked_Raw(this, &SNebulaForgeAIToolApproval::OnAllowForConversation)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Deny", "Deny"))
                    .OnClicked_Raw(this, &SNebulaForgeAIToolApproval::OnDeny)
                ]
            ]
        ]
    ];
}

void SNebulaForgeAIToolApproval::ShowApproval(const FNebulaForgeAIToolGateway::FPendingApproval& Approval)
{
    CurrentApproval = Approval;
}

void SNebulaForgeAIToolApproval::Clear()
{
    CurrentApproval.Reset();
}

FReply SNebulaForgeAIToolApproval::OnAllowOnce()
{
    if (CurrentApproval.IsSet())
    {
        FNebulaForgeAIService::Get().Tools()->ResolveApproval(
            CurrentApproval->CallId, FNebulaForgeAIToolGateway::EApprovalDecision::ApproveOnce);
        CurrentApproval.Reset();
    }
    return FReply::Handled();
}

FReply SNebulaForgeAIToolApproval::OnAllowForConversation()
{
    if (CurrentApproval.IsSet())
    {
        FNebulaForgeAIService::Get().Tools()->ResolveApproval(
            CurrentApproval->CallId, FNebulaForgeAIToolGateway::EApprovalDecision::AllowForConversation);
        CurrentApproval.Reset();
    }
    return FReply::Handled();
}

FReply SNebulaForgeAIToolApproval::OnDeny()
{
    if (CurrentApproval.IsSet())
    {
        FNebulaForgeAIService::Get().Tools()->ResolveApproval(
            CurrentApproval->CallId, FNebulaForgeAIToolGateway::EApprovalDecision::Deny);
        CurrentApproval.Reset();
    }
    return FReply::Handled();
}

FText SNebulaForgeAIToolApproval::GetTitleText() const
{
    if (!CurrentApproval.IsSet())
    {
        return FText::GetEmpty();
    }
    return FText::Format(
        LOCTEXT("TitleFmt", "Approve tool: {0}   ·   Risk: {1}"),
        FText::FromString(CurrentApproval->ToolName),
        FNebulaForgeAIToolGateway::RiskToDisplayText(CurrentApproval->Risk));
}

FText SNebulaForgeAIToolApproval::GetDetailText() const
{
    if (!CurrentApproval.IsSet())
    {
        return FText::GetEmpty();
    }
    return FText::Format(
        LOCTEXT("DetailFmt", "Arguments: {0}"),
        FText::FromString(CurrentApproval->ArgumentsSummary));
}

EVisibility SNebulaForgeAIToolApproval::GetBannerVisibility() const
{
    return CurrentApproval.IsSet() ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
