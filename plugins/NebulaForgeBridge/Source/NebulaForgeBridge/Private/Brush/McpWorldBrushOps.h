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
    ScatterFoliage
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
    /** Paint-only: target weight layer name. */
    FString LayerName = TEXT("Grass");
    /** Paint-only: existing layer info asset path (empty = auto-resolve). */
    FString LayerInfoPath;
    /** Foliage-only: static mesh to instance. */
    FString FoliageMeshPath;
    /** Foliage-only: instances placed per dab. */
    int32 FoliagePerDab = 6;
    /** Foliage-only: seed advancing every stroke. */
    int32 FoliageSeed = 90210;
    /** Landscape actor label to target (empty = resolve from cursor hit). */
    FString TargetLandscapeName;
};

/** Per-stroke transient state owned by the brush mode. */
struct FMcpWorldBrushStrokeState
{
    bool bActive = false;
    FVector LastDabLocation = FVector::ZeroVector;
    bool bHasLastDab = false;
    double FlattenTargetZ = 0.0;
    bool bHasFlattenTarget = false;
    FGuid StrokeId = FGuid();
};

/**
 * Resolves the landscape actor for a brush hit (ALandscape or proxy).
 * @return The ALandscape actor, or nullptr when the hit is not landscape.
 */
ALandscape *McpWorldBrushResolveLandscape(UWorld *World, const FHitResult &Hit);

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
 * Applies one circular paint dab to a weight layer (max-blend with the
 * existing weights). Creates/registers the layer when needed.
 * @return Number of vertices written, or -1 on failure.
 */
int32 McpWorldBrushApplyPaintDab(ALandscape *Landscape, const FVector &WorldLocation,
                                 const FString &LayerName, const FString &LayerInfoPath,
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
