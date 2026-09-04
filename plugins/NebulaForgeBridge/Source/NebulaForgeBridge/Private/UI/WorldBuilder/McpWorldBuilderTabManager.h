// =============================================================================
// McpWorldBuilderTabManager.h
// =============================================================================
// Registers the dockable "Generate World" editor tab that exposes the
// generate_world recipe orchestration through Slate, mirroring the AI chat
// tab manager registration pattern.
//
// Copyright (c) 2024 NebulaForge Bridge Contributors
// =============================================================================

#pragma once

#include "CoreMinimal.h"

class FMcpWorldBuilderTabManager
{
public:
    /** Registers the nomad tab spawner and Window menu entry. */
    static void RegisterTabSpawner();

    /** Unregisters the tab spawner and menu entries. */
    static void UnregisterTabSpawner();

    /** Opens the Generate World tab. */
    static void InvokeTab();

private:
    static const FName TabName;
};
