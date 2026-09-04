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
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
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

ALandscape *McpWorldBrushFindLandscapeAlongRay(UWorld *World, const FVector &Origin,
                                               const FVector &Direction)
{
    if (!World)
        return nullptr;
    TArray<FHitResult> Hits;
    FCollisionQueryParams Params;
    Params.bTraceComplex = true;
    if (!World->LineTraceMultiByChannel(Hits, Origin, Origin + Direction * 1000000.0f,
                                        ECC_Visibility, Params))
        return nullptr;
    for (const FHitResult &Hit : Hits)
    {
        if (ALandscape *Landscape = McpWorldBrushResolveLandscape(World, Hit))
            return Landscape;
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
                                 const TArray<FMcpBrushPaintLayer> &Layers,
                                 const FString &LayerInfoPath,
                                 double Radius, double Strength, double Falloff)
{
    if (!Landscape || Layers.Num() == 0)
        return -1;
    ULandscapeInfo *LandscapeInfo = Landscape->GetLandscapeInfo();
    if (!LandscapeInfo)
        return -1;

    // Resolve up to 4 non-empty layers first so a single bad entry cannot
    // abort the whole multi-material dab.
    struct FResolvedLayer
    {
        FString Name;
        ULandscapeLayerInfoObject *Info = nullptr;
        double Strength = 1.0;
    };
    TArray<FResolvedLayer> Resolved;
    for (const FMcpBrushPaintLayer &Entry : Layers)
    {
        if (Resolved.Num() >= 4 || Entry.LayerName.IsEmpty())
            continue;
        ULandscapeLayerInfoObject *Info =
            McpBrushEnsureLayerInfo(Landscape, LandscapeInfo, Entry.LayerName, LayerInfoPath);
        if (!Info)
            continue;
        FResolvedLayer Out;
        Out.Name = Entry.LayerName;
        Out.Info = Info;
        Out.Strength = FMath::Clamp(Entry.Strength, 0.0, 1.0);
        Resolved.Add(Out);
    }
    if (Resolved.Num() == 0)
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
    for (const FResolvedLayer &Layer : Resolved)
    {
        if (!Landscape->HasTargetLayer(FName(*Layer.Name)))
        {
            Landscape->AddTargetLayer(FName(*Layer.Name), FLandscapeTargetLayerSettings(Layer.Info));
        }
    }
    LandscapeInfo->UpdateLayerInfoMap(Landscape);
    for (const FResolvedLayer &Layer : Resolved)
    {
        const int32 LayerInfoIndex = LandscapeInfo->GetLayerInfoIndex(FName(*Layer.Name));
        if (LayerInfoIndex != INDEX_NONE)
            LandscapeInfo->Layers[LayerInfoIndex].LayerInfoObj = Layer.Info;
    }

    // This object holds writable weightmap texture locks and must be
    // destroyed before safe package saving at stroke end.
    FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo, Landscape->GetEditingLayer(), false);

    int32 Written = 0;
    const double ClampedStrength = FMath::Clamp(Strength, 0.0, 1.0);
    TArray<uint8> Alpha;
    for (const FResolvedLayer &Layer : Resolved)
    {
        Alpha.SetNumZeroed(SizeX * SizeY);
        LandscapeEdit.GetWeightData(Layer.Info, MinX, MinY, MaxX, MaxY, Alpha.GetData(), SizeX);

        const double EffectiveStrength = ClampedStrength * Layer.Strength;
        int32 LayerWritten = 0;
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
                const uint8 BrushValue = static_cast<uint8>(BrushAlpha * EffectiveStrength * 255.0f + 0.5f);
                const int32 Index = (Y - MinY) * SizeX + (X - MinX);
                // Max-blend keeps strokes additive instead of overwriting.
                if (BrushValue > Alpha[Index])
                {
                    Alpha[Index] = BrushValue;
                    ++LayerWritten;
                }
            }
        }

        if (LayerWritten == 0)
            continue;
        LandscapeEdit.SetAlphaData(Layer.Info, MinX, MinY, MaxX, MaxY, Alpha.GetData(), SizeX);
        Written += LayerWritten;
    }

    if (Written == 0)
        return 0;

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

FMcpWorldBrushRoadDraft &McpWorldBrushGetRoadDraft()
{
    static FMcpWorldBrushRoadDraft Draft;
    return Draft;
}

int32 McpBrushFinishRiverStroke(UWorld *World, ALandscape *Landscape,
                                const TArray<FMcpBrushRiverSample> &Samples,
                                const FMcpWorldBrushSettings &Settings,
                                FString &OutError)
{
    if (!World || !Landscape)
    {
        OutError = TEXT("No world or landscape for river completion.");
        return -1;
    }
    if (Samples.Num() < 2)
    {
        OutError = TEXT("River stroke needs a longer drag (at least two course samples).");
        return -1;
    }

    // Resample the recorded course so water spline points stay evenly spaced.
    const double Spacing = FMath::Max(200.0, Settings.RiverWidth * 0.5);
    TArray<FMcpBrushRiverSample> Course;
    Course.Add(Samples[0]);
    for (int32 Index = 1; Index < Samples.Num(); ++Index)
    {
        if ((Samples[Index].Location - Course.Last().Location).Size() >= Spacing)
            Course.Add(Samples[Index]);
    }
    if ((Course.Last().Location - Samples.Last().Location).Size() > 1.0)
        Course.Add(Samples.Last());
    if (Course.Num() < 2)
    {
        OutError = TEXT("River stroke is too short for a water course.");
        return -1;
    }

    // Carve the channel toward per-sample bed heights.
    const FScopedTransaction Transaction(FText::FromString(TEXT("World Brush River")));
    int32 DabCount = 0;
    const double HalfWidth = FMath::Max(100.0, Settings.RiverWidth);
    for (const FMcpBrushRiverSample &Sample : Course)
    {
        const double BedZ = Sample.WaterZ - FMath::Max(50.0, Settings.RiverDepth);
        const int32 Modified = McpWorldBrushApplyHeightDab(Landscape, Sample.Location,
                                                           EMcpWorldBrushTool::Flatten,
                                                           HalfWidth, 1.0, 0.4, BedZ);
        if (Modified < 0)
        {
            OutError = TEXT("River channel carving failed.");
            return -1;
        }
        ++DabCount;
    }

    if (Settings.bCreateRiverWater)
    {
        UClass *WaterClass = LoadClass<AActor>(nullptr, TEXT("/Script/Water.WaterBodyRiver"));
        if (!WaterClass)
        {
            UE_LOG(LogMcpWorldBrush, Warning,
                   TEXT("World brush river carved %d dabs but the Water plugin WaterBodyRiver class is unavailable; skipping water."),
                   DabCount);
        }
        else
        {
            FString WaterName = Settings.RiverWaterActor;
            if (WaterName.IsEmpty())
                WaterName = FString::Printf(TEXT("MCP_BrushRiver_%d"), FMath::RandRange(1000, 9999));
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
                FActorSpawnParameters SpawnParams;
                SpawnParams.ObjectFlags |= RF_Transactional;
                WaterActor = World->SpawnActor<AActor>(WaterClass, Course[0].Location,
                                                       FRotator::ZeroRotator, SpawnParams);
                if (WaterActor)
                    WaterActor->SetActorLabel(WaterName);
            }
            if (WaterActor)
            {
                if (USplineComponent *WaterSpline = WaterActor->FindComponentByClass<USplineComponent>())
                {
                    TArray<FVector> LocalPoints;
                    const FTransform WaterTransform = WaterActor->GetActorTransform();
                    for (const FMcpBrushRiverSample &Sample : Course)
                    {
                        LocalPoints.Add(WaterTransform.InverseTransformPosition(
                            FVector(Sample.Location.X, Sample.Location.Y, Sample.WaterZ)));
                    }
                    WaterSpline->SetSplinePoints(LocalPoints, ESplineCoordinateSpace::Local, true);
                }
                WaterActor->MarkPackageDirty();
            }
        }
    }

    if (!McpWorldBrushEndStroke(Landscape, OutError))
        return -1;
    return DabCount;
}

#else // !WITH_EDITOR

ALandscape *McpWorldBrushResolveLandscape(UWorld * /*World*/, const FHitResult & /*Hit*/) { return nullptr; }
ALandscape *McpWorldBrushFindLandscapeAlongRay(UWorld * /*World*/, const FVector & /*Origin*/,
                                               const FVector & /*Direction*/) { return nullptr; }
int32 McpWorldBrushApplyHeightDab(ALandscape * /*Landscape*/, const FVector & /*WorldLocation*/,
                                  EMcpWorldBrushTool /*Tool*/, double /*Radius*/, double /*Strength*/,
                                  double /*Falloff*/, double /*FlattenTargetZ*/) { return -1; }
int32 McpWorldBrushApplyPaintDab(ALandscape * /*Landscape*/, const FVector & /*WorldLocation*/,
                                 const TArray<FMcpBrushPaintLayer> & /*Layers*/,
                                 const FString & /*LayerInfoPath*/,
                                 double /*Radius*/, double /*Strength*/, double /*Falloff*/) { return -1; }
int32 McpWorldBrushApplyFoliageDab(UWorld * /*World*/, ALandscape * /*Landscape*/,
                                    const FVector & /*WorldLocation*/, const FMcpWorldBrushSettings & /*Settings*/,
                                    int32 /*StrokeSalt*/) { return -1; }
bool McpWorldBrushEndStroke(ALandscape * /*Landscape*/, FString &OutError)
{
    OutError = TEXT("World brush requires editor build.");
    return false;
}
FMcpWorldBrushRoadDraft &McpWorldBrushGetRoadDraft()
{
    static FMcpWorldBrushRoadDraft Draft;
    return Draft;
}
int32 McpBrushFinishRiverStroke(UWorld * /*World*/, ALandscape * /*Landscape*/,
                                const TArray<FMcpBrushRiverSample> & /*Samples*/,
                                const FMcpWorldBrushSettings & /*Settings*/,
                                FString &OutError)
{
    OutError = TEXT("World brush requires editor build.");
    return -1;
}

#endif // WITH_EDITOR
