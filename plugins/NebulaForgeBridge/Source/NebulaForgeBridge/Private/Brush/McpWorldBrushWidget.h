// =============================================================================
// McpWorldBrushWidget.h
// =============================================================================
// Shared Slate controls for the interactive world brush. The same control set
// is embedded in the brush mode toolkit (Modes panel) and the Generate World
// tab. All controls bind to a process-wide shared FMcpWorldBrushSettings so
// both surfaces stay in sync.
//
// Copyright (c) 2024 NebulaForge Bridge Contributors
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Brush/McpWorldBrushOps.h"
#include "Widgets/SCompoundWidget.h"

/** Process-wide brush settings shared by the toolkit and the world panel. */
FMcpWorldBrushSettings &McpWorldBrushGetSettings();

/** Brush stroke state owned by the active editor mode. */
FMcpWorldBrushStrokeState &McpWorldBrushGetStrokeState();

/** Builds the shared brush settings control column. */
TSharedRef<SWidget> MakeMcpWorldBrushSettingsWidget();

/** Human-readable tool option labels in combo order matching EMcpWorldBrushTool. */
const TArray<FString> &McpWorldBrushToolLabels();
