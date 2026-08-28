// =============================================================================
// SNebulaForgeAIComposer.h
// =============================================================================
// Multiline input with Enter-to-send preference, stop button during
// streaming, context-chip attachment controls, and per-message mode
// (Ask / Plan / Act) (plan section 3.2).
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "NebulaForgeAIContextCollector.h"
#include "NebulaForgeAIModels.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_ThreeParams(FOnNebulaAIComposerSend,
    const FString& /*Text*/,
    ENebulaAIInteractionMode /*Mode*/,
    const TArray<FNebulaAIContextChip>& /*Chips*/);
DECLARE_DELEGATE(FOnNebulaAIComposerStop);

class SNebulaForgeAIComposer : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SNebulaForgeAIComposer) {}
    SLATE_EVENT(FOnNebulaAIComposerSend, OnSend)
    SLATE_EVENT(FOnNebulaAIComposerStop, OnStop)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Enable/disable input while a request is streaming. */
    void SetBusy(bool bInBusy);

    /** Explain why the composer is disabled (no provider etc.). */
    void SetDisabledReason(const FText& Reason);

private:
    FReply OnSendClicked();
    FReply OnStopClicked();
    void OnTextCommitted(const FText& Text, ETextCommit::Type CommitType);

    bool IsEnabledCheck() const;
    FText GetSendButtonText() const;
    FText GetStatusText() const;

    void ToggleContextKind(FNebulaForgeAIContextCollector::EContextKind Kind);
    FText GetContextToggleLabel(FNebulaForgeAIContextCollector::EContextKind Kind) const;

    using FModeOption = TSharedPtr<FString>;
    TArray<FModeOption> ModeOptions;

    TSharedPtr<class SMultiLineEditableTextBox> InputBox;
    FOnNebulaAIComposerSend OnSend;
    FOnNebulaAIComposerStop OnStop;

    bool bBusy = false;
    FText DisabledReason;
    ENebulaAIInteractionMode Mode = ENebulaAIInteractionMode::Ask;

    /** Context kinds the user toggled on for the next message. */
    TArray<FNebulaForgeAIContextCollector::EContextKind> SelectedContextKinds;
};
