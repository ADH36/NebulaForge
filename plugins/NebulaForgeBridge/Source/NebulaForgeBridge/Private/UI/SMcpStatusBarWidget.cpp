#include "UI/SMcpStatusBarWidget.h"
#include "AI/UI/NebulaForgeAITabManager.h"
#include "AI/UI/SNebulaForgeAISettingsWidget.h"
#include "NebulaForgeBridgeSubsystem.h"
#include "NebulaForgeBridgeSettings.h"
#include "MCP/McpNativeTransport.h"
#include "Editor.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "SMcpStatusBarWidget"

void SMcpStatusBarWidget::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SButton)
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
		.OnClicked_Raw(this, &SMcpStatusBarWidget::OnClicked)
		.ToolTipText(FText::FromString(TEXT("NebulaForge Bridge: click to open bridge settings or the AI chat")))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f, 2.f)
			[
				SNew(SImage)
				.ColorAndOpacity_Raw(this, &SMcpStatusBarWidget::GetStatusColor)
				.Image(FAppStyle::Get().GetBrush("Icons.FilledCircle"))
				.DesiredSizeOverride(FVector2D(8.0, 8.0))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.f, 0.f, 4.f)
			[
				SNew(STextBlock)
				.Text_Raw(this, &SMcpStatusBarWidget::GetStatusText)
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]
	];
}

FReply SMcpStatusBarWidget::OnClicked()
{
	// Open a small context menu with bridge settings plus the AI chat
	// entry points (plan section 3.1 status bar actions).
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("NebulaForgeAI", "OpenAIChat", "Open AI Chat"),
		NSLOCTEXT("NebulaForgeAI", "OpenAIChatTooltip", "Open the dockable NebulaForge AI chat window."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Stats"),
		FUIAction(FExecuteAction::CreateStatic(&FNebulaForgeAITabManager::InvokeTab)));

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("NebulaForgeAI", "OpenAISettings", "Open AI Settings"),
		NSLOCTEXT("NebulaForgeAI", "OpenAISettingsTooltip", "Configure AI providers, models, permissions, and privacy."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Editor.Settings"),
		FUIAction(FExecuteAction::CreateStatic(&SNebulaForgeAISettingsWidget::OpenSettingsWindowStatic)));

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("NebulaForgeBridge", "OpenBridgeSettings", "Open Bridge Settings"),
		NSLOCTEXT("NebulaForgeBridge", "OpenBridgeSettingsTooltip", "Open NebulaForge Bridge project settings."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			ISettingsModule* SettingsModule = FModuleManager::LoadModulePtr<ISettingsModule>("Settings");
			if (SettingsModule)
			{
				SettingsModule->ShowViewer("Project", "Plugins", "NebulaForgeBridgeSettings");
			}
		})));

	FSlateApplication::Get().PushMenu(
		SharedThis(this),
		FWidgetPath(),
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

	return FReply::Handled();
}

FText SMcpStatusBarWidget::GetStatusText() const
{
	if (!GEditor) return FText::FromString(TEXT("MCP"));

	auto* Subsystem = GEditor->GetEditorSubsystem<UNebulaForgeBridgeSubsystem>();
	if (Subsystem && Subsystem->NativeTransport && Subsystem->NativeTransport->IsRunning())
	{
		const int32 Port = Subsystem->NativeTransport->GetListenPort();
		const int32 Sessions = Subsystem->NativeTransport->GetActiveSessionCount();
		return FText::FromString(FString::Printf(TEXT("MCP :%d (%d)"), Port, Sessions));
	}

	return FText::FromString(TEXT("MCP off"));
}

FSlateColor SMcpStatusBarWidget::GetStatusColor() const
{
	if (!GEditor) return FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f));

	auto* Subsystem = GEditor->GetEditorSubsystem<UNebulaForgeBridgeSubsystem>();
	if (Subsystem && Subsystem->NativeTransport && Subsystem->NativeTransport->IsRunning())
	{
		return FSlateColor(FLinearColor(0.1f, 0.8f, 0.1f)); // Green
	}

	return FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f)); // Gray
}

#undef LOCTEXT_NAMESPACE
