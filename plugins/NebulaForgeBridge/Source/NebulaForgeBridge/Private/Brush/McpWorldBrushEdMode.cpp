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
#include "UnrealWidget.h"

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

    UE_LOG(LogMcpWorldBrushMode, Log, TEXT("World brush mode entered (tool=%d radius=%.0f)."),
           static_cast<int32>(McpWorldBrushGetSettings().Tool), McpWorldBrushGetSettings().Radius);

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
        // Resolve through the ray so meshes, water, or volumes above the
        // terrain do not block painting the landscape underneath.
        Landscape = McpWorldBrushFindLandscapeAlongRay(World, Origin, Direction);
        if (!Landscape)
        {
            // Throttled warning naming the topmost blocker for diagnosis.
            static double LastNoLandscapeWarn = 0.0;
            const double Now = FPlatformTime::Seconds();
            if (Now - LastNoLandscapeWarn > 3.0)
            {
                LastNoLandscapeWarn = Now;
                const AActor *Blocker = Hit.GetActor();
                const bool bHlodBlocker =
                    Blocker && Blocker->GetClass()->GetName().Contains(TEXT("HLOD"));
                UE_LOG(LogMcpWorldBrushMode, Warning,
                       TEXT("World brush found no landscape along the ray (top hit: %s [%s] at %s). %s"),
                       Blocker ? *Blocker->GetActorLabel() : TEXT("<none>"),
                       Blocker ? *Blocker->GetClass()->GetName() : TEXT("<none>"),
                       *Hit.ImpactPoint.ToString(),
                       bHlodBlocker
                           ? TEXT("An HLOD stands in for an unloaded World Partition cell here; move closer or load the cell, then brush the loaded landscape.")
                           : TEXT("Add a landscape actor or set the brush Landscape field."));
            }
            return false;
        }
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
        if (Modified >= 0 && !bLoggedFirstDab)
        {
            bLoggedFirstDab = true;
            UE_LOG(LogMcpWorldBrushMode, Log, TEXT("World brush painting: first dab applied (%d verts)."),
                   Modified);
        }
        return Modified >= 0;
    }
    case EMcpWorldBrushTool::PaintLayer:
    {
        // Multi-material stack: legacy single layer plus optional extras.
        TArray<FMcpBrushPaintLayer> Stack;
        if (!Settings.LayerName.IsEmpty())
        {
            FMcpBrushPaintLayer Primary;
            Primary.LayerName = Settings.LayerName;
            Primary.Strength = 1.0;
            Stack.Add(Primary);
        }
        for (const FMcpBrushPaintLayer &Extra : Settings.PaintLayers)
        {
            if (!Extra.LayerName.IsEmpty() && Stack.Num() < 4)
                Stack.Add(Extra);
        }
        const int32 Written = McpWorldBrushApplyPaintDab(Landscape, Hit.ImpactPoint, Stack,
                                                         Settings.LayerInfoPath, Radius, Strength, Falloff);
        if (Written >= 0 && !bLoggedFirstDab)
        {
            bLoggedFirstDab = true;
            UE_LOG(LogMcpWorldBrushMode, Log,
                   TEXT("World brush painting: first dab applied (%d weights across %d layers)."),
                   Written, Stack.Num());
        }
        return Written >= 0;
    }
    case EMcpWorldBrushTool::ScatterFoliage:
    {
        const int32 Placed = McpWorldBrushApplyFoliageDab(World, Landscape, Hit.ImpactPoint,
                                                          Settings, StrokeSalt);
        if (Placed >= 0 && !bLoggedFirstDab)
        {
            bLoggedFirstDab = true;
            UE_LOG(LogMcpWorldBrushMode, Log, TEXT("World brush painting: first dab applied (%d instances)."),
                   Placed);
        }
        return Placed >= 0;
    }
    case EMcpWorldBrushTool::River:
    {
        // Record the course; the channel is carved live and water is built
        // when the stroke ends.
        FMcpBrushRiverSample Sample;
        Sample.Location = Hit.ImpactPoint;
        Sample.WaterZ = Hit.ImpactPoint.Z;
        if (Stroke.RiverSamples.Num() == 0 ||
            (Sample.Location - Stroke.RiverSamples.Last().Location).Size() >=
                FMath::Max(50.0, Settings.RiverWidth * 0.5))
        {
            Stroke.RiverSamples.Add(Sample);
        }
        const double BedZ = Sample.WaterZ - FMath::Max(50.0, Settings.RiverDepth);
        const int32 Modified = McpWorldBrushApplyHeightDab(
            Landscape, Sample.Location, EMcpWorldBrushTool::Flatten,
            FMath::Max(100.0, Settings.RiverWidth), Strength, Falloff, BedZ);
        if (Modified >= 0 && !bLoggedFirstDab)
        {
            bLoggedFirstDab = true;
            UE_LOG(LogMcpWorldBrushMode, Log, TEXT("World brush river: carving channel."));
        }
        return Modified >= 0;
    }
    case EMcpWorldBrushTool::RoadSpline:
    {
        // RoadSpline drops points on click (see InputKey); drags only move
        // the hover cursor.
        return true;
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
    if (IsCameraOrWidgetDrag(InViewportClient, InViewport))
        return false;

    BeginStroke();
    return true;
}

bool FMcpWorldBrushEdMode::EndTracking(FEditorViewportClient * /*InViewportClient*/, FViewport * /*InViewport*/)
{
    return EndStrokeAndSave();
}

bool FMcpWorldBrushEdMode::IsCameraOrWidgetDrag(FEditorViewportClient *ViewportClient,
                                                 FViewport *Viewport) const
{
    if (!ViewportClient || !Viewport)
        return true;
    // Middle/right mouse or Alt held: the camera owns the gesture.
    if (Viewport->KeyState(EKeys::MiddleMouseButton) || Viewport->KeyState(EKeys::RightMouseButton) ||
        Viewport->KeyState(EKeys::LeftAlt) || Viewport->KeyState(EKeys::RightAlt))
        return true;
    // A transform widget drag owns the gesture.
    if (ViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
        return true;
    return false;
}

void FMcpWorldBrushEdMode::AppendRoadPointAtMouse(FEditorViewportClient *ViewportClient, FViewport *Viewport)
{
    if (!ViewportClient || !Viewport)
        return;
    FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
        ViewportClient->Viewport,
        ViewportClient->GetScene(),
        ViewportClient->EngineShowFlags)
        .SetRealtimeUpdate(ViewportClient->IsRealtime()));
    FSceneView *View = ViewportClient->CalcSceneView(&ViewFamily);
    FViewportCursorLocation MouseViewportRay(View, ViewportClient, Viewport->GetMouseX(), Viewport->GetMouseY());
    FVector Origin = MouseViewportRay.GetOrigin();
    const FVector Direction = MouseViewportRay.GetDirection();
    if (ViewportClient->IsOrtho())
    {
        Origin += -WORLD_MAX * Direction;
    }
    UWorld *World = ViewportClient->GetWorld();
    if (!World)
        return;
    FHitResult Hit;
    if (!TraceCursor(World, Origin, Direction, Hit))
        return;
    FMcpWorldBrushRoadDraft &Draft = McpWorldBrushGetRoadDraft();
    if (Draft.Points.Num() > 0 &&
        (Hit.ImpactPoint - Draft.Points.Last()).Size() < 50.0)
        return;
    Draft.Points.Add(Hit.ImpactPoint);
    BrushCursor = Hit.ImpactPoint;
    bHasBrushCursor = true;
    UE_LOG(LogMcpWorldBrushMode, Log, TEXT("World brush road point %d dropped at %s."),
           Draft.Points.Num(), *Hit.ImpactPoint.ToString());
}

void FMcpWorldBrushEdMode::BeginStroke()
{
    FMcpWorldBrushStrokeState &Stroke = McpWorldBrushGetStrokeState();
    if (Stroke.bActive)
        return;
    Stroke = FMcpWorldBrushStrokeState();
    Stroke.bActive = true;
    StrokeSalt = NextStrokeSalt++;
    bLoggedFirstDab = false;
    GEditor->BeginTransaction(LOCTEXT("WorldBrushStroke", "World Brush Stroke"));
}

bool FMcpWorldBrushEdMode::EndStrokeAndSave()
{
    FMcpWorldBrushStrokeState &Stroke = McpWorldBrushGetStrokeState();
    if (!Stroke.bActive)
        return false;

    Stroke.bActive = false;
    GEditor->EndTransaction();

    // River strokes finish by building the water course from the recording.
    UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    const FMcpWorldBrushSettings &Settings = McpWorldBrushGetSettings();
    if (World && Settings.Tool == EMcpWorldBrushTool::River && Stroke.RiverSamples.Num() >= 2)
    {
        ALandscape *RiverLandscape = nullptr;
        if (bHasBrushCursor)
        {
            for (TActorIterator<ALandscape> It(World); It; ++It)
            {
                ALandscape *Candidate = *It;
                if (Candidate && Candidate->GetComponentsBoundingBox(true).IsInsideOrOn(BrushCursor))
                {
                    RiverLandscape = Candidate;
                    break;
                }
            }
        }
        if (!RiverLandscape && !Settings.TargetLandscapeName.IsEmpty())
        {
            for (TActorIterator<ALandscape> It(World); It; ++It)
            {
                ALandscape *Candidate = *It;
                if (Candidate && Candidate->GetActorLabel().Equals(Settings.TargetLandscapeName,
                                                                  ESearchCase::IgnoreCase))
                {
                    RiverLandscape = Candidate;
                    break;
                }
            }
        }
        if (RiverLandscape)
        {
            FString RiverError;
            const int32 RiverDabs = McpBrushFinishRiverStroke(World, RiverLandscape, Stroke.RiverSamples,
                                                              Settings, RiverError);
            if (RiverDabs < 0)
            {
                UE_LOG(LogMcpWorldBrushMode, Warning, TEXT("World brush river failed: %s"), *RiverError);
            }
            else
            {
                UE_LOG(LogMcpWorldBrushMode, Log, TEXT("World brush river finished (%d dabs)."), RiverDabs);
            }
            return true;
        }
    }

    // Persist the stroke on the active landscape, if we can still resolve it.
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

bool FMcpWorldBrushEdMode::InputKey(FEditorViewportClient *ViewportClient, FViewport *Viewport,
                                    FKey Key, EInputEvent Event)
{
    if (!ViewportClient || !Viewport)
        return false;

    if (Key == EKeys::LeftMouseButton && Event == IE_Pressed)
    {
        if (IsCameraOrWidgetDrag(ViewportClient, Viewport))
            return false;
        // RoadSpline drops route points on click instead of stroking.
        if (McpWorldBrushGetSettings().Tool == EMcpWorldBrushTool::RoadSpline)
        {
            AppendRoadPointAtMouse(ViewportClient, Viewport);
            return true;
        }
        // Consume the press so camera orbit and marquee selection never start.
        // The first dab lands on the first MouseMove/CapturedMouseMove event.
        BeginStroke();
        UE_LOG(LogMcpWorldBrushMode, Verbose, TEXT("World brush press consumed, stroke begun."));
        return true;
    }

    if (Key == EKeys::LeftMouseButton && Event == IE_Released)
    {
        if (EndStrokeAndSave())
            return true;
    }
    return false;
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
    if (!PDI)
        return;

    const FLinearColor MarkerColor(1.0f, 0.8f, 0.2f, 1.0f);
    const FMcpWorldBrushRoadDraft &Draft = McpWorldBrushGetRoadDraft();
    // Road draft route: numbered polyline with point markers.
    for (int32 Index = 0; Index < Draft.Points.Num(); ++Index)
    {
        const FVector &Point = Draft.Points[Index];
        constexpr double Arm = 150.0;
        PDI->DrawLine(Point + FVector(-Arm, 0.0, 2.0), Point + FVector(Arm, 0.0, 2.0),
                      MarkerColor, SDPG_Foreground, 3.0f);
        PDI->DrawLine(Point + FVector(0.0, -Arm, 2.0), Point + FVector(0.0, Arm, 2.0),
                      MarkerColor, SDPG_Foreground, 3.0f);
        if (Index > 0)
        {
            PDI->DrawLine(Draft.Points[Index - 1] + FVector(0.0, 0.0, 2.0),
                          Point + FVector(0.0, 0.0, 2.0), MarkerColor, SDPG_Foreground, 2.0f);
        }
    }

    // Radius ring is meaningless for click-to-drop road points.
    if (!bHasBrushCursor ||
        McpWorldBrushGetSettings().Tool == EMcpWorldBrushTool::RoadSpline)
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
