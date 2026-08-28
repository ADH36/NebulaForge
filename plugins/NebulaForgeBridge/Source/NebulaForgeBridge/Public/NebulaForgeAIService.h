// =============================================================================
// NebulaForgeAIService.h
// =============================================================================
// Editor-only service facade owning the AI chat services (plan 4.2).
//
// FNebulaForgeAIService::Get() provides access to:
//   - Conversations()  : local conversation lifecycle + persistence
//   - Coordinator()    : provider request lifecycle
//   - Contexts()       : editor context collection
//   - Tools()          : permission-gated Unreal tool gateway
// The facade also owns the output-log ring buffer and the game-thread ticker
// that drains provider events.
// =============================================================================

#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"

class FNebulaForgeAIConversationService;
class FNebulaForgeAIRequestCoordinator;
class FNebulaForgeAIContextCollector;
class FNebulaForgeAIToolGateway;
class FNebulaAIOutputLogRingBuffer;

/** Ring buffer of recent output-log lines (definition in AI/NebulaForgeAIDiagnostics.h). */
extern TSharedPtr<FNebulaAIOutputLogRingBuffer> GNebulaAILogRingBuffer;

class FNebulaForgeAIService
{
public:
    static FNebulaForgeAIService& Get();

    FNebulaForgeAIConversationService* Conversations() const { return ConversationsPtr.Get(); }
    FNebulaForgeAIRequestCoordinator* Coordinator() const { return CoordinatorPtr.Get(); }
    FNebulaForgeAIContextCollector* Contexts() const { return ContextsPtr.Get(); }
    FNebulaForgeAIToolGateway* Tools() const { return ToolsPtr.Get(); }

    /** Called from module startup (editor, game thread). */
    void Initialize();

    /** Called from module shutdown; flushes conversations and detaches log. */
    void Shutdown();

private:
    FNebulaForgeAIService() = default;

    bool Tick(float DeltaTime);

    TSharedPtr<FNebulaForgeAIConversationService> ConversationsPtr;
    TSharedPtr<FNebulaForgeAIRequestCoordinator> CoordinatorPtr;
    TSharedPtr<FNebulaForgeAIContextCollector> ContextsPtr;
    TSharedPtr<FNebulaForgeAIToolGateway> ToolsPtr;

    FTSTicker::FDelegateHandle TickHandle;
    bool bInitialized = false;
};
