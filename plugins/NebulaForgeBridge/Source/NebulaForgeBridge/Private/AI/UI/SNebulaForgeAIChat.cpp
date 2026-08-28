#include "AI/UI/SNebulaForgeAIChat.h"
#include "AI/NebulaForgeAIConversationService.h"
#include "AI/NebulaForgeAIContextCollector.h"
#include "AI/NebulaForgeAIProviderService.h"
#include "AI/NebulaForgeAIRequestCoordinator.h"
#include "AI/NebulaForgeAISecretStore.h"
#include "AI/NebulaForgeAIService.h"
#include "AI/NebulaForgeAISettings.h"
#include "AI/UI/NebulaForgeAIStyle.h"
#include "AI/UI/SNebulaForgeAIComposer.h"
#include "AI/UI/SNebulaForgeAIConversationList.h"
#include "AI/UI/SNebulaForgeAIMessageView.h"
#include "AI/UI/SNebulaForgeAISettingsWidget.h"
#include "AI/UI/SNebulaForgeAIToolApproval.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Misc/MessageDialog.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NebulaForgeAIChat"

void SNebulaForgeAIChat::Construct(const FArguments& InArgs)
{
    EnsureActiveConversation();

    // Wire service delegates (all delivered on the game thread).
    FNebulaForgeAIService& Service = FNebulaForgeAIService::Get();
    Service.Conversations()->OnChanged.AddRaw(this, &SNebulaForgeAIChat::OnConversationChanged);
    Service.Coordinator()->OnMessageAdded.AddRaw(this, &SNebulaForgeAIChat::OnMessageAdded);
    Service.Coordinator()->OnMessageUpdated.AddRaw(this, &SNebulaForgeAIChat::OnMessageUpdated);
    Service.Coordinator()->OnStateChanged.AddRaw(this, &SNebulaForgeAIChat::OnCoordinatorStateChanged);
    Service.Tools()->OnApprovalRequested.AddRaw(this, &SNebulaForgeAIChat::OnApprovalRequested);

    ChildSlot
    [
        SNew(SVerticalBox)
        // ---------------- Header ----------------
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(6.0f, 4.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text_Raw(this, &SNebulaForgeAIChat::GetProviderModelText)
                .Font(FAppStyle::GetFontStyle("NormalTextBold"))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(8.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text_Raw(this, &SNebulaForgeAIChat::GetStatusText)
                .ColorAndOpacity_Raw(this, &SNebulaForgeAIChat::GetStatusColor)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(LOCTEXT("Settings", "Settings"))
                .OnClicked_Raw(this, &SNebulaForgeAIChat::OnSettingsClicked)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("NewChat", "New chat"))
                .OnClicked_Raw(this, &SNebulaForgeAIChat::OnNewChatClicked)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("Rename", "Rename"))
                .OnClicked_Raw(this, &SNebulaForgeAIChat::OnRenameClicked)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("Export", "Export"))
                .OnClicked_Raw(this, &SNebulaForgeAIChat::OnExportClicked)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("ClearMessages", "Clear"))
                .ToolTipText(LOCTEXT("ClearTooltip", "Clear messages in this conversation."))
                .OnClicked_Raw(this, &SNebulaForgeAIChat::OnClearMessagesClicked)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("ToggleSidebar", "Sidebar"))
                .OnClicked_Raw(this, &SNebulaForgeAIChat::OnToggleSidebarClicked)
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SSeparator)
        ]
        // ---------------- Body ----------------
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SSplitter)
            + SSplitter::Slot()
            .Value(0.22f)
            [
                SNew(SBorder)
                .Visibility_Raw(this, &SNebulaForgeAIChat::GetSidebarVisibility)
                .BorderImage(FNebulaForgeAIStyle::GetBubbleBrush())
                .Padding(2.0f)
                [
                    SAssignNew(ConversationList, SNebulaForgeAIConversationList)
                    .ActiveConversationId(ActiveConversationId)
                    .OnConversationSelected(FOnNebulaAIConversationSelected::CreateRaw(
                        this, &SNebulaForgeAIChat::OnConversationSelected))
                ]
            ]
            + SSplitter::Slot()
            .Value(0.78f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                .Padding(4.0f)
                [
                    SAssignNew(MessageView, SNebulaForgeAIMessageView)
                    .ConversationId(ActiveConversationId)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(4.0f, 0.0f)
                [
                    SAssignNew(ApprovalBanner, SNebulaForgeAIToolApproval)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(4.0f)
                [
                    SAssignNew(Composer, SNebulaForgeAIComposer)
                    .OnSend(FOnNebulaAIComposerSend::CreateRaw(
                        this, &SNebulaForgeAIChat::OnComposerSend))
                    .OnStop(FOnNebulaAIComposerStop::CreateRaw(
                        this, &SNebulaForgeAIChat::OnComposerStop))
                ]
            ]
        ]
    ];
}

FText SNebulaForgeAIChat::GetProviderModelText() const
{
    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    const FNebulaAIProviderProfile* Profile = Settings ? Settings->GetActiveProfile() : nullptr;
    if (!Profile)
    {
        return LOCTEXT("NoProvider", "NebulaForge AI  ·  no provider configured");
    }
    const FString Kind = NebulaAIProviderKindToString(
        static_cast<ENebulaAIProviderKind>(static_cast<uint8>(Profile->Kind)));
    return FText::FromString(FString::Printf(TEXT("%s  ·  %s  ·  %s"),
        *Profile->DisplayName, *Kind,
        Profile->DefaultModel.IsEmpty() ? TEXT("(no model)") : *Profile->DefaultModel));
}

FText SNebulaForgeAIChat::GetStatusText() const
{
    const FNebulaForgeAIRequestCoordinator::FActiveState& State =
        FNebulaForgeAIService::Get().Coordinator()->GetActiveState();
    return FText::FromString(State.StatusText.IsEmpty() ? FString(TEXT("Ready")) : State.StatusText);
}

FSlateColor SNebulaForgeAIChat::GetStatusColor() const
{
    const FNebulaForgeAIRequestCoordinator::FActiveState& State =
        FNebulaForgeAIService::Get().Coordinator()->GetActiveState();
    if (State.bActive)
    {
        return State.bWaitingForApproval
            ? FNebulaForgeAIStyle::StreamingColor()
            : FNebulaForgeAIStyle::ReadyColor();
    }
    if (State.StatusText == TEXT("Error"))
    {
        return FNebulaForgeAIStyle::ErrorColor();
    }
    return FSlateColor::UseSubduedForeground();
}

FReply SNebulaForgeAIChat::OnSettingsClicked()
{
    SNebulaForgeAISettingsWidget::OpenSettingsWindow();
    return FReply::Handled();
}

FReply SNebulaForgeAIChat::OnNewChatClicked()
{
    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    const FNebulaAIProviderProfile* Profile = Settings ? Settings->GetActiveProfile() : nullptr;
    const FString ConversationId = FNebulaForgeAIService::Get().Conversations()->CreateConversation(
        TEXT("New chat"), Profile ? Profile->Id : FString(), Profile ? Profile->DefaultModel : FString());
    OnConversationSelected(ConversationId);
    return FReply::Handled();
}

FReply SNebulaForgeAIChat::OnRenameClicked()
{
    if (ActiveConversationId.IsEmpty())
    {
        return FReply::Handled();
    }
    // v1 rename via a simple text input window built from the settings
    // widget primitives; conversational title editing.
    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(LOCTEXT("RenameTitle", "Rename conversation"))
        .ClientSize(FVector2D(420.0f, 100.0f));

    TSharedPtr<SEditableTextBox> Input;
    Window->SetContent(SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(6.0f)
        [
            SAssignNew(Input, SEditableTextBox)
            .Text(FText::FromString(
                FNebulaForgeAIService::Get().Conversations()->FindConversation(ActiveConversationId)
                    ? FNebulaForgeAIService::Get().Conversations()->FindConversation(ActiveConversationId)->Meta.Title
                    : FString()))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(6.0f).HAlign(HAlign_Right)
        [
            SNew(SButton)
            .Text(LOCTEXT("RenameOk", "Rename"))
            .OnClicked_Lambda([this, Input, Window]()
            {
                if (Input.IsValid())
                {
                    FNebulaForgeAIService::Get().Conversations()->RenameConversation(
                        ActiveConversationId, Input->GetText().ToString());
                }
                FSlateApplication::Get().RequestDestroyWindow(Window);
                return FReply::Handled();
            })
        ]);

    FSlateApplication::Get().AddWindow(Window, true);
    return FReply::Handled();
}

FReply SNebulaForgeAIChat::OnExportClicked()
{
    if (ActiveConversationId.IsEmpty())
    {
        return FReply::Handled();
    }
    const FString DefaultName = TEXT("NebulaForgeAI-Transcript.md");
    TArray<FString> OutFiles;
    IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
    if (Desktop)
    {
        Desktop->SaveFileDialog(
            FSlateApplication::Get().FindBestParentWindowHandleForDialogs(SharedThis(this)),
            TEXT("Export conversation"),
            FPaths::ProjectSavedDir(),
            DefaultName,
            TEXT("Markdown (*.md)|*.md"),
            0,
            OutFiles);
    }
    if (OutFiles.Num() > 0)
    {
        const bool bOk = FNebulaForgeAIService::Get().Conversations()->ExportConversationMarkdown(
            ActiveConversationId, OutFiles[0]);
        if (!bOk)
        {
            FMessageDialog::Open(EAppMsgType::Ok,
                LOCTEXT("ExportFailed", "Export failed. See the output log for details."));
        }
    }
    return FReply::Handled();
}

FReply SNebulaForgeAIChat::OnClearMessagesClicked()
{
    if (ActiveConversationId.IsEmpty())
    {
        return FReply::Handled();
    }
    if (FMessageDialog::Open(EAppMsgType::YesNo,
        LOCTEXT("ClearConfirm", "Clear all messages in this conversation?")) == EAppReturnType::No)
    {
        return FReply::Handled();
    }
    if (auto Conversation = FNebulaForgeAIService::Get().Conversations()->FindConversation(ActiveConversationId))
    {
        Conversation->Messages.Reset();
        Conversation->Meta.NextSequence = 1;
    }
    if (MessageView.IsValid())
    {
        MessageView->RefreshAll();
    }
    return FReply::Handled();
}

FReply SNebulaForgeAIChat::OnToggleSidebarClicked()
{
    bSidebarVisible = !bSidebarVisible;
    return FReply::Handled();
}

EVisibility SNebulaForgeAIChat::GetSidebarVisibility() const
{
    return bSidebarVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

void SNebulaForgeAIChat::OnConversationSelected(const FString& ConversationId)
{
    ActiveConversationId = ConversationId;
    if (MessageView.IsValid())
    {
        MessageView->SetConversation(ConversationId);
    }
    if (ConversationList.IsValid())
    {
        ConversationList->Refresh();
    }
}

void SNebulaForgeAIChat::OnComposerSend(
    const FString& Text, ENebulaAIInteractionMode Mode, const TArray<FNebulaAIContextChip>& Chips)
{
    EnsureActiveConversation();

    // Collect editor context (opt-in via the composer toggles and privacy
    // settings), then send. Context arrives asynchronously on the game
    // thread; the user message embeds it as fenced JSON blocks.
    TArray<FNebulaForgeAIContextCollector::EContextKind> Kinds = {
        FNebulaForgeAIContextCollector::EContextKind::ProjectInfo,
        FNebulaForgeAIContextCollector::EContextKind::CurrentLevel,
        FNebulaForgeAIContextCollector::EContextKind::EditorMode
    };
    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    if (Settings && Settings->Permissions.bReadSelection)
    {
        Kinds.Add(FNebulaForgeAIContextCollector::EContextKind::SelectedActors);
        Kinds.Add(FNebulaForgeAIContextCollector::EContextKind::SelectedAssets);
    }
    if (Settings && Settings->Privacy.bIncludeOutputLog && Settings->Permissions.bReadOutputLog)
    {
        Kinds.Add(FNebulaForgeAIContextCollector::EContextKind::OutputLog);
    }

    const FString ConversationId = ActiveConversationId;
    const FString UserText = Text;
    FNebulaForgeAIService::Get().Contexts()->Collect(Kinds,
        [this, ConversationId, UserText, Mode](const TArray<FNebulaAIContextChip>& Collected)
        {
            FString Composed = UserText;
            int32 TotalTokens = 0;
            for (const FNebulaAIContextChip& Chip : Collected)
            {
                TotalTokens += Chip.EstimatedTokens;
                Composed += FString::Printf(TEXT("\n\n```context %s\n%s\n```"),
                    *Chip.Label, *Chip.PayloadJson);
            }
            const UNebulaForgeAISettings* SettingsInner = GetDefault<UNebulaForgeAISettings>();
            if (SettingsInner && SettingsInner->Model.ContextWindowWarningTokens > 0 &&
                TotalTokens > SettingsInner->Model.ContextWindowWarningTokens)
            {
                Composed += FString::Printf(
                    TEXT("\n\n[Context size estimate: ~%d tokens; some context may exceed the model window.]"),
                    TotalTokens);
            }
            if (Mode == ENebulaAIInteractionMode::Act)
            {
                Composed += TEXT("\n\n[Mode: Act — mutating tool calls allowed subject to approval.]");
            }
            else if (Mode == ENebulaAIInteractionMode::Plan)
            {
                Composed += TEXT("\n\n[Mode: Plan — read-only; propose a plan before any change.]");
            }

            FNebulaForgeAIService::Get().Coordinator()->SendUserMessage(
                ConversationId, Composed, Mode,
                [this](const FString& Failure)
                {
                    if (Composer.IsValid())
                    {
                        Composer->SetDisabledReason(FText::FromString(Failure));
                    }
                });
        });
}

void SNebulaForgeAIChat::OnComposerStop()
{
    FNebulaForgeAIService::Get().Coordinator()->Cancel(ActiveConversationId);
    if (ApprovalBanner.IsValid())
    {
        ApprovalBanner->Clear();
    }
}

void SNebulaForgeAIChat::EnsureActiveConversation()
{
    if (!ActiveConversationId.IsEmpty() &&
        FNebulaForgeAIService::Get().Conversations()->FindConversation(ActiveConversationId).IsValid())
    {
        return;
    }
    const TArray<FNebulaAIConversationMeta> Metas =
        FNebulaForgeAIService::Get().Conversations()->GetConversationMetas();
    if (Metas.Num() > 0)
    {
        ActiveConversationId = Metas[0].Id;
        return;
    }

    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    const FNebulaAIProviderProfile* Profile = Settings ? Settings->GetActiveProfile() : nullptr;
    ActiveConversationId = FNebulaForgeAIService::Get().Conversations()->CreateConversation(
        TEXT("New chat"), Profile ? Profile->Id : FString(), Profile ? Profile->DefaultModel : FString());
}

void SNebulaForgeAIChat::OnConversationChanged()
{
    if (ConversationList.IsValid())
    {
        ConversationList->Refresh();
    }
}

void SNebulaForgeAIChat::OnMessageAdded(const FString& ConversationId, const FNebulaAIMessage& Message)
{
    if (ConversationId != ActiveConversationId)
    {
        return;
    }
    if (MessageView.IsValid())
    {
        MessageView->RefreshAll();
    }
    if (Composer.IsValid())
    {
        Composer->SetBusy(FNebulaForgeAIService::Get().Coordinator()->IsBusy(ConversationId));
    }
}

void SNebulaForgeAIChat::OnMessageUpdated(const FString& ConversationId, const FNebulaAIMessage& Message)
{
    if (ConversationId != ActiveConversationId)
    {
        return;
    }
    if (MessageView.IsValid())
    {
        MessageView->RefreshMessage(Message.Id);
    }
}

void SNebulaForgeAIChat::OnCoordinatorStateChanged()
{
    const FNebulaForgeAIRequestCoordinator::FActiveState& State =
        FNebulaForgeAIService::Get().Coordinator()->GetActiveState();
    if (Composer.IsValid())
    {
        Composer->SetBusy(State.bActive);
    }
}

void SNebulaForgeAIChat::OnApprovalRequested(const FNebulaForgeAIToolGateway::FPendingApproval& Approval)
{
    if (Approval.ConversationId != ActiveConversationId)
    {
        return;
    }
    if (ApprovalBanner.IsValid())
    {
        ApprovalBanner->ShowApproval(Approval);
    }
}

#undef LOCTEXT_NAMESPACE

SNebulaForgeAIChat::~SNebulaForgeAIChat()
{
    // Unsubscribe from service delegates so late broadcasts never hit a
    // destroyed widget. Services may already be shut down during editor
    // exit; null-check every access.
    FNebulaForgeAIService& Service = FNebulaForgeAIService::Get();
    if (FNebulaForgeAIConversationService* Conversations = Service.Conversations())
    {
        Conversations->OnChanged.RemoveAll(this);
    }
    if (FNebulaForgeAIRequestCoordinator* Coordinator = Service.Coordinator())
    {
        Coordinator->OnMessageAdded.RemoveAll(this);
        Coordinator->OnMessageUpdated.RemoveAll(this);
        Coordinator->OnStateChanged.RemoveAll(this);
    }
    if (FNebulaForgeAIToolGateway* Tools = Service.Tools())
    {
        Tools->OnApprovalRequested.RemoveAll(this);
    }
}
