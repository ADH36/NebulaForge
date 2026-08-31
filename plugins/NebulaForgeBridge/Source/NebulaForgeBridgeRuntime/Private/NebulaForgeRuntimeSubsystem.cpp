#include "NebulaForgeRuntimeSubsystem.h"

#include "Async/Async.h"
#include "Containers/StringConv.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Misc/App.h"
#include "Misc/Base64.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SocketSubsystem.h"

namespace
{
constexpr uint32 MaxMessageBytes = 1024 * 1024;
constexpr TCHAR WebSocketGuid[] = TEXT("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

FString BytesToString(const TArray<uint8>& Bytes)
{
    if (Bytes.Num() == 0)
    {
        return FString();
    }
    const ANSICHAR* Data = reinterpret_cast<const ANSICHAR*>(Bytes.GetData());
    FUTF8ToTCHAR Converter(Data, Bytes.Num());
    return FString(Converter.Length(), Converter.Get());
}

bool SendAll(FSocket* Socket, const uint8* Data, int32 Length)
{
    int32 Offset = 0;
    while (Offset < Length)
    {
        int32 Sent = 0;
        if (!Socket->Send(Data + Offset, Length - Offset, Sent) || Sent <= 0)
        {
            return false;
        }
        Offset += Sent;
    }
    return true;
}

FString JsonString(const TSharedRef<FJsonObject>& Object)
{
    FString Output;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Object, Writer);
    return Output;
}
}

void UNebulaForgeRuntimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    bool bEnabled = false;
    if (GConfig)
    {
        GConfig->GetBool(TEXT("/Script/NebulaForgeBridgeRuntime.NebulaForgeRuntimeSettings"), TEXT("bEnabled"), bEnabled, GGameIni);
        GConfig->GetInt(TEXT("/Script/NebulaForgeBridgeRuntime.NebulaForgeRuntimeSettings"), TEXT("ListenPort"), ListenPort, GGameIni);
        GConfig->GetString(TEXT("/Script/NebulaForgeBridgeRuntime.NebulaForgeRuntimeSettings"), TEXT("CapabilityToken"), CapabilityToken, GGameIni);
    }

    const FString EnabledEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("NEBULAFORGE_RUNTIME_BRIDGE_ENABLED"));
    if (EnabledEnv.Equals(TEXT("1"), ESearchCase::IgnoreCase) || EnabledEnv.Equals(TEXT("true"), ESearchCase::IgnoreCase))
    {
        bEnabled = true;
    }
    const FString EnvPort = FPlatformMisc::GetEnvironmentVariable(TEXT("NEBULAFORGE_RUNTIME_BRIDGE_PORT"));
    if (!EnvPort.IsEmpty())
    {
        ListenPort = FCString::Atoi(*EnvPort);
    }
    const FString EnvToken = FPlatformMisc::GetEnvironmentVariable(TEXT("NEBULAFORGE_RUNTIME_CAPABILITY_TOKEN"));
    if (!EnvToken.IsEmpty())
    {
        CapabilityToken = EnvToken;
    }

    if (bEnabled && ListenPort >= 1024 && ListenPort <= 65535 && StartServer())
    {
        TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UNebulaForgeRuntimeSubsystem::Tick), 0.0f);
    }
}

void UNebulaForgeRuntimeSubsystem::Deinitialize()
{
    if (TickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        TickHandle.Reset();
    }
    StopServer();
    Super::Deinitialize();
}

bool UNebulaForgeRuntimeSubsystem::StartServer()
{
    ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!Sockets)
    {
        return false;
    }
    ListenSocket = Sockets->CreateSocket(NAME_Stream, TEXT("NebulaForgeRuntimeBridge"), false);
    if (!ListenSocket)
    {
        return false;
    }
    ListenSocket->SetReuseAddr(true);
    ListenSocket->SetNonBlocking(true);
    TSharedRef<FInternetAddr> Address = Sockets->CreateInternetAddr();
    bool bValidIp = false;
    Address->SetIp(TEXT("127.0.0.1"), bValidIp);
    Address->SetPort(ListenPort);
    if (!bValidIp || !ListenSocket->Bind(*Address) || !ListenSocket->Listen(8))
    {
        Sockets->DestroySocket(ListenSocket);
        ListenSocket = nullptr;
        return false;
    }
    return true;
}

void UNebulaForgeRuntimeSubsystem::StopServer()
{
    ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    for (FNebulaForgeRuntimeClient& Client : Clients)
    {
        if (Client.Socket)
        {
            Client.Socket->Close();
            if (Sockets)
            {
                Sockets->DestroySocket(Client.Socket);
            }
            Client.Socket = nullptr;
        }
    }
    Clients.Reset();
    if (ListenSocket)
    {
        ListenSocket->Close();
        if (Sockets)
        {
            Sockets->DestroySocket(ListenSocket);
        }
        ListenSocket = nullptr;
    }
}

bool UNebulaForgeRuntimeSubsystem::Tick(float DeltaSeconds)
{
    AcceptClients();
    for (int32 Index = Clients.Num() - 1; Index >= 0; --Index)
    {
        if (Clients[Index].Socket)
        {
            ServiceClient(Clients[Index]);
        }
        if (!Clients[Index].Socket)
        {
            Clients.RemoveAtSwap(Index);
        }
    }
    return true;
}

void UNebulaForgeRuntimeSubsystem::AcceptClients()
{
    if (!ListenSocket)
    {
        return;
    }
    ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!Sockets)
    {
        return;
    }
    bool bHasPendingConnection = false;
    while (ListenSocket->HasPendingConnection(bHasPendingConnection) && bHasPendingConnection)
    {
        FSocket* ClientSocket = ListenSocket->Accept(TEXT("NebulaForgeRuntimeClient"));
        if (!ClientSocket)
        {
            break;
        }
        ClientSocket->SetNonBlocking(true);
        FNebulaForgeRuntimeClient& Client = Clients.AddDefaulted_GetRef();
        Client.Socket = ClientSocket;
    }
}

void UNebulaForgeRuntimeSubsystem::ServiceClient(FNebulaForgeRuntimeClient& Client)
{
    uint32 Pending = 0;
    while (Client.Socket && Client.Socket->HasPendingData(Pending) && Pending > 0)
    {
        const int32 ReadSize = FMath::Min<uint32>(Pending, 64 * 1024);
        const int32 OldNum = Client.ReceiveBuffer.Num();
        Client.ReceiveBuffer.AddUninitialized(ReadSize);
        int32 BytesRead = 0;
        if (!Client.Socket->Recv(Client.ReceiveBuffer.GetData() + OldNum, ReadSize, BytesRead) || BytesRead <= 0)
        {
            Client.ReceiveBuffer.SetNum(OldNum);
            CloseClient(Client);
            return;
        }
        Client.ReceiveBuffer.SetNum(OldNum + BytesRead);
        if (Client.ReceiveBuffer.Num() > static_cast<int32>(MaxMessageBytes * 2))
        {
            CloseClient(Client);
            return;
        }
    }

    if (!Client.bHandshakeComplete && !CompleteWebSocketHandshake(Client))
    {
        return;
    }
    if (Client.bHandshakeComplete)
    {
        ProcessWebSocketFrames(Client);
    }
}

bool UNebulaForgeRuntimeSubsystem::CompleteWebSocketHandshake(FNebulaForgeRuntimeClient& Client)
{
    const FString Request = BytesToString(Client.ReceiveBuffer);
    const int32 HeaderEnd = Request.Find(TEXT("\r\n\r\n"));
    if (HeaderEnd == INDEX_NONE)
    {
        return true;
    }
    FString Key;
    TArray<FString> Lines;
    Request.Left(HeaderEnd).ParseIntoArrayLines(Lines, false);
    for (const FString& Line : Lines)
    {
        FString Name;
        FString Value;
        if (Line.Split(TEXT(":"), &Name, &Value))
        {
            if (Name.TrimStartAndEnd().Equals(TEXT("Sec-WebSocket-Key"), ESearchCase::IgnoreCase))
            {
                Key = Value.TrimStartAndEnd();
            }
        }
    }
    if (Key.IsEmpty())
    {
        CloseClient(Client);
        return false;
    }
    const FString HashInput = Key + FString(WebSocketGuid);
    FTCHARToUTF8 Utf8(*HashInput);
    uint8 Digest[FSHA1::DigestSize];
    FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Digest);
    const FString Accept = FBase64::Encode(Digest, FSHA1::DigestSize);
    const FString Response = FString::Printf(TEXT("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n"), *Accept);
    FTCHARToUTF8 ResponseUtf8(*Response);
    if (!SendAll(Client.Socket, reinterpret_cast<const uint8*>(ResponseUtf8.Get()), ResponseUtf8.Length()))
    {
        CloseClient(Client);
        return false;
    }
    const FTCHARToUTF8 HeaderUtf8(*FString::Printf(TEXT("%s"), *Request.Left(HeaderEnd + 4)));
    Client.ReceiveBuffer.RemoveAt(0, HeaderUtf8.Length(), EAllowShrinking::No);
    Client.bHandshakeComplete = true;
    return true;
}

bool UNebulaForgeRuntimeSubsystem::ProcessWebSocketFrames(FNebulaForgeRuntimeClient& Client)
{
    while (Client.ReceiveBuffer.Num() >= 2)
    {
        const uint8* Data = Client.ReceiveBuffer.GetData();
        const uint8 Opcode = Data[0] & 0x0F;
        const bool bMasked = (Data[1] & 0x80) != 0;
        uint64 PayloadLength = Data[1] & 0x7F;
        int32 HeaderLength = 2;
        if (PayloadLength == 126)
        {
            if (Client.ReceiveBuffer.Num() < 4) return true;
            PayloadLength = (static_cast<uint64>(Data[2]) << 8) | Data[3];
            HeaderLength = 4;
        }
        else if (PayloadLength == 127)
        {
            if (Client.ReceiveBuffer.Num() < 10) return true;
            PayloadLength = 0;
            for (int32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
            {
                PayloadLength = (PayloadLength << 8) | Data[2 + ByteIndex];
            }
            HeaderLength = 10;
        }
        const int32 MaskLength = bMasked ? 4 : 0;
        if (PayloadLength > MaxMessageBytes || Client.ReceiveBuffer.Num() < HeaderLength + MaskLength + static_cast<int32>(PayloadLength))
        {
            if (PayloadLength > MaxMessageBytes) CloseClient(Client);
            return Client.Socket != nullptr;
        }
        const uint8* Mask = bMasked ? Data + HeaderLength : nullptr;
        const int32 PayloadOffset = HeaderLength + MaskLength;
        TArray<uint8> Payload;
        Payload.Append(Data + PayloadOffset, static_cast<int32>(PayloadLength));
        if (Mask)
        {
            for (int32 Index = 0; Index < Payload.Num(); ++Index) Payload[Index] ^= Mask[Index % 4];
        }
        Client.ReceiveBuffer.RemoveAt(0, PayloadOffset + Payload.Num(), EAllowShrinking::No);
        if (Opcode == 0x8)
        {
            CloseClient(Client);
            return false;
        }
        if (Opcode == 0x9)
        {
            SendFrame(Client, 0xA, Payload);
        }
        else if (Opcode == 0x1)
        {
            HandleMessage(Client, BytesToString(Payload));
        }
        if (!Client.Socket) return false;
    }
    return true;
}

void UNebulaForgeRuntimeSubsystem::HandleMessage(FNebulaForgeRuntimeClient& Client, const FString& Message)
{
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        return;
    }
    FString Type;
    Root->TryGetStringField(TEXT("type"), Type);
    if (Type.Equals(TEXT("bridge_hello"), ESearchCase::IgnoreCase))
    {
        FString ReceivedToken;
        Root->TryGetStringField(TEXT("capabilityToken"), ReceivedToken);
        if (!CapabilityToken.IsEmpty() && !ReceivedToken.Equals(CapabilityToken, ESearchCase::CaseSensitive))
        {
            TSharedRef<FJsonObject> Error = MakeShared<FJsonObject>();
            Error->SetStringField(TEXT("type"), TEXT("bridge_error"));
            Error->SetStringField(TEXT("error"), TEXT("CAPABILITY_TOKEN_INVALID"));
            SendTextFrame(Client, JsonString(Error));
            CloseClient(Client);
            return;
        }
        Client.bAuthenticated = true;
        TSharedRef<FJsonObject> Ack = MakeShared<FJsonObject>();
        Ack->SetStringField(TEXT("type"), TEXT("bridge_ack"));
        Ack->SetStringField(TEXT("serverName"), TEXT("NebulaForgeRuntime"));
        Ack->SetStringField(TEXT("serverVersion"), TEXT("0.5.30"));
        Ack->SetStringField(TEXT("executionMode"), TEXT("runtime"));
        Ack->SetBoolField(TEXT("editorAutomation"), false);
        Ack->SetBoolField(TEXT("pieRuntimeWorld"), true);
        Ack->SetBoolField(TEXT("packagedRuntimeAuthoring"), false);
        Ack->SetNumberField(TEXT("protocolVersion"), 1);
        TArray<TSharedPtr<FJsonValue>> Capabilities;
        Capabilities.Add(MakeShared<FJsonValueString>(TEXT("runtime_health")));
        Capabilities.Add(MakeShared<FJsonValueString>(TEXT("get_runtime_capabilities")));
        Capabilities.Add(MakeShared<FJsonValueString>(TEXT("get_runtime_world")));
        Capabilities.Add(MakeShared<FJsonValueString>(TEXT("get_runtime_actors")));
        Capabilities.Add(MakeShared<FJsonValueString>(TEXT("get_runtime_actor")));
        Ack->SetArrayField(TEXT("capabilities"), Capabilities);
        SendTextFrame(Client, JsonString(Ack));
        return;
    }
    if (!Type.Equals(TEXT("automation_request"), ESearchCase::IgnoreCase) || !Client.bAuthenticated)
    {
        return;
    }
    FString RequestId;
    FString Action;
    Root->TryGetStringField(TEXT("requestId"), RequestId);
    Root->TryGetStringField(TEXT("action"), Action);
    TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetStringField(TEXT("type"), TEXT("automation_response"));
    Response->SetStringField(TEXT("requestId"), RequestId);
    Response->SetStringField(TEXT("action"), Action);
    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    bool bSuccess = true;
    FString Error;
    if (Action.Equals(TEXT("runtime_health"), ESearchCase::IgnoreCase))
    {
        Result->SetStringField(TEXT("executionMode"), TEXT("runtime"));
        Result->SetStringField(TEXT("platform"), FPlatformProperties::PlatformName());
        Result->SetStringField(TEXT("projectName"), FApp::GetProjectName());
        Result->SetNumberField(TEXT("frameCounter"), static_cast<double>(GFrameCounter));
        Result->SetNumberField(TEXT("timeSeconds"), FPlatformTime::Seconds());
    }
    else if (Action.Equals(TEXT("get_runtime_capabilities"), ESearchCase::IgnoreCase))
    {
        TArray<TSharedPtr<FJsonValue>> Actions;
        Actions.Add(MakeShared<FJsonValueString>(TEXT("runtime_health")));
        Actions.Add(MakeShared<FJsonValueString>(TEXT("get_runtime_capabilities")));
        Actions.Add(MakeShared<FJsonValueString>(TEXT("get_runtime_world")));
        Actions.Add(MakeShared<FJsonValueString>(TEXT("get_runtime_actors")));
        Actions.Add(MakeShared<FJsonValueString>(TEXT("get_runtime_actor")));
        Result->SetArrayField(TEXT("actions"), Actions);
        Result->SetBoolField(TEXT("packagedRuntimeAuthoring"), false);
    }
    else if (Action.Equals(TEXT("get_runtime_world"), ESearchCase::IgnoreCase))
    {
        UWorld* World = GetWorld();
        if (!World)
        {
            bSuccess = false;
            Error = TEXT("WORLD_UNAVAILABLE");
        }
        else
        {
            Result->SetStringField(TEXT("worldName"), World->GetName());
            Result->SetNumberField(TEXT("netMode"), static_cast<int32>(World->GetNetMode()));
            Result->SetBoolField(TEXT("isGameWorld"), World->IsGameWorld());
        }
    }
    else if (Action.Equals(TEXT("get_runtime_actors"), ESearchCase::IgnoreCase))
    {
        UWorld* World = GetWorld();
        if (!World)
        {
            bSuccess = false;
            Error = TEXT("WORLD_UNAVAILABLE");
        }
        else
        {
            const TSharedPtr<FJsonObject>* PayloadObject = nullptr;
            Root->TryGetObjectField(TEXT("payload"), PayloadObject);
            int32 Limit = 100;
            FString ClassFilter;
            if (PayloadObject && PayloadObject->IsValid())
            {
                (*PayloadObject)->TryGetNumberField(TEXT("limit"), Limit);
                (*PayloadObject)->TryGetStringField(TEXT("classFilter"), ClassFilter);
            }
            Limit = FMath::Clamp(Limit, 1, 500);
            ClassFilter = ClassFilter.TrimStartAndEnd();

            TArray<TSharedPtr<FJsonValue>> Actors;
            for (TActorIterator<AActor> Iterator(World); Iterator && Actors.Num() < Limit; ++Iterator)
            {
                AActor* Actor = *Iterator;
                if (!IsValid(Actor) || (!ClassFilter.IsEmpty() && !Actor->GetClass()->GetName().Contains(ClassFilter)))
                {
                    continue;
                }

                TSharedRef<FJsonObject> ActorResult = MakeShared<FJsonObject>();
                ActorResult->SetStringField(TEXT("name"), Actor->GetName());
                ActorResult->SetStringField(TEXT("class"), Actor->GetClass()->GetPathName());
                ActorResult->SetBoolField(TEXT("hidden"), Actor->IsHidden());
                ActorResult->SetBoolField(TEXT("pendingKill"), Actor->IsPendingKillPending());
                const FVector Location = Actor->GetActorLocation();
                ActorResult->SetNumberField(TEXT("x"), Location.X);
                ActorResult->SetNumberField(TEXT("y"), Location.Y);
                ActorResult->SetNumberField(TEXT("z"), Location.Z);
                Actors.Add(MakeShared<FJsonValueObject>(ActorResult));
            }
            Result->SetArrayField(TEXT("actors"), Actors);
            Result->SetNumberField(TEXT("count"), Actors.Num());
            Result->SetNumberField(TEXT("limit"), Limit);
            Result->SetStringField(TEXT("classFilter"), ClassFilter);
        }
    }
    else if (Action.Equals(TEXT("get_runtime_actor"), ESearchCase::IgnoreCase))
    {
        UWorld* World = GetWorld();
        FString ActorName;
        const TSharedPtr<FJsonObject>* PayloadObject = nullptr;
        Root->TryGetObjectField(TEXT("payload"), PayloadObject);
        if (PayloadObject && PayloadObject->IsValid())
        {
            (*PayloadObject)->TryGetStringField(TEXT("actorName"), ActorName);
        }
        ActorName = ActorName.TrimStartAndEnd();
        if (!World)
        {
            bSuccess = false;
            Error = TEXT("WORLD_UNAVAILABLE");
        }
        else if (ActorName.IsEmpty())
        {
            bSuccess = false;
            Error = TEXT("ACTOR_NAME_REQUIRED");
        }
        else
        {
            AActor* FoundActor = nullptr;
            for (TActorIterator<AActor> Iterator(World); Iterator; ++Iterator)
            {
                if (*Iterator && (*Iterator)->GetName().Equals(ActorName, ESearchCase::CaseSensitive))
                {
                    FoundActor = *Iterator;
                    break;
                }
            }
            if (!FoundActor)
            {
                bSuccess = false;
                Error = TEXT("ACTOR_NOT_FOUND");
            }
            else
            {
                Result->SetStringField(TEXT("name"), FoundActor->GetName());
                Result->SetStringField(TEXT("class"), FoundActor->GetClass()->GetPathName());
                Result->SetBoolField(TEXT("hidden"), FoundActor->IsHidden());
                const FVector Location = FoundActor->GetActorLocation();
                const FRotator Rotation = FoundActor->GetActorRotation();
                Result->SetNumberField(TEXT("locationX"), Location.X);
                Result->SetNumberField(TEXT("locationY"), Location.Y);
                Result->SetNumberField(TEXT("locationZ"), Location.Z);
                Result->SetNumberField(TEXT("rotationPitch"), Rotation.Pitch);
                Result->SetNumberField(TEXT("rotationYaw"), Rotation.Yaw);
                Result->SetNumberField(TEXT("rotationRoll"), Rotation.Roll);
            }
        }
    }
    else
    {
        bSuccess = false;
        Error = TEXT("RUNTIME_ACTION_UNSUPPORTED");
    }
    Response->SetBoolField(TEXT("success"), bSuccess);
    Response->SetStringField(TEXT("error"), Error);
    if (bSuccess) Response->SetObjectField(TEXT("result"), Result);
    SendTextFrame(Client, JsonString(Response));
}

bool UNebulaForgeRuntimeSubsystem::SendFrame(FNebulaForgeRuntimeClient& Client, uint8 Opcode, const TArray<uint8>& Payload)
{
    if (!Client.Socket || Payload.Num() > static_cast<int32>(MaxMessageBytes)) return false;
    TArray<uint8> Frame;
    Frame.Add(0x80 | Opcode);
    if (Payload.Num() < 126)
    {
        Frame.Add(static_cast<uint8>(Payload.Num()));
    }
    else if (Payload.Num() <= MAX_uint16)
    {
        Frame.Add(126);
        Frame.Add(static_cast<uint8>((Payload.Num() >> 8) & 0xFF));
        Frame.Add(static_cast<uint8>(Payload.Num() & 0xFF));
    }
    else
    {
        Frame.Add(127);
        const uint64 Size = static_cast<uint64>(Payload.Num());
        for (int32 Index = 7; Index >= 0; --Index) Frame.Add(static_cast<uint8>((Size >> (Index * 8)) & 0xFF));
    }
    Frame.Append(Payload);
    if (!SendAll(Client.Socket, Frame.GetData(), Frame.Num()))
    {
        CloseClient(Client);
        return false;
    }
    return true;
}

bool UNebulaForgeRuntimeSubsystem::SendTextFrame(FNebulaForgeRuntimeClient& Client, const FString& Message)
{
    FTCHARToUTF8 Utf8(*Message);
    TArray<uint8> Payload;
    Payload.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
    return SendFrame(Client, 0x1, Payload);
}

void UNebulaForgeRuntimeSubsystem::CloseClient(FNebulaForgeRuntimeClient& Client)
{
    if (!Client.Socket) return;
    ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    Client.Socket->Close();
    if (Sockets) Sockets->DestroySocket(Client.Socket);
    Client.Socket = nullptr;
}
