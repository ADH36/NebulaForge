#include "AI/UI/SNebulaForgeAISettingsWidget.h"
#include "AI/NebulaForgeAIDiagnostics.h"
#include "AI/NebulaForgeAIProviderService.h"
#include "AI/NebulaForgeAISecretStore.h"
#include "AI/NebulaForgeAIService.h"
#include "AI/NebulaForgeAIConversationService.h"
#include "AI/UI/NebulaForgeAIStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Misc/MessageDialog.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NebulaForgeAISettingsWidget"

namespace
{
    TSharedRef<SWidget> MakeSectionLabel(const FText& Text)
    {
        return SNew(STextBlock)
            .Text(Text)
            .Font(FAppStyle::GetFontStyle("NormalTextBold"));
    }
}

void SNebulaForgeAISettingsWidget::Construct(const FArguments& InArgs)
{
    OnSettingsChanged = InArgs._OnSettingsChanged;

    LoadFromSettings();

    ChildSlot
    [
        SNew(SScrollBox)
        // ---------------- Provider section ----------------
        + SScrollBox::Slot()
        .Padding(6.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight() [ MakeSectionLabel(LOCTEXT("ProviderSection", "Provider profile")) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    SAssignNew(ProfilePicker, SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&ProfileOptions)
                    .OnSelectionChanged_Raw(this, &SNebulaForgeAISettingsWidget::OnActiveProfileChanged)
                    .Content()
                    [
                        SNew(STextBlock).Text(LOCTEXT("ProfilePickerHint", "Select profile"))
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("NewOpenAI", "New OpenAI"))
                    .OnClicked_Raw(this, &SNebulaForgeAISettingsWidget::OnNewProfileClicked,
                        ENebulaAIProviderKind::OpenAI)
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("NewCompatible", "New OpenAI-compatible"))
                    .OnClicked_Raw(this, &SNebulaForgeAISettingsWidget::OnNewProfileClicked,
                        ENebulaAIProviderKind::OpenAICompatible)
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("NewCodex", "New Codex"))
                    .ToolTipText(LOCTEXT("NewCodexTooltip",
                        "Codex models run through the OpenAI Responses API."))
                    .OnClicked_Raw(this, &SNebulaForgeAISettingsWidget::OnNewProfileClicked,
                        ENebulaAIProviderKind::CodexResponses)
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
            [
                SNew(STextBlock).Text(LOCTEXT("DisplayNameLabel", "Display name"))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SAssignNew(NameBox, SEditableTextBox).Text(FText::FromString(EditingProfile.DisplayName))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
            [
                SNew(STextBlock).Text(LOCTEXT("BaseUrlLabel", "Base URL (e.g. https://api.openai.com/v1)"))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SAssignNew(BaseUrlBox, SEditableTextBox).Text(FText::FromString(EditingProfile.BaseUrl))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
            [
                SNew(STextBlock).Text(LOCTEXT("ModelLabel", "Default model"))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SAssignNew(ModelBox, SEditableTextBox).Text(FText::FromString(EditingProfile.DefaultModel))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ApiKeyLabel", "API key (stored in the OS credential store; never in config)"))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    SAssignNew(ApiKeyBox, SEditableTextBox)
                    .IsPassword(true)
                    .HintText(LOCTEXT("ApiKeyHint", "Paste key; saved value stays masked"))
                    .OnTextChanged_Raw(this, &SNebulaForgeAISettingsWidget::OnApiKeyChanged)
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f).VAlign(VAlign_Center)
                [
                    SNew(STextBlock).Text_Raw(this, &SNebulaForgeAISettingsWidget::GetSecretStatusText)
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
            [
                SNew(STextBlock).Text(LOCTEXT("OrgLabel", "Organization (optional)"))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SAssignNew(OrgBox, SEditableTextBox).Text(FText::FromString(EditingProfile.OrganizationId))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
            [
                SNew(STextBlock).Text(LOCTEXT("ProjectLabel", "Project (optional)"))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SAssignNew(ProjectBox, SEditableTextBox).Text(FText::FromString(EditingProfile.ProjectId))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("TestConnection", "Test connection"))
                    .OnClicked_Raw(this, &SNebulaForgeAISettingsWidget::OnTestConnectionClicked)
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("SaveProfile", "Save profile"))
                    .OnClicked_Raw(this, &SNebulaForgeAISettingsWidget::OnSaveClicked)
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("DeleteProfile", "Delete profile"))
                    .OnClicked_Raw(this, &SNebulaForgeAISettingsWidget::OnDeleteProfileClicked)
                ]
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(6.0f, 0.0f).VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text_Raw(this, &SNebulaForgeAISettingsWidget::GetTestResultText)
                    .ColorAndOpacity_Raw(this, &SNebulaForgeAISettingsWidget::GetTestResultColor)
                ]
            ]
        ]
        // ---------------- Model defaults ----------------
        + SScrollBox::Slot()
        .Padding(6.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight() [ MakeSectionLabel(LOCTEXT("ModelSection", "Model defaults")) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
            [
                SNew(STextBlock).Text(LOCTEXT("SystemInstructionsLabel", "System instructions (optional)"))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SAssignNew(InstructionsBox, SEditableTextBox)
                .Text(FText::FromString(GetDefault<UNebulaForgeAISettings>()->Model.SystemInstructions))
                .HintText(LOCTEXT("SystemInstructionsHint",
                    "Leave empty to use provider-aware defaults (Unreal conventions, approval before mutations)."))
            ]
        ]
        // ---------------- Privacy / storage ----------------
        + SScrollBox::Slot()
        .Padding(6.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight() [ MakeSectionLabel(LOCTEXT("PrivacySection", "Privacy and storage")) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("ClearConversations", "Clear all local conversations"))
                    .OnClicked_Raw(this, &SNebulaForgeAISettingsWidget::OnClearConversationsClicked)
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("ClearCredentials", "Clear all saved credentials"))
                    .OnClicked_Raw(this, &SNebulaForgeAISettingsWidget::OnClearCredentialsClicked)
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("PrivacyNote",
                    "Provider API usage is billed by your provider. Keys and conversations stay on this machine. Mutating Unreal operations always require approval in chat; enable them in Project Settings -> NebulaForge AI Chat -> Permissions."))
                .AutoWrapText(true)
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            ]
        ]
    ];

    RefreshProfilePicker();
}

void SNebulaForgeAISettingsWidget::LoadFromSettings()
{
    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    const FNebulaAIProviderProfile* Profile = Settings ? Settings->GetActiveProfile() : nullptr;
    if (Profile)
    {
        EditingProfile = *Profile;
        EditingProfileId = Profile->Id;
    }
    else
    {
        EditingProfile = FNebulaAIProviderProfile();
        EditingProfileId.Reset();
    }
}

void SNebulaForgeAISettingsWidget::SaveProfile()
{
    if (EditingProfileId.IsEmpty())
    {
        return;
    }

    EditingProfile.Id = EditingProfileId;
    EditingProfile.DisplayName = NameBox.IsValid() ? NameBox->GetText().ToString() : EditingProfile.DisplayName;
    EditingProfile.BaseUrl = BaseUrlBox.IsValid() ? BaseUrlBox->GetText().ToString() : EditingProfile.BaseUrl;
    EditingProfile.DefaultModel = ModelBox.IsValid() ? ModelBox->GetText().ToString() : EditingProfile.DefaultModel;
    EditingProfile.OrganizationId = OrgBox.IsValid() ? OrgBox->GetText().ToString() : EditingProfile.OrganizationId;
    EditingProfile.ProjectId = ProjectBox.IsValid() ? ProjectBox->GetText().ToString() : EditingProfile.ProjectId;

    FNebulaForgeAIProviderService::UpsertProfile(EditingProfile);

    if (bApiKeyEntered && !EnteredApiKey.IsEmpty())
    {
        const FString Reference = FNebulaAISecretStore::StoreSecret(EditingProfileId, EnteredApiKey);
        if (!Reference.IsEmpty())
        {
            UNebulaForgeAISettings* Settings = GetMutableDefault<UNebulaForgeAISettings>();
            if (FNebulaAIProviderProfile* Stored = Settings->FindMutableProfile(EditingProfileId))
            {
                Stored->ApiSecretReference = Reference;
            }
            Settings->SaveConfig();
        }
    }

    if (UNebulaForgeAISettings* Settings = GetMutableDefault<UNebulaForgeAISettings>())
    {
        Settings->ActiveProfileId = EditingProfileId;
        if (InstructionsBox.IsValid())
        {
            Settings->Model.SystemInstructions = InstructionsBox->GetText().ToString();
        }
        Settings->bFirstRunComplete = true;
        Settings->SaveConfig();
    }

    // Clear the transient key buffer best-effort.
    EnteredApiKey.Reset();
    bApiKeyEntered = false;
    if (ApiKeyBox.IsValid())
    {
        ApiKeyBox->SetText(FText::GetEmpty());
    }

    OnSettingsChanged.ExecuteIfBound();
}

void SNebulaForgeAISettingsWidget::OnApiKeyChanged(const FText& NewText)
{
    EnteredApiKey = NewText.ToString();
    bApiKeyEntered = !EnteredApiKey.TrimStartAndEnd().IsEmpty();
}

FReply SNebulaForgeAISettingsWidget::OnTestConnectionClicked()
{
    if (EditingProfileId.IsEmpty())
    {
        return FReply::Handled();
    }

    // Persist current edits first so the transport uses them.
    SaveProfile();

    bTestInProgress = true;
    bTestSucceeded = false;
    TestResultMessage = TEXT("Testing...");

    FString Error;
    TSharedPtr<INebulaAIProviderTransport> Transport =
        FNebulaForgeAIProviderService::CreateTransport(EditingProfileId, Error);
    if (!Transport.IsValid())
    {
        bTestInProgress = false;
        TestResultMessage = Error;
        return FReply::Handled();
    }

    Transport->TestConnection([this](bool bOk, const FString& Message)
    {
        bTestInProgress = false;
        bTestSucceeded = bOk;
        TestResultMessage = Message;
    });
    return FReply::Handled();
}

FReply SNebulaForgeAISettingsWidget::OnSaveClicked()
{
    SaveProfile();
    return FReply::Handled();
}

FReply SNebulaForgeAISettingsWidget::OnNewProfileClicked(ENebulaAIProviderKind Kind)
{
    const FString NewId = FNebulaForgeAIProviderService::CreatePresetProfile(Kind);
    if (!NewId.IsEmpty())
    {
        EditingProfileId = NewId;
        const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
        if (const FNebulaAIProviderProfile* Profile = Settings->FindProfile(NewId))
        {
            EditingProfile = *Profile;
        }
        RefreshProfilePicker();
        if (NameBox.IsValid()) NameBox->SetText(FText::FromString(EditingProfile.DisplayName));
        if (BaseUrlBox.IsValid()) BaseUrlBox->SetText(FText::FromString(EditingProfile.BaseUrl));
        if (ModelBox.IsValid()) ModelBox->SetText(FText::FromString(EditingProfile.DefaultModel));
    }
    return FReply::Handled();
}

FReply SNebulaForgeAISettingsWidget::OnDeleteProfileClicked()
{
    if (EditingProfileId.IsEmpty())
    {
        return FReply::Handled();
    }
    if (FSlateApplication::IsInitialized() &&
        FMessageDialog::Open(EAppMsgType::YesNo,
            LOCTEXT("DeleteProfileConfirm", "Delete this provider profile and its stored key?")) ==
            EAppReturnType::No)
    {
        return FReply::Handled();
    }

    FNebulaForgeAIProviderService::DeleteProfile(EditingProfileId);
    LoadFromSettings();
    RefreshProfilePicker();
    if (NameBox.IsValid()) NameBox->SetText(FText::FromString(EditingProfile.DisplayName));
    if (BaseUrlBox.IsValid()) BaseUrlBox->SetText(FText::FromString(EditingProfile.BaseUrl));
    if (ModelBox.IsValid()) ModelBox->SetText(FText::FromString(EditingProfile.DefaultModel));
    OnSettingsChanged.ExecuteIfBound();
    return FReply::Handled();
}

FReply SNebulaForgeAISettingsWidget::OnClearConversationsClicked()
{
    if (FSlateApplication::IsInitialized() &&
        FMessageDialog::Open(EAppMsgType::YesNo,
            LOCTEXT("ClearConversationsConfirm", "Delete ALL locally stored conversations?")) ==
            EAppReturnType::No)
    {
        return FReply::Handled();
    }
    FNebulaForgeAIService::Get().Conversations()->DeleteAll();
    return FReply::Handled();
}

FReply SNebulaForgeAISettingsWidget::OnClearCredentialsClicked()
{
    if (FSlateApplication::IsInitialized() &&
        FMessageDialog::Open(EAppMsgType::YesNo,
            LOCTEXT("ClearCredentialsConfirm", "Remove all NebulaForge AI credentials from this machine?")) ==
            EAppReturnType::No)
    {
        return FReply::Handled();
    }
    FNebulaAISecretStore::ClearAll();
    return FReply::Handled();
}

FText SNebulaForgeAISettingsWidget::GetTestResultText() const
{
    return FText::FromString(TestResultMessage);
}

FText SNebulaForgeAISettingsWidget::GetSecretStatusText() const
{
    if (bApiKeyEntered)
    {
        return LOCTEXT("SecretEntered", "(new key will be saved)");
    }
    if (!EditingProfileId.IsEmpty() && FNebulaAISecretStore::HasSecret(EditingProfileId))
    {
        return FNebulaAISecretStore::LastUsedFallback()
            ? LOCTEXT("SecretFallback", "(saved · encrypted local fallback)")
            : LOCTEXT("SecretSaved", "(saved)");
    }
    return LOCTEXT("SecretMissing", "(no key saved)");
}

FSlateColor SNebulaForgeAISettingsWidget::GetTestResultColor() const
{
    if (bTestInProgress) return FNebulaForgeAIStyle::StreamingColor();
    if (TestResultMessage.IsEmpty()) return FSlateColor::UseForeground();
    return bTestSucceeded ? FNebulaForgeAIStyle::ReadyColor() : FNebulaForgeAIStyle::ErrorColor();
}

void SNebulaForgeAISettingsWidget::OnActiveProfileChanged(TSharedPtr<FString> ProfileLabel, ESelectInfo::Type)
{
    if (!ProfileLabel.IsValid())
    {
        return;
    }
    // Map the display name back to its profile and load it for editing.
    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    if (!Settings)
    {
        return;
    }
    for (const FNebulaAIProviderProfile& Profile : Settings->ProviderProfiles)
    {
        const FString Label = Profile.DisplayName.IsEmpty() ? Profile.Id : Profile.DisplayName;
        if (Label == *ProfileLabel)
        {
            EditingProfile = Profile;
            EditingProfileId = Profile.Id;
            if (NameBox.IsValid()) NameBox->SetText(FText::FromString(Profile.DisplayName));
            if (BaseUrlBox.IsValid()) BaseUrlBox->SetText(FText::FromString(Profile.BaseUrl));
            if (ModelBox.IsValid()) ModelBox->SetText(FText::FromString(Profile.DefaultModel));
            if (OrgBox.IsValid()) OrgBox->SetText(FText::FromString(Profile.OrganizationId));
            if (ProjectBox.IsValid()) ProjectBox->SetText(FText::FromString(Profile.ProjectId));
            break;
        }
    }
}

void SNebulaForgeAISettingsWidget::RefreshProfilePicker()
{
    ProfileOptions.Reset();
    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    if (Settings)
    {
        for (const FNebulaAIProviderProfile& Profile : Settings->ProviderProfiles)
        {
            ProfileOptions.Add(MakeShared<FString>(
                Profile.DisplayName.IsEmpty() ? Profile.Id : Profile.DisplayName));
        }
    }
    if (ProfilePicker.IsValid())
    {
        ProfilePicker->RefreshOptions();
    }
}

void SNebulaForgeAISettingsWidget::OpenSettingsWindow()
{
    const FString WindowTitle = TEXT("NebulaForge AI Settings");
    if (FSlateApplication::IsInitialized())
    {
        for (const TSharedRef<SWindow>& Window : FSlateApplication::Get().GetInteractiveTopLevelWindows())
        {
            if (Window->GetTitle().ToString() == WindowTitle)
            {
                Window->BringToFront();
                return;
            }
        }
    }

    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(FText::FromString(WindowTitle))
        .ClientSize(FVector2D(640.0f, 560.0f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    Window->SetContent(SNew(SNebulaForgeAISettingsWidget));

    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().AddWindow(Window, true);
    }
}

#undef LOCTEXT_NAMESPACE
