// =============================================================================
// SNebulaForgeAIToolApproval.h
// =============================================================================
// Approval gate shown before the AI may execute a mutating Unreal operation
// (plan section 6.3): tool name, purpose, target summary, risk label, and
// Allow once / Allow for this conversation / Deny decisions.
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "NebulaForgeAIToolGateway.h"
#include "Widgets/SCompoundWidget.h"

class SNebulaForgeAIToolApproval : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SNebulaForgeAIToolApproval) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Show the given pending approval; hidden when none. */
    void ShowApproval(const FNebulaForgeAIToolGateway::FPendingApproval& Approval);

    /** Hide without resolving (used on cancellation). */
    void Clear();

private:
    FReply OnAllowOnce();
    FReply OnAllowForConversation();
    FReply OnDeny();

    FText GetTitleText() const;
    FText GetDetailText() const;
    EVisibility GetBannerVisibility() const;

    TOptional<FNebulaForgeAIToolGateway::FPendingApproval> CurrentApproval;
};
