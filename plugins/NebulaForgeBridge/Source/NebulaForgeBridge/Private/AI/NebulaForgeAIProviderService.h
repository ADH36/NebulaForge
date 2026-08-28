// =============================================================================
// NebulaForgeAIProviderService.h
// =============================================================================
// Provider profile management, presets, adapter construction, and model
// discovery (plan sections 3.3/5.5).
//
// Codex profiles are always routed through the Responses adapter; the
// service refuses to imply Codex support for generic compatible endpoints.
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "NebulaForgeAIModels.h"
#include "NebulaForgeAISettings.h"
#include "AI/Providers/NebulaForgeAIProviderAdapter.h"

class FNebulaForgeAIProviderService
{
public:
    /** Create a profile from a preset and persist it. Returns the new id. */
    static FString CreatePresetProfile(ENebulaAIProviderKind Kind);

    /** Insert or update a profile; saves settings. */
    static bool UpsertProfile(const FNebulaAIProviderProfile& Profile);

    /** Delete a profile and its stored secret. */
    static bool DeleteProfile(const FString& ProfileId);

    /** Set the active profile. */
    static void SetActiveProfile(const FString& ProfileId);

    /** Resolve the effective protocol for a profile (Codex pins Responses). */
    static ENebulaAIProtocol ResolveProtocol(const FNebulaAIProviderProfile& Profile);

    /**
     * Build a transport for the given profile id. Fails when the profile is
     * disabled, has no usable secret, or has an invalid configuration.
     */
    static TSharedPtr<INebulaAIProviderTransport> CreateTransport(const FString& ProfileId, FString& OutError);

    /** Default system instructions for a profile kind. */
    static FString GetDefaultSystemInstructions(ENebulaAIProviderKind Kind);

    /** True when reasoning-effort controls apply to this profile kind. */
    static bool SupportsReasoningEffort(ENebulaAIProviderKind Kind);
};
