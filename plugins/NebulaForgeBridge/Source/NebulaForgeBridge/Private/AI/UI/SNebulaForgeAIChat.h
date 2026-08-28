// =============================================================================
// SNebulaForgeAIChat.h
// =============================================================================
// Root dockable chat widget (plan section 3.2): header with provider/model
// and status, collapsible conversation sidebar, message timeline, approval
// banner, and composer. All state changes arrive on the game thread through
// the AI service delegates.
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "NebulaForgeAIModels.h"
#include "NebulaForgeAIToolGateway.h"
#include "Widgets/SCompoundWidget.h"

class SNebulaForgeAIConversationList;
class SNebulaForgeAIMessageView;
class SNebulaForgeAIComposer;
class SNebulaForgeAIToolApproval;

class SNebulaForgeAIChat : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SNebulaForgeAIChat) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual ~SNebulaForgeAIChat() override;

private:
    // Header
    FText GetProviderModelText() const;
    FText GetStatusText() const;
    FSlateColor GetStatusColor() const;
    FReply OnSettingsClicked();
    FReply OnNewChatClicked();
    FReply OnRenameClicked();
    FReply OnExportClicked();
    FReply OnClearMessagesClicked();
    FReply OnToggleSidebarClicked();
    EVisibility GetSidebarVisibility() const;

    // Conversation / send flow
    void OnConversationSelected(const FString& ConversationId);
    void OnComposerSend(const FString& Text, ENebulaAIInteractionMode Mode,
        const TArray<FNebulaAIContextChip>& Chips);
    void OnComposerStop();
    void EnsureActiveConversation();

    // Service callbacks (game thread)
    void OnConversationChanged();
    void OnMessageAdded(const FString& ConversationId, const FNebulaAIMessage& Message);
    void OnMessageUpdated(const FString& ConversationId, const FNebulaAIMessage& Message);
    void OnCoordinatorStateChanged();
    void OnApprovalRequested(const FNebulaForgeAIToolGateway::FPendingApproval& Approval);

    FString ActiveConversationId;
    bool bSidebarVisible = true;

    TSharedPtr<SNebulaForgeAIConversationList> ConversationList;
    TSharedPtr<SNebulaForgeAIMessageView> MessageView;
    TSharedPtr<SNebulaForgeAIComposer> Composer;
    TSharedPtr<SNebulaForgeAIToolApproval> ApprovalBanner;
};
