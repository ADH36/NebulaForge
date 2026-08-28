#include "AI/UI/NebulaForgeAITabManager.h"
#include "AI/NebulaForgeAISecretStore.h"
#include "AI/NebulaForgeAISettings.h"
#include "AI/UI/SNebulaForgeAIChat.h"
#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "NebulaForgeAITabManager"

namespace
{
    const FName MenuOwner(TEXT("NebulaForgeAIChat"));
    void RegisterMenuEntries();

    /** Tiny registrar object so the startup callback can be unregistered by
     *  pointer on every supported UE version (5.0-5.8). */
    struct FMenuStartupRegistrar
    {
        void Register() { RegisterMenuEntries(); }
    };
    FMenuStartupRegistrar MenuStartupRegistrar;

    TSharedRef<SDockTab> SpawnNebulaForgeAIChatTab(const FSpawnTabArgs& /*Args*/)
    {
        return SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            .Label(NSLOCTEXT("NebulaForgeAI", "ChatTabTitle", "NebulaForge AI"))
            [
                SNew(SNebulaForgeAIChat)
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

        // Window -> NebulaForge AI Chat
        UToolMenu* WindowMenu = ToolMenus->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"));
        if (WindowMenu)
        {
            FToolMenuSection& Section = WindowMenu->FindOrAddSection(TEXT("WindowLayoutSection"));
            Section.AddEntry(FToolMenuEntry::InitMenuEntry(
                TEXT("NebulaForgeAIChatTabEntry"),
                NSLOCTEXT("NebulaForgeAI", "OpenAIChatLabel", "NebulaForge AI Chat"),
                NSLOCTEXT("NebulaForgeAI", "OpenAIChatTooltip",
                    "Open the dockable NebulaForge AI chat window."),
                FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Stats"),
                FToolUIActionChoice(FUIAction(FExecuteAction::CreateStatic(&FNebulaForgeAITabManager::InvokeTab)))));
        }

        // Optional Level Editor toolbar button (behind a plugin setting).
        const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
        if (Settings && Settings->bShowToolbarButton)
        {
            UToolMenu* Toolbar = ToolMenus->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar"));
            if (Toolbar)
            {
                FToolMenuSection& Section = Toolbar->AddSection(
                    TEXT("NebulaForgeAI"), NSLOCTEXT("NebulaForgeAI", "SectionLabel", "NebulaForge AI"));
                Section.AddEntry(FToolMenuEntry::InitToolBarButton(
                    TEXT("NebulaForgeAIChatToolbarButton"),
                    FExecuteAction::CreateStatic(&FNebulaForgeAITabManager::InvokeTab),
                    NSLOCTEXT("NebulaForgeAI", "ToolbarLabel", "AI Chat"),
                    NSLOCTEXT("NebulaForgeAI", "ToolbarTooltip", "Open NebulaForge AI Chat"),
                    FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Stats")));
            }
        }
    }
}

void FNebulaForgeAITabManager::RegisterTabSpawner()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        TabName,
        FOnSpawnTab::CreateStatic(&SpawnNebulaForgeAIChatTab))
        .SetDisplayName(NSLOCTEXT("NebulaForgeAI", "TabDisplayName", "NebulaForge AI"))
        .SetTooltipText(NSLOCTEXT("NebulaForgeAI", "TabTooltip",
            "Chat with an AI about the current Unreal project."))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Stats"));

    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(&MenuStartupRegistrar, &FMenuStartupRegistrar::Register));
}

void FNebulaForgeAITabManager::UnregisterTabSpawner()
{
    if (UToolMenus::IsToolMenuUIEnabled())
    {
        UToolMenus::UnRegisterStartupCallback(&MenuStartupRegistrar);
        UToolMenus::Get()->UnregisterOwnerByName(MenuOwner);
    }
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
}

void FNebulaForgeAITabManager::InvokeTab()
{
    FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TabName));
}

bool FNebulaForgeAITabManager::IsToolbarButtonEnabled()
{
    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    return Settings && Settings->bShowToolbarButton;
}

#undef LOCTEXT_NAMESPACE
