// =============================================================================
// NebulaForgeAITabManager.h
// =============================================================================
// Dockable tab spawner and editor entry points for the AI chat window
// (plan section 3.1): Window menu entry, optional Level Editor toolbar
// button, and programmatic open/focus.
// =============================================================================

#pragma once

#include "CoreMinimal.h"

class FNebulaForgeAITabManager
{
public:
    static constexpr const TCHAR* TabName = TEXT("NebulaForgeAIChat");

    /** Register the nomad tab spawner (editor startup, game thread). */
    static void RegisterTabSpawner();

    /** Unregister the tab spawner (editor shutdown). */
    static void UnregisterTabSpawner();

    /** Open or focus the AI chat tab. */
    static void InvokeTab();

    /** True when the user enabled the optional toolbar button. */
    static bool IsToolbarButtonEnabled();
};
