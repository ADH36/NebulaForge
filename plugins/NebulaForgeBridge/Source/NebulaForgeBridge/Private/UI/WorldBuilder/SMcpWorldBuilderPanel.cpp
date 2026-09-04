// =============================================================================
// SMcpWorldBuilderPanel.cpp
// =============================================================================

#include "UI/WorldBuilder/SMcpWorldBuilderPanel.h"

#include "Brush/McpWorldBrushWidget.h"
#include "Editor.h"
#include "McpHandlerUtils.h"
#include "NebulaForgeBridgeSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SMcpWorldBuilderPanel"

void SMcpWorldBuilderPanel::Construct(const FArguments &InArgs)
{
    RefreshPresets();

    TerrainFeatureOptions.Reset();
    for (const TCHAR *Feature : {TEXT("mountains"), TEXT("hills"), TEXT("valleys"),
                                 TEXT("plains"), TEXT("lakeshore")})
    {
        TerrainFeatureOptions.Add(MakeShared<FString>(Feature));
    }

    ChildSlot
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
              .Padding(8.0f)
              [
                  SNew(SVerticalBox)
                  + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 4.0f)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("Header", "Generate World"))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
                    ]
                  + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("SubHeader",
                                      "Builds a landscape, procedural terrain, rule-painted layers, and deterministic foliage from a biome preset or defaults."))
                        .AutoWrapText(true)
                    ]

                    // --- Preset selection ---------------------------------
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                          .AutoWidth()
                          .VAlign(VAlign_Center)
                          [
                              SNew(STextBlock)
                              .Text(LOCTEXT("PresetLabel", "Biome preset: "))
                          ]
                        + SHorizontalBox::Slot()
                          .FillWidth(1.0f)
                          [
                              SAssignNew(PresetComboBox, SComboBox<FStringPtr>)
                              .OptionsSource(&PresetOptions)
                              .OnGenerateWidget_Lambda([](FStringPtr Option)
                                                        {
                                                            return SNew(STextBlock)
                                                                .Text(FText::FromString(*Option));
                                                        })
                              .OnSelectionChanged(this, &SMcpWorldBuilderPanel::OnPresetSelected)
                              .Content()
                              [
                                  SNew(STextBlock)
                                  .Text(this, &SMcpWorldBuilderPanel::GetSelectedPresetText)
                              ]
                          ]
                        + SHorizontalBox::Slot()
                          .AutoWidth()
                          .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                          [
                              SNew(SButton)
                              .Text(LOCTEXT("RefreshPresets", "Refresh"))
                              .OnClicked(this, &SMcpWorldBuilderPanel::OnRefreshPresets)
                          ]
                    ]

                    // --- Landscape settings -------------------------------
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 8.0f, 0.0f, 2.0f)
                    [
                        SNew(SSeparator)
                    ]
                  + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                          .FillWidth(0.4f)
                          .VAlign(VAlign_Center)
                          [
                              SNew(STextBlock).Text(LOCTEXT("LandscapeName", "Landscape name: "))
                          ]
                        + SHorizontalBox::Slot()
                          .FillWidth(0.6f)
                          [
                              SNew(SEditableTextBox)
                              .Text(FText::FromString(LandscapeName))
                              .OnTextChanged_Lambda([this](const FText &NewText)
                                                    {
                                                        LandscapeName = NewText.ToString();
                                                    })
                          ]
                    ]
                  + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 2.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                          .FillWidth(0.4f)
                          .VAlign(VAlign_Center)
                          [
                              SNew(STextBlock).Text(LOCTEXT("Components", "Components X / Y: "))
                          ]
                        + SHorizontalBox::Slot()
                          .FillWidth(0.3f)
                          [
                              SNew(SSpinBox<int32>)
                              .MinValue(1)
                              .MaxValue(32)
                              .Value(ComponentsX)
                              .OnValueChanged_Lambda([this](int32 NewValue)
                                                      {
                                                          ComponentsX = NewValue;
                                                      })
                          ]
                        + SHorizontalBox::Slot()
                          .FillWidth(0.3f)
                          [
                              SNew(SSpinBox<int32>)
                              .MinValue(1)
                              .MaxValue(32)
                              .Value(ComponentsY)
                              .OnValueChanged_Lambda([this](int32 NewValue)
                                                      {
                                                          ComponentsY = NewValue;
                                                      })
                          ]
                    ]
                  + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 2.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                          .FillWidth(0.4f)
                          .VAlign(VAlign_Center)
                          [
                              SNew(STextBlock).Text(LOCTEXT("Quads", "Quads per section: "))
                          ]
                        + SHorizontalBox::Slot()
                          .FillWidth(0.6f)
                          [
                              SNew(SSpinBox<int32>)
                              .MinValue(7)
                              .MaxValue(255)
                              .Value(QuadsPerComponent)
                              .OnValueChanged_Lambda([this](int32 NewValue)
                                                      {
                                                          const int32 ValidValues[] = {7, 15, 31, 63, 127, 255};
                                                          int32 Closest = ValidValues[0];
                                                          for (const int32 Valid : ValidValues)
                                                          {
                                                              if (FMath::Abs(NewValue - Valid) <
                                                                  FMath::Abs(NewValue - Closest))
                                                              {
                                                                  Closest = Valid;
                                                              }
                                                          }
                                                          QuadsPerComponent = Closest;
                                                      })
                          ]
                    ]
                  + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 2.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                          .FillWidth(0.4f)
                          .VAlign(VAlign_Center)
                          [
                              SNew(STextBlock).Text(LOCTEXT("Reuse", "Reuse existing landscape: "))
                          ]
                        + SHorizontalBox::Slot()
                          .FillWidth(0.6f)
                          .VAlign(VAlign_Center)
                          [
                              SNew(SCheckBox)
                              .IsChecked(bReuseExistingLandscape ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                              .OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
                                                          {
                                                              bReuseExistingLandscape =
                                                                  NewState == ECheckBoxState::Checked;
                                                          })
                          ]
                    ]

                    // --- Terrain settings ----------------------------------
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 8.0f, 0.0f, 2.0f)
                    [
                        SNew(SSeparator)
                    ]
                  + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                          .FillWidth(0.4f)
                          .VAlign(VAlign_Center)
                          [
                              SNew(STextBlock).Text(LOCTEXT("Feature", "Terrain feature: "))
                          ]
                        + SHorizontalBox::Slot()
                          .FillWidth(0.6f)
                          [
                              SNew(SComboBox<TSharedPtr<FString>>)
                              .OptionsSource(&TerrainFeatureOptions)
                              .OnGenerateWidget_Lambda([](TSharedPtr<FString> Option)
                                                        {
                                                            return SNew(STextBlock)
                                                                .Text(FText::FromString(*Option));
                                                        })
                              .OnSelectionChanged_Lambda([this](TSharedPtr<FString> Selected, ESelectInfo::Type)
                                                         {
                                                             if (Selected.IsValid())
                                                             {
                                                                 TerrainFeature = *Selected;
                                                             }
                                                         })
                              .Content()
                              [
                                  SNew(STextBlock)
                                  .Text_Lambda([this]()
                                               {
                                                   return FText::FromString(TerrainFeature);
                                               })
                              ]
                          ]
                    ]
                  + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 2.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                          .FillWidth(0.4f)
                          .VAlign(VAlign_Center)
                          [
                              SNew(STextBlock).Text(LOCTEXT("Seed", "Seed: "))
                          ]
                        + SHorizontalBox::Slot()
                          .FillWidth(0.6f)
                          [
                              SNew(SSpinBox<int32>)
                              .MinValue(0)
                              .MaxValue(2147483647)
                              .Value(Seed)
                              .OnValueChanged_Lambda([this](int32 NewValue)
                                                      {
                                                          Seed = NewValue;
                                                      })
                          ]
                    ]
                  + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 2.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                          .FillWidth(0.4f)
                          .VAlign(VAlign_Center)
                          [
                              SNew(STextBlock).Text(LOCTEXT("HeightScale", "Height scale: "))
                          ]
                        + SHorizontalBox::Slot()
                          .FillWidth(0.6f)
                          [
                              SNew(SSpinBox<double>)
                              .MinValue(0.0)
                              .MaxValue(32767.0)
                              .Value(HeightScale)
                              .OnValueChanged_Lambda([this](double NewValue)
                                                      {
                                                          HeightScale = NewValue;
                                                      })
                          ]
                    ]
                  + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 2.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                          .FillWidth(0.4f)
                          .VAlign(VAlign_Center)
                          [
                              SNew(STextBlock).Text(LOCTEXT("Erosion", "Thermal erosion: "))
                          ]
                        + SHorizontalBox::Slot()
                          .AutoWidth()
                          .VAlign(VAlign_Center)
                          [
                              SNew(SCheckBox)
                              .IsChecked(bErosion ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                              .OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
                                                          {
                                                              bErosion = NewState == ECheckBoxState::Checked;
                                                          })
                          ]
                        + SHorizontalBox::Slot()
                          .FillWidth(1.0f)
                          .Padding(8.0f, 0.0f, 0.0f, 0.0f)
                          [
                              SNew(SSpinBox<int32>)
                              .MinValue(1)
                              .MaxValue(64)
                              .Value(ErosionIterations)
                              .OnValueChanged_Lambda([this](int32 NewValue)
                                                      {
                                                          ErosionIterations = NewValue;
                                                      })
                          ]
                    ]

                    // --- Interactive brush ----------------------------------
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 8.0f, 0.0f, 2.0f)
                    [
                        SNew(SSeparator)
                    ]
                  + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f, 0.0f, 2.0f)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("BrushHeader", "Interactive Brush"))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                    ]
                  + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 2.0f)
                    [
                        // Shared controls with the brush mode toolkit.
                        MakeMcpWorldBrushSettingsWidget()
                    ]

                    // --- Generate -------------------------------------------
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 12.0f, 0.0f, 4.0f)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("Generate", "Generate World"))
                        .OnClicked(this, &SMcpWorldBuilderPanel::OnGenerate)
                        .IsEnabled(this, &SMcpWorldBuilderPanel::IsGenerateEnabled)
                        .HAlign(HAlign_Center)
                    ]
                  + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f)
                    [
                        SNew(STextBlock)
                        .Text(this, &SMcpWorldBuilderPanel::GetStatusText)
                        .ColorAndOpacity(this, &SMcpWorldBuilderPanel::GetStatusColor)
                        .AutoWrapText(true)
                    ]
              ]
        ];
}

void SMcpWorldBuilderPanel::RefreshPresets()
{
    PresetOptions.Reset();
    if (GEditor)
    {
        if (UNebulaForgeBridgeSubsystem *Subsystem =
                GEditor->GetEditorSubsystem<UNebulaForgeBridgeSubsystem>())
        {
            TArray<FString> Paths;
            Subsystem->ListBiomePresetAssetPaths(Paths);
            for (const FString &Path : Paths)
            {
                PresetOptions.Add(MakeShared<FString>(Path));
            }
        }
    }
    if (PresetComboBox.IsValid())
    {
        PresetComboBox->RefreshOptions();
    }
}

FReply SMcpWorldBuilderPanel::OnRefreshPresets()
{
    RefreshPresets();
    return FReply::Handled();
}

bool SMcpWorldBuilderPanel::IsGenerateEnabled() const
{
    return !bRunning;
}

FText SMcpWorldBuilderPanel::GetStatusText() const
{
    return FText::FromString(StatusText);
}

FSlateColor SMcpWorldBuilderPanel::GetStatusColor() const
{
    switch (StatusSeverity)
    {
    case 1:
        return FSlateColor(FColor::Yellow);
    case 2:
        return FSlateColor(FColor::Green);
    case 3:
        return FSlateColor(FColor(255, 80, 80));
    default:
        break;
    }
    return FSlateColor(FColor::White);
}

FText SMcpWorldBuilderPanel::GetSelectedPresetText() const
{
    return SelectedPreset.IsValid() ? FText::FromString(*SelectedPreset)
                                    : LOCTEXT("NoPreset", "<defaults>");
}

void SMcpWorldBuilderPanel::OnPresetSelected(FStringPtr Selected, ESelectInfo::Type SelectInfo)
{
    SelectedPreset = Selected;
}

FReply SMcpWorldBuilderPanel::OnGenerate()
{
    if (bRunning)
    {
        return FReply::Handled();
    }
    if (!GEditor)
    {
        return FReply::Handled();
    }
    UNebulaForgeBridgeSubsystem *Subsystem =
        GEditor->GetEditorSubsystem<UNebulaForgeBridgeSubsystem>();
    if (!Subsystem)
    {
        StatusText = TEXT("Bridge subsystem unavailable.");
        StatusSeverity = 3;
        return FReply::Handled();
    }
    // World recipes need a saved /Game map to persist the landscape into.
    // Fail fast here with an actionable message instead of a bare step count.
    UWorld *EditorWorld = GEditor->GetEditorWorldContext().World();
    if (!EditorWorld || !EditorWorld->GetOutermost()->GetName().StartsWith(TEXT("/Game/")))
    {
        StatusText = TEXT("Save the current level under /Game first (File > Save Current As...), then generate.");
        StatusSeverity = 3;
        return FReply::Handled();
    }
    LandscapeName.TrimStartAndEndInline();
    if (LandscapeName.IsEmpty())
    {
        LandscapeName = TEXT("MCP_WorldLandscape");
    }

    TSharedPtr<FJsonObject> Payload = McpHandlerUtils::CreateResultObject();
    Payload->SetStringField(TEXT("name"), LandscapeName);
    Payload->SetNumberField(TEXT("componentsX"), ComponentsX);
    Payload->SetNumberField(TEXT("componentsY"), ComponentsY);
    Payload->SetNumberField(TEXT("quadsPerComponent"), QuadsPerComponent);
    Payload->SetNumberField(TEXT("sectionsPerComponent"), SectionsPerComponent);
    Payload->SetStringField(TEXT("terrainFeature"), TerrainFeature);
    Payload->SetNumberField(TEXT("seed"), Seed);
    Payload->SetNumberField(TEXT("heightScale"), HeightScale);
    Payload->SetBoolField(TEXT("reuseExistingLandscape"), bReuseExistingLandscape);
    if (bErosion)
    {
        Payload->SetNumberField(TEXT("iterations"), ErosionIterations);
    }
    if (SelectedPreset.IsValid() && !SelectedPreset->IsEmpty())
    {
        Payload->SetStringField(TEXT("biomePresetPath"), *SelectedPreset);
    }

    bRunning = true;
    StatusSeverity = 1;
    StatusText = FString::Printf(TEXT("Generating world '%s' (seed %d)..."), *LandscapeName, Seed);

    TWeakObjectPtr<UNebulaForgeBridgeSubsystem> WeakSubsystem(Subsystem);
    TWeakPtr<SMcpWorldBuilderPanel> WeakThis(SharedThis(this));
    const bool bStarted = Subsystem->RunWorldRecipe(
        Payload,
        [WeakThis, WeakSubsystem](bool bSuccess, const TSharedPtr<FJsonObject> &Result)
        {
            if (SMcpWorldBuilderPanel *Panel = WeakThis.Pin().Get())
            {
                FString Status = TEXT("World recipe failed.");
                if (Result.IsValid())
                {
                    // Surface the real backend error when the chain never ran.
                    FString BackendError;
                    if (Result->TryGetStringField(TEXT("error"), BackendError) && !BackendError.IsEmpty())
                    {
                        Status = BackendError;
                    }
                    else
                    {
                        const FString ResultStatus = Result->GetStringField(TEXT("status"));
                        const int32 Failed = static_cast<int32>(Result->GetNumberField(TEXT("failedSteps")));
                        const int32 Total = static_cast<int32>(Result->GetNumberField(TEXT("stepCount")));
                        Status = FString::Printf(TEXT("World recipe %s: %d/%d steps failed."),
                                                 *ResultStatus, Failed, Total);
                    }
                }
                Panel->bRunning = false;
                Panel->StatusText = Status;
                Panel->StatusSeverity = bSuccess ? 2 : 3;

                FNotificationInfo Info(FText::FromString(Status));
                Info.ExpireDuration = 5.0f;
                Info.bUseLargeFont = true;
                FSlateNotificationManager::Get().AddNotification(Info);
            }
        });
    if (!bStarted)
    {
        bRunning = false;
        StatusSeverity = 3;
        StatusText = TEXT("Failed to start world recipe (editor build required).");
    }
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
