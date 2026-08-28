#include "NebulaForgeAIService.h"
#include "AI/NebulaForgeAIConversationService.h"
#include "AI/NebulaForgeAIContextCollector.h"
#include "AI/NebulaForgeAIDiagnostics.h"
#include "AI/NebulaForgeAIRequestCoordinator.h"
#include "AI/NebulaForgeAISecretStore.h"
#include "AI/NebulaForgeAIToolGateway.h"
#include "Containers/Ticker.h"

TSharedPtr<FNebulaAIOutputLogRingBuffer> GNebulaAILogRingBuffer;

FNebulaForgeAIService& FNebulaForgeAIService::Get()
{
    static FNebulaForgeAIService Instance;
    return Instance;
}

void FNebulaForgeAIService::Initialize()
{
    if (bInitialized)
    {
        return;
    }
    bInitialized = true;

    ConversationsPtr = MakeShared<FNebulaForgeAIConversationService>();
    CoordinatorPtr = MakeShared<FNebulaForgeAIRequestCoordinator>();
    ContextsPtr = MakeShared<FNebulaForgeAIContextCollector>();
    ToolsPtr = MakeShared<FNebulaForgeAIToolGateway>();

    // Capture recent output-log lines for the optional context chip.
    GNebulaAILogRingBuffer = MakeShared<FNebulaAIOutputLogRingBuffer>();
    GLog->AddOutputDevice(GNebulaAILogRingBuffer.Get());

    ConversationsPtr->Initialize();

    TickHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FNebulaForgeAIService::Tick), 0.1f);

    UE_LOG(LogNebulaForgeAI, Log, TEXT("NebulaForge AI chat services initialized."));
}

void FNebulaForgeAIService::Shutdown()
{
    if (!bInitialized)
    {
        return;
    }

    if (TickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        TickHandle.Reset();
    }

    if (GNebulaAILogRingBuffer.IsValid() && GLog)
    {
        GLog->RemoveOutputDevice(GNebulaAILogRingBuffer.Get());
    }
    GNebulaAILogRingBuffer.Reset();

    if (ConversationsPtr.IsValid())
    {
        ConversationsPtr->Shutdown();
    }

    ConversationsPtr.Reset();
    CoordinatorPtr.Reset();
    ContextsPtr.Reset();
    ToolsPtr.Reset();
    bInitialized = false;

    UE_LOG(LogNebulaForgeAI, Log, TEXT("NebulaForge AI chat services shut down."));
}

bool FNebulaForgeAIService::Tick(float DeltaTime)
{
    if (CoordinatorPtr.IsValid())
    {
        CoordinatorPtr->Tick(DeltaTime);
    }
    return true;
}
