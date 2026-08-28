#include "AI/NebulaForgeAISecretStore.h"
#include "AI/NebulaForgeAIDiagnostics.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/Crc.h"
#include "Misc/Base64.h"
#include "Misc/SecureHash.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

#include <atomic>

DEFINE_LOG_CATEGORY(LogNebulaForgeAI);

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <wincred.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
    constexpr const TCHAR* CredentialPrefix = TEXT("NebulaForge/AI/Profile/");
    constexpr const TCHAR* FallbackDir = TEXT("NebulaForgeAI/");
    constexpr const TCHAR* FallbackFile = TEXT("Secrets.bin");

    std::atomic<bool> GbLastUsedFallback{false};

    void SetLastUsedFallback(bool bValue)
    {
        GbLastUsedFallback.store(bValue);
    }

#if PLATFORM_WINDOWS
    FString GetCredentialTarget(const FString& ProfileId)
    {
        return FString(CredentialPrefix) + ProfileId;
    }
#endif
}

FString FNebulaAISecretStore::MakeReferenceForProfile(const FString& ProfileId)
{
#if PLATFORM_WINDOWS
    return FString(TEXT("credman:")) + CredentialPrefix + ProfileId;
#else
    return FString(TEXT("fallback:")) + CredentialPrefix + ProfileId;
#endif
}

FString FNebulaAISecretStore::StoreSecret(const FString& ProfileId, const FString& Secret)
{
    if (ProfileId.IsEmpty() || Secret.IsEmpty())
    {
        return FString();
    }

#if PLATFORM_WINDOWS
    if (StoreCredentialManager(GetCredentialTarget(ProfileId), Secret))
    {
        SetLastUsedFallback(false);
        return MakeReferenceForProfile(ProfileId);
    }
    UE_LOG(LogNebulaForgeAI, Warning,
        TEXT("Credential Manager unavailable; using encrypted local fallback for profile %s."),
        *ProfileId);
#endif

    TMap<FString, FString> Map;
    LoadFallbackMap(Map);
    Map.Add(ProfileId, EncryptString(Secret, DeriveMachineKey()));
    if (SaveFallbackMap(Map))
    {
        SetLastUsedFallback(true);
        return MakeReferenceForProfile(ProfileId);
    }

    UE_LOG(LogNebulaForgeAI, Error, TEXT("Failed to store secret for profile %s."), *ProfileId);
    return FString();
}

bool FNebulaAISecretStore::TryGetSecret(const FString& ProfileId, FNebulaAIStoredSecret& OutSecret)
{
    OutSecret = FNebulaAIStoredSecret();
    if (ProfileId.IsEmpty())
    {
        return false;
    }

#if PLATFORM_WINDOWS
    FString Secret;
    if (ReadCredentialManager(GetCredentialTarget(ProfileId), Secret))
    {
        OutSecret.Secret = MoveTemp(Secret);
        OutSecret.bFromFallback = false;
        SetLastUsedFallback(false);
        return true;
    }
#endif

    TMap<FString, FString> Map;
    if (LoadFallbackMap(Map))
    {
        const FString* Cipher = Map.Find(ProfileId);
        if (Cipher)
        {
            OutSecret.Secret = DecryptString(*Cipher, DeriveMachineKey());
            OutSecret.bFromFallback = true;
            SetLastUsedFallback(true);
            return OutSecret.IsValid();
        }
    }
    return false;
}

bool FNebulaAISecretStore::HasSecret(const FString& ProfileId)
{
    FNebulaAIStoredSecret Temp;
    return TryGetSecret(ProfileId, Temp);
}

bool FNebulaAISecretStore::DeleteSecret(const FString& ProfileId)
{
    bool bDeleted = false;
#if PLATFORM_WINDOWS
    bDeleted = DeleteCredentialManager(GetCredentialTarget(ProfileId));
#endif
    TMap<FString, FString> Map;
    if (LoadFallbackMap(Map) && Map.Remove(ProfileId) > 0)
    {
        SaveFallbackMap(Map);
        bDeleted = true;
    }
    return bDeleted;
}

int32 FNebulaAISecretStore::ClearAll()
{
    int32 Removed = 0;

    TMap<FString, FString> Map;
    if (LoadFallbackMap(Map))
    {
        Removed += Map.Num();
        SaveFallbackMap(TMap<FString, FString>());
    }

    // Credential Manager entries are keyed by profile id; remove any whose
    // profile entry no longer exists by scanning known profile ids is not
    // possible without enumeration on older Windows, so per-profile deletes
    // are driven by profile deletion in the provider service.
    return Removed;
}

bool FNebulaAISecretStore::LastUsedFallback()
{
    return GbLastUsedFallback.load();
}

#if PLATFORM_WINDOWS
bool FNebulaAISecretStore::StoreCredentialManager(const FString& TargetName, const FString& Secret)
{
    const FTCHARToUTF8 Utf8Secret(*Secret);

    CREDENTIALW Credential = {};
    Credential.Type = CRED_TYPE_GENERIC;
    Credential.TargetName = const_cast<LPWSTR>(TCHAR_TO_WCHAR(*TargetName));
    Credential.CredentialBlobSize = static_cast<DWORD>(Utf8Secret.Length());
    Credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<ANSICHAR*>(Utf8Secret.Get()));
    Credential.Persist = CRED_PERSIST_LOCAL_MACHINE;

    return ::CredWriteW(&Credential, 0) != 0;
}

bool FNebulaAISecretStore::ReadCredentialManager(const FString& TargetName, FString& OutSecret)
{
    PCREDENTIALW Credential = nullptr;
    if (::CredReadW(TCHAR_TO_WCHAR(*TargetName), CRED_TYPE_GENERIC, 0, &Credential) == 0)
    {
        return false;
    }

    bool bOk = false;
    if (Credential->CredentialBlob && Credential->CredentialBlobSize > 0)
    {
        const ANSICHAR* Blob = reinterpret_cast<const ANSICHAR*>(Credential->CredentialBlob);
        const int32 Length = FMath::Min<int32>(Credential->CredentialBlobSize, 8192);
        // Blob is written as UTF-8 without a terminator.
        FUTF8ToTCHAR Convert(Blob, Length);
        OutSecret = FString(Convert.Length(), Convert.Get());
        bOk = !OutSecret.IsEmpty();
    }
    ::CredFree(Credential);
    return bOk;
}

bool FNebulaAISecretStore::DeleteCredentialManager(const FString& TargetName)
{
    return ::CredDeleteW(TCHAR_TO_WCHAR(*TargetName), CRED_TYPE_GENERIC, 0) != 0;
}
#endif

FString FNebulaAISecretStore::GetFallbackFilePath()
{
    return FPaths::ProjectSavedDir() + FallbackDir + FallbackFile;
}

bool FNebulaAISecretStore::LoadFallbackMap(TMap<FString, FString>& OutMap)
{
    OutMap.Reset();
    const FString Path = GetFallbackFilePath();
    if (!IFileManager::Get().FileExists(*Path))
    {
        return true;
    }

    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Path))
    {
        UE_LOG(LogNebulaForgeAI, Error, TEXT("Failed to read secret fallback file."));
        return false;
    }

    FMemoryReader Reader(Bytes);
    int32 Count = 0;
    Reader << Count;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        FString Key;
        FString Value;
        Reader << Key;
        Reader << Value;
        OutMap.Add(MoveTemp(Key), MoveTemp(Value));
    }
    return true;
}

bool FNebulaAISecretStore::SaveFallbackMap(const TMap<FString, FString>& Map)
{
    FBufferArchive Archive;
    int32 Count = Map.Num();
    Archive << Count;
    for (const TPair<FString, FString>& Pair : Map)
    {
        FString Key = Pair.Key;
        FString Value = Pair.Value;
        Archive << Key;
        Archive << Value;
    }

    const FString Path = GetFallbackFilePath();
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    return FFileHelper::SaveArrayToFile(Archive, *Path);
}

FString FNebulaAISecretStore::DeriveMachineKey()
{
    // Derive a stable per-install key from project identity + machine guid.
    // This is obfuscation for the fallback file, not a substitute for the
    // platform credential store; the UI reports fallback usage to the user.
    FString MachineId = FPlatformProcess::ComputerName();
    MachineId += TEXT("|");
    MachineId += FApp::GetProjectName();
    MachineId += TEXT("|NebulaForgeAI.v1");
    return FMD5::HashAnsiString(*MachineId);
}

FString FNebulaAISecretStore::EncryptString(const FString& Plain, const FString& Key)
{
    // Repeat-key XOR stream keyed by CRC of the derived machine key, then
    // base64 for text-safe storage.
    const uint32 KeySeed = FCrc::StrCrc32(*Key);
    uint32 Counter = KeySeed;

    TArray<TCHAR> Chars(Plain.GetCharArray());
    TArray<uint8> Bytes;
    Bytes.Reserve(Chars.Num() * 2);
    for (TCHAR Ch : Chars)
    {
        Counter = Counter * 1664525u + 1013904223u;
        const uint16 Value = static_cast<uint16>(Ch) ^ static_cast<uint16>(Counter >> 16);
        Bytes.Add(static_cast<uint8>(Value & 0xFF));
        Bytes.Add(static_cast<uint8>((Value >> 8) & 0xFF));
    }
    return FBase64::Encode(Bytes);
}

FString FNebulaAISecretStore::DecryptString(const FString& Cipher, const FString& Key)
{
    TArray<uint8> Bytes;
    if (!FBase64::Decode(Cipher, Bytes) || Bytes.Num() % 2 != 0)
    {
        return FString();
    }

    const uint32 KeySeed = FCrc::StrCrc32(*Key);
    uint32 Counter = KeySeed;

    FString Result;
    Result.Reserve(Bytes.Num() / 2);
    for (int32 Index = 0; Index < Bytes.Num(); Index += 2)
    {
        Counter = Counter * 1664525u + 1013904223u;
        const uint16 Value =
            static_cast<uint16>(Bytes[Index]) |
            (static_cast<uint16>(Bytes[Index + 1]) << 8);
        Result.AppendChar(static_cast<TCHAR>(Value ^ static_cast<uint16>(Counter >> 16)));
    }
    return Result;
}
