// =============================================================================
// SMcpWorldBuilderPanel.h
// =============================================================================
// Slate panel exposing the generate_world recipe orchestration. Builders pick
// a biome preset (or defaults), tune global knobs, and generate a complete
// world (landscape, terrain, rule-painted layers, deterministic foliage)
// through the same subsystem pipeline the MCP generate_world action uses.
//
// Copyright (c) 2024 NebulaForge Bridge Contributors
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"

class SMcpWorldBuilderPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMcpWorldBuilderPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments &InArgs);

private:
    typedef TSharedPtr<FString> FStringPtr;

    /** Refreshes the preset combo options from the asset registry. */
    void RefreshPresets();

    FReply OnRefreshPresets();
    FReply OnGenerate();

    bool IsGenerateEnabled() const;
    FText GetStatusText() const;
    FSlateColor GetStatusColor() const;
    void OnPresetSelected(FStringPtr Selected, ESelectInfo::Type SelectInfo);
    FText GetSelectedPresetText() const;

    TArray<FStringPtr> PresetOptions;
    FStringPtr SelectedPreset;
    TSharedPtr<class SComboBox<FStringPtr>> PresetComboBox;
    TArray<TSharedPtr<FString>> TerrainFeatureOptions;

    FString LandscapeName = TEXT("MCP_WorldLandscape");
    int32 ComponentsX = 8;
    int32 ComponentsY = 8;
    int32 QuadsPerComponent = 63;
    int32 SectionsPerComponent = 1;
    FString TerrainFeature = TEXT("mountains");
    int32 Seed = 1337;
    double HeightScale = 8192.0;
    bool bErosion = true;
    int32 ErosionIterations = 8;
    bool bReuseExistingLandscape = true;

    FString StatusText = TEXT("Ready. Select a biome preset or generate with defaults.");
    bool bRunning = false;
};
