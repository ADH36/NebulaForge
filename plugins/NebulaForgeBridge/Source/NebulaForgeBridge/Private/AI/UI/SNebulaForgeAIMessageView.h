// =============================================================================
// SNebulaForgeAIMessageView.h
// =============================================================================
// Virtualized-friendly message timeline. Distinct styles for user,
// assistant, tool-call, approval, and error messages; copy actions for code
// blocks; streaming updates rebuild only the affected message row
// (plan section 3.2).
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "NebulaForgeAIModels.h"
#include "Widgets/SCompoundWidget.h"

class SScrollBox;

class SNebulaForgeAIMessageView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SNebulaForgeAIMessageView) {}
    SLATE_ARGUMENT(FString, ConversationId)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Point the view at another conversation and rebuild. */
    void SetConversation(const FString& ConversationId);

    /** Rebuild a single message widget (streaming update). */
    void RefreshMessage(const FString& MessageId);

    /** Full rebuild (new message, conversation switch). */
    void RefreshAll();

    /** Scroll to the newest message. */
    void ScrollToBottom();

private:
    /** Build one message row widget. */
    TSharedRef<SWidget> BuildMessageRow(const FNebulaAIMessage& Message) const;

    /** Plain-text render with simple code-fence extraction for copy. */
    TSharedRef<SWidget> BuildMessageContent(const FNebulaAIMessage& Message) const;

    FString ConversationId;
    TSharedPtr<SScrollBox> ScrollBox;
    TMap<FString, TSharedPtr<SWidget>> MessageRows;
};
