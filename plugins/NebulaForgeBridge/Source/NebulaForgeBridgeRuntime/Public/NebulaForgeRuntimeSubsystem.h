#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Sockets.h"
#include "NebulaForgeRuntimeSubsystem.generated.h"

struct FNebulaForgeRuntimeClient
{
    FSocket* Socket = nullptr;
    TArray<uint8> ReceiveBuffer;
    bool bHandshakeComplete = false;
    bool bAuthenticated = false;
};

/**
 * Packaged-game MCP bridge. This subsystem intentionally exposes only runtime-safe
 * inspection actions; editor asset authoring remains in NebulaForgeBridge.
 *
 * The bridge is opt-in. Enable it with [NebulaForgeBridgeRuntime] bEnabled=true
 * in the project game ini or NEBULAFORGE_RUNTIME_BRIDGE_ENABLED=true.
 */
UCLASS()
class NEBULAFORGEBRIDGERUNTIME_API UNebulaForgeRuntimeSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    bool StartServer();
    void StopServer();
    bool Tick(float DeltaSeconds);
    void AcceptClients();
    void ServiceClient(FNebulaForgeRuntimeClient& Client);
    bool CompleteWebSocketHandshake(FNebulaForgeRuntimeClient& Client);
    bool ProcessWebSocketFrames(FNebulaForgeRuntimeClient& Client);
    void HandleMessage(FNebulaForgeRuntimeClient& Client, const FString& Message);
    bool SendFrame(FNebulaForgeRuntimeClient& Client, uint8 Opcode, const TArray<uint8>& Payload);
    bool SendTextFrame(FNebulaForgeRuntimeClient& Client, const FString& Message);
    void CloseClient(FNebulaForgeRuntimeClient& Client);

    FSocket* ListenSocket = nullptr;
    TArray<FNebulaForgeRuntimeClient> Clients;
    FTSTicker::FDelegateHandle TickHandle;
    FString CapabilityToken;
    int32 ListenPort = 8092;
};
