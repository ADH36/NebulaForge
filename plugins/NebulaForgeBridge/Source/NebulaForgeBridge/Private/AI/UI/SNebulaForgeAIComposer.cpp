#include "AI/UI/SNebulaForgeAIComposer.h"
#include "AI/NebulaForgeAISecretStore.h"
#include "AI/NebulaForgeAIService.h"
#include "AI/UI/NebulaForgeAIStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NebulaForgeAIComposer"

namespace
{
    using FModeOption = TSharedPtr<FString>;

    ENebulaAIInteractionMode ModeFromLabel(const FString& Label)
    {
        if (Label == TEXT("Plan")) return ENebulaAIInteractionMode::Plan;
        if (Label == TEXT("Act")) return ENebulaAIInteractionMode::Act;
        return ENebulaAIInteractionMode::Ask;
    }
}

void SNebulaForgeAIComposer::Construct(const FArguments& InArgs)
{
    OnSend = InArgs._OnSend;
    OnStop = InArgs._OnStop;

    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();

    ModeOptions = { MakeShared<FString>(TEXT("Ask")),
                    MakeShared<FString>(TEXT("Plan")),
                    MakeShared<FString>(TEXT("Act")) };

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(2.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ContextLabel", "Context:"))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SScrollBox)
                .Orientation(Orient_Horizontal)
                + SScrollBox::Slot()
                [
                    SNew(SButton)
                    .Text(FText::Format(LOCTEXT("CtxProject", "{0}"), FText::FromString(FApp::GetProjectName())))
                    .OnClicked_Lambda([this]()
                    {
                        ToggleContextKind(FNebulaForgeAIContextCollector::EContextKind::ProjectInfo);
                        return FReply::Handled();
                    })
                ]
                + SScrollBox::Slot()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CtxLevel", "Level"))
                    .OnClicked_Lambda([this]()
                    {
                        ToggleContextKind(FNebulaForgeAIContextCollector::EContextKind::CurrentLevel);
                        return FReply::Handled();
                    })
                ]
                + SScrollBox::Slot()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CtxActors", "Actors"))
                    .OnClicked_Lambda([this]()
                    {
                        ToggleContextKind(FNebulaForgeAIContextCollector::EContextKind::SelectedActors);
                        return FReply::Handled();
                    })
                ]
                + SScrollBox::Slot()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CtxAssets", "Assets"))
                    .OnClicked_Lambda([this]()
                    {
                        ToggleContextKind(FNebulaForgeAIContextCollector::EContextKind::SelectedAssets);
                        return FReply::Handled();
                    })
                ]
                + SScrollBox::Slot()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CtxLog", "Log"))
                    .OnClicked_Lambda([this]()
                    {
                        ToggleContextKind(FNebulaForgeAIContextCollector::EContextKind::OutputLog);
                        return FReply::Handled();
                    })
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(2.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SAssignNew(InputBox, SMultiLineEditableTextBox)
                .HintText(LOCTEXT("ComposerHint", "Ask NebulaForge..."))
                .AutoWrapText(true)
                .OnTextCommitted_Raw(this, &SNebulaForgeAIComposer::OnTextCommitted)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(4.0f, 0.0f)
            .VAlign(VAlign_Bottom)
            [
                SNew(SButton)
                .Text_Raw(this, &SNebulaForgeAIComposer::GetSendButtonText)
                .OnClicked_Raw(this, &SNebulaForgeAIComposer::OnSendClicked)
                .IsEnabled_Raw(this, &SNebulaForgeAIComposer::IsEnabledCheck)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            .VAlign(VAlign_Bottom)
            [
                SNew(SButton)
                .Text(LOCTEXT("Stop", "Stop"))
                .Visibility_Lambda([this]()
                {
                    return bBusy ? EVisibility::Visible : EVisibility::Collapsed;
                })
                .OnClicked_Raw(this, &SNebulaForgeAIComposer::OnStopClicked)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            .VAlign(VAlign_Bottom)
            [
                SNew(SComboBox<FModeOption>)
                .OptionsSource(&ModeOptions)
                .OnSelectionChanged_Lambda([this](FModeOption Option, ESelectInfo::Type)
                {
                    Mode = ModeFromLabel(*Option);
                })
                .Content()
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]()
                    {
                        return FText::FromString(NebulaAIInteractionModeToString(Mode));
                    })
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text_Raw(this, &SNebulaForgeAIComposer::GetStatusText)
            .ColorAndOpacity(FNebulaForgeAIStyle::ErrorColor())
            .Visibility_Lambda([this]()
            {
                return DisabledReason.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
            })
        ]
    ];

    if (Settings && !Settings->bFirstRunComplete)
    {
        SetDisabledReason(LOCTEXT("SetupNeeded",
            "Set up a provider in Settings (key stays local), then send your first message."));
    }
}

void SNebulaForgeAIComposer::SetBusy(bool bInBusy)
{
    bBusy = bInBusy;
    if (InputBox.IsValid())
    {
        InputBox->SetIsReadOnly(bInBusy);
    }
}

void SNebulaForgeAIComposer::SetDisabledReason(const FText& Reason)
{
    DisabledReason = Reason;
}

FReply SNebulaForgeAIComposer::OnSendClicked()
{
    const FString Text = InputBox.IsValid() ? InputBox->GetText().ToString() : FString();
    if (Text.TrimStartAndEnd().IsEmpty() || bBusy)
    {
        return FReply::Handled();
    }

    // Collect selected context synchronously from cached chips; the chat
    // widget collects context before invoking Send, so forward empty chips
    // here and let the owner enrich the message.
    OnSend.ExecuteIfBound(Text, Mode, TArray<FNebulaAIContextChip>());
    if (InputBox.IsValid())
    {
        InputBox->SetText(FText::GetEmpty());
    }
    return FReply::Handled();
}

FReply SNebulaForgeAIComposer::OnStopClicked()
{
    OnStop.ExecuteIfBound();
    return FReply::Handled();
}

void SNebulaForgeAIComposer::OnTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    const bool bEnterSends = !Settings || Settings->bEnterToSend;
    if (CommitType == ETextCommit::OnEnter && bEnterSends)
    {
        OnSendClicked();
    }
}

bool SNebulaForgeAIComposer::IsEnabledCheck() const
{
    return !bBusy;
}

FText SNebulaForgeAIComposer::GetSendButtonText() const
{
    return bBusy ? LOCTEXT("Sending", "Sending...") : LOCTEXT("Send", "Send");
}

FText SNebulaForgeAIComposer::GetStatusText() const
{
    return DisabledReason;
}

void SNebulaForgeAIComposer::ToggleContextKind(FNebulaForgeAIContextCollector::EContextKind Kind)
{
    if (SelectedContextKinds.Contains(Kind))
    {
        SelectedContextKinds.Remove(Kind);
    }
    else
    {
        SelectedContextKinds.Add(Kind);
    }
}

FText SNebulaForgeAIComposer::GetContextToggleLabel(FNebulaForgeAIContextCollector::EContextKind Kind) const
{
    return FNebulaForgeAIContextCollector::GetKindLabel(Kind);
}
