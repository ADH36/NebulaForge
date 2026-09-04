// =============================================================================
// NebulaForgeBridge_WorldRecipeHandlers.cpp
// =============================================================================
// World-builder recipe orchestration (worldBLD-style) for NebulaForge Bridge.
//
// HANDLERS IMPLEMENTED:
// --------------------
// - HandleWorldRecipeAction : dispatcher for build_environment world recipes
//   Sub-actions:
//     - generate_world       : orchestrated world pipeline (see below)
//     - apply_biome          : alias of generate_world driven by a preset
//     - create_biome_preset  : create a UMcpBiomePreset data asset
//     - inspect_biome_preset : dump a preset back to JSON
//     - list_biome_presets   : list all UMcpBiomePreset assets under /Game
//
// GENERATE_WORLD PIPELINE:
// ------------------------
// The recipe composes existing registered automation actions into one
// deterministic chain, executed one step per game-thread task:
//   1. create_landscape_material     (when no materialPath is supplied)
//   2. create_landscape_layer_info   (one per rule layer)
//   3. create_landscape              (skipped when reusing an existing actor)
//   4. set_landscape_material        (only when reusing an existing actor)
//   5. apply_landscape_erosion /
//      generate_landscape_heightmap  (procedural heightfield, seeded)
//   6. custom rule-based layer paint (height/slope/altitude/noise masks)
//   7. scatter_landscape_foliage     (one deterministic HISM scatter per entry)
//
// Sub-step responses are captured in SendAutomationResponse (the plugin
// subsystem intercepts responses addressed to RecipeRequestId while a chain is
// capturing), recorded per step, and a single combined summary response is
// sent when the chain finishes. Every step reports PASS/PARTIAL/FAIL evidence
// so truthful status stays intact.
//
// SECURITY NOTES:
// -----------------------------------------------------------------------------
// - Preset/material paths validated via SanitizeProjectRelativePath()
// - Asset names validated via SanitizeAssetName() + ValidateAssetCreationPath()
// - Preset assets save through McpSafeAssetSave (never modal saves)
//
// Copyright (c) 2024 NebulaForge Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h" // MUST be first include
#include "McpHandlerUtils.h"
#include "McpPropertyReflection.h"

#include "Dom/JsonObject.h"
#include "McpBiomePreset.h"
#include "Brush/McpWorldBrushOps.h"
#include "NebulaForgeBridgeGlobals.h"
#include "NebulaForgeBridgeHelpers.h"
#include "NebulaForgeBridgeSubsystem.h"
#include "McpSafeOperations.h"

#include <initializer_list>

#if WITH_EDITOR

#include "Async/Async.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "ScopedTransaction.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"

#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif

#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogMcpWorldRecipes, Log, All);

namespace
{
constexpr int32 MaxBiomePresetsToList = 256;

#if WITH_EDITOR
FString McpRecipeFirstString(const TSharedPtr<FJsonObject> &Payload,
                             std::initializer_list<const TCHAR *> Fields)
{
    if (!Payload.IsValid())
        return FString();
    for (const TCHAR *Field : Fields)
    {
        FString Value;
        if (Payload->TryGetStringField(Field, Value) && !Value.IsEmpty())
            return Value;
    }
    return FString();
}

/** Reads a number from an object field when present. */
template <typename T>
void McpRecipeReadNumber(const TSharedPtr<FJsonObject> &Object, const TCHAR *Field, T &OutValue)
{
    if (!Object.IsValid())
        return;
    double Value = 0.0;
    if (Object->TryGetNumberField(Field, Value))
        OutValue = static_cast<T>(Value);
}

/** Reads an {x,y,z} or [x,y,z] field into a vector when present. */
bool McpRecipeReadVector(const TSharedPtr<FJsonObject> &Payload, const TCHAR *Field, FVector &OutValue)
{
    if (!Payload.IsValid())
        return false;
    const TSharedPtr<FJsonObject> *Object = nullptr;
    if (Payload->TryGetObjectField(Field, Object) && Object && (*Object).IsValid())
    {
        double X = 0.0, Y = 0.0, Z = 0.0;
        (*Object)->TryGetNumberField(TEXT("x"), X);
        (*Object)->TryGetNumberField(TEXT("y"), Y);
        (*Object)->TryGetNumberField(TEXT("z"), Z);
        OutValue = FVector(X, Y, Z);
        return true;
    }
    const TArray<TSharedPtr<FJsonValue>> *Array = nullptr;
    if (Payload->TryGetArrayField(Field, Array) && Array && Array->Num() >= 3)
    {
        OutValue = FVector((*Array)[0]->AsNumber(), (*Array)[1]->AsNumber(), (*Array)[2]->AsNumber());
        return true;
    }
    return false;
}

/** Resolved world recipe configuration merged from preset + payload. */
struct FMcpWorldRecipeConfig
{
    FString LandscapeName = TEXT("MCP_WorldLandscape");
    FVector Location = FVector::ZeroVector;
    int32 ComponentsX = 8;
    int32 ComponentsY = 8;
    int32 QuadsPerComponent = 63;
    int32 SectionsPerComponent = 1;

    FString TerrainFeature = TEXT("mountains");
    double HeightScale = 8192.0;
    double Frequency = 2.0;
    int32 Resolution = 513;
    bool bErosion = true;
    int32 ErosionIterations = 8;

    int32 Seed = 1337;
    FString MaterialPath;
    bool bCreateMaterial = true;
    bool bReuseExistingLandscape = true;
    bool bSkipLandscape = false;
    bool bSkipTerrain = false;
    bool bSkipPaint = false;
    bool bSkipFoliage = false;

    struct FLayerRule
    {
        FString LayerName;
        FString MaskType = TEXT("height");
        double TargetHeight = 0.0;
        double MinHeight = -1000000.0;
        double MaxHeight = 1000000.0;
        double FadeDistance = 512.0;
        double MinSlope = 0.0;
        double MaxSlope = 90.0;
        double FadeSlope = 5.0;
        double Strength = 1.0;
    };
    TArray<FLayerRule> Layers;

    struct FFoliageEntry
    {
        FString MeshPath;
        int32 Count = 32;
        double MinScale = 0.8;
        double MaxScale = 1.2;
        double MinSlope = 0.0;
        double MaxSlope = 90.0;
        double MinHeight = -1000000.0;
        double MaxHeight = 1000000.0;
    };
    TArray<FFoliageEntry> Foliage;
};

void McpRecipeApplyPreset(const UMcpBiomePreset *Preset, FMcpWorldRecipeConfig &Config)
{
    if (!Preset)
        return;
    Config.Seed = Preset->Seed;
    Config.LandscapeName = Preset->Landscape.LandscapeName;
    Config.ComponentsX = Preset->Landscape.ComponentsX;
    Config.ComponentsY = Preset->Landscape.ComponentsY;
    Config.QuadsPerComponent = Preset->Landscape.QuadsPerComponent;
    Config.SectionsPerComponent = Preset->Landscape.SectionsPerComponent;
    Config.Location = Preset->Landscape.Location;
    Config.TerrainFeature = Preset->Terrain.TerrainFeature;
    Config.HeightScale = Preset->Terrain.HeightScale;
    Config.Frequency = Preset->Terrain.Frequency;
    Config.Resolution = Preset->Terrain.Resolution;
    Config.bErosion = Preset->Terrain.bEnableErosion;
    Config.ErosionIterations = Preset->Terrain.ErosionIterations;
    Config.MaterialPath = Preset->MaterialPath;
    for (const FMcpBiomeLayerRule &Rule : Preset->Layers)
    {
        FMcpWorldRecipeConfig::FLayerRule Out;
        Out.LayerName = Rule.LayerName;
        Out.MaskType = Rule.MaskType;
        Out.TargetHeight = Rule.TargetHeight;
        Out.MinHeight = Rule.MinHeight;
        Out.MaxHeight = Rule.MaxHeight;
        Out.FadeDistance = Rule.FadeDistance;
        Out.MinSlope = Rule.MinSlope;
        Out.MaxSlope = Rule.MaxSlope;
        Out.FadeSlope = Rule.FadeSlope;
        Out.Strength = Rule.Strength;
        Config.Layers.Add(Out);
    }
    for (const FMcpBiomeFoliageEntry &Entry : Preset->Foliage)
    {
        FMcpWorldRecipeConfig::FFoliageEntry Out;
        Out.MeshPath = Entry.MeshPath;
        Out.Count = Entry.Count;
        Out.MinScale = Entry.MinScale;
        Out.MaxScale = Entry.MaxScale;
        Out.MinSlope = Entry.MinSlope;
        Out.MaxSlope = Entry.MaxSlope;
        Out.MinHeight = Entry.MinHeight;
        Out.MaxHeight = Entry.MaxHeight;
        Config.Foliage.Add(Out);
    }
}

void McpRecipeApplyPayloadOverrides(const TSharedPtr<FJsonObject> &Payload,
                                    FMcpWorldRecipeConfig &Config,
                                    bool bPayloadHasLayers,
                                    bool bPayloadHasFoliage)
{
    if (!Payload.IsValid())
        return;

    const FString Name = McpRecipeFirstString(Payload, {TEXT("name"), TEXT("landscapeName")});
    if (!Name.IsEmpty())
        Config.LandscapeName = Name;
    McpRecipeReadVector(Payload, TEXT("location"), Config.Location);

    const TSharedPtr<FJsonObject> *LandscapeObject = nullptr;
    if (Payload->TryGetObjectField(TEXT("landscape"), LandscapeObject) && LandscapeObject)
    {
        const FString ObjectLandscapeName =
            McpRecipeFirstString(*LandscapeObject, {TEXT("landscapeName"), TEXT("name")});
        if (!ObjectLandscapeName.IsEmpty())
            Config.LandscapeName = ObjectLandscapeName;
        McpRecipeReadNumber(*LandscapeObject, TEXT("componentsX"), Config.ComponentsX);
        McpRecipeReadNumber(*LandscapeObject, TEXT("componentsY"), Config.ComponentsY);
        McpRecipeReadNumber(*LandscapeObject, TEXT("quadsPerComponent"), Config.QuadsPerComponent);
        McpRecipeReadNumber(*LandscapeObject, TEXT("sectionsPerComponent"), Config.SectionsPerComponent);
        McpRecipeReadVector(*LandscapeObject, TEXT("location"), Config.Location);
    }
    double Number = 0.0;
    if (Payload->TryGetNumberField(TEXT("componentsX"), Number))
        Config.ComponentsX = static_cast<int32>(Number);
    if (Payload->TryGetNumberField(TEXT("componentsY"), Number))
        Config.ComponentsY = static_cast<int32>(Number);
    if (Payload->TryGetNumberField(TEXT("quadsPerComponent"), Number) ||
        Payload->TryGetNumberField(TEXT("quadsPerSection"), Number))
        Config.QuadsPerComponent = static_cast<int32>(Number);
    if (Payload->TryGetNumberField(TEXT("sectionsPerComponent"), Number))
        Config.SectionsPerComponent = static_cast<int32>(Number);

    const TSharedPtr<FJsonObject> *TerrainObject = nullptr;
    if (Payload->TryGetObjectField(TEXT("terrain"), TerrainObject) && TerrainObject)
    {
        const FString Feature = McpRecipeFirstString(*TerrainObject, {TEXT("terrainFeature"), TEXT("feature")});
        if (!Feature.IsEmpty())
            Config.TerrainFeature = Feature;
        McpRecipeReadNumber(*TerrainObject, TEXT("heightScale"), Config.HeightScale);
        McpRecipeReadNumber(*TerrainObject, TEXT("frequency"), Config.Frequency);
        McpRecipeReadNumber(*TerrainObject, TEXT("resolution"), Config.Resolution);
        int32 IntValue = Config.Resolution;
        McpRecipeReadNumber(*TerrainObject, TEXT("resolutionX"), IntValue);
        Config.Resolution = IntValue;
        int32 Iterations = Config.ErosionIterations;
        McpRecipeReadNumber(*TerrainObject, TEXT("iterations"), Iterations);
        Config.ErosionIterations = Iterations;
        bool bBool = false;
        if ((*TerrainObject)->TryGetBoolField(TEXT("erosion"), bBool))
            Config.bErosion = bBool;
        if ((*TerrainObject)->TryGetBoolField(TEXT("enableErosion"), bBool))
            Config.bErosion = bBool;
    }
    const FString Feature = McpRecipeFirstString(Payload, {TEXT("terrainFeature")});
    if (!Feature.IsEmpty())
        Config.TerrainFeature = Feature;
    if (Payload->TryGetNumberField(TEXT("heightScale"), Number))
        Config.HeightScale = Number;
    if (Payload->TryGetNumberField(TEXT("frequency"), Number))
        Config.Frequency = Number;
    if (Payload->TryGetNumberField(TEXT("resolution"), Number))
        Config.Resolution = static_cast<int32>(Number);
    if (Payload->TryGetNumberField(TEXT("iterations"), Number))
    {
        Config.ErosionIterations = static_cast<int32>(Number);
        Config.bErosion = Config.ErosionIterations > 0;
    }

    if (Payload->TryGetNumberField(TEXT("seed"), Number))
        Config.Seed = static_cast<int32>(Number);

    const FString MaterialPath = McpRecipeFirstString(Payload, {TEXT("materialPath")});
    if (!MaterialPath.IsEmpty())
        Config.MaterialPath = MaterialPath;
    bool bBool = false;
    if (Payload->TryGetBoolField(TEXT("generateMaterial"), bBool))
        Config.bCreateMaterial = bBool;
    if (Payload->TryGetBoolField(TEXT("reuseExistingLandscape"), bBool))
        Config.bReuseExistingLandscape = bBool;
    if (Payload->TryGetBoolField(TEXT("skipLandscape"), bBool))
        Config.bSkipLandscape = bBool;
    if (Payload->TryGetBoolField(TEXT("skipTerrain"), bBool))
        Config.bSkipTerrain = bBool;
    if (Payload->TryGetBoolField(TEXT("skipPaint"), bBool))
        Config.bSkipPaint = bBool;
    if (Payload->TryGetBoolField(TEXT("skipFoliage"), bBool))
        Config.bSkipFoliage = bBool;

    if (bPayloadHasLayers)
    {
        Config.Layers.Reset();
        const TArray<TSharedPtr<FJsonValue>> *Layers = nullptr;
        if (Payload->TryGetArrayField(TEXT("layers"), Layers) && Layers)
        {
            for (const TSharedPtr<FJsonValue> &Value : *Layers)
            {
                const TSharedPtr<FJsonObject> *RuleObject = nullptr;
                if (!Value.IsValid() || !Value->TryGetObject(RuleObject) || !RuleObject)
                    continue;
                FMcpWorldRecipeConfig::FLayerRule Rule;
                Rule.LayerName = McpRecipeFirstString(*RuleObject, {TEXT("layerName"), TEXT("name")});
                Rule.MaskType = McpRecipeFirstString(*RuleObject, {TEXT("maskType"), TEXT("mask")});
                if (Rule.MaskType.IsEmpty())
                    Rule.MaskType = TEXT("height");
                McpRecipeReadNumber(*RuleObject, TEXT("targetHeight"), Rule.TargetHeight);
                McpRecipeReadNumber(*RuleObject, TEXT("minHeight"), Rule.MinHeight);
                McpRecipeReadNumber(*RuleObject, TEXT("maxHeight"), Rule.MaxHeight);
                McpRecipeReadNumber(*RuleObject, TEXT("fadeDistance"), Rule.FadeDistance);
                McpRecipeReadNumber(*RuleObject, TEXT("minSlope"), Rule.MinSlope);
                McpRecipeReadNumber(*RuleObject, TEXT("maxSlope"), Rule.MaxSlope);
                McpRecipeReadNumber(*RuleObject, TEXT("fadeSlope"), Rule.FadeSlope);
                McpRecipeReadNumber(*RuleObject, TEXT("strength"), Rule.Strength);
                if (!Rule.LayerName.IsEmpty())
                    Config.Layers.Add(Rule);
            }
        }
    }

    if (bPayloadHasFoliage)
    {
        Config.Foliage.Reset();
        const TArray<TSharedPtr<FJsonValue>> *Foliage = nullptr;
        if (Payload->TryGetArrayField(TEXT("foliageTypes"), Foliage) && Foliage)
        {
            for (const TSharedPtr<FJsonValue> &Value : *Foliage)
            {
                const TSharedPtr<FJsonObject> *EntryObject = nullptr;
                if (!Value.IsValid() || !Value->TryGetObject(EntryObject) || !EntryObject)
                    continue;
                FMcpWorldRecipeConfig::FFoliageEntry Entry;
                Entry.MeshPath = McpRecipeFirstString(*EntryObject, {TEXT("meshPath"), TEXT("mesh")});
                McpRecipeReadNumber(*EntryObject, TEXT("count"), Entry.Count);
                McpRecipeReadNumber(*EntryObject, TEXT("density"), Entry.Count);
                McpRecipeReadNumber(*EntryObject, TEXT("minScale"), Entry.MinScale);
                McpRecipeReadNumber(*EntryObject, TEXT("maxScale"), Entry.MaxScale);
                McpRecipeReadNumber(*EntryObject, TEXT("minSlope"), Entry.MinSlope);
                McpRecipeReadNumber(*EntryObject, TEXT("maxSlope"), Entry.MaxSlope);
                McpRecipeReadNumber(*EntryObject, TEXT("minHeight"), Entry.MinHeight);
                McpRecipeReadNumber(*EntryObject, TEXT("maxHeight"), Entry.MaxHeight);
                if (!Entry.MeshPath.IsEmpty() && Entry.Count > 0)
                    Config.Foliage.Add(Entry);
            }
        }
    }
}

bool McpRecipeValidateConfig(const FMcpWorldRecipeConfig &Config, FString &OutError,
                             FString &OutErrorCode)
{
    const bool bValidSectionSize = Config.QuadsPerComponent == 7 ||
                                   Config.QuadsPerComponent == 15 ||
                                   Config.QuadsPerComponent == 31 ||
                                   Config.QuadsPerComponent == 63 ||
                                   Config.QuadsPerComponent == 127 ||
                                   Config.QuadsPerComponent == 255;
    if (!bValidSectionSize ||
        (Config.SectionsPerComponent != 1 && Config.SectionsPerComponent != 2) ||
        Config.ComponentsX < 1 || Config.ComponentsY < 1 ||
        Config.ComponentsX > 32 || Config.ComponentsY > 32 ||
        Config.ComponentsX * Config.ComponentsY > 1024)
    {
        OutError = TEXT("Invalid landscape resolution. quadsPerComponent must be 7, 15, 31, 63, 127, or 255; sectionsPerComponent must be 1 or 2; component dimensions must be 1..32 (max 1024 components).");
        OutErrorCode = TEXT("INVALID_LANDSCAPE_RESOLUTION");
        return false;
    }
    if (!Config.LandscapeName.IsEmpty() && Config.LandscapeName.Len() > 128)
    {
        OutError = TEXT("Landscape name must be 128 characters or fewer.");
        OutErrorCode = TEXT("INVALID_NAME");
        return false;
    }
    for (const FMcpWorldRecipeConfig::FLayerRule &Rule : Config.Layers)
    {
        if (Rule.LayerName.IsEmpty())
        {
            OutError = TEXT("Every world recipe layer requires a layerName.");
            OutErrorCode = TEXT("INVALID_ARGUMENT");
            return false;
        }
    }
    return true;
}

TSharedPtr<FJsonObject> McpRecipeMakeStepPayload(const TCHAR *SubAction)
{
    TSharedPtr<FJsonObject> Payload = McpHandlerUtils::CreateResultObject();
    Payload->SetStringField(TEXT("action"), SubAction);
    Payload->SetStringField(TEXT("subAction"), SubAction);
    return Payload;
}

ALandscape *McpRecipeFindLandscapeByName(UWorld *World, const FString &LandscapeName)
{
    if (!World || LandscapeName.IsEmpty())
        return nullptr;
    for (TActorIterator<ALandscape> It(World); It; ++It)
    {
        ALandscape *Landscape = *It;
        if (Landscape && Landscape->GetActorLabel().Equals(LandscapeName, ESearchCase::IgnoreCase))
            return Landscape;
    }
    return nullptr;
}

bool McpRecipeSaveLandscapePersistence(UWorld *World, ALandscape *Landscape, FString &OutError)
{
    if (!World || !World->PersistentLevel || !Landscape)
    {
        OutError = TEXT("Landscape world is unavailable for saving.");
        return false;
    }
    Landscape->MarkPackageDirty();
    World->PersistentLevel->MarkPackageDirty();
    if (!McpSafeAssetSave(Landscape) ||
        !McpSafeLevelSave(World->PersistentLevel, World->GetOutermost()->GetName()))
    {
        OutError = TEXT("Failed to save rule-painted landscape packages.");
        return false;
    }
    return true;
}

/** Deterministic per-vertex noise in [0,1] used by noise masks. */
double McpRecipeVertexNoise(int32 X, int32 Y, int32 Seed)
{
    const double Value = FMath::Sin(X * 0.37 + Y * 0.61 + Seed * 0.013) *
                             FMath::Cos(X * 0.11 - Y * 0.29 + Seed * 0.007) * 0.5 +
                         0.5;
    return FMath::Clamp(Value, 0.0, 1.0);
}

/**
 * Rule-based landscape layer paint. Reads the current heightfield, evaluates a
 * height/slope/altitude/noise mask per vertex, and writes the resulting layer
 * weights through FLandscapeEditDataInterface — the same write path as
 * paint_landscape_layer, but per-vertex instead of uniform.
 */
bool McpPaintWorldLayerByRule(UWorld *World,
                              const FMcpWorldRecipeConfig::FLayerRule &Rule,
                              const FString &LandscapeName,
                              const FString &LayerInfoPath,
                              int32 Seed,
                              TSharedPtr<FJsonObject> &OutResult,
                              FString &OutMessage,
                              FString &OutErrorCode)
{
    ALandscape *Landscape = McpRecipeFindLandscapeByName(World, LandscapeName);
    if (!Landscape)
    {
        OutErrorCode = TEXT("LANDSCAPE_NOT_FOUND");
        OutMessage = FString::Printf(TEXT("Landscape '%s' not found for rule paint."), *LandscapeName);
        return false;
    }
    ULandscapeInfo *LandscapeInfo = Landscape->GetLandscapeInfo();
    if (!LandscapeInfo)
    {
        OutErrorCode = TEXT("INVALID_LANDSCAPE");
        OutMessage = TEXT("Landscape has no info for rule paint.");
        return false;
    }

    // Resolve or create the layer info object (mirrors paint_landscape_layer).
    ULandscapeLayerInfoObject *LayerInfo = nullptr;
    for (const FLandscapeInfoLayerSettings &Layer : LandscapeInfo->Layers)
    {
        if (Layer.LayerName == FName(*Rule.LayerName))
        {
            LayerInfo = Layer.LayerInfoObj;
            break;
        }
    }
    if (!LayerInfo && !LayerInfoPath.IsEmpty())
    {
        LayerInfo = LoadObject<ULandscapeLayerInfoObject>(nullptr, *LayerInfoPath);
    }
    if (!LayerInfo)
    {
        const FString NewLayerPackagePath = FPackageName::GetLongPackagePath(
                                                World->GetOutermost()->GetName()) /
                                            FString::Printf(TEXT("%s_LayerInfo"), *Rule.LayerName);
        UPackage *NewLayerPackage = CreatePackage(*NewLayerPackagePath);
        ULandscapeLayerInfoObject *NewLayerInfo = NewObject<ULandscapeLayerInfoObject>(
            NewLayerPackage, FName(*FString::Printf(TEXT("%s_LayerInfo"), *Rule.LayerName)),
            RF_Public | RF_Standalone | RF_Transactional);
        if (!NewLayerInfo)
        {
            OutErrorCode = TEXT("LAYER_CREATION_FAILED");
            OutMessage = FString::Printf(TEXT("Failed to create layer info '%s'."), *Rule.LayerName);
            return false;
        }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
        NewLayerInfo->SetLayerName(FName(*Rule.LayerName), true);
#else
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        NewLayerInfo->LayerName = FName(*Rule.LayerName);
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
        NewLayerInfo->MarkPackageDirty();
        if (!McpSafeAssetSave(NewLayerInfo))
        {
            OutErrorCode = TEXT("SAVE_FAILED");
            OutMessage = TEXT("Auto-created layer info package could not be saved.");
            return false;
        }
        LayerInfo = NewLayerInfo;
    }

    const FScopedTransaction Transaction(FText::FromString(TEXT("Paint MCP World Recipe Layer")));
    Landscape->Modify();
    if (!Landscape->HasTargetLayer(FName(*Rule.LayerName)))
    {
        Landscape->AddTargetLayer(FName(*Rule.LayerName), FLandscapeTargetLayerSettings(LayerInfo));
    }
    LandscapeInfo->UpdateLayerInfoMap(Landscape);
    const int32 LayerInfoIndex = LandscapeInfo->GetLayerInfoIndex(FName(*Rule.LayerName));
    if (LayerInfoIndex == INDEX_NONE)
    {
        OutErrorCode = TEXT("LAYER_REGISTRATION_FAILED");
        OutMessage = TEXT("Failed to register landscape target layer.");
        return false;
    }
    LandscapeInfo->Layers[LayerInfoIndex].LayerInfoObj = LayerInfo;
    if (Landscape->GetLayersConst().Num() == 0)
    {
        Landscape->CreateLayer(FName(TEXT("MCP_Base")), nullptr, false);
    }
    if (const FLandscapeLayer *BaseEditLayer = Landscape->GetLayerConst(0);
        BaseEditLayer && BaseEditLayer->EditLayer)
    {
        Landscape->SetEditingLayer(BaseEditLayer->EditLayer->GetGuid());
    }

    int32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
    if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
    {
        OutErrorCode = TEXT("INVALID_LANDSCAPE");
        OutMessage = TEXT("Landscape has no extents for rule paint.");
        return false;
    }
    const int32 SizeX = MaxX - MinX + 1;
    const int32 SizeY = MaxY - MinY + 1;

    // Validate region size to prevent huge allocations (matches the
    // paint_landscape_layer guard).
    constexpr int64 MaxRulePaintVertices = 16777216; // 16M vertices
    if (static_cast<int64>(SizeX) * SizeY > MaxRulePaintVertices)
    {
        OutErrorCode = TEXT("REGION_TOO_LARGE");
        OutMessage = FString::Printf(
            TEXT("Rule paint region too large: %dx%d (%lld vertices). Maximum: %lld"),
            SizeX, SizeY, static_cast<int64>(SizeX) * SizeY, MaxRulePaintVertices);
        return false;
    }

    const FVector LandscapeScale = Landscape->GetActorScale3D();
    const double ScaleX = FMath::Max(1.0, static_cast<double>(LandscapeScale.X));
    const double ScaleZ = static_cast<double>(LandscapeScale.Z);
    const double ActorZ = Landscape->GetActorLocation().Z;

    TArray<uint16> Heights;
    Heights.SetNumZeroed(SizeX * SizeY);
    {
        // This object holds heightmap texture locks and must be destroyed
        // before package saving below.
        FLandscapeEditDataInterface Reader(LandscapeInfo, false);
        Reader.GetHeightData(MinX, MinY, MaxX, MaxY, Heights.GetData(), 0);
    }

    auto WorldZ = [&](int32 LocalX, int32 LocalY) -> double
    {
        const uint16 H = Heights[LocalY * SizeX + LocalX];
        return ActorZ + (static_cast<double>(H) - 32768.0) * ScaleZ / 128.0;
    };
    auto SlopeDegrees = [&](int32 LocalX, int32 LocalY) -> double
    {
        const int32 X0 = FMath::Max(0, LocalX - 1);
        const int32 X1 = FMath::Min(SizeX - 1, LocalX + 1);
        const int32 Y0 = FMath::Max(0, LocalY - 1);
        const int32 Y1 = FMath::Min(SizeY - 1, LocalY + 1);
        const double DzDx = (WorldZ(X1, LocalY) - WorldZ(X0, LocalY)) /
                            FMath::Max(1.0, static_cast<double>(X1 - X0) * ScaleX);
        const double DzDy = (WorldZ(LocalX, Y1) - WorldZ(LocalX, Y0)) /
                            FMath::Max(1.0, static_cast<double>(Y1 - Y0) * ScaleX);
        return FMath::Atan(FMath::Sqrt(FMath::Square(DzDx) + FMath::Square(DzDy))) * 180.0 / PI;
    };

    // Resolve height band defaults: an explicit targetHeight with a full-range
    // band collapses to target +/- fade.
    double MinHeight = Rule.MinHeight;
    double MaxHeight = Rule.MaxHeight;
    const bool bFullHeightBand = Rule.MinHeight <= -999999.0 && Rule.MaxHeight >= 999999.0;
    if (bFullHeightBand && Rule.TargetHeight != 0.0)
    {
        MinHeight = Rule.TargetHeight - FMath::Max(1.0, Rule.FadeDistance);
        MaxHeight = Rule.TargetHeight + FMath::Max(1.0, Rule.FadeDistance);
    }
    const double FadeDistance = FMath::Max(0.0, Rule.FadeDistance);
    const double FadeSlope = FMath::Clamp(Rule.FadeSlope, 0.0, 45.0);
    const bool bIsHeightMask = Rule.MaskType.Equals(TEXT("height"), ESearchCase::IgnoreCase) ||
                               Rule.MaskType.Equals(TEXT("altitude"), ESearchCase::IgnoreCase);

    TArray<uint8> Alpha;
    Alpha.SetNumZeroed(SizeX * SizeY);
    int32 NonZeroWeights = 0;

    if (Rule.MaskType.Equals(TEXT("constant"), ESearchCase::IgnoreCase))
    {
        Alpha.Init(static_cast<uint8>(FMath::Clamp(Rule.Strength, 0.0, 1.0) * 255.0 + 0.5), SizeX * SizeY);
        for (uint8 Value : Alpha)
            if (Value > 0)
                ++NonZeroWeights;
    }
    else
    {
        for (int32 Y = 0; Y < SizeY; ++Y)
        {
            for (int32 X = 0; X < SizeX; ++X)
            {
                double Weight = 1.0;
                if (bIsHeightMask)
                {
                    const double Z = WorldZ(X, Y);
                    if (FadeDistance <= 0.0)
                        Weight = (Z >= MinHeight && Z <= MaxHeight) ? 1.0 : 0.0;
                    else
                        Weight = FMath::Clamp(
                                     FMath::Min((Z - (MinHeight - FadeDistance)) / FadeDistance,
                                                ((MaxHeight + FadeDistance) - Z) / FadeDistance),
                                     0.0, 1.0);
                }
                else if (Rule.MaskType.Equals(TEXT("slope"), ESearchCase::IgnoreCase))
                {
                    const double Slope = SlopeDegrees(X, Y);
                    if (FadeSlope <= 0.0)
                        Weight = (Slope >= Rule.MinSlope && Slope <= Rule.MaxSlope) ? 1.0 : 0.0;
                    else
                        Weight = FMath::Clamp(
                                     FMath::Min((Slope - (Rule.MinSlope - FadeSlope)) / FadeSlope,
                                                ((Rule.MaxSlope + FadeSlope) - Slope) / FadeSlope),
                                     0.0, 1.0);
                }
                else if (Rule.MaskType.Equals(TEXT("noise"), ESearchCase::IgnoreCase))
                {
                    Weight = McpRecipeVertexNoise(X + MinX, Y + MinY, Seed);
                }
                else
                {
                    OutErrorCode = TEXT("INVALID_ARGUMENT");
                    OutMessage = FString::Printf(TEXT("Unknown layer maskType '%s'."), *Rule.MaskType);
                    return false;
                }
                const uint8 Value = static_cast<uint8>(
                    FMath::Clamp(Weight * FMath::Clamp(Rule.Strength, 0.0, 1.0), 0.0, 1.0) * 255.0 + 0.5);
                Alpha[Y * SizeX + X] = Value;
                if (Value > 0)
                    ++NonZeroWeights;
            }
        }
    }

    int64 NonZeroAfterWrite = 0;
    {
        // This object holds writable weightmap texture locks and must be
        // destroyed before safe package saving, which may compress textures.
        FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo, Landscape->GetEditingLayer(), false);
        LandscapeEdit.SetAlphaData(LayerInfo, MinX, MinY, MaxX, MaxY, Alpha.GetData(), SizeX);

        // A successful authoring request is persistence-bound; never leave the
        // edit unflushed.
        LandscapeEdit.Flush();

        TArray<uint8> SavedAlpha;
        SavedAlpha.SetNumUninitialized(SizeX * SizeY);
        LandscapeEdit.GetWeightData(LayerInfo, MinX, MinY, MaxX, MaxY,
                                    SavedAlpha.GetData(), SizeX);
        for (uint8 Value : SavedAlpha)
            if (Value > 0)
                ++NonZeroAfterWrite;
    }
    Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Weightmap_All);
    LandscapeInfo->ForceLayersFullUpdate();

    FString SaveError;
    if (!McpRecipeSaveLandscapePersistence(World, Landscape, SaveError))
    {
        OutErrorCode = TEXT("SAVE_FAILED");
        OutMessage = SaveError;
        return false;
    }

    OutResult = McpHandlerUtils::CreateResultObject();
    OutResult->SetBoolField(TEXT("success"), true);
    OutResult->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
    OutResult->SetStringField(TEXT("landscapePath"), Landscape->GetPathName());
    OutResult->SetStringField(TEXT("layerName"), Rule.LayerName);
    OutResult->SetStringField(TEXT("maskType"), Rule.MaskType);
    OutResult->SetStringField(TEXT("layerInfoPath"), LayerInfo->GetPathName());
    OutResult->SetNumberField(TEXT("nonZeroWeightCount"), static_cast<double>(NonZeroAfterWrite));
    OutResult->SetNumberField(TEXT("vertexCount"), static_cast<double>(SizeX * SizeY));
    OutMessage = FString::Printf(TEXT("Layer '%s' painted by %s mask (%d weighted vertices)."),
                                 *Rule.LayerName, *Rule.MaskType, static_cast<int32>(NonZeroAfterWrite));
    return true;
}
#endif // WITH_EDITOR
} // namespace

// =============================================================================
// Section A: World Recipe Chain Runner
// =============================================================================

bool UNebulaForgeBridgeSubsystem::IsWorldRecipeChainActive() const
{
    return RecipeStepIndex != INDEX_NONE;
}

#if WITH_EDITOR

void UNebulaForgeBridgeSubsystem::RunNextWorldRecipeStep()
{
    if (RecipeStepIndex == INDEX_NONE)
        return;
    if (RecipeStepIndex >= RecipeSteps.Num())
    {
        FinalizeWorldRecipeChain();
        return;
    }

    const FMcpWorldRecipeStep &Step = RecipeSteps[RecipeStepIndex];
    const float Percent = 100.0f * static_cast<float>(RecipeStepIndex) /
                          FMath::Max(1.0f, static_cast<float>(RecipeSteps.Num()));
    SendProgressUpdate(RecipeRequestId, Percent,
                       FString::Printf(TEXT("World recipe step %d/%d: %s"),
                                       RecipeStepIndex + 1, RecipeSteps.Num(), *Step.Label),
                       true);

    if (Step.CustomStep)
    {
        TSharedPtr<FJsonObject> StepResult;
        FString StepMessage;
        FString StepErrorCode;
        const bool bStepSuccess = Step.CustomStep(Step.StepId, StepResult, StepMessage, StepErrorCode);
        TSharedPtr<FJsonObject> Captured = McpHandlerUtils::CreateResultObject();
        Captured->SetBoolField(TEXT("success"), bStepSuccess);
        Captured->SetStringField(TEXT("message"), StepMessage);
        if (!StepErrorCode.IsEmpty())
            Captured->SetStringField(TEXT("errorCode"), StepErrorCode);
        if (StepResult.IsValid())
            Captured->SetObjectField(TEXT("result"), StepResult);
        RecipeCapturedResponse = Captured;
        AdvanceWorldRecipeChain();
        return;
    }

    RecipeCapturedResponse.Reset();
    bRecipeCapturing = true;
    const FAutomationHandler *Handler = AutomationHandlers.Find(Step.Action);
    const bool bHandled = Handler ? (*Handler)(RecipeRequestId, Step.Action, Step.Payload, nullptr) : false;
    if (!bHandled && bRecipeCapturing)
    {
        // No registered handler claimed the step action and nothing responded;
        // synthesize a failure so the chain still terminates truthfully.
        TSharedPtr<FJsonObject> Captured = McpHandlerUtils::CreateResultObject();
        Captured->SetBoolField(TEXT("success"), false);
        Captured->SetStringField(TEXT("message"),
                                 FString::Printf(TEXT("No handler for step action '%s'."), *Step.Action));
        Captured->SetStringField(TEXT("errorCode"), TEXT("UNKNOWN_STEP_ACTION"));
        RecipeCapturedResponse = Captured;
        bRecipeCapturing = false;
        AdvanceWorldRecipeChain();
    }
    // When bRecipeCapturing is still true the step handler is completing
    // asynchronously; its SendAutomationResponse call will be captured by the
    // subsystem and advance the chain there.
}

void UNebulaForgeBridgeSubsystem::AdvanceWorldRecipeChain()
{
    if (RecipeStepIndex == INDEX_NONE || RecipeStepIndex >= RecipeSteps.Num())
        return;
    if (!RecipeCapturedResponse.IsValid())
        return;

    const FMcpWorldRecipeStep &Step = RecipeSteps[RecipeStepIndex];
    RecipeCapturedResponse->SetStringField(TEXT("action"), Step.Action.IsEmpty() ? TEXT("custom") : Step.Action);
    RecipeCapturedResponse->SetStringField(TEXT("label"), Step.Label);
    RecipeCapturedResponse->SetStringField(TEXT("stepId"), Step.StepId.ToString());
    RecipeStepResults.Add(RecipeCapturedResponse);
    RecipeCapturedResponse.Reset();

    bool bFailed = false;
    if (!RecipeStepResults.Last()->GetBoolField(TEXT("success")))
    {
        bFailed = true;
        ++RecipeFailedSteps;
    }

    if (bFailed && Step.bAbortOnFailure)
    {
        for (int32 Index = RecipeStepIndex + 1; Index < RecipeSteps.Num(); ++Index)
        {
            TSharedPtr<FJsonObject> Skipped = McpHandlerUtils::CreateResultObject();
            Skipped->SetBoolField(TEXT("success"), false);
            Skipped->SetStringField(TEXT("status"), TEXT("SKIPPED"));
            Skipped->SetStringField(TEXT("action"),
                                    RecipeSteps[Index].Action.IsEmpty() ? TEXT("custom") : RecipeSteps[Index].Action);
            Skipped->SetStringField(TEXT("label"), RecipeSteps[Index].Label);
            Skipped->SetStringField(TEXT("stepId"), RecipeSteps[Index].StepId.ToString());
            Skipped->SetStringField(TEXT("message"), TEXT("Skipped because an abort-on-failure step failed."));
            RecipeStepResults.Add(Skipped);
            ++RecipeSkippedSteps;
        }
        RecipeStepIndex = RecipeSteps.Num();
        TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
        AsyncTask(ENamedThreads::GameThread, [WeakThis]()
                  {
            if (UNebulaForgeBridgeSubsystem *Subsystem = WeakThis.Get())
                Subsystem->FinalizeWorldRecipeChain(); });
        return;
    }

    ++RecipeStepIndex;
    TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
    AsyncTask(ENamedThreads::GameThread, [WeakThis]()
              {
        if (UNebulaForgeBridgeSubsystem *Subsystem = WeakThis.Get())
            Subsystem->RunNextWorldRecipeStep(); });
}

void UNebulaForgeBridgeSubsystem::FinalizeWorldRecipeChain()
{
    if (RecipeStepIndex == INDEX_NONE)
        return;

    // Copy chain state and clear it first so the completion callback (or a
    // response-side re-entry) can safely start a new chain.
    TArray<TSharedPtr<FJsonObject>> StepResults = MoveTemp(RecipeStepResults);
    const int32 FailedSteps = RecipeFailedSteps;
    const int32 SkippedSteps = RecipeSkippedSteps;
    const int32 TotalSteps = RecipeSteps.Num();
    TSharedPtr<FJsonObject> Meta = RecipeSummaryMeta;
    TSharedPtr<FMcpBridgeWebSocket> Socket = RecipeSocket;
    const FString RequestId = RecipeRequestId;
    auto Completion = MoveTemp(RecipeCompletion);

    RecipeSteps.Reset();
    RecipeStepResults.Reset();
    RecipeStepIndex = INDEX_NONE;
    RecipeFailedSteps = 0;
    RecipeSkippedSteps = 0;
    RecipeRequestId.Reset();
    RecipeSocket = nullptr;
    RecipeCapturedResponse.Reset();
    RecipeCompletion = nullptr;
    bRecipeCapturing = false;
    RecipeSummaryMeta.Reset();

    const bool bOverallSuccess = FailedSteps == 0;
    int32 SucceededSteps = 0;
    TArray<TSharedPtr<FJsonValue>> StepJson;
    StepJson.Reserve(StepResults.Num());
    for (const TSharedPtr<FJsonObject> &StepResult : StepResults)
    {
        if (StepResult.IsValid() && StepResult->GetBoolField(TEXT("success")))
            ++SucceededSteps;
        StepJson.Add(MakeShared<FJsonValueObject>(StepResult));
    }
    const FString Status = bOverallSuccess ? TEXT("PASS")
                          : (SucceededSteps > 0 ? TEXT("PARTIAL") : TEXT("FAIL"));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), bOverallSuccess);
    Result->SetStringField(TEXT("status"), Status);
    Result->SetNumberField(TEXT("stepCount"), TotalSteps);
    Result->SetNumberField(TEXT("succeededSteps"), SucceededSteps);
    Result->SetNumberField(TEXT("failedSteps"), FailedSteps);
    Result->SetNumberField(TEXT("skippedSteps"), SkippedSteps);
    Result->SetArrayField(TEXT("steps"), StepJson);
    if (Meta.IsValid())
    {
        for (const TPair<FString, TSharedPtr<FJsonValue>> &Pair : Meta->Values)
            Result->SetField(Pair.Key, Pair.Value);
    }

    const FString Message = bOverallSuccess
        ? FString::Printf(TEXT("World recipe completed: %d steps passed."), TotalSteps)
        : FString::Printf(TEXT("World recipe finished with %d failed and %d skipped of %d steps."),
                          FailedSteps, SkippedSteps, TotalSteps);

    if (Completion)
    {
        Completion(bOverallSuccess, Result);
        return;
    }
    SendAutomationResponse(Socket, RequestId, bOverallSuccess, Message, Result, FString());
}

#endif // WITH_EDITOR (chain runner)

#if WITH_EDITOR // preset authoring, recipe builders, dispatch

bool UNebulaForgeBridgeSubsystem::ListBiomePresetAssetPaths(TArray<FString> &OutPaths) const
{
#if WITH_EDITOR
    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry &Registry = AssetRegistryModule.Get();
    TArray<FAssetData> AssetData;
    Registry.GetAssetsByClass(
        UMcpBiomePreset::StaticClass()->GetClassPathName(), AssetData, true);
    for (const FAssetData &Data : AssetData)
    {
        OutPaths.Add(Data.GetSoftObjectPath().ToString());
        if (OutPaths.Num() >= MaxBiomePresetsToList)
            break;
    }
    OutPaths.Sort();
    return true;
#else
    return false;
#endif
}

namespace
{
#if WITH_EDITOR
void McpRecipeBuildSummaryMeta(const FMcpWorldRecipeConfig &Config,
                               const FString &PresetPath,
                               int32 StepCount,
                               TSharedPtr<FJsonObject> &OutMeta)
{
    OutMeta = McpHandlerUtils::CreateResultObject();
    OutMeta->SetStringField(TEXT("landscapeName"), Config.LandscapeName);
    OutMeta->SetNumberField(TEXT("seed"), Config.Seed);
    OutMeta->SetStringField(TEXT("terrainFeature"), Config.TerrainFeature);
    OutMeta->SetNumberField(TEXT("layerRuleCount"), Config.Layers.Num());
    OutMeta->SetNumberField(TEXT("foliageEntryCount"), Config.Foliage.Num());
    if (!PresetPath.IsEmpty())
        OutMeta->SetStringField(TEXT("biomePresetPath"), PresetPath);
    if (!Config.MaterialPath.IsEmpty())
        OutMeta->SetStringField(TEXT("materialPath"), Config.MaterialPath);
    OutMeta->SetNumberField(TEXT("requestedStepCount"), StepCount);
}
#endif
} // namespace

bool UNebulaForgeBridgeSubsystem::RunWorldRecipe(
    const TSharedPtr<FJsonObject> &Payload,
    TFunction<void(bool bSuccess, const TSharedPtr<FJsonObject> &Result)> Completion)
{
#if WITH_EDITOR
    if (!Completion)
        return false;
    const FString RequestId = FString::Printf(TEXT("worldbuilder-ui-%s"), *FGuid::NewGuid().ToString());
    TSharedPtr<FJsonObject> UiPayload = Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
    const TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
    AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestId, UiPayload, Completion]()
              {
        if (UNebulaForgeBridgeSubsystem *Subsystem = WeakThis.Get())
            Subsystem->BeginGenerateWorld(RequestId, UiPayload, nullptr, Completion); });
    return true;
#else
    return false;
#endif
}

void UNebulaForgeBridgeSubsystem::BeginGenerateWorld(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const TFunction<void(bool bSuccess, const TSharedPtr<FJsonObject> &Result)> &Completion)
{
    if (IsWorldRecipeChainActive())
    {
        if (Completion)
        {
            TSharedPtr<FJsonObject> Busy = McpHandlerUtils::CreateResultObject();
            Busy->SetBoolField(TEXT("success"), false);
            Busy->SetStringField(TEXT("error"), TEXT("A world recipe chain is already running."));
            Completion(false, Busy);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId,
                                TEXT("A world recipe chain is already running."),
                                TEXT("RECIPE_BUSY"));
        }
        return;
    }

    FMcpWorldRecipeConfig Config;
    FString PresetPath = McpRecipeFirstString(Payload, {TEXT("biomePresetPath"), TEXT("presetPath")});
    UMcpBiomePreset *Preset = nullptr;
    if (!PresetPath.IsEmpty())
    {
        const FString SafePresetPath = SanitizeProjectRelativePath(PresetPath);
        if (SafePresetPath.IsEmpty())
        {
            const FString Message = FString::Printf(TEXT("Invalid or unsafe biome preset path: %s"), *PresetPath);
            if (Completion)
            {
                TSharedPtr<FJsonObject> Error = McpHandlerUtils::CreateResultObject();
                Error->SetBoolField(TEXT("success"), false);
                Error->SetStringField(TEXT("error"), Message);
                Completion(false, Error);
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, Message, TEXT("SECURITY_VIOLATION"));
            }
            return;
        }
        PresetPath = SafePresetPath;
        Preset = LoadObject<UMcpBiomePreset>(nullptr, *PresetPath);
        if (!Preset)
        {
            const FString Message = FString::Printf(TEXT("Biome preset not found: %s"), *PresetPath);
            if (Completion)
            {
                TSharedPtr<FJsonObject> Error = McpHandlerUtils::CreateResultObject();
                Error->SetBoolField(TEXT("success"), false);
                Error->SetStringField(TEXT("error"), Message);
                Completion(false, Error);
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, Message, TEXT("PRESET_NOT_FOUND"));
            }
            return;
        }
        McpRecipeApplyPreset(Preset, Config);
    }

    const bool bPayloadHasLayers = Payload.IsValid() && Payload->HasField(TEXT("layers"));
    const bool bPayloadHasFoliage = Payload.IsValid() && Payload->HasField(TEXT("foliageTypes"));
    McpRecipeApplyPayloadOverrides(Payload, Config, bPayloadHasLayers, bPayloadHasFoliage);

    if (!Config.MaterialPath.IsEmpty())
        Config.bCreateMaterial = false;

    FString ValidationError;
    FString ValidationErrorCode;
    if (!McpRecipeValidateConfig(Config, ValidationError, ValidationErrorCode))
    {
        if (Completion)
        {
            TSharedPtr<FJsonObject> Error = McpHandlerUtils::CreateResultObject();
            Error->SetBoolField(TEXT("success"), false);
            Error->SetStringField(TEXT("error"), ValidationError);
            Completion(false, Error);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, ValidationError, ValidationErrorCode);
        }
        return;
    }

    if (!GEditor || !GEditor->GetEditorWorldContext().World() ||
        !GEditor->GetEditorWorldContext().World()->GetOutermost()->GetName().StartsWith(TEXT("/Game/")))
    {
        const FString Message = TEXT("generate_world requires a saved /Game map. Create or load the destination map first.");
        if (Completion)
        {
            TSharedPtr<FJsonObject> Error = McpHandlerUtils::CreateResultObject();
            Error->SetBoolField(TEXT("success"), false);
            Error->SetStringField(TEXT("error"), Message);
            Completion(false, Error);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, Message, TEXT("MAP_NOT_SAVED"));
        }
        return;
    }

    UWorld *World = GEditor->GetEditorWorldContext().World();
    ALandscape *ExistingLandscape =
        Config.bReuseExistingLandscape ? McpRecipeFindLandscapeByName(World, Config.LandscapeName) : nullptr;

    TArray<FMcpWorldRecipeStep> Steps;
    FString MaterialAssetPath;

    if (Config.bCreateMaterial)
    {
        const FString MaterialName = SanitizeAssetName(
            FString::Printf(TEXT("M_%s"), *Config.LandscapeName));
        const FString MaterialFolderPath = TEXT("/Game/MCPWorldBuilder/Materials");
        TSharedPtr<FJsonObject> MaterialPayload = McpRecipeMakeStepPayload(TEXT("create_landscape_material"));
        MaterialPayload->SetStringField(TEXT("name"), MaterialName);
        MaterialPayload->SetStringField(TEXT("path"), MaterialFolderPath);
        MaterialPayload->SetBoolField(TEXT("save"), true);
        FMcpWorldRecipeStep Step;
        Step.Action = TEXT("build_environment");
        Step.Label = FString::Printf(TEXT("create landscape material %s"), *MaterialName);
        Step.Payload = MaterialPayload;
        Steps.Add(MoveTemp(Step));
        MaterialAssetPath = MaterialFolderPath / MaterialName + TEXT(".") + MaterialName;
    }
    else if (!Config.MaterialPath.IsEmpty())
    {
        MaterialAssetPath = SanitizeProjectRelativePath(Config.MaterialPath);
        if (MaterialAssetPath.IsEmpty())
            MaterialAssetPath = Config.MaterialPath;
    }

    for (const FMcpWorldRecipeConfig::FLayerRule &Rule : Config.Layers)
    {
        if (Config.bSkipPaint)
            break;
        TSharedPtr<FJsonObject> LayerInfoPayload = McpRecipeMakeStepPayload(TEXT("create_landscape_layer_info"));
        LayerInfoPayload->SetStringField(TEXT("layerName"), Rule.LayerName);
        LayerInfoPayload->SetStringField(TEXT("path"), TEXT("/Game/MCPWorldBuilder/Layers"));
        FMcpWorldRecipeStep LayerInfoStep;
        LayerInfoStep.Action = TEXT("build_environment");
        LayerInfoStep.Label = FString::Printf(TEXT("create layer info %s"), *Rule.LayerName);
        LayerInfoStep.Payload = LayerInfoPayload;
        Steps.Add(MoveTemp(LayerInfoStep));
    }

    if (Config.bCreateMaterial && !Config.bSkipPaint && Config.Layers.Num() > 0 &&
        !MaterialAssetPath.IsEmpty())
    {
        // Wire the auto-created material with a LandscapeLayerBlend node per
        // rule layer connected to BaseColor (closes the auto-material gap).
        TSharedPtr<FJsonObject> BlendPayload = McpRecipeMakeStepPayload(TEXT("configure_landscape_layer_blend"));
        BlendPayload->SetStringField(TEXT("assetPath"), MaterialAssetPath);
        BlendPayload->SetBoolField(TEXT("save"), true);
        TArray<TSharedPtr<FJsonValue>> BlendLayers;
        for (const FMcpWorldRecipeConfig::FLayerRule &Rule : Config.Layers)
        {
            TSharedPtr<FJsonObject> BlendEntry = McpHandlerUtils::CreateResultObject();
            BlendEntry->SetStringField(TEXT("layerName"), Rule.LayerName);
            BlendEntry->SetStringField(TEXT("blendType"), TEXT("LB_WeightBlend"));
            BlendLayers.Add(MakeShared<FJsonValueObject>(BlendEntry));
        }
        BlendPayload->SetArrayField(TEXT("layers"), BlendLayers);
        FMcpWorldRecipeStep BlendStep;
        BlendStep.Action = TEXT("build_environment");
        BlendStep.Label = TEXT("wire landscape layer blend graph");
        BlendStep.Payload = BlendPayload;
        Steps.Add(MoveTemp(BlendStep));
    }

    if (!Config.bSkipLandscape && !ExistingLandscape)
    {
        TSharedPtr<FJsonObject> LandscapePayload = McpRecipeMakeStepPayload(TEXT("create_landscape"));
        LandscapePayload->SetStringField(TEXT("name"), Config.LandscapeName);
        LandscapePayload->SetNumberField(TEXT("x"), Config.Location.X);
        LandscapePayload->SetNumberField(TEXT("y"), Config.Location.Y);
        LandscapePayload->SetNumberField(TEXT("z"), Config.Location.Z);
        LandscapePayload->SetNumberField(TEXT("componentsX"), Config.ComponentsX);
        LandscapePayload->SetNumberField(TEXT("componentsY"), Config.ComponentsY);
        LandscapePayload->SetNumberField(TEXT("quadsPerSection"), Config.QuadsPerComponent);
        LandscapePayload->SetNumberField(TEXT("sectionsPerComponent"), Config.SectionsPerComponent);
        if (!MaterialAssetPath.IsEmpty())
            LandscapePayload->SetStringField(TEXT("materialPath"), MaterialAssetPath);
        FMcpWorldRecipeStep Step;
        Step.Action = TEXT("build_environment");
        Step.Label = FString::Printf(TEXT("create landscape %s"), *Config.LandscapeName);
        Step.Payload = LandscapePayload;
        // Heightfield, painting, and scatter steps all require a landscape.
        Step.bAbortOnFailure = true;
        Steps.Add(MoveTemp(Step));
    }
    else if (ExistingLandscape && !MaterialAssetPath.IsEmpty() && Config.bCreateMaterial)
    {
        TSharedPtr<FJsonObject> MaterialPayload = McpRecipeMakeStepPayload(TEXT("set_landscape_material"));
        MaterialPayload->SetStringField(TEXT("landscapeName"), Config.LandscapeName);
        MaterialPayload->SetStringField(TEXT("materialPath"), MaterialAssetPath);
        FMcpWorldRecipeStep Step;
        Step.Action = TEXT("build_environment");
        Step.Label = TEXT("assign landscape material");
        Step.Payload = MaterialPayload;
        Steps.Add(MoveTemp(Step));
    }

    if (!Config.bSkipTerrain)
    {
        const TCHAR *TerrainAction = Config.bErosion ? TEXT("apply_landscape_erosion")
                                                     : TEXT("generate_landscape_heightmap");
        TSharedPtr<FJsonObject> TerrainPayload = McpRecipeMakeStepPayload(TerrainAction);
        TerrainPayload->SetStringField(TEXT("landscapeName"), Config.LandscapeName);
        TerrainPayload->SetNumberField(TEXT("resolutionX"), Config.Resolution);
        TerrainPayload->SetNumberField(TEXT("resolutionY"), Config.Resolution);
        TerrainPayload->SetNumberField(TEXT("heightScale"), Config.HeightScale);
        TerrainPayload->SetNumberField(TEXT("frequency"), Config.Frequency);
        TerrainPayload->SetNumberField(TEXT("seed"), Config.Seed);
        TerrainPayload->SetStringField(TEXT("terrainFeature"), Config.TerrainFeature);
        if (Config.bErosion)
            TerrainPayload->SetNumberField(TEXT("iterations"), Config.ErosionIterations);
        FMcpWorldRecipeStep Step;
        Step.Action = TEXT("build_environment");
        Step.Label = FString::Printf(TEXT("%s (%s)"),
                                     Config.bErosion ? TEXT("erode heightfield") : TEXT("generate heightfield"),
                                     *Config.TerrainFeature);
        Step.Payload = TerrainPayload;
        Steps.Add(MoveTemp(Step));
    }

    for (const FMcpWorldRecipeConfig::FLayerRule &Rule : Config.Layers)
    {
        if (Config.bSkipPaint)
            break;
        const FMcpWorldRecipeConfig::FLayerRule CapturedRule = Rule;
        const FString CapturedLandscapeName = Config.LandscapeName;
        const FString CapturedLayerInfoPath =
            FString::Printf(TEXT("/Game/MCPWorldBuilder/Layers/%s.%s"), *Rule.LayerName, *Rule.LayerName);
        const int32 CapturedSeed = Config.Seed;
        const TWeakObjectPtr<UWorld> CapturedWorld = World;
        FMcpWorldRecipeStep Step;
        Step.Label = FString::Printf(TEXT("paint layer %s (%s mask)"), *Rule.LayerName, *Rule.MaskType);
        Step.CustomStep = [CapturedRule, CapturedLandscapeName, CapturedLayerInfoPath, CapturedSeed, CapturedWorld](
                              const FGuid &, TSharedPtr<FJsonObject> &OutResult,
                              FString &OutMessage, FString &OutErrorCode) -> bool
        {
            return McpPaintWorldLayerByRule(CapturedWorld.Get(), CapturedRule, CapturedLandscapeName,
                                            CapturedLayerInfoPath, CapturedSeed, OutResult,
                                            OutMessage, OutErrorCode);
        };
        Steps.Add(MoveTemp(Step));
    }

    int32 FoliageIndex = 0;
    for (const FMcpWorldRecipeConfig::FFoliageEntry &Entry : Config.Foliage)
    {
        if (Config.bSkipFoliage)
            break;
        TSharedPtr<FJsonObject> FoliagePayload = McpRecipeMakeStepPayload(TEXT("scatter_landscape_foliage"));
        FoliagePayload->SetStringField(TEXT("landscapeName"), Config.LandscapeName);
        FoliagePayload->SetNumberField(TEXT("seed"), Config.Seed + FoliageIndex * 7919);
        const FString MeshBaseName = FPaths::GetBaseFilename(Entry.MeshPath);
        FoliagePayload->SetStringField(
            TEXT("foliageName"),
            SanitizeAssetName(FString::Printf(TEXT("%s_Foliage_%d_%s"),
                                              *Config.LandscapeName, FoliageIndex, *MeshBaseName)));
        TSharedPtr<FJsonObject> TypeEntry = McpHandlerUtils::CreateResultObject();
        TypeEntry->SetStringField(TEXT("meshPath"), Entry.MeshPath);
        TypeEntry->SetNumberField(TEXT("count"), Entry.Count);
        TypeEntry->SetNumberField(TEXT("minScale"), Entry.MinScale);
        TypeEntry->SetNumberField(TEXT("maxScale"), Entry.MaxScale);
        TArray<TSharedPtr<FJsonValue>> TypeArray;
        TypeArray.Add(MakeShared<FJsonValueObject>(TypeEntry));
        FoliagePayload->SetArrayField(TEXT("foliageTypes"), TypeArray);
        FoliagePayload->SetNumberField(TEXT("minSlope"), Entry.MinSlope);
        FoliagePayload->SetNumberField(TEXT("maxSlope"), Entry.MaxSlope);
        FoliagePayload->SetNumberField(TEXT("minHeight"), Entry.MinHeight);
        FoliagePayload->SetNumberField(TEXT("maxHeight"), Entry.MaxHeight);
        FMcpWorldRecipeStep Step;
        Step.Action = TEXT("build_environment");
        Step.Label = FString::Printf(TEXT("scatter foliage %s x%d"), *MeshBaseName, Entry.Count);
        Step.Payload = FoliagePayload;
        Steps.Add(MoveTemp(Step));
        ++FoliageIndex;
    }

    if (Steps.Num() == 0)
    {
        const FString Message = TEXT("World recipe produced no steps; nothing to generate.");
        if (Completion)
        {
            TSharedPtr<FJsonObject> Error = McpHandlerUtils::CreateResultObject();
            Error->SetBoolField(TEXT("success"), false);
            Error->SetStringField(TEXT("error"), Message);
            Completion(false, Error);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, Message, TEXT("EMPTY_RECIPE"));
        }
        return;
    }

    RecipeSummaryMeta = McpHandlerUtils::CreateResultObject();
    McpRecipeBuildSummaryMeta(Config, PresetPath, Steps.Num(), RecipeSummaryMeta);
    if (ExistingLandscape)
        RecipeSummaryMeta->SetBoolField(TEXT("reusedExistingLandscape"), true);
    if (!MaterialAssetPath.IsEmpty())
        RecipeSummaryMeta->SetStringField(TEXT("materialAssetPath"), MaterialAssetPath);

    RecipeSteps = MoveTemp(Steps);
    RecipeStepResults.Reset();
    RecipeStepIndex = 0;
    RecipeFailedSteps = 0;
    RecipeSkippedSteps = 0;
    RecipeRequestId = RequestId;
    RecipeSocket = RequestingSocket;
    RecipeCompletion = Completion;
    RecipeCapturedResponse.Reset();
    bRecipeCapturing = false;

    UE_LOG(LogMcpWorldRecipes, Log,
           TEXT("Starting world recipe chain: request=%s steps=%d seed=%d landscape=%s"),
           *RequestId, RecipeSteps.Num(), Config.Seed, *Config.LandscapeName);

    const TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
    AsyncTask(ENamedThreads::GameThread, [WeakThis]()
              {
        if (UNebulaForgeBridgeSubsystem *Subsystem = WeakThis.Get())
            Subsystem->RunNextWorldRecipeStep(); });
}

// =============================================================================
// Section B: Preset Authoring Actions
// =============================================================================

bool UNebulaForgeBridgeSubsystem::HandleCreateBiomePreset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("create_biome_preset payload missing"),
                            TEXT("INVALID_PAYLOAD"));
        return true;
    }
    const FString Name = McpRecipeFirstString(Payload, {TEXT("name"), TEXT("presetName")});
    if (Name.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("name required for create_biome_preset"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    FString Path = McpRecipeFirstString(Payload, {TEXT("path"), TEXT("folder")});
    if (Path.IsEmpty())
        Path = TEXT("/Game/MCPWorldBuilder/Presets");

    const FString SanitizedName = SanitizeAssetName(Name);
    const FString NormalizedOriginal = Name.Replace(TEXT("_"), TEXT(""));
    const FString NormalizedSanitized = SanitizedName.Replace(TEXT("_"), TEXT(""));
    if (SanitizedName.IsEmpty() || NormalizedSanitized != NormalizedOriginal)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Invalid preset name '%s'. Names can only contain alphanumeric characters and underscores."), *Name),
                            TEXT("INVALID_NAME"));
        return true;
    }
    FString ValidatedPath;
    FString PathError;
    if (!ValidateAssetCreationPath(Path, SanitizedName, ValidatedPath, PathError))
    {
        SendAutomationError(RequestingSocket, RequestId, PathError, TEXT("INVALID_PATH"));
        return true;
    }
    const FString FullAssetPath = ValidatedPath + TEXT(".") + SanitizedName;
    if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
    {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Asset already exists at path: %s"), *FullAssetPath),
                            TEXT("ASSET_EXISTS"));
        return true;
    }

    // Fill a recipe config from the same payload grammar generate_world uses,
    // then transfer it into the preset asset.
    FMcpWorldRecipeConfig Config;
    const bool bHasLayers = Payload->HasField(TEXT("layers"));
    const bool bHasFoliage = Payload->HasField(TEXT("foliageTypes"));
    McpRecipeApplyPayloadOverrides(Payload, Config, bHasLayers, bHasFoliage);

    UPackage *Package = CreatePackage(*ValidatedPath);
    if (!Package)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create preset package."),
                            TEXT("PACKAGE_ERROR"));
        return true;
    }
    UMcpBiomePreset *Preset =
        NewObject<UMcpBiomePreset>(Package, FName(*SanitizedName), RF_Public | RF_Standalone);
    if (!Preset)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create biome preset."),
                            TEXT("CREATE_FAILED"));
        return true;
    }

    FString Description;
    Payload->TryGetStringField(TEXT("description"), Description);
    Preset->Description = Description;
    Preset->Seed = Config.Seed;
    Preset->Landscape.LandscapeName = Config.LandscapeName;
    Preset->Landscape.ComponentsX = Config.ComponentsX;
    Preset->Landscape.ComponentsY = Config.ComponentsY;
    Preset->Landscape.QuadsPerComponent = Config.QuadsPerComponent;
    Preset->Landscape.SectionsPerComponent = Config.SectionsPerComponent;
    Preset->Landscape.Location = Config.Location;
    Preset->Terrain.TerrainFeature = Config.TerrainFeature;
    Preset->Terrain.HeightScale = Config.HeightScale;
    Preset->Terrain.Frequency = Config.Frequency;
    Preset->Terrain.Resolution = Config.Resolution;
    Preset->Terrain.bEnableErosion = Config.bErosion;
    Preset->Terrain.ErosionIterations = Config.ErosionIterations;
    Preset->MaterialPath = SanitizeProjectRelativePath(Config.MaterialPath);
    for (const FMcpWorldRecipeConfig::FLayerRule &Rule : Config.Layers)
    {
        FMcpBiomeLayerRule &Out = Preset->Layers.AddDefaulted_GetRef();
        Out.LayerName = Rule.LayerName;
        Out.MaskType = Rule.MaskType;
        Out.TargetHeight = Rule.TargetHeight;
        Out.MinHeight = Rule.MinHeight;
        Out.MaxHeight = Rule.MaxHeight;
        Out.FadeDistance = Rule.FadeDistance;
        Out.MinSlope = Rule.MinSlope;
        Out.MaxSlope = Rule.MaxSlope;
        Out.FadeSlope = Rule.FadeSlope;
        Out.Strength = Rule.Strength;
    }
    for (const FMcpWorldRecipeConfig::FFoliageEntry &Entry : Config.Foliage)
    {
        FMcpBiomeFoliageEntry &Out = Preset->Foliage.AddDefaulted_GetRef();
        Out.MeshPath = Entry.MeshPath;
        Out.Count = Entry.Count;
        Out.MinScale = Entry.MinScale;
        Out.MaxScale = Entry.MaxScale;
        Out.MinSlope = Entry.MinSlope;
        Out.MaxSlope = Entry.MaxSlope;
        Out.MinHeight = Entry.MinHeight;
        Out.MaxHeight = Entry.MaxHeight;
    }

    Preset->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Preset);

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave && !McpSafeAssetSave(Preset))
    {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Failed to save biome preset: %s"), *FullAssetPath),
                            TEXT("SAVE_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetName"), SanitizedName);
    Result->SetStringField(TEXT("assetPath"), Preset->GetPathName());
    Result->SetNumberField(TEXT("seed"), Preset->Seed);
    Result->SetNumberField(TEXT("layerRuleCount"), Preset->Layers.Num());
    Result->SetNumberField(TEXT("foliageEntryCount"), Preset->Foliage.Num());
    McpHandlerUtils::AddVerification(Result, Preset);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           FString::Printf(TEXT("Biome preset '%s' created."), *SanitizedName), Result);
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("create_biome_preset requires editor build."),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleInspectBiomePreset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString PresetPath = McpRecipeFirstString(Payload, {TEXT("biomePresetPath"), TEXT("presetPath"), TEXT("assetPath")});
    if (PresetPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("biomePresetPath required for inspect_biome_preset"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const FString SafePath = SanitizeProjectRelativePath(PresetPath);
    if (SafePath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Invalid or unsafe preset path: %s"), *PresetPath),
                            TEXT("SECURITY_VIOLATION"));
        return true;
    }
    UMcpBiomePreset *Preset = LoadObject<UMcpBiomePreset>(nullptr, *SafePath);
    if (!Preset)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Biome preset not found: %s"), *SafePath),
                            TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), Preset->GetPathName());
    Result->SetStringField(TEXT("description"), Preset->Description);
    Result->SetNumberField(TEXT("seed"), Preset->Seed);
    Result->SetNumberField(TEXT("layerRuleCount"), Preset->Layers.Num());
    Result->SetNumberField(TEXT("foliageEntryCount"), Preset->Foliage.Num());
    Result->SetObjectField(TEXT("preset"), McpPropertyReflection::ExportObjectToJson(Preset));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           FString::Printf(TEXT("Biome preset '%s' inspected."), *SafePath), Result);
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("inspect_biome_preset requires editor build."),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UNebulaForgeBridgeSubsystem::HandleListBiomePresets(
    const FString &RequestId, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    TArray<FString> Paths;
    ListBiomePresetAssetPaths(Paths);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> PathValues;
    for (const FString &Path : Paths)
        PathValues.Add(MakeShared<FJsonValueString>(Path));
    Result->SetArrayField(TEXT("presets"), PathValues);
    Result->SetNumberField(TEXT("presetCount"), Paths.Num());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           FString::Printf(TEXT("Found %d biome presets."), Paths.Num()), Result);
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("list_biome_presets requires editor build."),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

// =============================================================================
// Section D: Road Recipe (roadBLD-style build_road)
// =============================================================================
// Orchestrates spline-based road/river/path construction:
//   1. create_road_spline / create_river_spline / create_path_spline
//      (terrain-conformed, world-space route points)
//   2. custom cut/fill pass: flatten terrain toward the spline profile
//   3. generate_spline_mesh_segments (roadbed from a user mesh)
//   4. custom furniture scatter with lateral offsets (guardrails, lamps,
//      center-line markings), both sides supported
//   5. custom junction discs (terrain flatten + optional center mesh)
//   6. custom river water: WaterBodyRiver following the spline
//
// Only UWorld/USplineComponent/FLandscapeEditDataInterface engine APIs are
// used; project-specific lane logic and traffic simulation stay out of scope.

namespace
{
#if WITH_EDITOR
struct FMcpRoadFurnitureEntry
{
    FString MeshPath;
    double Spacing = 2000.0;
    double Offset = 0.0;
    bool bBothSides = false;
    bool bAlignToSpline = true;
    bool bProjectToSurface = true;
    double SurfaceOffset = 0.0;
    double MinScale = 1.0;
    double MaxScale = 1.0;
};

struct FMcpRoadJunction
{
    FVector Location = FVector::ZeroVector;
    bool bHasLocation = false;
    double Radius = 1500.0;
    FString JunctionMeshPath;
};

struct FMcpRoadRecipeConfig
{
    FString RoadName = TEXT("MCP_Road");
    FString RoadKind = TEXT("road"); // road | river | path
    TArray<TSharedPtr<FJsonValue>> RoutePoints;
    double RoadWidth = 800.0;
    double ShoulderWidth = 400.0;
    bool bCutFill = true;
    double SurfaceOffset = 0.0;
    double MaxSlopeDegrees = 0.0;
    FString LandscapeName;
    FString RoadbedMeshPath;
    FString RoadbedMaterialPath;
    FString ForwardAxis = TEXT("X");
    bool bCollisionEnabled = true;
    TArray<FMcpRoadFurnitureEntry> Furniture;
    TArray<FMcpRoadJunction> Junctions;
    bool bWater = false;
    FString WaterMaterialPath;
    int32 Seed = 1337;
    bool bSkipTerrain = false;
    bool bSkipRoadbed = false;
    bool bSkipFurniture = false;
    bool bSkipJunctions = false;
    bool bSkipWater = false;
};

void McpRoadApplyPayload(const TSharedPtr<FJsonObject> &Payload, FMcpRoadRecipeConfig &Config)
{
    if (!Payload.IsValid())
        return;
    const FString Name = McpRecipeFirstString(Payload, {TEXT("roadName"), TEXT("name")});
    if (!Name.IsEmpty())
        Config.RoadName = Name;
    const FString Kind = McpRecipeFirstString(Payload, {TEXT("roadKind"), TEXT("kind")});
    if (!Kind.IsEmpty())
        Config.RoadKind = Kind.ToLower();
    const TArray<TSharedPtr<FJsonValue>> *Points = nullptr;
    if (Payload->TryGetArrayField(TEXT("routePoints"), Points) && Points)
        Config.RoutePoints = *Points;
    else if (Payload->TryGetArrayField(TEXT("points"), Points) && Points)
        Config.RoutePoints = *Points;
    else if (Payload->TryGetArrayField(TEXT("initialPoints"), Points) && Points)
        Config.RoutePoints = *Points;

    double Number = 0.0;
    if (Payload->TryGetNumberField(TEXT("roadWidth"), Number))
        Config.RoadWidth = Number;
    else if (Payload->TryGetNumberField(TEXT("width"), Number))
        Config.RoadWidth = Number;
    if (Payload->TryGetNumberField(TEXT("shoulderWidth"), Number))
        Config.ShoulderWidth = Number;
    if (Payload->TryGetNumberField(TEXT("surfaceOffset"), Number))
        Config.SurfaceOffset = Number;
    if (Payload->TryGetNumberField(TEXT("maxSlopeDegrees"), Number))
        Config.MaxSlopeDegrees = Number;
    if (Payload->TryGetNumberField(TEXT("seed"), Number))
        Config.Seed = static_cast<int32>(Number);
    const FString LandscapeName = McpRecipeFirstString(Payload, {TEXT("landscapeName")});
    if (!LandscapeName.IsEmpty())
        Config.LandscapeName = LandscapeName;
    const FString RoadbedMesh = McpRecipeFirstString(Payload, {TEXT("roadbedMeshPath"), TEXT("meshPath")});
    if (!RoadbedMesh.IsEmpty())
        Config.RoadbedMeshPath = RoadbedMesh;
    const FString RoadbedMaterial = McpRecipeFirstString(Payload, {TEXT("roadbedMaterialPath"), TEXT("materialPath")});
    if (!RoadbedMaterial.IsEmpty())
        Config.RoadbedMaterialPath = RoadbedMaterial;
    const FString Axis = McpRecipeFirstString(Payload, {TEXT("forwardAxis")});
    if (!Axis.IsEmpty())
        Config.ForwardAxis = Axis;
    bool bBool = false;
    if (Payload->TryGetBoolField(TEXT("cutFill"), bBool))
        Config.bCutFill = bBool;
    if (Payload->TryGetBoolField(TEXT("collisionEnabled"), bBool))
        Config.bCollisionEnabled = bBool;
    if (Payload->TryGetBoolField(TEXT("water"), bBool))
        Config.bWater = bBool;
    const FString WaterMaterial = McpRecipeFirstString(Payload, {TEXT("waterMaterialPath")});
    if (!WaterMaterial.IsEmpty())
        Config.WaterMaterialPath = WaterMaterial;
    if (Payload->TryGetBoolField(TEXT("skipTerrain"), bBool))
        Config.bSkipTerrain = bBool;
    if (Payload->TryGetBoolField(TEXT("skipRoadbed"), bBool))
        Config.bSkipRoadbed = bBool;
    if (Payload->TryGetBoolField(TEXT("skipFurniture"), bBool))
        Config.bSkipFurniture = bBool;
    if (Payload->TryGetBoolField(TEXT("skipJunctions"), bBool))
        Config.bSkipJunctions = bBool;
    if (Payload->TryGetBoolField(TEXT("skipWater"), bBool))
        Config.bSkipWater = bBool;

    const TArray<TSharedPtr<FJsonValue>> *Furniture = nullptr;
    if (Payload->TryGetArrayField(TEXT("furniture"), Furniture) && Furniture)
    {
        for (const TSharedPtr<FJsonValue> &Value : *Furniture)
        {
            const TSharedPtr<FJsonObject> *EntryObject = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(EntryObject) || !EntryObject)
                continue;
            FMcpRoadFurnitureEntry Entry;
            Entry.MeshPath = McpRecipeFirstString(*EntryObject, {TEXT("meshPath"), TEXT("mesh")});
            if (Entry.MeshPath.IsEmpty())
                continue;
            McpRecipeReadNumber(*EntryObject, TEXT("spacing"), Entry.Spacing);
            McpRecipeReadNumber(*EntryObject, TEXT("offset"), Entry.Offset);
            McpRecipeReadNumber(*EntryObject, TEXT("surfaceOffset"), Entry.SurfaceOffset);
            McpRecipeReadNumber(*EntryObject, TEXT("minScale"), Entry.MinScale);
            McpRecipeReadNumber(*EntryObject, TEXT("maxScale"), Entry.MaxScale);
            bool bEntryBool = false;
            if ((*EntryObject)->TryGetBoolField(TEXT("bothSides"), bEntryBool))
                Entry.bBothSides = bEntryBool;
            if ((*EntryObject)->TryGetBoolField(TEXT("alignToSpline"), bEntryBool))
                Entry.bAlignToSpline = bEntryBool;
            if ((*EntryObject)->TryGetBoolField(TEXT("projectToSurface"), bEntryBool))
                Entry.bProjectToSurface = bEntryBool;
            Config.Furniture.Add(Entry);
        }
    }

    const TArray<TSharedPtr<FJsonValue>> *Junctions = nullptr;
    if (Payload->TryGetArrayField(TEXT("junctions"), Junctions) && Junctions)
    {
        for (const TSharedPtr<FJsonValue> &Value : *Junctions)
        {
            const TSharedPtr<FJsonObject> *JunctionObject = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(JunctionObject) || !JunctionObject)
                continue;
            FMcpRoadJunction Junction;
            if (McpRecipeReadVector(*JunctionObject, TEXT("location"), Junction.Location))
                Junction.bHasLocation = true;
            McpRecipeReadNumber(*JunctionObject, TEXT("radius"), Junction.Radius);
            Junction.JunctionMeshPath =
                McpRecipeFirstString(*JunctionObject, {TEXT("junctionMeshPath"), TEXT("meshPath")});
            if (Junction.bHasLocation)
                Config.Junctions.Add(Junction);
        }
    }
}

USplineComponent *McpRoadFindSpline(UWorld *World, const FString &RoadName)
{
    if (!World || RoadName.IsEmpty())
        return nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor *Actor = *It;
        if (!Actor ||
            (!Actor->GetActorLabel().Equals(RoadName, ESearchCase::IgnoreCase) &&
             !Actor->GetName().Equals(RoadName, ESearchCase::IgnoreCase)))
            continue;
        TArray<USplineComponent *> Splines;
        Actor->GetComponents<USplineComponent>(Splines);
        if (Splines.Num() > 0 && Splines[0])
            return Splines[0];
    }
    return nullptr;
}

ALandscape *McpRoadResolveLandscape(UWorld *World, const FString &LandscapeName, const FVector &NearPoint)
{
    if (!World)
        return nullptr;
    if (!LandscapeName.IsEmpty())
    {
        for (TActorIterator<ALandscape> It(World); It; ++It)
        {
            ALandscape *Candidate = *It;
            if (Candidate && Candidate->GetActorLabel().Equals(LandscapeName, ESearchCase::IgnoreCase))
                return Candidate;
        }
        return nullptr;
    }
    // Resolve from a downward trace at the sample point.
    FHitResult Hit;
    const FVector Start = NearPoint + FVector(0.0, 0.0, 50000.0);
    if (World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.0, 0.0, 100000.0), ECC_WorldStatic))
        return McpWorldBrushResolveLandscape(World, Hit);
    return nullptr;
}

/** Cut/fill pass: flatten terrain toward the spline profile with shoulders. */
bool McpRoadApplyCutFill(USplineComponent *Spline, const FMcpRoadRecipeConfig &Config,
                         TSharedPtr<FJsonObject> &OutResult, FString &OutMessage, FString &OutErrorCode)
{
    if (!Spline || !Spline->GetWorld())
    {
        OutErrorCode = TEXT("SPLINE_NOT_FOUND");
        OutMessage = TEXT("Road spline not found for cut/fill.");
        return false;
    }
    UWorld *World = Spline->GetWorld();
    const double HalfWidth = FMath::Max(50.0, Config.RoadWidth * 0.5 + Config.ShoulderWidth);
    const float SplineLength = Spline->GetSplineLength();
    const double Step = FMath::Max(50.0, HalfWidth * 0.5);

    ALandscape *Landscape = nullptr;
    int32 DabCount = 0;
    int32 ModifiedVerts = 0;
    for (double Distance = 0.0; Distance <= SplineLength; Distance += Step)
    {
        const FVector Sample = Spline->GetLocationAtDistanceAlongSpline(
            static_cast<float>(Distance), ESplineCoordinateSpace::World);
        if (!Landscape)
        {
            Landscape = McpRoadResolveLandscape(World, Config.LandscapeName, Sample);
            if (!Landscape)
            {
                OutErrorCode = TEXT("LANDSCAPE_NOT_FOUND");
                OutMessage = TEXT("No landscape under the road spline for cut/fill.");
                return false;
            }
        }
        const int32 Modified = McpWorldBrushApplyHeightDab(
            Landscape, Sample, EMcpWorldBrushTool::Flatten, HalfWidth, 1.0, 0.35,
            Sample.Z + Config.SurfaceOffset);
        if (Modified < 0)
        {
            OutErrorCode = TEXT("CUTFILL_FAILED");
            OutMessage = TEXT("Cut/fill dab failed on the road profile.");
            return false;
        }
        ModifiedVerts += Modified;
        ++DabCount;
    }

    if (Landscape)
    {
        FString SaveError;
        if (!McpWorldBrushEndStroke(Landscape, SaveError))
        {
            OutErrorCode = TEXT("SAVE_FAILED");
            OutMessage = SaveError;
            return false;
        }
    }

    OutResult = McpHandlerUtils::CreateResultObject();
    OutResult->SetBoolField(TEXT("success"), true);
    OutResult->SetNumberField(TEXT("dabCount"), DabCount);
    OutResult->SetNumberField(TEXT("modifiedVertices"), ModifiedVerts);
    OutResult->SetNumberField(TEXT("corridorHalfWidth"), HalfWidth);
    OutResult->SetStringField(TEXT("landscapeName"), Landscape ? Landscape->GetActorLabel() : FString());
    OutMessage = FString::Printf(TEXT("Cut/fill applied along %.0f uu of road (%d dabs)."),
                                 SplineLength, DabCount);
    return true;
}

/** Furniture scatter with lateral offsets along the spline. */
bool McpRoadScatterFurniture(USplineComponent *Spline, const FMcpRoadRecipeConfig &Config,
                             const FMcpRoadFurnitureEntry &Entry, int32 EntryIndex,
                             TSharedPtr<FJsonObject> &OutResult, FString &OutMessage, FString &OutErrorCode)
{
    if (!Spline || !Spline->GetWorld())
    {
        OutErrorCode = TEXT("SPLINE_NOT_FOUND");
        OutMessage = TEXT("Road spline not found for furniture scatter.");
        return false;
    }
    UWorld *World = Spline->GetWorld();
    UStaticMesh *StaticMesh = LoadObject<UStaticMesh>(nullptr, *Entry.MeshPath);
    if (!StaticMesh)
    {
        OutErrorCode = TEXT("ASSET_NOT_FOUND");
        OutMessage = FString::Printf(TEXT("Furniture mesh not found: %s"), *Entry.MeshPath);
        return false;
    }

    const FString MeshBase = SanitizeAssetName(FPaths::GetBaseFilename(Entry.MeshPath));
    const FString ActorLabel = FString::Printf(TEXT("%s_Furniture_%d_%s"), *Config.RoadName, EntryIndex, *MeshBase);
    AActor *Actor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor *Candidate = *It;
        if (Candidate && Candidate->ActorHasTag(TEXT("MCP.GeneratedLandscapeFoliage")) &&
            Candidate->GetActorLabel().Equals(ActorLabel, ESearchCase::IgnoreCase))
        {
            Actor = Candidate;
            break;
        }
    }
    UHierarchicalInstancedStaticMeshComponent *Hism = nullptr;
    if (!Actor)
    {
        const FScopedTransaction Transaction(FText::FromString(TEXT("Create Road Furniture Collection")));
        FActorSpawnParameters SpawnParams;
        SpawnParams.ObjectFlags |= RF_Transactional;
        Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        if (!Actor)
        {
            OutErrorCode = TEXT("SPAWN_FAILED");
            OutMessage = TEXT("Failed to spawn furniture collection actor.");
            return false;
        }
        Actor->SetIsSpatiallyLoaded(false);
        Actor->SetActorLabel(ActorLabel);
        Actor->Tags.Add(TEXT("MCP.GeneratedLandscapeFoliage"));
        Actor->Tags.Add(FName(*(FString(TEXT("MCP.GeneratedLandscapeFoliage.Name=")) + ActorLabel)));
        USceneComponent *Root = NewObject<USceneComponent>(Actor, TEXT("RoadFurnitureRoot"), RF_Transactional);
        Actor->AddInstanceComponent(Root);
        Actor->SetRootComponent(Root);
        Root->RegisterComponent();
    }
    else
    {
        for (UActorComponent *Component : Actor->GetComponents())
        {
            if (UHierarchicalInstancedStaticMeshComponent *Candidate =
                    Cast<UHierarchicalInstancedStaticMeshComponent>(Component))
            {
                if (Candidate->GetStaticMesh() == StaticMesh)
                {
                    Hism = Candidate;
                    break;
                }
            }
        }
    }
    if (!Hism)
    {
        Hism = NewObject<UHierarchicalInstancedStaticMeshComponent>(Actor, NAME_None, RF_Transactional);
        Actor->AddInstanceComponent(Hism);
        Hism->SetStaticMesh(StaticMesh);
        Hism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Hism->SetupAttachment(Actor->GetRootComponent());
        Hism->RegisterComponent();
    }

    const FScopedTransaction Transaction(FText::FromString(TEXT("Scatter Road Furniture")));
    Actor->Modify();
    FRandomStream Random(Config.Seed + EntryIndex * 104729);

    const float SplineLength = Spline->GetSplineLength();
    const double Spacing = FMath::Max(50.0, Entry.Spacing);
    TArray<double> Offsets;
    Offsets.Add(Entry.Offset);
    if (Entry.bBothSides && !FMath::IsNearlyZero(Entry.Offset))
        Offsets.Add(-Entry.Offset);

    int32 Placed = 0;
    for (double Distance = 0.0; Distance <= SplineLength; Distance += Spacing)
    {
        const FVector Base = Spline->GetLocationAtDistanceAlongSpline(
            static_cast<float>(Distance), ESplineCoordinateSpace::World);
        const FVector Tangent = Spline->GetTangentAtDistanceAlongSpline(
            static_cast<float>(Distance), ESplineCoordinateSpace::World).GetSafeNormal();
        const FVector Right = FVector::CrossProduct(FVector::UpVector, Tangent).GetSafeNormal();
        const double Yaw = Entry.bAlignToSpline
            ? FMath::RadiansToDegrees(FMath::Atan2(Tangent.Y, Tangent.X))
            : Random.FRandRange(0.0, 360.0);
        for (double Lateral : Offsets)
        {
            FVector Position = Base + Right * Lateral;
            if (Entry.bProjectToSurface)
            {
                FHitResult Hit;
                const FVector Start = Position + FVector(0.0, 0.0, 20000.0);
                if (!World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.0, 0.0, 40000.0),
                                                     ECC_WorldStatic))
                    continue;
                Position = Hit.ImpactPoint + Hit.ImpactNormal * static_cast<float>(Entry.SurfaceOffset);
            }
            const float Scale = Random.FRandRange(static_cast<float>(Entry.MinScale),
                                                  static_cast<float>(Entry.MaxScale));
            Hism->AddInstance(FTransform(FRotator(0.0f, static_cast<float>(Yaw), 0.0f), Position, FVector(Scale)));
            ++Placed;
        }
    }

    if (Placed > 0)
    {
        Actor->MarkPackageDirty();
        McpSafeAssetSave(Actor);
    }

    OutResult = McpHandlerUtils::CreateResultObject();
    OutResult->SetBoolField(TEXT("success"), true);
    OutResult->SetStringField(TEXT("actorLabel"), ActorLabel);
    OutResult->SetNumberField(TEXT("instancesPlaced"), Placed);
    OutResult->SetStringField(TEXT("mesh"), Entry.MeshPath);
    OutMessage = FString::Printf(TEXT("Furniture '%s' scattered: %d instances."), *MeshBase, Placed);
    return true;
}

/** Junction discs: flatten terrain and optionally spawn a center mesh. */
bool McpRoadBuildJunctions(UWorld *World, const FMcpRoadRecipeConfig &Config,
                           TSharedPtr<FJsonObject> &OutResult, FString &OutMessage, FString &OutErrorCode)
{
    if (!World)
    {
        OutErrorCode = TEXT("INVALID_WORLD");
        OutMessage = TEXT("No world for junction construction.");
        return false;
    }
    int32 BuiltJunctions = 0;
    int32 SpawnedMeshes = 0;
    for (const FMcpRoadJunction &Junction : Config.Junctions)
    {
        ALandscape *Landscape = McpRoadResolveLandscape(World, Config.LandscapeName, Junction.Location);
        if (!Landscape)
        {
            OutErrorCode = TEXT("LANDSCAPE_NOT_FOUND");
            OutMessage = TEXT("No landscape under a road junction.");
            return false;
        }
        // Average the terrain height around the junction center first.
        FHitResult CenterHit;
        const FVector Start = Junction.Location + FVector(0.0, 0.0, 50000.0);
        double TargetZ = Junction.Location.Z;
        if (World->LineTraceSingleByChannel(CenterHit, Start, Start - FVector(0.0, 0.0, 100000.0),
                                            ECC_WorldStatic))
            TargetZ = CenterHit.ImpactPoint.Z;
        const double Radius = FMath::Max(100.0, Junction.Radius);
        const int32 Modified = McpWorldBrushApplyHeightDab(Landscape, Junction.Location,
                                                           EMcpWorldBrushTool::Flatten, Radius, 1.0,
                                                           0.25, TargetZ);
        if (Modified < 0)
        {
            OutErrorCode = TEXT("JUNCTION_FAILED");
            OutMessage = TEXT("Junction terrain flatten failed.");
            return false;
        }
        if (!Junction.JunctionMeshPath.IsEmpty())
        {
            UStaticMesh *Mesh = LoadObject<UStaticMesh>(nullptr, *Junction.JunctionMeshPath);
            if (!Mesh)
            {
                OutErrorCode = TEXT("ASSET_NOT_FOUND");
                OutMessage = FString::Printf(TEXT("Junction mesh not found: %s"), *Junction.JunctionMeshPath);
                return false;
            }
            const FScopedTransaction Transaction(FText::FromString(TEXT("Spawn Road Junction Mesh")));
            FActorSpawnParameters SpawnParams;
            SpawnParams.ObjectFlags |= RF_Transactional;
            AStaticMeshActor *JunctionActor = World->SpawnActor<AStaticMeshActor>(
                AStaticMeshActor::StaticClass(),
                FVector(Junction.Location.X, Junction.Location.Y, TargetZ), FRotator::ZeroRotator, SpawnParams);
            if (!JunctionActor)
            {
                OutErrorCode = TEXT("SPAWN_FAILED");
                OutMessage = TEXT("Failed to spawn junction mesh actor.");
                return false;
            }
            JunctionActor->SetActorLabel(
                FString::Printf(TEXT("%s_Junction_%d"), *Config.RoadName, BuiltJunctions));
            JunctionActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
            JunctionActor->MarkPackageDirty();
            ++SpawnedMeshes;
        }
        FString SaveError;
        if (!McpWorldBrushEndStroke(Landscape, SaveError))
        {
            OutErrorCode = TEXT("SAVE_FAILED");
            OutMessage = SaveError;
            return false;
        }
        ++BuiltJunctions;
    }

    OutResult = McpHandlerUtils::CreateResultObject();
    OutResult->SetBoolField(TEXT("success"), true);
    OutResult->SetNumberField(TEXT("junctionCount"), BuiltJunctions);
    OutResult->SetNumberField(TEXT("spawnedMeshes"), SpawnedMeshes);
    OutMessage = FString::Printf(TEXT("Road junctions built: %d flattened, %d meshes spawned."),
                                 BuiltJunctions, SpawnedMeshes);
    return true;
}

/** River water: WaterBodyRiver actor following the spline profile. */
bool McpRoadBuildWater(USplineComponent *Spline, const FMcpRoadRecipeConfig &Config,
                       TSharedPtr<FJsonObject> &OutResult, FString &OutMessage, FString &OutErrorCode)
{
    if (!Spline || !Spline->GetWorld())
    {
        OutErrorCode = TEXT("SPLINE_NOT_FOUND");
        OutMessage = TEXT("Road spline not found for water construction.");
        return false;
    }
    UWorld *World = Spline->GetWorld();
    UClass *WaterClass = LoadClass<AActor>(nullptr, TEXT("/Script/Water.WaterBodyRiver"));
    if (!WaterClass)
    {
        OutErrorCode = TEXT("WATER_UNAVAILABLE");
        OutMessage = TEXT("Water plugin WaterBodyRiver class is unavailable in this project.");
        return false;
    }

    const int32 PointCount = Spline->GetNumberOfSplinePoints();
    if (PointCount < 2)
    {
        OutErrorCode = TEXT("INVALID_SPLINE");
        OutMessage = TEXT("River spline needs at least two points for water.");
        return false;
    }

    const FString WaterName = FString::Printf(TEXT("%s_Water"), *Config.RoadName);
    AActor *WaterActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor *Candidate = *It;
        if (Candidate && Candidate->IsA(WaterClass) &&
            Candidate->GetActorLabel().Equals(WaterName, ESearchCase::IgnoreCase))
        {
            WaterActor = Candidate;
            break;
        }
    }
    if (!WaterActor)
    {
        const FScopedTransaction Transaction(FText::FromString(TEXT("Create River Water Body")));
        FActorSpawnParameters SpawnParams;
        SpawnParams.ObjectFlags |= RF_Transactional;
        const FVector StartLocation = Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
        WaterActor = World->SpawnActor<AActor>(WaterClass, StartLocation, FRotator::ZeroRotator, SpawnParams);
        if (!WaterActor)
        {
            OutErrorCode = TEXT("SPAWN_FAILED");
            OutMessage = TEXT("Failed to spawn WaterBodyRiver actor.");
            return false;
        }
        WaterActor->SetActorLabel(WaterName);
    }

    USplineComponent *WaterSpline = WaterActor->FindComponentByClass<USplineComponent>();
    int32 CopiedPoints = 0;
    if (WaterSpline)
    {
        TArray<FVector> LocalPoints;
        LocalPoints.Reserve(PointCount);
        const FTransform WaterTransform = WaterActor->GetActorTransform();
        for (int32 Index = 0; Index < PointCount; ++Index)
        {
            const FVector WorldPoint = Spline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World);
            LocalPoints.Add(WaterTransform.InverseTransformPosition(WorldPoint));
        }
        WaterSpline->SetSplinePoints(LocalPoints, ESplineCoordinateSpace::Local, true);
        CopiedPoints = LocalPoints.Num();
    }

    WaterActor->MarkPackageDirty();
    if (!McpSafeLevelSave(World->PersistentLevel, World->GetOutermost()->GetName()))
    {
        OutErrorCode = TEXT("SAVE_FAILED");
        OutMessage = TEXT("River water actor could not be saved.");
        return false;
    }

    OutResult = McpHandlerUtils::CreateResultObject();
    OutResult->SetBoolField(TEXT("success"), true);
    OutResult->SetStringField(TEXT("waterActor"), WaterName);
    OutResult->SetStringField(TEXT("waterActorPath"), WaterActor->GetPathName());
    OutResult->SetNumberField(TEXT("copiedSplinePoints"), CopiedPoints);
    OutMessage = FString::Printf(TEXT("River water follows the spline (%d points)."), CopiedPoints);
    return true;
}
#endif
} // namespace

void UNebulaForgeBridgeSubsystem::BeginBuildRoad(
    const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const TFunction<void(bool bSuccess, const TSharedPtr<FJsonObject> &Result)> &Completion)
{
    auto Fail = [&](const FString &Message, const FString &Code)
    {
        if (Completion)
        {
            TSharedPtr<FJsonObject> Error = McpHandlerUtils::CreateResultObject();
            Error->SetBoolField(TEXT("success"), false);
            Error->SetStringField(TEXT("error"), Message);
            Completion(false, Error);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, Message, Code);
        }
    };

    if (IsWorldRecipeChainActive())
    {
        Fail(TEXT("A world recipe chain is already running."), TEXT("RECIPE_BUSY"));
        return;
    }

    FMcpRoadRecipeConfig Config;
    McpRoadApplyPayload(Payload, Config);
    if (Action.ToLower() == TEXT("build_river"))
        Config.RoadKind = TEXT("river");
    if (Config.RoadKind != TEXT("road") && Config.RoadKind != TEXT("river") &&
        Config.RoadKind != TEXT("path"))
    {
        Fail(FString::Printf(TEXT("Invalid roadKind '%s'. Use road, river, or path."), *Config.RoadKind),
             TEXT("INVALID_ARGUMENT"));
        return;
    }
    if (Config.RoadKind == TEXT("river") && !Payload->HasField(TEXT("water")))
        Config.bWater = true;
    if (Config.RoutePoints.Num() < 2)
    {
        Fail(TEXT("build_road requires at least two routePoints."), TEXT("INVALID_ARGUMENT"));
        return;
    }

    if (!GEditor || !GEditor->GetEditorWorldContext().World() ||
        !GEditor->GetEditorWorldContext().World()->GetOutermost()->GetName().StartsWith(TEXT("/Game/")))
    {
        Fail(TEXT("build_road requires a saved /Game map. Create or load the destination map first."),
             TEXT("MAP_NOT_SAVED"));
        return;
    }

    const TCHAR *SplineAction = Config.RoadKind == TEXT("river") ? TEXT("create_river_spline")
                              : Config.RoadKind == TEXT("path")  ? TEXT("create_path_spline")
                                                                 : TEXT("create_road_spline");
    const FString CapturedRoadName = Config.RoadName;

    TArray<FMcpWorldRecipeStep> Steps;

    {
        TSharedPtr<FJsonObject> SplinePayload = McpRecipeMakeStepPayload(SplineAction);
        SplinePayload->SetStringField(TEXT("actorName"), Config.RoadName);
        SplinePayload->SetStringField(TEXT("coordinateSpace"), TEXT("World"));
        TArray<TSharedPtr<FJsonValue>> PointsCopy = Config.RoutePoints;
        SplinePayload->SetArrayField(TEXT("points"), PointsCopy);
        SplinePayload->SetNumberField(TEXT("width"), Config.RoadWidth);
        SplinePayload->SetBoolField(TEXT("conformToLandscape"), true);
        SplinePayload->SetNumberField(TEXT("surfaceOffset"), Config.SurfaceOffset);
        SplinePayload->SetNumberField(TEXT("maxSlopeDegrees"), Config.MaxSlopeDegrees);
        FMcpWorldRecipeStep Step;
        Step.Action = TEXT("build_environment");
        Step.Label = FString::Printf(TEXT("create %s spline %s"), *Config.RoadKind, *Config.RoadName);
        Step.Payload = SplinePayload;
        // Cut/fill, roadbed, furniture, junctions, and water all need the spline.
        Step.bAbortOnFailure = true;
        Steps.Add(MoveTemp(Step));
    }

    if (Config.bCutFill && !Config.bSkipTerrain)
    {
        const FMcpRoadRecipeConfig CapturedConfig = Config;
        FMcpWorldRecipeStep Step;
        Step.Label = TEXT("cut/fill terrain to road profile");
        Step.CustomStep = [CapturedConfig, CapturedRoadName](
                              const FGuid &, TSharedPtr<FJsonObject> &OutResult,
                              FString &OutMessage, FString &OutErrorCode) -> bool
        {
            UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
            if (!World)
            {
                OutErrorCode = TEXT("INVALID_WORLD");
                OutMessage = TEXT("No editor world for cut/fill.");
                return false;
            }
            USplineComponent *Spline = McpRoadFindSpline(World, CapturedRoadName);
            return McpRoadApplyCutFill(Spline, CapturedConfig, OutResult, OutMessage, OutErrorCode);
        };
        Steps.Add(MoveTemp(Step));
    }

    if (!Config.bSkipRoadbed && !Config.RoadbedMeshPath.IsEmpty())
    {
        TSharedPtr<FJsonObject> RoadbedPayload = McpRecipeMakeStepPayload(TEXT("generate_spline_mesh_segments"));
        RoadbedPayload->SetStringField(TEXT("actorName"), Config.RoadName);
        RoadbedPayload->SetStringField(TEXT("meshPath"), Config.RoadbedMeshPath);
        if (!Config.RoadbedMaterialPath.IsEmpty())
            RoadbedPayload->SetStringField(TEXT("materialPath"), Config.RoadbedMaterialPath);
        RoadbedPayload->SetStringField(TEXT("forwardAxis"), Config.ForwardAxis);
        RoadbedPayload->SetBoolField(TEXT("collisionEnabled"), Config.bCollisionEnabled);
        FMcpWorldRecipeStep Step;
        Step.Action = TEXT("build_environment");
        Step.Label = TEXT("generate roadbed mesh segments");
        Step.Payload = RoadbedPayload;
        Steps.Add(MoveTemp(Step));
    }

    int32 FurnitureIndex = 0;
    for (const FMcpRoadFurnitureEntry &Entry : Config.Furniture)
    {
        if (Config.bSkipFurniture)
            break;
        const FMcpRoadFurnitureEntry CapturedEntry = Entry;
        const FMcpRoadRecipeConfig CapturedConfig = Config;
        const int32 CapturedIndex = FurnitureIndex;
        FMcpWorldRecipeStep Step;
        Step.Label = FString::Printf(TEXT("scatter furniture %s"), *FPaths::GetBaseFilename(Entry.MeshPath));
        Step.CustomStep = [CapturedConfig, CapturedEntry, CapturedIndex, CapturedRoadName](
                              const FGuid &, TSharedPtr<FJsonObject> &OutResult,
                              FString &OutMessage, FString &OutErrorCode) -> bool
        {
            UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
            if (!World)
            {
                OutErrorCode = TEXT("INVALID_WORLD");
                OutMessage = TEXT("No editor world for furniture scatter.");
                return false;
            }
            USplineComponent *Spline = McpRoadFindSpline(World, CapturedRoadName);
            return McpRoadScatterFurniture(Spline, CapturedConfig, CapturedEntry, CapturedIndex,
                                           OutResult, OutMessage, OutErrorCode);
        };
        Steps.Add(MoveTemp(Step));
        ++FurnitureIndex;
    }

    if (!Config.bSkipJunctions && Config.Junctions.Num() > 0)
    {
        const FMcpRoadRecipeConfig CapturedConfig = Config;
        FMcpWorldRecipeStep Step;
        Step.Label = FString::Printf(TEXT("build %d road junctions"), Config.Junctions.Num());
        Step.CustomStep = [CapturedConfig](
                              const FGuid &, TSharedPtr<FJsonObject> &OutResult,
                              FString &OutMessage, FString &OutErrorCode) -> bool
        {
            UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
            return McpRoadBuildJunctions(World, CapturedConfig, OutResult, OutMessage, OutErrorCode);
        };
        Steps.Add(MoveTemp(Step));
    }

    if ((Config.RoadKind == TEXT("river") || Config.bWater) && !Config.bSkipWater)
    {
        const FMcpRoadRecipeConfig CapturedConfig = Config;
        FMcpWorldRecipeStep Step;
        Step.Label = TEXT("create river water body");
        Step.CustomStep = [CapturedConfig, CapturedRoadName](
                              const FGuid &, TSharedPtr<FJsonObject> &OutResult,
                              FString &OutMessage, FString &OutErrorCode) -> bool
        {
            UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
            if (!World)
            {
                OutErrorCode = TEXT("INVALID_WORLD");
                OutMessage = TEXT("No editor world for water construction.");
                return false;
            }
            USplineComponent *Spline = McpRoadFindSpline(World, CapturedRoadName);
            return McpRoadBuildWater(Spline, CapturedConfig, OutResult, OutMessage, OutErrorCode);
        };
        Steps.Add(MoveTemp(Step));
    }

    RecipeSummaryMeta = McpHandlerUtils::CreateResultObject();
    RecipeSummaryMeta->SetStringField(TEXT("roadName"), Config.RoadName);
    RecipeSummaryMeta->SetStringField(TEXT("roadKind"), Config.RoadKind);
    RecipeSummaryMeta->SetNumberField(TEXT("seed"), Config.Seed);
    RecipeSummaryMeta->SetNumberField(TEXT("routePointCount"), Config.RoutePoints.Num());
    RecipeSummaryMeta->SetNumberField(TEXT("junctionCount"), Config.Junctions.Num());
    RecipeSummaryMeta->SetNumberField(TEXT("furnitureEntryCount"), Config.Furniture.Num());

    RecipeSteps = MoveTemp(Steps);
    RecipeStepResults.Reset();
    RecipeStepIndex = 0;
    RecipeFailedSteps = 0;
    RecipeSkippedSteps = 0;
    RecipeRequestId = RequestId;
    RecipeSocket = RequestingSocket;
    RecipeCompletion = Completion;
    RecipeCapturedResponse.Reset();
    bRecipeCapturing = false;

    UE_LOG(LogMcpWorldRecipes, Log,
           TEXT("Starting road recipe chain: request=%s steps=%d road=%s kind=%s"),
           *RequestId, RecipeSteps.Num(), *Config.RoadName, *Config.RoadKind);

    const TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
    AsyncTask(ENamedThreads::GameThread, [WeakThis]()
              {
        if (UNebulaForgeBridgeSubsystem *Subsystem = WeakThis.Get())
            Subsystem->RunNextWorldRecipeStep(); });
}

// =============================================================================
// Section C: Action Dispatch
// =============================================================================

bool UNebulaForgeBridgeSubsystem::HandleWorldRecipeAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (Lower != TEXT("generate_world") && Lower != TEXT("apply_biome") &&
        Lower != TEXT("create_biome_preset") && Lower != TEXT("inspect_biome_preset") &&
        Lower != TEXT("list_biome_presets") && Lower != TEXT("build_road") &&
        Lower != TEXT("build_river"))
    {
        return false;
    }

#if WITH_EDITOR
    if (Lower == TEXT("create_biome_preset"))
        return HandleCreateBiomePreset(RequestId, Payload, RequestingSocket);
    if (Lower == TEXT("inspect_biome_preset"))
        return HandleInspectBiomePreset(RequestId, Payload, RequestingSocket);
    if (Lower == TEXT("list_biome_presets"))
        return HandleListBiomePresets(RequestId, RequestingSocket);
    if (Lower == TEXT("build_road") || Lower == TEXT("build_river"))
    {
        if (!Payload.IsValid())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("build_road payload missing"),
                                TEXT("INVALID_PAYLOAD"));
            return true;
        }
        const TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
        const TFunction<void(bool bSuccess, const TSharedPtr<FJsonObject> &Result)> NoCompletion;
        AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestId, Lower, Payload, RequestingSocket, NoCompletion]()
                  {
            if (UNebulaForgeBridgeSubsystem *Subsystem = WeakThis.Get())
                Subsystem->BeginBuildRoad(RequestId, Lower, Payload, RequestingSocket, NoCompletion); });
        return true;
    }

    // generate_world / apply_biome: validate the payload synchronously, then
    // build the step chain on the game thread. The single combined response is
    // sent by FinalizeWorldRecipeChain when the chain completes; progress
    // updates keep the request alive in the meantime.
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("generate_world payload missing"),
                            TEXT("INVALID_PAYLOAD"));
        return true;
    }
    if (Lower == TEXT("apply_biome"))
    {
        const FString PresetPath = McpRecipeFirstString(Payload, {TEXT("biomePresetPath"), TEXT("presetPath")});
        if (PresetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                                TEXT("apply_biome requires biomePresetPath"),
                                TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }

    const TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakThis(this);
    const TFunction<void(bool bSuccess, const TSharedPtr<FJsonObject> &Result)> NoCompletion;
    AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestId, Payload, RequestingSocket, NoCompletion]()
              {
        if (UNebulaForgeBridgeSubsystem *Subsystem = WeakThis.Get())
            Subsystem->BeginGenerateWorld(RequestId, Payload, RequestingSocket, NoCompletion); });
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("World recipe actions require editor build."),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

#endif // WITH_EDITOR (preset authoring, recipe builders, dispatch)
