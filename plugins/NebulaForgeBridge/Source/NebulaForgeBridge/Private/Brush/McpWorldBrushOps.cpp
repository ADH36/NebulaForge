// =============================================================================
// McpWorldBrushOps.cpp
// =============================================================================
// Shared world-builder brush operations. Patterns mirror the verified
// landscape/foliage handler implementations:
//   - Height read/modify/write: HandleSculptLandscape (SetHeightData+Flush,
//     MarkPackageDirty instead of PostEditChange).
//   - Layer registration: HandlePaintLandscapeLayer (target layer ensure,
//     edit-layer selection, SetAlphaData+Flush).
//   - HISM instancing: scatter_landscape_foliage core (tool-tagged actor,
//     USceneComponent root, per-mesh HISM, AddInstance, persistence save).
//
// Copyright (c) 2024 NebulaForge Bridge Contributors
// =============================================================================

#include "Brush/McpWorldBrushOps.h"

#include "McpHandlerUtils.h"
#include "McpSafeOperations.h"
#include "NebulaForgeBridgeHelpers.h"

#include "Dom/JsonObject.h"
#include "ScopedTransaction.h"
#include "Misc/PackageName.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Async/Async.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "LandscapeProxy.h"

#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogMcpWorldBrush, Log, All);

#if WITH_EDITOR
namespace
{
constexpr const TCHAR *BrushFoliageTag = TEXT("MCP.GeneratedLandscapeFoliage");
constexpr const TCHAR *BrushFoliageNamePrefix = TEXT("MCP.GeneratedLandscapeFoliage.Name=");
constexpr const TCHAR *BrushFoliageSeedPrefix = TEXT("MCP.GeneratedLandscapeFoliage.Seed=");

bool McpBrushSaveLandscapePersistence(UWorld *World, ALandscape *Landscape, FString &OutError)
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
        OutError = TEXT("Failed to save brushed landscape packages.");
        return false;
    }
    return true;
}
} // namespace

ALandscape *McpWorldBrushResolveLandscape(UWorld *World, const FHitResult &Hit)
{
    if (!World || !Hit.GetActor())
        return nullptr;
    if (ALandscape *Direct = Cast<ALandscape>(Hit.GetActor()))
        return Direct;
    if (const ALandscapeProxy *Proxy = Cast<ALandscapeProxy>(Hit.GetActor()))
    {
        const FGuid ProxyGuid = Proxy->GetLandscapeGuid();
        for (TActorIterator<ALandscape> It(World); It; ++It)
        {
            ALandscape *Candidate = *It;
            if (Candidate && Candidate->GetLandscapeGuid() == ProxyGuid)
                return Candidate;
        }
    }
    return nullptr;
}

int32 McpWorldBrushApplyHeightDab(ALandscape *Landscape, const FVector &WorldLocation,
                                  EMcpWorldBrushTool Tool, double Radius, double Strength,
                                  double Falloff, double FlattenTargetZ)
{
    if (!Landscape || Tool == EMcpWorldBrushTool::PaintLayer ||
        Tool == EMcpWorldBrushTool::ScatterFoliage)
        return -1;

    ULandscapeInfo *LandscapeInfo = Landscape->GetLandscapeInfo();
    if (!LandscapeInfo)
        return -1;

    const FVector LocalPos = Landscape->GetActorTransform().InverseTransformPosition(WorldLocation);
    const int32 CenterX = FMath::RoundToInt(LocalPos.X);
    const int32 CenterY = FMath::RoundToInt(LocalPos.Y);
    const FVector LandscapeScale = Landscape->GetActorScale3D();
    if (FMath::IsNearlyZero(LandscapeScale.X) || FMath::IsNearlyZero(LandscapeScale.Z))
        return -1;

    const int32 RadiusVerts = FMath::Max(1, FMath::RoundToInt(Radius / LandscapeScale.X));
    const int32 FalloffVerts = FMath::RoundToInt(static_cast<double>(RadiusVerts) * FMath::Clamp(Falloff, 0.0, 1.0));
    int32 MinX = CenterX - RadiusVerts;
    int32 MaxX = CenterX + RadiusVerts;
    int32 MinY = CenterY - RadiusVerts;
    int32 MaxY = CenterY + RadiusVerts;
    int32 LMinX = 0, LMinY = 0, LMaxX = 0, LMaxY = 0;
    if (LandscapeInfo->GetLandscapeExtent(LMinX, LMinY, LMaxX, LMaxY))
    {
        MinX = FMath::Max(MinX, LMinX);
        MinY = FMath::Max(MinY, LMinY);
        MaxX = FMath::Min(MaxX, LMaxX);
        MaxY = FMath::Min(MaxY, LMaxY);
    }
    if (MinX > MaxX || MinY > MaxY)
        return 0;

    const int32 SizeX = MaxX - MinX + 1;
    const int32 SizeY = MaxY - MinY + 1;
    TArray<uint16> HeightData;
    HeightData.SetNumZeroed(SizeX * SizeY);

    // Pass false for bInUploadTextureChangesToGPU to prevent GPU sync hangs.
    FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo, false);
    LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0);

    const float HeightScale = 128.0f / static_cast<float>(LandscapeScale.Z);
    const float FlattenTarget = static_cast<float>((FlattenTargetZ - Landscape->GetActorLocation().Z) /
                                                   LandscapeScale.Z * 128.0 + 32768.0);

    // Smooth pass needs the region average before blending.
    double RegionAverage = 0.0;
    int32 RegionSamples = 0;
    const bool bSmooth = Tool == EMcpWorldBrushTool::Smooth;
    if (bSmooth)
    {
        for (int32 Y = MinY; Y <= MaxY; ++Y)
        {
            for (int32 X = MinX; X <= MaxX; ++X)
            {
                const float Dist = FMath::Sqrt(FMath::Square(static_cast<float>(X - CenterX)) +
                                               FMath::Square(static_cast<float>(Y - CenterY)));
                if (Dist > RadiusVerts)
                    continue;
                RegionAverage += HeightData[(Y - MinY) * SizeX + (X - MinX)];
                ++RegionSamples;
            }
        }
        if (RegionSamples > 0)
            RegionAverage /= RegionSamples;
    }

    bool bModified = false;
    for (int32 Y = MinY; Y <= MaxY; ++Y)
    {
        for (int32 X = MinX; X <= MaxX; ++X)
        {
            const float Dist = FMath::Sqrt(FMath::Square(static_cast<float>(X - CenterX)) +
                                           FMath::Square(static_cast<float>(Y - CenterY)));
            if (Dist > RadiusVerts)
                continue;
            float Alpha = 1.0f;
            if (FalloffVerts > 0 && Dist > (RadiusVerts - FalloffVerts))
                Alpha = 1.0f - ((Dist - (RadiusVerts - FalloffVerts)) / static_cast<float>(FalloffVerts));
            Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

            const int32 Index = (Y - MinY) * SizeX + (X - MinX);
            if (Index < 0 || Index >= HeightData.Num())
                continue;
            const uint16 CurrentHeight = HeightData[Index];

            float Delta = 0.0f;
            if (Tool == EMcpWorldBrushTool::Raise)
            {
                Delta = static_cast<float>(Strength) * Alpha * 100.0f * HeightScale;
            }
            else if (Tool == EMcpWorldBrushTool::Lower)
            {
                Delta = -static_cast<float>(Strength) * Alpha * 100.0f * HeightScale;
            }
            else if (Tool == EMcpWorldBrushTool::Flatten)
            {
                Delta = (FlattenTarget - static_cast<float>(CurrentHeight)) *
                        static_cast<float>(Strength) * Alpha;
            }
            else if (bSmooth && RegionSamples > 0)
            {
                Delta = (static_cast<float>(RegionAverage) - static_cast<float>(CurrentHeight)) *
                        static_cast<float>(Strength) * Alpha;
            }

            const int32 NewHeight = FMath::Clamp(static_cast<int32>(CurrentHeight + Delta), 0, 65535);
            if (NewHeight != CurrentHeight)
            {
                HeightData[Index] = static_cast<uint16>(NewHeight);
                bModified = true;
            }
        }
    }

    if (!bModified)
        return 0;

    const FScopedTransaction Transaction(FText::FromString(TEXT("World Brush Sculpt")));
    Landscape->Modify();
    LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0, true);
    LandscapeEdit.Flush();

    // Use MarkPackageDirty instead of PostEditChange to avoid full rebuilds.
    Landscape->MarkPackageDirty();
    return HeightData.Num();
}

namespace
{
ULandscapeLayerInfoObject *McpBrushEnsureLayerInfo(ALandscape *Landscape, ULandscapeInfo *LandscapeInfo,
                                                   const FString &LayerName, const FString &LayerInfoPath)
{
    for (const FLandscapeInfoLayerSettings &Layer : LandscapeInfo->Layers)
    {
        if (Layer.LayerName == FName(*LayerName) && Layer.LayerInfoObj)
            return Layer.LayerInfoObj;
    }
    if (!LayerInfoPath.IsEmpty())
    {
        if (ULandscapeLayerInfoObject *Loaded = LoadObject<ULandscapeLayerInfoObject>(nullptr, *LayerInfoPath))
            return Loaded;
    }
    const FString NewLayerPackagePath = FPackageName::GetLongPackagePath(
                                            Landscape->GetWorld()->GetOutermost()->GetName()) /
                                        FString::Printf(TEXT("%s_LayerInfo"), *LayerName);
    UPackage *NewLayerPackage = CreatePackage(*NewLayerPackagePath);
    ULandscapeLayerInfoObject *NewLayerInfo = NewObject<ULandscapeLayerInfoObject>(
        NewLayerPackage, FName(*FString::Printf(TEXT("%s_LayerInfo"), *LayerName)),
        RF_Public | RF_Standalone | RF_Transactional);
    if (!NewLayerInfo)
        return nullptr;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
    NewLayerInfo->SetLayerName(FName(*LayerName), true);
#else
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    NewLayerInfo->LayerName = FName(*LayerName);
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
    NewLayerInfo->MarkPackageDirty();
    if (!McpSafeAssetSave(NewLayerInfo))
        return nullptr;
    return NewLayerInfo;
}
} // namespace

int32 McpWorldBrushApplyPaintDab(ALandscape *Landscape, const FVector &WorldLocation,
                                 const FString &LayerName, const FString &LayerInfoPath,
                                 double Radius, double Strength, double Falloff)
{
    if (!Landscape || LayerName.IsEmpty())
        return -1;
    ULandscapeInfo *LandscapeInfo = Landscape->GetLandscapeInfo();
    if (!LandscapeInfo)
        return -1;

    ULandscapeLayerInfoObject *LayerInfo = McpBrushEnsureLayerInfo(Landscape, LandscapeInfo, LayerName, LayerInfoPath);
    if (!LayerInfo)
        return -1;

    const FVector LocalPos = Landscape->GetActorTransform().InverseTransformPosition(WorldLocation);
    const int32 CenterX = FMath::RoundToInt(LocalPos.X);
    const int32 CenterY = FMath::RoundToInt(LocalPos.Y);
    const FVector LandscapeScale = Landscape->GetActorScale3D();
    if (FMath::IsNearlyZero(LandscapeScale.X))
        return -1;

    const int32 RadiusVerts = FMath::Max(1, FMath::RoundToInt(Radius / LandscapeScale.X));
    const int32 FalloffVerts = FMath::RoundToInt(static_cast<double>(RadiusVerts) * FMath::Clamp(Falloff, 0.0, 1.0));
    int32 MinX = CenterX - RadiusVerts;
    int32 MaxX = CenterX + RadiusVerts;
    int32 MinY = CenterY - RadiusVerts;
    int32 MaxY = CenterY + RadiusVerts;
    int32 LMinX = 0, LMinY = 0, LMaxX = 0, LMaxY = 0;
    if (LandscapeInfo->GetLandscapeExtent(LMinX, LMinY, LMaxX, LMaxY))
    {
        MinX = FMath::Max(MinX, LMinX);
        MinY = FMath::Max(MinY, LMinY);
        MaxX = FMath::Min(MaxX, LMaxX);
        MaxY = FMath::Min(MaxY, LMaxY);
    }
    if (MinX > MaxX || MinY > MaxY)
        return 0;

    const int32 SizeX = MaxX - MinX + 1;
    const int32 SizeY = MaxY - MinY + 1;
    constexpr int32 MaxDabPixels = 4194304; // 4M pixels keeps stroke dabs interactive.
    if (SizeX * SizeY > MaxDabPixels)
        return -1;

    const FScopedTransaction Transaction(FText::FromString(TEXT("World Brush Paint")));
    Landscape->Modify();
    if (!Landscape->HasTargetLayer(FName(*LayerName)))
    {
        Landscape->AddTargetLayer(FName(*LayerName), FLandscapeTargetLayerSettings(LayerInfo));
    }
    LandscapeInfo->UpdateLayerInfoMap(Landscape);
    const int32 LayerInfoIndex = LandscapeInfo->GetLayerInfoIndex(FName(*LayerName));
    if (LayerInfoIndex == INDEX_NONE)
        return -1;
    LandscapeInfo->Layers[LayerInfoIndex].LayerInfoObj = LayerInfo;

    // This object holds writable weightmap texture locks and must be
    // destroyed before safe package saving at stroke end.
    FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo, Landscape->GetEditingLayer(), false);

    TArray<uint8> Alpha;
    Alpha.SetNumZeroed(SizeX * SizeY);
    LandscapeEdit.GetWeightData(LayerInfo, MinX, MinY, MaxX, MaxY, Alpha.GetData(), SizeX);

    int32 Written = 0;
    const double ClampedStrength = FMath::Clamp(Strength, 0.0, 1.0);
    for (int32 Y = MinY; Y <= MaxY; ++Y)
    {
        for (int32 X = MinX; X <= MaxX; ++X)
        {
            const float Dist = FMath::Sqrt(FMath::Square(static_cast<float>(X - CenterX)) +
                                           FMath::Square(static_cast<float>(Y - CenterY)));
            if (Dist > RadiusVerts)
                continue;
            float BrushAlpha = 1.0f;
            if (FalloffVerts > 0 && Dist > (RadiusVerts - FalloffVerts))
                BrushAlpha = 1.0f - ((Dist - (RadiusVerts - FalloffVerts)) / static_cast<float>(FalloffVerts));
            BrushAlpha = FMath::Clamp(BrushAlpha, 0.0f, 1.0f);
            const uint8 BrushValue = static_cast<uint8>(BrushAlpha * ClampedStrength * 255.0f + 0.5f);
            const int32 Index = (Y - MinY) * SizeX + (X - MinX);
            // Max-blend keeps strokes additive instead of overwriting.
            if (BrushValue > Alpha[Index])
            {
                Alpha[Index] = BrushValue;
                ++Written;
            }
        }
    }

    if (Written == 0)
        return 0;

    LandscapeEdit.SetAlphaData(LayerInfo, MinX, MinY, MaxX, MaxY, Alpha.GetData(), SizeX);
    LandscapeEdit.Flush();
    Landscape->MarkPackageDirty();
    return Written;
}

int32 McpWorldBrushApplyFoliageDab(UWorld *World, ALandscape *Landscape,
                                   const FVector &WorldLocation, const FMcpWorldBrushSettings &Settings,
                                   int32 StrokeSalt)
{
    if (!World || !Landscape || Settings.FoliageMeshPath.IsEmpty() || Settings.FoliagePerDab <= 0)
        return -1;

    UStaticMesh *StaticMesh = LoadObject<UStaticMesh>(nullptr, *Settings.FoliageMeshPath);
    if (!StaticMesh)
        return -1;

    const FString MeshBase = SanitizeAssetName(FPaths::GetBaseFilename(Settings.FoliageMeshPath));
    const FString ActorLabel = FString::Printf(TEXT("MCP_BrushFoliage_%s"), *MeshBase);

    AActor *Actor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor *Candidate = *It;
        if (Candidate && Candidate->ActorHasTag(FName(BrushFoliageTag)) &&
            Candidate->GetActorLabel().Equals(ActorLabel, ESearchCase::IgnoreCase))
        {
            Actor = Candidate;
            break;
        }
    }
    UHierarchicalInstancedStaticMeshComponent *Hism = nullptr;
    if (!Actor)
    {
        const FScopedTransaction Transaction(FText::FromString(TEXT("Create Brush Foliage Collection")));
        FActorSpawnParameters SpawnParams;
        SpawnParams.ObjectFlags |= RF_Transactional;
        Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        if (!Actor)
            return -1;
        Actor->SetIsSpatiallyLoaded(false);
        Actor->SetActorLabel(ActorLabel);
        Actor->Tags.Add(FName(BrushFoliageTag));
        Actor->Tags.Add(FName(*(FString(BrushFoliageNamePrefix) + ActorLabel)));
        USceneComponent *Root = NewObject<USceneComponent>(Actor, TEXT("BrushFoliageRoot"), RF_Transactional);
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

    const FScopedTransaction Transaction(FText::FromString(TEXT("World Brush Scatter")));
    Actor->Modify();
    static int32 DabCounter = 0;
    FRandomStream Random(Settings.FoliageSeed + StrokeSalt * 131071 + (++DabCounter) * 7919);

    const double Radius = FMath::Max(1.0, Settings.Radius);
    int32 Placed = 0;
    const int32 Attempts = Settings.FoliagePerDab * 24;
    for (int32 Attempt = 0; Attempt < Attempts && Placed < Settings.FoliagePerDab; ++Attempt)
    {
        const double Angle = Random.FRandRange(0.0, 2.0 * PI);
        const double Distance = Radius * FMath::Sqrt(Random.FRand());
        const FVector SamplePoint(WorldLocation.X + FMath::Cos(Angle) * Distance,
                                  WorldLocation.Y + FMath::Sin(Angle) * Distance,
                                  WorldLocation.Z);
        const FVector Start = SamplePoint + FVector(0.0, 0.0, 50000.0);
        FHitResult Hit;
        if (!World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.0, 0.0, 100000.0),
                                             ECC_WorldStatic) ||
            !Cast<ALandscapeProxy>(Hit.GetActor()))
            continue;
        const float Scale = Random.FRandRange(0.8f, 1.2f);
        const FTransform Transform(
            FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f),
            Hit.ImpactPoint + Hit.ImpactNormal * 2.0f, FVector(Scale));
        Hism->AddInstance(Transform);
        ++Placed;
    }

    if (Placed > 0)
        Actor->MarkPackageDirty();
    return Placed;
}

bool McpWorldBrushEndStroke(ALandscape *Landscape, FString &OutError)
{
    if (!Landscape || !Landscape->GetWorld())
    {
        OutError = TEXT("No landscape for stroke completion.");
        return false;
    }
    Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Weightmap_All);
    if (ULandscapeInfo *LandscapeInfo = Landscape->GetLandscapeInfo())
        LandscapeInfo->ForceLayersFullUpdate();
    return McpBrushSaveLandscapePersistence(Landscape->GetWorld(), Landscape, OutError);
}

#else // !WITH_EDITOR

ALandscape *McpWorldBrushResolveLandscape(UWorld * /*World*/, const FHitResult & /*Hit*/) { return nullptr; }
int32 McpWorldBrushApplyHeightDab(ALandscape * /*Landscape*/, const FVector & /*WorldLocation*/,
                                  EMcpWorldBrushTool /*Tool*/, double /*Radius*/, double /*Strength*/,
                                  double /*Falloff*/, double /*FlattenTargetZ*/) { return -1; }
int32 McpWorldBrushApplyPaintDab(ALandscape * /*Landscape*/, const FVector & /*WorldLocation*/,
                                 const FString & /*LayerName*/, const FString & /*LayerInfoPath*/,
                                 double /*Radius*/, double /*Strength*/, double /*Falloff*/) { return -1; }
int32 McpWorldBrushApplyFoliageDab(UWorld * /*World*/, ALandscape * /*Landscape*/,
                                   const FVector & /*WorldLocation*/, const FMcpWorldBrushSettings & /*Settings*/,
                                   int32 /*StrokeSalt*/) { return -1; }
bool McpWorldBrushEndStroke(ALandscape * /*Landscape*/, FString &OutError)
{
    OutError = TEXT("World brush requires editor build.");
    return false;
}

#endif // WITH_EDITOR
