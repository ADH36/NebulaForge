// =============================================================================
// McpWorldBrushWidget.cpp
// =============================================================================

#include "Brush/McpWorldBrushWidget.h"

#include "Brush/McpWorldBrushEdMode.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "McpWorldBrushWidget"

FMcpWorldBrushSettings &McpWorldBrushGetSettings()
{
    static FMcpWorldBrushSettings Settings;
    return Settings;
}

FMcpWorldBrushStrokeState &McpWorldBrushGetStrokeState()
{
    static FMcpWorldBrushStrokeState State;
    return State;
}

const TArray<FString> &McpWorldBrushToolLabels()
{
    static const TArray<FString> Labels = {
        TEXT("Raise"), TEXT("Lower"), TEXT("Flatten"),
        TEXT("Smooth"), TEXT("Paint Layer"), TEXT("Scatter Foliage")};
    return Labels;
}

namespace
{
EMcpWorldBrushTool McpBrushToolFromIndex(int32 Index)
{
    return static_cast<EMcpWorldBrushTool>(FMath::Clamp(Index, 0, 5));
}

int32 McpBrushToolToIndex(EMcpWorldBrushTool Tool)
{
    return FMath::Clamp(static_cast<int32>(Tool), 0, 5);
}

FReply McpBrushToggleMode()
{
    if (GLevelEditorModeTools().IsModeActive(FMcpWorldBrushEdMode::EM_McpWorldBrush))
        GLevelEditorModeTools().DeactivateMode(FMcpWorldBrushEdMode::EM_McpWorldBrush);
    else
        GLevelEditorModeTools().ActivateMode(FMcpWorldBrushEdMode::EM_McpWorldBrush);
    return FReply::Handled();
}

bool McpBrushIsModeActive()
{
    return GLevelEditorModeTools().IsModeActive(FMcpWorldBrushEdMode::EM_McpWorldBrush);
}

TSharedRef<SWidget> McpBrushLabeledRow(const FText &Label, TSharedRef<SWidget> Control)
{
    return SNew(SHorizontalBox)
           + SHorizontalBox::Slot()
             .FillWidth(0.42f)
             .VAlign(VAlign_Center)
             [
                 SNew(STextBlock).Text(Label)
             ]
           + SHorizontalBox::Slot()
             .FillWidth(0.58f)
             [
                 Control
             ];
}
} // namespace

TSharedRef<SWidget> MakeMcpWorldBrushSettingsWidget()
{
    FMcpWorldBrushSettings &Settings = McpWorldBrushGetSettings();

    TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

    // Mode toggle -----------------------------------------------------------
    Box->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SButton)
            .Text_Lambda([]()
                         {
                return McpBrushIsModeActive()
                    ? LOCTEXT("DeactivateBrush", "Deactivate Brush (Esc)")
                    : LOCTEXT("ActivateBrush", "Activate Brush Mode");
            })
            .OnClicked_Static(&McpBrushToggleMode)
            .HAlign(HAlign_Center)
        ];

    // Tool selector ---------------------------------------------------------
    // Options array must outlive the combo box, so it is function-static.
    static TArray<TSharedPtr<FString>> ToolOptions;
    if (ToolOptions.Num() == 0)
    {
        for (const FString &Label : McpWorldBrushToolLabels())
            ToolOptions.Add(MakeShared<FString>(Label));
    }
    Box->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            McpBrushLabeledRow(
                LOCTEXT("BrushTool", "Tool: "),
                SNew(SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&ToolOptions)
                    .InitiallySelectedItem(ToolOptions[McpBrushToolToIndex(Settings.Tool)])
                    .OnGenerateWidget_Lambda([](TSharedPtr<FString> Option)
                                             {
                        return SNew(STextBlock).Text(FText::FromString(*Option));
                    })
                    .OnSelectionChanged_Lambda([](TSharedPtr<FString> Selected, ESelectInfo::Type)
                                               {
                        if (!Selected.IsValid())
                            return;
                        const int32 Index = McpWorldBrushToolLabels().IndexOfByKey(*Selected);
                        if (Index != INDEX_NONE)
                            McpWorldBrushGetSettings().Tool = McpBrushToolFromIndex(Index);
                    })
                    .Content()
                    [
                        SNew(STextBlock)
                        .Text_Lambda([]()
                                     {
                            return FText::FromString(
                                McpWorldBrushToolLabels()[McpBrushToolToIndex(
                                    McpWorldBrushGetSettings().Tool)]);
                        })
                    ])
        ];

    // Radius / strength / falloff -------------------------------------------
    Box->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            McpBrushLabeledRow(
                LOCTEXT("BrushRadius", "Radius: "),
                SNew(SSpinBox<double>)
                    .MinValue(10.0)
                    .MaxValue(20000.0)
                    .Value(Settings.Radius)
                    .OnValueChanged_Lambda([](double NewValue)
                                           {
                        McpWorldBrushGetSettings().Radius = NewValue;
                    }))
        ];
    Box->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            McpBrushLabeledRow(
                LOCTEXT("BrushStrength", "Strength: "),
                SNew(SSpinBox<double>)
                    .MinValue(0.0)
                    .MaxValue(1.0)
                    .Delta(0.05)
                    .Value(Settings.Strength)
                    .OnValueChanged_Lambda([](double NewValue)
                                           {
                        McpWorldBrushGetSettings().Strength = NewValue;
                    }))
        ];
    Box->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            McpBrushLabeledRow(
                LOCTEXT("BrushFalloff", "Falloff: "),
                SNew(SSpinBox<double>)
                    .MinValue(0.0)
                    .MaxValue(1.0)
                    .Delta(0.05)
                    .Value(Settings.Falloff)
                    .OnValueChanged_Lambda([](double NewValue)
                                           {
                        McpWorldBrushGetSettings().Falloff = NewValue;
                    }))
        ];

    // Target landscape ------------------------------------------------------
    Box->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            McpBrushLabeledRow(
                LOCTEXT("BrushLandscape", "Landscape (blank = cursor): "),
                SNew(SEditableTextBox)
                    .Text(FText::FromString(Settings.TargetLandscapeName))
                    .OnTextChanged_Lambda([](const FText &NewText)
                                          {
                        McpWorldBrushGetSettings().TargetLandscapeName = NewText.ToString();
                    }))
        ];

    // Paint options ----------------------------------------------------------
    Box->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            McpBrushLabeledRow(
                LOCTEXT("BrushLayer", "Paint layer: "),
                SNew(SEditableTextBox)
                    .Text(FText::FromString(Settings.LayerName))
                    .OnTextChanged_Lambda([](const FText &NewText)
                                          {
                        McpWorldBrushGetSettings().LayerName = NewText.ToString();
                    }))
        ];

    // Foliage options ---------------------------------------------------------
    Box->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            McpBrushLabeledRow(
                LOCTEXT("BrushFoliageMesh", "Foliage mesh: "),
                SNew(SEditableTextBox)
                    .Text(FText::FromString(Settings.FoliageMeshPath))
                    .OnTextChanged_Lambda([](const FText &NewText)
                                          {
                        McpWorldBrushGetSettings().FoliageMeshPath = NewText.ToString();
                    }))
        ];
    Box->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            McpBrushLabeledRow(
                LOCTEXT("BrushFoliagePerDab", "Instances / dab: "),
                SNew(SSpinBox<int32>)
                    .MinValue(1)
                    .MaxValue(64)
                    .Value(Settings.FoliagePerDab)
                    .OnValueChanged_Lambda([](int32 NewValue)
                                           {
                        McpWorldBrushGetSettings().FoliagePerDab = NewValue;
                    }))
        ];

    Box->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 6.0f, 0.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("BrushHint",
                          "Drag with left mouse in a viewport to paint strokes. Esc exits brush mode."))
            .AutoWrapText(true)
        ];

    return Box;
}

#undef LOCTEXT_NAMESPACE
