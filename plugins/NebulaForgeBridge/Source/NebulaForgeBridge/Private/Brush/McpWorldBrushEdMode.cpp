// =============================================================================
// McpWorldBrushEdMode.cpp
// =============================================================================

#include "Brush/McpWorldBrushEdMode.h"

#include "Brush/McpWorldBrushWidget.h"
#include "EditorModeManager.h"
#include "EditorModeRegistry.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Landscape.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "McpWorldBrushEdMode"

DEFINE_LOG_CATEGORY_STATIC(LogMcpWorldBrushMode, Log, All);

const FEditorModeID FMcpWorldBrushEdMode::EM_McpWorldBrush = MCP_WORLD_BRUSH_MODE_ID;
int32 FMcpWorldBrushEdMode::NextStrokeSalt = 1;

FMcpWorldBrushEdMode::FMcpWorldBrushEdMode()
{
    // Mode identity is assigned by FEditorModeRegistry on registration.
}

FMcpWorldBrushEdMode::~FMcpWorldBrushEdMode()
{
}

void FMcpWorldBrushEdMode::RegisterMode()
{
    FEditorModeRegistry::Get().RegisterMode<FMcpWorldBrushEdMode>(
        EM_McpWorldBrush,
        NSLOCTEXT("McpWorldBrush", "ModeName", "World Brush"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Stats"),
        true, 400);
}

void FMcpWorldBrushEdMode::UnregisterMode()
{
    FEditorModeRegistry::Get().UnregisterMode(EM_McpWorldBrush);
}

void FMcpWorldBrushEdMode::Enter()
{
    FEdMode::Enter();

    FMcpWorldBrushStrokeState &Stroke = McpWorldBrushGetStrokeState();
    Stroke = FMcpWorldBrushStrokeState();

    if (!Toolkit.IsValid())
    {
        Toolkit = MakeShareable(new FMcpWorldBrushEdModeToolkit);
        Toolkit->Init(Owner->GetToolkitHost());
    }
}

void FMcpWorldBrushEdMode::Exit()
{
    FMcpWorldBrushStrokeState &Stroke = McpWorldBrushGetStrokeState();
    if (Stroke.bActive)
    {
        Stroke.bActive = false;
        GEditor->EndTransaction();
        bHasBrushCursor = false;
    }
    FEdMode::Exit();
}

bool FMcpWorldBrushEdMode::TraceCursor(UWorld *World, const FVector &Origin,
                                       const FVector &Direction, FHitResult &OutHit) const
{
    if (!World)
        return false;
    FCollisionQueryParams Params;
    Params.bTraceComplex = true;
    return World->LineTraceSingleByChannel(OutHit, Origin, Origin + Direction * 1000000.0f,
                                           ECC_Visibility, Params);
}

bool FMcpWorldBrushEdMode::ApplyDabAtCursor(FEditorViewportClient *ViewportClient, const FVector &Origin,
                                            const FVector &Direction)
{
    UWorld *World = ViewportClient ? ViewportClient->GetWorld() : nullptr;
    if (!World)
        return false;

    FHitResult Hit;
    if (!TraceCursor(World, Origin, Direction, Hit))
        return false;

    const FMcpWorldBrushSettings &Settings = McpWorldBrushGetSettings();
    ALandscape *Landscape = nullptr;
    if (!Settings.TargetLandscapeName.IsEmpty())
    {
        for (TActorIterator<ALandscape> It(World); It; ++It)
        {
            ALandscape *Candidate = *It;
            if (Candidate && Candidate->GetActorLabel().Equals(Settings.TargetLandscapeName,
                                                              ESearchCase::IgnoreCase))
            {
                Landscape = Candidate;
                break;
            }
        }
        if (!Landscape)
            return false;
    }
    else
    {
        Landscape = McpWorldBrushResolveLandscape(World, Hit);
        if (!Landscape)
            return false;
    }

    FMcpWorldBrushStrokeState &Stroke = McpWorldBrushGetStrokeState();
    BrushCursor = Hit.ImpactPoint;
    bHasBrushCursor = true;

    // Space dabs along the stroke so fast drags do not over-apply.
    const double Spacing = FMath::Max(10.0, Settings.Radius * 0.35);
    if (Stroke.bHasLastDab && (Hit.ImpactPoint - Stroke.LastDabLocation).Size() < Spacing)
        return true;
    Stroke.LastDabLocation = Hit.ImpactPoint;
    Stroke.bHasLastDab = true;

    const double Radius = FMath::Clamp(Settings.Radius, 10.0, 20000.0);
    const double Strength = FMath::Clamp(Settings.Strength, 0.0, 1.0);
    const double Falloff = FMath::Clamp(Settings.Falloff, 0.0, 1.0);

    switch (Settings.Tool)
    {
    case EMcpWorldBrushTool::Raise:
    case EMcpWorldBrushTool::Lower:
    case EMcpWorldBrushTool::Flatten:
    case EMcpWorldBrushTool::Smooth:
    {
        double FlattenTargetZ = Stroke.FlattenTargetZ;
        if (Settings.Tool == EMcpWorldBrushTool::Flatten && !Stroke.bHasFlattenTarget)
        {
            FlattenTargetZ = Hit.ImpactPoint.Z;
            Stroke.FlattenTargetZ = FlattenTargetZ;
            Stroke.bHasFlattenTarget = true;
        }
        const int32 Modified = McpWorldBrushApplyHeightDab(Landscape, Hit.ImpactPoint, Settings.Tool,
                                                           Radius, Strength, Falloff, FlattenTargetZ);
        return Modified >= 0;
    }
    case EMcpWorldBrushTool::PaintLayer:
    {
        const int32 Written = McpWorldBrushApplyPaintDab(Landscape, Hit.ImpactPoint, Settings.LayerName,
                                                         Settings.LayerInfoPath, Radius, Strength, Falloff);
        return Written >= 0;
    }
    case EMcpWorldBrushTool::ScatterFoliage:
    {
        const int32 Placed = McpWorldBrushApplyFoliageDab(World, Landscape, Hit.ImpactPoint,
                                                          Settings, StrokeSalt);
        return Placed >= 0;
    }
    }
    return false;
}

bool FMcpWorldBrushEdMode::StartTracking(FEditorViewportClient *InViewportClient, FViewport *InViewport)
{
    if (!InViewportClient || !InViewport)
        return false;
    if (!InViewport->KeyState(EKeys::LeftMouseButton))
        return false;

    FMcpWorldBrushStrokeState &Stroke = McpWorldBrushGetStrokeState();
    Stroke = FMcpWorldBrushStrokeState();
    Stroke.bActive = true;
    StrokeSalt = NextStrokeSalt++;
    GEditor->BeginTransaction(LOCTEXT("WorldBrushStroke", "World Brush Stroke"));
    return true;
}

bool FMcpWorldBrushEdMode::EndTracking(FEditorViewportClient * /*InViewportClient*/, FViewport * /*InViewport*/)
{
    FMcpWorldBrushStrokeState &Stroke = McpWorldBrushGetStrokeState();
    if (!Stroke.bActive)
        return false;

    Stroke.bActive = false;
    GEditor->EndTransaction();

    // Persist the stroke on the active landscape, if we can still resolve it.
    UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    const FMcpWorldBrushSettings &Settings = McpWorldBrushGetSettings();
    if (World && bHasBrushCursor)
    {
        ALandscape *Landscape = nullptr;
        if (!Settings.TargetLandscapeName.IsEmpty())
        {
            for (TActorIterator<ALandscape> It(World); It; ++It)
            {
                ALandscape *Candidate = *It;
                if (Candidate && Candidate->GetActorLabel().Equals(Settings.TargetLandscapeName,
                                                                  ESearchCase::IgnoreCase))
                {
                    Landscape = Candidate;
                    break;
                }
            }
        }
        else
        {
            for (TActorIterator<ALandscape> It(World); It; ++It)
            {
                ALandscape *Candidate = *It;
                if (Candidate && Candidate->GetComponentsBoundingBox(true).IsInsideOrOn(BrushCursor))
                {
                    Landscape = Candidate;
                    break;
                }
            }
        }
        if (Landscape)
        {
            FString SaveError;
            if (!McpWorldBrushEndStroke(Landscape, SaveError))
            {
                UE_LOG(LogMcpWorldBrushMode, Warning, TEXT("World brush stroke save failed: %s"),
                       *SaveError);
            }
        }
    }
    return true;
}

bool FMcpWorldBrushEdMode::MouseMove(FEditorViewportClient *ViewportClient, FViewport *Viewport,
                                     int32 x, int32 y)
{
    if (!ViewportClient || !Viewport)
        return false;

    FMcpWorldBrushStrokeState &Stroke = McpWorldBrushGetStrokeState();
    if (Stroke.bActive && Viewport->KeyState(EKeys::LeftMouseButton))
    {
        // Active stroke drag: paint and consume the input so the camera stays put.
        return PaintAtCursor(ViewportClient, x, y);
    }

    // Hover: keep the cursor ring fresh without consuming input.
    UpdateBrushCursor(ViewportClient, x, y);
    return false;
}

bool FMcpWorldBrushEdMode::CapturedMouseMove(FEditorViewportClient *InViewportClient, FViewport *InViewport,
                                             int32 InMouseX, int32 InMouseY)
{
    if (!InViewportClient || !InViewport)
        return false;
    // Captured drags (mouse capture during tracking) paint like regular moves.
    return PaintAtCursor(InViewportClient, InMouseX, InMouseY);
}

bool FMcpWorldBrushEdMode::PaintAtCursor(FEditorViewportClient *ViewportClient, int32 x, int32 y)
{
    // World-space ray from screen coordinates (foliage paint mode pattern).
    FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
        ViewportClient->Viewport,
        ViewportClient->GetScene(),
        ViewportClient->EngineShowFlags)
        .SetRealtimeUpdate(ViewportClient->IsRealtime()));

    FSceneView *View = ViewportClient->CalcSceneView(&ViewFamily);
    FViewportCursorLocation MouseViewportRay(View, ViewportClient, x, y);
    FVector Origin = MouseViewportRay.GetOrigin();
    const FVector Direction = MouseViewportRay.GetDirection();
    if (ViewportClient->IsOrtho())
    {
        Origin += -WORLD_MAX * Direction;
    }
    return ApplyDabAtCursor(ViewportClient, Origin, Direction);
}

void FMcpWorldBrushEdMode::UpdateBrushCursor(FEditorViewportClient *ViewportClient, int32 x, int32 y)
{
    FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
        ViewportClient->Viewport,
        ViewportClient->GetScene(),
        ViewportClient->EngineShowFlags)
        .SetRealtimeUpdate(ViewportClient->IsRealtime()));

    FSceneView *View = ViewportClient->CalcSceneView(&ViewFamily);
    FViewportCursorLocation MouseViewportRay(View, ViewportClient, x, y);
    FVector Origin = MouseViewportRay.GetOrigin();
    const FVector Direction = MouseViewportRay.GetDirection();
    if (ViewportClient->IsOrtho())
    {
        Origin += -WORLD_MAX * Direction;
    }

    UWorld *World = ViewportClient->GetWorld();
    if (World)
    {
        FHitResult Hit;
        if (TraceCursor(World, Origin, Direction, Hit))
        {
            BrushCursor = Hit.ImpactPoint;
            bHasBrushCursor = true;
            return;
        }
    }
    bHasBrushCursor = false;
}

void FMcpWorldBrushEdMode::Render(const FSceneView * /*View*/, FViewport * /*Viewport*/,
                                  FPrimitiveDrawInterface *PDI)
{
    if (!bHasBrushCursor || !PDI)
        return;

    const double Radius = FMath::Clamp(McpWorldBrushGetSettings().Radius, 10.0, 20000.0);
    constexpr int32 Segments = 48;
    const FLinearColor Color(0.2f, 1.0f, 0.4f, 1.0f);
    FVector Previous = BrushCursor + FVector(Radius, 0.0, 2.0);
    for (int32 Index = 1; Index <= Segments; ++Index)
    {
        const double Angle = 2.0 * PI * static_cast<double>(Index) / static_cast<double>(Segments);
        const FVector Current = BrushCursor +
                                FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 2.0);
        PDI->DrawLine(Previous, Current, Color, SDPG_Foreground, 2.0f);
        Previous = Current;
    }
    PDI->DrawLine(BrushCursor + FVector(0.0, 0.0, -Radius * 0.15),
                  BrushCursor + FVector(0.0, 0.0, Radius * 0.15), Color, SDPG_Foreground, 2.0f);
}

// =============================================================================
// Toolkit
// =============================================================================

FMcpWorldBrushEdModeToolkit::FMcpWorldBrushEdModeToolkit()
{
}

void FMcpWorldBrushEdModeToolkit::Init(const TSharedPtr<IToolkitHost> &InitHost)
{
    FModeToolkit::Init(InitHost);
    ToolkitWidget = MakeMcpWorldBrushSettingsWidget();
}

FName FMcpWorldBrushEdModeToolkit::GetToolkitFName() const
{
    static const FName ToolkitName(TEXT("McpWorldBrush"));
    return ToolkitName;
}

FText FMcpWorldBrushEdModeToolkit::GetBaseToolkitName() const
{
    return NSLOCTEXT("McpWorldBrush", "ToolkitName", "World Brush");
}

#undef LOCTEXT_NAMESPACE
