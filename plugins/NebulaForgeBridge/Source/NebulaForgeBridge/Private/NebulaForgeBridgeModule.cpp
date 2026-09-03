#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "NebulaForgeBridgeSettings.h"

#include "AI/UI/NebulaForgeAITabManager.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "NebulaForgeAIService.h"
#include "ToolMenus.h"
#include "UI/SMcpStatusBarWidget.h"
#include "UI/WorldBuilder/McpWorldBuilderTabManager.h"

// Save current LOCTEXT_NAMESPACE if defined, then set our own
#pragma push_macro("LOCTEXT_NAMESPACE")
#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "FNebulaForgeBridgeModule"

DEFINE_LOG_CATEGORY_STATIC(LogNebulaForgeBridge, Log, All);

class FNebulaForgeBridgeModule final : public IModuleInterface
{
public:
    /**
     * @brief Initializes the NebulaForge Bridge module.
     *
     * Performs module startup tasks required by the plugin. In editor builds, it records that
     * UNebulaForgeBridgeSettings are exposed via the Project Settings UI, initializes the AI
     * chat services, and registers the dockable AI chat tab.
     */
    virtual void StartupModule() override
    {
        UE_LOG(LogNebulaForgeBridge, Log, TEXT("NebulaForge Bridge module initialized."));

#if WITH_EDITOR
        // UDeveloperSettings (UNebulaForgeBridgeSettings) are auto-registered with the
        // Project Settings UI. Do not manually register them via ISettingsModule as this
        // produces duplicate entries in Project Settings. The settings class saves
        // automatically in PostEditChangeProperty.
        UE_LOG(LogNebulaForgeBridge, Verbose, TEXT("UNebulaForgeBridgeSettings are exposed via Project Settings (auto-registered)."));

        // AI chat feature: services first (conversations, coordinator,
        // context collector, tool gateway), then the dockable tab + menus.
        FNebulaForgeAIService::Get().Initialize();
        FNebulaForgeAITabManager::RegisterTabSpawner();

        // World-builder recipe panel (generate_world orchestration UI).
        FMcpWorldBuilderTabManager::RegisterTabSpawner();

        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
            this, &FNebulaForgeBridgeModule::RegisterStatusBarWidget));
#endif
    }

    /**
     * @brief Shuts down the NebulaForge Bridge module.
     *
     * Logs a shutdown message, unregisters the AI chat tab/services, and does
     * not attempt to unregister project settings because UDeveloperSettings
     * instances are managed by the engine.
     */
    virtual void ShutdownModule() override
    {
        UE_LOG(LogNebulaForgeBridge, Log, TEXT("NebulaForge Bridge module shut down."));

#if WITH_EDITOR
        FMcpWorldBuilderTabManager::UnregisterTabSpawner();
        FNebulaForgeAITabManager::UnregisterTabSpawner();
        FNebulaForgeAIService::Get().Shutdown();

        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
#endif
    }

    /**
     * @brief Persists UNebulaForgeBridgeSettings to DefaultGame.ini when project settings are modified.
     *
     * Saves the mutable default UNebulaForgeBridgeSettings to disk and logs the save action if the settings object is available.
     *
     * @return `true` if the settings object was found and saved, `false` otherwise.
     */
    bool HandleSettingsModified()
    {
        if (UNebulaForgeBridgeSettings* Settings = GetMutableDefault<UNebulaForgeBridgeSettings>())
        {
            Settings->SaveConfig();
            UE_LOG(LogNebulaForgeBridge, Log, TEXT("NebulaForge Bridge settings saved to DefaultGame.ini"));
            return true;
        }
        return false;
    }

private:
    void RegisterStatusBarWidget()
    {
        FToolMenuOwnerScoped OwnerScoped(this);
        UToolMenu* StatusBar = UToolMenus::Get()->ExtendMenu("LevelEditor.StatusBar.ToolBar");
        if (!StatusBar) return;

        FToolMenuSection& Section = StatusBar->AddSection(
            "McpStatus", FText::GetEmpty(),
            FToolMenuInsert("SourceControl", EToolMenuInsertType::Before));

        Section.AddEntry(FToolMenuEntry::InitWidget(
            "McpStatusWidget",
            SNew(SMcpStatusBarWidget),
            FText::GetEmpty(),
            true, false));
    }

    // Hold the registered settings section so we can unbind and unregister it cleanly
    TSharedPtr<class ISettingsSection> SettingsSection;
};

// Restore the previous LOCTEXT_NAMESPACE
#pragma pop_macro("LOCTEXT_NAMESPACE")

IMPLEMENT_MODULE(FNebulaForgeBridgeModule, NebulaForgeBridge)
