// =============================================================================
// McpWorldBuilderTabManager.cpp
// =============================================================================

#include "UI/WorldBuilder/McpWorldBuilderTabManager.h"
#include "UI/WorldBuilder/SMcpWorldBuilderPanel.h"

#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "McpWorldBuilderTabManager"

namespace
{
    const FName MenuOwner(TEXT("McpWorldBuilder"));
    void RegisterMenuEntries();

    /** Tiny registrar object so the startup callback can be unregistered by
     *  pointer on every supported UE version (5.0-5.8). */
    struct FMenuStartupRegistrar
    {
        void Register() { RegisterMenuEntries(); }
    };
    FMenuStartupRegistrar MenuStartupRegistrar;

    TSharedRef<SDockTab> SpawnMcpWorldBuilderTab(const FSpawnTabArgs& /*Args*/)
    {
        return SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            .Label(NSLOCTEXT("McpWorldBuilder", "WorldBuilderTabTitle", "Generate World"))
            [
                SNew(SMcpWorldBuilderPanel)
            ];
    }

    void RegisterMenuEntries()
    {
        UToolMenus* ToolMenus = UToolMenus::Get();
        if (!ToolMenus)
        {
            return;
        }

        FToolMenuOwnerScoped OwnerScoped(MenuOwner);

        // Window -> Generate World
        UToolMenu* WindowMenu = ToolMenus->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"));
        if (WindowMenu)
        {
            FToolMenuSection& Section = WindowMenu->FindOrAddSection(TEXT("WindowLayoutSection"));
            Section.AddEntry(FToolMenuEntry::InitMenuEntry(
                TEXT("McpWorldBuilderTabEntry"),
                NSLOCTEXT("McpWorldBuilder", "OpenWorldBuilderLabel", "Generate World"),
                NSLOCTEXT("McpWorldBuilder", "OpenWorldBuilderTooltip",
                    "Open the world-builder panel to generate landscapes, terrain, and foliage from biome presets."),
                FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Stats"),
                FToolUIActionChoice(FUIAction(FExecuteAction::CreateStatic(&FMcpWorldBuilderTabManager::InvokeTab)))));
        }
    }
}

const FName FMcpWorldBuilderTabManager::TabName(TEXT("McpWorldBuilderPanel"));

void FMcpWorldBuilderTabManager::RegisterTabSpawner()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        TabName,
        FOnSpawnTab::CreateStatic(&SpawnMcpWorldBuilderTab))
        .SetDisplayName(NSLOCTEXT("McpWorldBuilder", "TabDisplayName", "Generate World"))
        .SetTooltipText(NSLOCTEXT("McpWorldBuilder", "TabTooltip",
            "Generate worlds from biome presets: landscape, terrain, rule-painted layers, and deterministic foliage."))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Stats"));

    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(&MenuStartupRegistrar, &FMenuStartupRegistrar::Register));
}

void FMcpWorldBuilderTabManager::UnregisterTabSpawner()
{
    if (UToolMenus::IsToolMenuUIEnabled())
    {
        UToolMenus::UnRegisterStartupCallback(&MenuStartupRegistrar);
        UToolMenus::Get()->UnregisterOwnerByName(MenuOwner);
    }
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
}

void FMcpWorldBuilderTabManager::InvokeTab()
{
    FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TabName));
}

#undef LOCTEXT_NAMESPACE
