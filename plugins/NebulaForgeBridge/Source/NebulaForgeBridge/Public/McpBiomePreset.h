// =============================================================================
// McpBiomePreset.h
// =============================================================================
// World-builder biome preset data asset for NebulaForge Bridge.
//
// A UMcpBiomePreset captures a complete worldBLD-style world recipe: landscape
// dimensions, procedural terrain shaping, rule-based surface layers, and
// deterministic foliage placement. Presets are authored once (via MCP, the
// content browser, or the Generate World editor panel) and regenerated with a
// stable seed through the `generate_world` / `apply_biome` orchestration in
// NebulaForgeBridge_WorldRecipeHandlers.cpp.
//
// Copyright (c) 2024 NebulaForge Bridge Contributors
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "McpBiomePreset.generated.h"

/** Landscape actor sizing for a world recipe. */
USTRUCT(BlueprintType)
struct FMcpBiomeLandscapeConfig
{
    GENERATED_BODY()

    /** Target landscape actor label. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    FString LandscapeName = TEXT("MCP_WorldLandscape");

    /** Landscape components along X (1..32). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "1", ClampMax = "32"))
    int32 ComponentsX = 8;

    /** Landscape components along Y (1..32). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "1", ClampMax = "32"))
    int32 ComponentsY = 8;

    /** Quads per subsection: must be 7, 15, 31, 63, 127, or 255. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    int32 QuadsPerComponent = 63;

    /** Subsections per component (1 or 2). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "1", ClampMax = "2"))
    int32 SectionsPerComponent = 1;

    /** World-space spawn location for the landscape actor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    FVector Location = FVector::ZeroVector;
};

/** Procedural heightfield shaping for a world recipe. */
USTRUCT(BlueprintType)
struct FMcpBiomeTerrainConfig
{
    GENERATED_BODY()

    /** Heightfield archetype: mountains, hills, valleys, plains, or lakeshore. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    FString TerrainFeature = TEXT("mountains");

    /** Peak deviation from the 32768 mid height, in heightmap units. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "0.0", ClampMax = "32767.0"))
    double HeightScale = 8192.0;

    /** Base noise frequency. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "0.05", ClampMax = "32.0"))
    double Frequency = 2.0;

    /** Generation grid resolution per axis (2..513). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "2", ClampMax = "513"))
    int32 Resolution = 513;

    /** When enabled, thermal erosion smoothing is applied to the heightfield. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    bool bEnableErosion = true;

    /** Thermal erosion iterations (1..64). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "1", ClampMax = "64"))
    int32 ErosionIterations = 8;
};

/** One rule-based landscape weight layer. */
USTRUCT(BlueprintType)
struct FMcpBiomeLayerRule
{
    GENERATED_BODY()

    /** Target layer name, e.g. Grass, Rock, Snow. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    FString LayerName;

    /** Mask source: constant, height, slope, altitude, or noise. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    FString MaskType = TEXT("height");

    /** World-space height center for height/altitude masks (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    double TargetHeight = 0.0;

    /** Minimum world-space height included in the mask (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    double MinHeight = -1000000.0;

    /** Maximum world-space height included in the mask (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    double MaxHeight = 1000000.0;

    /** Linear fade width at height band borders (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "0.0"))
    double FadeDistance = 512.0;

    /** Minimum slope included in the mask (degrees). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "0.0", ClampMax = "90.0"))
    double MinSlope = 0.0;

    /** Maximum slope included in the mask (degrees). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "0.0", ClampMax = "90.0"))
    double MaxSlope = 90.0;

    /** Linear fade width at slope band borders (degrees). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "0.0", ClampMax = "45.0"))
    double FadeSlope = 5.0;

    /** Paint strength multiplier (0..1). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    double Strength = 1.0;
};

/** One deterministic foliage scatter entry. */
USTRUCT(BlueprintType)
struct FMcpBiomeFoliageEntry
{
    GENERATED_BODY()

    /** Static mesh asset path to instance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    FString MeshPath;

    /** Number of instances to sample across the landscape. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "0"))
    int32 Count = 32;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    double MinScale = 0.8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    double MaxScale = 1.2;

    /** Optional slope band restriction (degrees). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "0.0", ClampMax = "90.0"))
    double MinSlope = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "0.0", ClampMax = "90.0"))
    double MaxSlope = 90.0;

    /** Optional world height band restriction (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    double MinHeight = -1000000.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    double MaxHeight = 1000000.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    bool bAlignToNormal = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    bool bRandomYaw = true;
};

/**
 * Reusable world recipe preset. Created via the create_biome_preset
 * automation action or directly in the editor, consumed by generate_world /
 * apply_biome to rebuild the described world deterministically from a seed.
 */
UCLASS(BlueprintType)
class NEBULAFORGEBRIDGE_API UMcpBiomePreset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    FString Description;

    /** Deterministic generation seed shared by terrain and foliage steps. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    int32 Seed = 1337;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    FMcpBiomeLandscapeConfig Landscape;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    FMcpBiomeTerrainConfig Terrain;

    /** Rule-based surface layers painted after terrain shaping. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    TArray<FMcpBiomeLayerRule> Layers;

    /** Deterministic HISM foliage entries scattered after painting. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    TArray<FMcpBiomeFoliageEntry> Foliage;

    /** Optional existing landscape material path. When empty, generate_world
     * creates a basic landscape material and per-layer layer-info assets. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    FString MaterialPath;
};
