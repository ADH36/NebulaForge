// =============================================================================
// McpWorldBrushOps.h
// =============================================================================
// Shared world-builder brush operations for NebulaForge Bridge.
//
// These helpers back both the interactive viewport brush mode
// (FMcpWorldBrushEdMode) and the sculpt_landscape "Smooth" toolMode, and reuse
// the proven FLandscapeEditDataInterface read/write patterns from the
// landscape handlers (height read/modify, alpha set/flush, HISM instancing).
//
// All functions must be called on the game thread in an editor build.
//
// Copyright (c) 2024 NebulaForge Bridge Contributors
// =============================================================================

#pragma once

#include "CoreMinimal.h"

class ALandscape;
class UWorld;
struct FHitResult;

/** Brush tools available to the interactive world brush and MCP. */
enum class EMcpWorldBrushTool : uint8
{
    Raise,
    Lower,
    Flatten,
    Smooth,
    PaintLayer,
    ScatterFoliage,
    River,
    RoadSpline
};

/** One paint entry in the multi-material paint stack (max 4 honored). */
struct FMcpBrushPaintLayer
{
    FString LayerName;
    double Strength = 1.0;
};

/** One recorded river course sample with its local water level. */
struct FMcpBrushRiverSample
{
    FVector Location = FVector::ZeroVector;
    double WaterZ = 0.0;
};

/** Shared brush settings edited from the brush toolkit and world panel. */
struct FMcpWorldBrushSettings
{
    EMcpWorldBrushTool Tool = EMcpWorldBrushTool::Raise;
    /** Brush radius in world units. */
    double Radius = 1000.0;
    /** Brush strength (0..1). */
    double Strength = 0.5;
    /** Edge falloff (0..1). */
    double Falloff = 0.5;
    /** Paint-only: target weight layer name (legacy single layer). */
    FString LayerName = TEXT("Grass");
    /** Paint-only: extra layers painted in the same dab (multi-material). */
    TArray<FMcpBrushPaintLayer> PaintLayers;
    /** Paint-only: existing layer info asset path (empty = auto-resolve). */
    FString LayerInfoPath;
    /** Foliage-only: static mesh to instance. */
    FString FoliageMeshPath;
    /** Foliage-only: instances placed per dab. */
    int32 FoliagePerDab = 6;
    /** Foliage-only: seed advancing every stroke. */
    int32 FoliageSeed = 90210;
    /** River-only: channel half-width in world units. */
    double RiverWidth = 800.0;
    /** River-only: channel depth below the local water level. */
    double RiverDepth = 400.0;
    /** River-only: spawn/update a WaterBodyRiver along the stroke. */
    bool bCreateRiverWater = true;
    /** River-only: water actor label (empty = MCP_BrushRiver_<stroke>). */
    FString RiverWaterActor;
    /** Road-only: corridor width for the finished road. */
    double RoadWidth = 800.0;
    /** Road-only: shoulder width for terrain cut/fill. */
    double ShoulderWidth = 400.0;
    /** Landscape actor label to target (empty = resolve from cursor hit). */
    FString TargetLandscapeName;
};

/** Persistent road spline draft shared by clicks and the Build Road button. */
struct FMcpWorldBrushRoadDraft
{
    TArray<FVector> Points;
};

/** Draft accessor (process-wide, shared by mode and widget). */
FMcpWorldBrushRoadDraft &McpWorldBrushGetRoadDraft();

/** Per-stroke transient state owned by the brush mode. */
struct FMcpWorldBrushStrokeState
{
    bool bActive = false;
    FVector LastDabLocation = FVector::ZeroVector;
    bool bHasLastDab = false;
    double FlattenTargetZ = 0.0;
    bool bHasFlattenTarget = false;
    FGuid StrokeId = FGuid();
    /** River tool: recorded course samples for this stroke. */
    TArray<FMcpBrushRiverSample> RiverSamples;
};

/**
 * Resolves the landscape actor for a brush hit (ALandscape or proxy).
 * @return The ALandscape actor, or nullptr when the hit is not landscape.
 */
ALandscape *McpWorldBrushResolveLandscape(UWorld *World, const FHitResult &Hit);

/**
 * Finds the first landscape along a ray, tracing through non-landscape
 * blockers (meshes, water surfaces, volumes) that would stop a single trace.
 * @return The ALandscape actor, or nullptr when the ray hits no landscape.
 */
ALandscape *McpWorldBrushFindLandscapeAlongRay(UWorld *World, const FVector &Origin,
                                               const FVector &Direction);

/**
 * Applies one height dab (Raise/Lower/Flatten/Smooth) at a world location.
 * Writes through FLandscapeEditDataInterface and marks packages dirty;
 * flushing/saving happens once per stroke via McpWorldBrushEndStroke.
 * @return Number of modified vertices, or -1 on failure.
 */
int32 McpWorldBrushApplyHeightDab(ALandscape *Landscape, const FVector &WorldLocation,
                                  EMcpWorldBrushTool Tool, double Radius, double Strength,
                                  double Falloff, double FlattenTargetZ);

/**
 * Applies one circular paint dab to a stack of weight layers (max-blend with
 * the existing weights). Creates/registers layers when needed.
 * @return Number of vertices written, or -1 on failure.
 */
int32 McpWorldBrushApplyPaintDab(ALandscape *Landscape, const FVector &WorldLocation,
                                 const TArray<FMcpBrushPaintLayer> &Layers,
                                 const FString &LayerInfoPath,
                                 double Radius, double Strength, double Falloff);

/**
 * Adds foliage instances around a dab point onto a brush-owned tool-tagged
 * HISM collection (created on first use, accumulated across strokes).
 * @return Number of instances placed, or -1 on failure.
 */
int32 McpWorldBrushApplyFoliageDab(UWorld *World, ALandscape *Landscape,
                                   const FVector &WorldLocation, const FMcpWorldBrushSettings &Settings,
                                   int32 StrokeSalt);

/** Flushes paint edits and persists landscape packages at stroke end. */
bool McpWorldBrushEndStroke(ALandscape *Landscape, FString &OutError);

/**
 * Finishes a river stroke: carves the recorded channel toward per-sample bed
 * heights and creates/updates a WaterBodyRiver along the course.
 * @return Number of channel dabs applied, or -1 on failure.
 */
int32 McpBrushFinishRiverStroke(UWorld *World, ALandscape *Landscape,
                                const TArray<FMcpBrushRiverSample> &Samples,
                                const FMcpWorldBrushSettings &Settings,
                                FString &OutError);
