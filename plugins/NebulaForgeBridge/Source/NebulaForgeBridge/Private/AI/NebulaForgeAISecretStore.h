// =============================================================================
// NebulaForgeAISecretStore.h
// =============================================================================
// Credential storage for provider API keys (plan section 7.2).
//
// Preferred backend: platform credential store.
//   - Windows: Credential Manager (advapi32 CredWriteW/CredReadW/CredDeleteW)
//   - macOS/Linux: platform backend unavailable in this release; the guarded
//     encrypted local fallback is used and clearly reported to the UI.
//
// Fallback backend: encrypted local file under
//   <Project>/Saved/NebulaForgeAI/Secrets.bin
// obfuscated with a per-machine key stream. It is explicitly reported as a
// fallback (bUsedFallbackStore) so the settings UI can warn the user.
//
// Raw secrets never enter Unreal config files or logs. Only opaque
// references (e.g. "credman:NebulaForge/AI/Profile/<uuid>") are persisted.
// =============================================================================

#pragma once

#include "CoreMinimal.h"

struct FNebulaAIStoredSecret
{
    /** True when the value came from the encrypted fallback file. */
    bool bFromFallback = false;
    /** Plaintext secret; clear from temporary buffers after use. */
    FString Secret;
    bool IsValid() const { return !Secret.IsEmpty(); }
};

class FNebulaAISecretStore
{
public:
    /** Build the credential reference for a profile id. */
    static FString MakeReferenceForProfile(const FString& ProfileId);

    /**
     * Store a secret for a profile. Returns the persisted opaque reference,
     * or an empty string on failure.
     */
    static FString StoreSecret(const FString& ProfileId, const FString& Secret);

    /** Retrieve a secret by profile id (reference resolved internally). */
    static bool TryGetSecret(const FString& ProfileId, FNebulaAIStoredSecret& OutSecret);

    /** True when a secret exists for the profile. */
    static bool HasSecret(const FString& ProfileId);

    /** Remove a stored secret. */
    static bool DeleteSecret(const FString& ProfileId);

    /** Remove every secret owned by the AI chat feature. */
    static int32 ClearAll();

    /** True when the last operation used the encrypted fallback file. */
    static bool LastUsedFallback();

private:
#if PLATFORM_WINDOWS
    static bool StoreCredentialManager(const FString& TargetName, const FString& Secret);
    static bool ReadCredentialManager(const FString& TargetName, FString& OutSecret);
    static bool DeleteCredentialManager(const FString& TargetName);
#endif

    static FString GetFallbackFilePath();
    static bool LoadFallbackMap(TMap<FString, FString>& OutMap);
    static bool SaveFallbackMap(const TMap<FString, FString>& Map);
    static FString EncryptString(const FString& Plain, const FString& Key);
    static FString DecryptString(const FString& Cipher, const FString& Key);
    static FString DeriveMachineKey();
};

DECLARE_LOG_CATEGORY_EXTERN(LogNebulaForgeAI, Log, All);
