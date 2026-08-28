// =============================================================================
// NebulaForgeAIStyle.h
// =============================================================================
// Minimal style helpers for the AI chat widgets. Uses editor style brushes
// and inline colors rather than a custom FSlateStyleSet so the plugin stays
// self-contained across UE 5.0-5.8.
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Styling/AppStyle.h"

struct FNebulaForgeAIStyle
{
    static FSlateColor UserBubbleColor() { return FSlateColor(FLinearColor(0.06f, 0.09f, 0.14f, 0.8f)); }
    static FSlateColor AssistantBubbleColor() { return FSlateColor(FLinearColor(0.10f, 0.11f, 0.12f, 0.6f)); }
    static FSlateColor ToolBubbleColor() { return FSlateColor(FLinearColor(0.08f, 0.14f, 0.10f, 0.7f)); }
    static FSlateColor ErrorBubbleColor() { return FSlateColor(FLinearColor(0.32f, 0.06f, 0.06f, 0.8f)); }
    static FSlateColor StreamingColor() { return FSlateColor(FLinearColor(0.95f, 0.75f, 0.2f)); }
    static FSlateColor ReadyColor() { return FSlateColor(FLinearColor(0.2f, 0.85f, 0.2f)); }
    static FSlateColor ErrorColor() { return FSlateColor(FLinearColor(0.95f, 0.3f, 0.25f)); }

    static const FSlateBrush* GetBubbleBrush() { return FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"); }
    static const FSlateBrush* GetButtonStyle() { return FAppStyle::Get().GetBrush("NoBorder"); }
};
