// =============================================================================
// SNebulaForgeAISettingsWidget.h
// =============================================================================
// Provider profile editor, model defaults, Unreal permissions, privacy,
// and diagnostics (plan section 3.3). Opened as a modal-ish SWindow from the
// chat header and first-run flow. Secrets are entered once and stored in the
// platform credential store; only masked values are shown afterwards.
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "NebulaForgeAIModels.h"
#include "NebulaForgeAISettings.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class STextBlock;

class SNebulaForgeAISettingsWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SNebulaForgeAISettingsWidget) {}
    SLATE_EVENT(FSimpleDelegate, OnSettingsChanged)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Open the settings in a floating window; focuses an existing one. */
    static void OpenSettingsWindow();

    /** Static entry point usable from menu/console actions. */
    static void OpenSettingsWindowStatic() { OpenSettingsWindow(); }

private:
    void LoadFromSettings();
    void SaveProfile();
    void OnApiKeyChanged(const FText& NewText);
    FReply OnTestConnectionClicked();
    FReply OnSaveClicked();
    FReply OnNewProfileClicked(ENebulaAIProviderKind Kind);
    FReply OnDeleteProfileClicked();
    FReply OnClearConversationsClicked();
    FReply OnClearCredentialsClicked();
    FText GetTestResultText() const;
    FText GetSecretStatusText() const;
    FSlateColor GetTestResultColor() const;
    void OnActiveProfileChanged(TSharedPtr<FString> ProfileLabel, ESelectInfo::Type);
    void RefreshProfilePicker();

    FSimpleDelegate OnSettingsChanged;

    /** Active profile editing state. */
    FString EditingProfileId;
    FNebulaAIProviderProfile EditingProfile;
    FString EnteredApiKey;
    bool bApiKeyEntered = false;

    TSharedPtr<class SComboBox<TSharedPtr<FString>>> ProfilePicker;
    TArray<TSharedPtr<FString>> ProfileOptions;

    TSharedPtr<SEditableTextBox> NameBox;
    TSharedPtr<SEditableTextBox> BaseUrlBox;
    TSharedPtr<SEditableTextBox> ModelBox;
    TSharedPtr<SEditableTextBox> ApiKeyBox;
    TSharedPtr<SEditableTextBox> OrgBox;
    TSharedPtr<SEditableTextBox> ProjectBox;
    TSharedPtr<SEditableTextBox> InstructionsBox;

    TSharedPtr<STextBlock> TestResultText;
    bool bTestInProgress = false;
    bool bTestSucceeded = false;
    FString TestResultMessage;
};
