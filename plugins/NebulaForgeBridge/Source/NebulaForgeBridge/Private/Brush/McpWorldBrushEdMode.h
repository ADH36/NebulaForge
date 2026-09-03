// =============================================================================
// McpWorldBrushEdMode.h
// =============================================================================
// Interactive world-builder brush editor mode for NebulaForge Bridge.
//
// Activate from the Modes panel ("World Brush"), the Generate World tab, or
// programmatically via GLevelEditorModeTools().ActivateMode(). While active,
// left-mouse drags in a perspective/ortho viewport apply brush dabs to the
// landscape under the cursor using the shared FMcpWorldBrushSettings:
// Raise/Lower/Flatten/Smooth height tools, additive layer painting, and
// accumulating HISM foliage scatter.
//
// Copyright (c) 2024 NebulaForge Bridge Contributors
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Brush/McpWorldBrushOps.h"
#include "EditorModes.h"
#include "Toolkits/BaseToolkit.h"

class FEditorViewportClient;
class FPrimitiveDrawInterface;
class FSceneView;
class FViewport;

/** Editor mode id for the world brush. */
#define MCP_WORLD_BRUSH_MODE_ID TEXT("EM_McpWorldBrush")

class FMcpWorldBrushEdMode : public FEdMode
{
public:
    const static FEditorModeID EM_McpWorldBrush;

    FMcpWorldBrushEdMode();
    virtual ~FMcpWorldBrushEdMode();

    // FEdMode interface
    virtual void Enter() override;
    virtual void Exit() override;
    virtual bool MouseMove(FEditorViewportClient *ViewportClient, FViewport *Viewport,
                           int32 x, int32 y) override;
    virtual bool StartTracking(FEditorViewportClient *InViewportClient, FViewport *InViewport) override;
    virtual bool EndTracking(FEditorViewportClient *InViewportClient, FViewport *InViewport) override;
    virtual void Render(const FSceneView *View, FViewport *Viewport,
                        FPrimitiveDrawInterface *PDI) override;
    virtual bool UsesToolkits() const override { return true; }

    /** Registers the mode with the editor mode registry (call at startup). */
    static void RegisterMode();
    /** Unregisters the mode (call at shutdown). */
    static void UnregisterMode();

private:
    /** Applies one dab when the stroke cursor moved far enough. */
    bool ApplyDabAtCursor(FEditorViewportClient *ViewportClient, const FVector &Origin,
                          const FVector &Direction);

    /** Traces the cursor ray against the world. */
    bool TraceCursor(UWorld *World, const FVector &Origin, const FVector &Direction,
                     FHitResult &OutHit) const;

    FVector BrushCursor = FVector::ZeroVector;
    bool bHasBrushCursor = false;
    int32 StrokeSalt = 0;
    static int32 NextStrokeSalt;
};

/** Modes-panel toolkit hosting the shared brush settings widget. */
class FMcpWorldBrushEdModeToolkit : public FModeToolkit
{
public:
    FMcpWorldBrushEdModeToolkit();

    // FModeToolkit interface
    virtual void Init(const TSharedPtr<IToolkitHost> &InitHost) override;
    virtual FName GetToolkitFName() const override;
    virtual FText GetBaseToolkitName() const override;
    virtual TSharedPtr<SWidget> GetInlineContent() const override { return ToolkitWidget; }

private:
    TSharedPtr<SWidget> ToolkitWidget;
};
