// =============================================================================
// McpWorldBrushWidget.cpp
// =============================================================================

#include "Brush/McpWorldBrushWidget.h"

#include "Brush/McpWorldBrushEdMode.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "NebulaForgeBridgeSubsystem.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Notifications/SNotificationList.h"
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
        TEXT("Smooth"), TEXT("Paint Layer"), TEXT("Scatter Foliage"),
        TEXT("River"), TEXT("Road Spline")};
    return Labels;
}

namespace
{
constexpr int32 McpBrushMaxToolIndex = 7;

EMcpWorldBrushTool McpBrushToolFromIndex(int32 Index)
{
    return static_cast<EMcpWorldBrushTool>(FMath::Clamp(Index, 0, McpBrushMaxToolIndex));
}

int32 McpBrushToolToIndex(EMcpWorldBrushTool Tool)
{
    return FMath::Clamp(static_cast<int32>(Tool), 0, McpBrushMaxToolIndex);
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

EVisibility McpBrushToolSectionVisibility(EMcpWorldBrushTool Tool)
{
    return McpWorldBrushGetSettings().Tool == Tool ? EVisibility::Visible : EVisibility::Collapsed;
}

/** Builds a road from the clicked draft points via the build_road recipe. */
FReply McpBrushBuildRoadFromDraft()
{
    FMcpWorldBrushRoadDraft &Draft = McpWorldBrushGetRoadDraft();
    if (Draft.Points.Num() < 2 || !GEditor)
        return FReply::Handled();
    UNebulaForgeBridgeSubsystem *Subsystem =
        GEditor->GetEditorSubsystem<UNebulaForgeBridgeSubsystem>();
    if (!Subsystem)
        return FReply::Handled();

    const FMcpWorldBrushSettings &Settings = McpWorldBrushGetSettings();
    TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("roadName"), TEXT("MCP_BrushRoad"));
    Payload->SetNumberField(TEXT("roadWidth"), Settings.RoadWidth);
    Payload->SetNumberField(TEXT("shoulderWidth"), Settings.ShoulderWidth);
    Payload->SetBoolField(TEXT("cutFill"), true);
    Payload->SetBoolField(TEXT("skipRoadbed"), true);
    Payload->SetBoolField(TEXT("skipFurniture"), true);
    Payload->SetBoolField(TEXT("skipJunctions"), true);
    Payload->SetBoolField(TEXT("skipWater"), true);
    TArray<TSharedPtr<FJsonValue>> Points;
    for (const FVector &Point : Draft.Points)
    {
        TSharedPtr<FJsonObject> Location = MakeShared<FJsonObject>();
        Location->SetNumberField(TEXT("x"), Point.X);
        Location->SetNumberField(TEXT("y"), Point.Y);
        Location->SetNumberField(TEXT("z"), Point.Z);
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetObjectField(TEXT("location"), Location);
        Points.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Payload->SetArrayField(TEXT("routePoints"), Points);

    const int32 PointCount = Draft.Points.Num();
    Draft.Points.Reset();
    const FString RequestId = FString::Printf(TEXT("brush-road-%s"), *FGuid::NewGuid().ToString());
    const TFunction<void(bool bSuccess, const TSharedPtr<FJsonObject> &Result)> NoCompletion;
    Subsystem->BeginBuildRoad(RequestId, TEXT("build_road"), Payload, nullptr, NoCompletion);

    FNotificationInfo Info(FText::FromString(
        FString::Printf(TEXT("Building brush road from %d points..."), PointCount)));
    Info.ExpireDuration = 4.0f;
    FSlateNotificationManager::Get().AddNotification(Info);
    return FReply::Handled();
}

FReply McpBrushClearRoadDraft()
{
    McpWorldBrushGetRoadDraft().Points.Reset();
    return FReply::Handled();
}

/** One optional extra paint-stack row (name + strength). */
TSharedRef<SWidget> McpBrushPaintExtraRow(int32 Index, const FText &Label)
{
    FMcpWorldBrushSettings &Settings = McpWorldBrushGetSettings();
    while (Settings.PaintLayers.Num() <= Index)
        Settings.PaintLayers.AddDefaulted();
    const FString CurrentName = Settings.PaintLayers[Index].LayerName;
    const double CurrentStrength = Settings.PaintLayers[Index].Strength;
    return SNew(SHorizontalBox)
           + SHorizontalBox::Slot()
             .FillWidth(0.42f)
             .VAlign(VAlign_Center)
             [
                 SNew(STextBlock).Text(Label)
             ]
           + SHorizontalBox::Slot()
             .FillWidth(0.36f)
             [
                 SNew(SEditableTextBox)
                 .Text(FText::FromString(CurrentName))
                 .OnTextChanged_Lambda([Index](const FText &NewText)
                                       {
                    FMcpWorldBrushSettings &Live = McpWorldBrushGetSettings();
                    while (Live.PaintLayers.Num() <= Index)
                        Live.PaintLayers.AddDefaulted();
                    Live.PaintLayers[Index].LayerName = NewText.ToString();
                })
             ]
           + SHorizontalBox::Slot()
             .FillWidth(0.22f)
             [
                 SNew(SSpinBox<double>)
                 .MinValue(0.0)
                 .MaxValue(1.0)
                 .Delta(0.05)
                 .Value(CurrentStrength)
                 .OnValueChanged_Lambda([Index](double NewValue)
                                        {
                    FMcpWorldBrushSettings &Live = McpWorldBrushGetSettings();
                    while (Live.PaintLayers.Num() <= Index)
                        Live.PaintLayers.AddDefaulted();
                    Live.PaintLayers[Index].Strength = NewValue;
                })
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

    // Paint options (primary + multi-material stack) -------------------------
    Box->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            SNew(SBox)
            .Visibility_Lambda([]()
                               {
                return McpBrushToolSectionVisibility(EMcpWorldBrushTool::PaintLayer);
            })
            .Content()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
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
                  ]
                + SVerticalBox::Slot()
                  .AutoHeight()
                  .Padding(0.0f, 2.0f)
                  [
                      McpBrushPaintExtraRow(0, LOCTEXT("BrushLayer2", "+ layer 2: "))
                  ]
                + SVerticalBox::Slot()
                  .AutoHeight()
                  .Padding(0.0f, 2.0f)
                  [
                      McpBrushPaintExtraRow(1, LOCTEXT("BrushLayer3", "+ layer 3: "))
                  ]
                + SVerticalBox::Slot()
                  .AutoHeight()
                  .Padding(0.0f, 2.0f)
                  [
                      McpBrushPaintExtraRow(2, LOCTEXT("BrushLayer4", "+ layer 4: "))
                  ]
            ]
        ];

    // Foliage options ---------------------------------------------------------
    Box->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            SNew(SBox)
            .Visibility_Lambda([]()
                               {
                return McpBrushToolSectionVisibility(EMcpWorldBrushTool::ScatterFoliage);
            })
            .Content()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
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
                  ]
                + SVerticalBox::Slot()
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
                  ]
            ]
        ];

    // River options ------------------------------------------------------------
    Box->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            SNew(SBox)
            .Visibility_Lambda([]()
                               {
                return McpBrushToolSectionVisibility(EMcpWorldBrushTool::River);
            })
            .Content()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                  .AutoHeight()
                  .Padding(0.0f, 2.0f)
                  [
                      McpBrushLabeledRow(
                          LOCTEXT("BrushRiverWidth", "Channel width: "),
                          SNew(SSpinBox<double>)
                              .MinValue(100.0)
                              .MaxValue(10000.0)
                              .Value(Settings.RiverWidth)
                              .OnValueChanged_Lambda([](double NewValue)
                                                     {
                                  McpWorldBrushGetSettings().RiverWidth = NewValue;
                              }))
                  ]
                + SVerticalBox::Slot()
                  .AutoHeight()
                  .Padding(0.0f, 2.0f)
                  [
                      McpBrushLabeledRow(
                          LOCTEXT("BrushRiverDepth", "Channel depth: "),
                          SNew(SSpinBox<double>)
                              .MinValue(0.0)
                              .MaxValue(5000.0)
                              .Value(Settings.RiverDepth)
                              .OnValueChanged_Lambda([](double NewValue)
                                                     {
                                  McpWorldBrushGetSettings().RiverDepth = NewValue;
                              }))
                  ]
                + SVerticalBox::Slot()
                  .AutoHeight()
                  .Padding(0.0f, 2.0f)
                  [
                      McpBrushLabeledRow(
                          LOCTEXT("BrushRiverWater", "Create water: "),
                          SNew(SCheckBox)
                              .IsChecked(Settings.bCreateRiverWater ? ECheckBoxState::Checked
                                                                    : ECheckBoxState::Unchecked)
                              .OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
                                                          {
                                  McpWorldBrushGetSettings().bCreateRiverWater =
                                      NewState == ECheckBoxState::Checked;
                              }))
                  ]
            ]
        ];

    // Road spline options --------------------------------------------------------
    Box->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            SNew(SBox)
            .Visibility_Lambda([]()
                               {
                return McpBrushToolSectionVisibility(EMcpWorldBrushTool::RoadSpline);
            })
            .Content()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                  .AutoHeight()
                  .Padding(0.0f, 2.0f)
                  [
                      SNew(STextBlock)
                      .Text(LOCTEXT("BrushRoadHint",
                                    "Click in the viewport to drop route points, then build the road."))
                      .AutoWrapText(true)
                  ]
                + SVerticalBox::Slot()
                  .AutoHeight()
                  .Padding(0.0f, 2.0f)
                  [
                      McpBrushLabeledRow(
                          LOCTEXT("BrushRoadWidth", "Road width: "),
                          SNew(SSpinBox<double>)
                              .MinValue(100.0)
                              .MaxValue(10000.0)
                              .Value(Settings.RoadWidth)
                              .OnValueChanged_Lambda([](double NewValue)
                                                     {
                                  McpWorldBrushGetSettings().RoadWidth = NewValue;
                              }))
                  ]
                + SVerticalBox::Slot()
                  .AutoHeight()
                  .Padding(0.0f, 2.0f)
                  [
                      McpBrushLabeledRow(
                          LOCTEXT("BrushRoadShoulder", "Shoulder: "),
                          SNew(SSpinBox<double>)
                              .MinValue(0.0)
                              .MaxValue(5000.0)
                              .Value(Settings.ShoulderWidth)
                              .OnValueChanged_Lambda([](double NewValue)
                                                     {
                                  McpWorldBrushGetSettings().ShoulderWidth = NewValue;
                              }))
                  ]
                + SVerticalBox::Slot()
                  .AutoHeight()
                  .Padding(0.0f, 4.0f)
                  [
                      SNew(SButton)
                      .Text_Lambda([]()
                                   {
                          return FText::FromString(FString::Printf(
                              TEXT("Build Road (%d points)"),
                              McpWorldBrushGetRoadDraft().Points.Num()));
                      })
                      .OnClicked_Static(&McpBrushBuildRoadFromDraft)
                      .HAlign(HAlign_Center)
                  ]
                + SVerticalBox::Slot()
                  .AutoHeight()
                  .Padding(0.0f, 2.0f)
                  [
                      SNew(SButton)
                      .Text(LOCTEXT("BrushRoadClear", "Clear Points"))
                      .OnClicked_Static(&McpBrushClearRoadDraft)
                      .HAlign(HAlign_Center)
                  ]
            ]
        ];

    Box->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 6.0f, 0.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("BrushHint",
                          "Drag with left mouse in a viewport to paint strokes. River carves a channel and builds water on release; Road Spline drops points on click. Esc exits brush mode."))
            .AutoWrapText(true)
        ];

    return Box;
}

#undef LOCTEXT_NAMESPACE
