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
#include "EdMode.h"
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
    virtual bool CapturedMouseMove(FEditorViewportClient *InViewportClient, FViewport *InViewport,
                                   int32 InMouseX, int32 InMouseY) override;
    virtual bool InputKey(FEditorViewportClient *ViewportClient, FViewport *Viewport,
                          FKey Key, EInputEvent Event) override;
    virtual void Render(const FSceneView *View, FViewport *Viewport,
                        FPrimitiveDrawInterface *PDI) override;
    virtual bool UsesToolkits() const override { return true; }

    /** Registers the mode with the editor mode registry (call at startup). */
    static void RegisterMode();
    /** Unregisters the mode (call at shutdown). */
    static void UnregisterMode();

private:
    /** Paints one spaced dab from screen coordinates (shared by move paths). */
    bool PaintAtCursor(FEditorViewportClient *ViewportClient, int32 x, int32 y);
    /** Refreshes the hover cursor ring without consuming input. */
    void UpdateBrushCursor(FEditorViewportClient *ViewportClient, int32 x, int32 y);
    /** Applies one dab when the stroke cursor moved far enough. */
    bool ApplyDabAtCursor(FEditorViewportClient *ViewportClient, const FVector &Origin,
                          const FVector &Direction);
    /** Drops a road spline draft point at the current mouse position. */
    void AppendRoadPointAtMouse(FEditorViewportClient *ViewportClient, FViewport *Viewport);
    /** True while the camera or a transform widget owns the mouse. */
    bool IsCameraOrWidgetDrag(FEditorViewportClient *ViewportClient, FViewport *Viewport) const;
    /** Begins a stroke transaction when none is active. */
    void BeginStroke();
    /** Ends the active stroke, persists edits, returns false when idle. */
    bool EndStrokeAndSave();

    /** Traces the cursor ray against the world. */
    bool TraceCursor(UWorld *World, const FVector &Origin, const FVector &Direction,
                     FHitResult &OutHit) const;

    FVector BrushCursor = FVector::ZeroVector;
    bool bHasBrushCursor = false;
    int32 StrokeSalt = 0;
    bool bLoggedFirstDab = false;
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
