// =============================================================================
// NebulaForgeAISettings.h
// =============================================================================
// Editor/user settings for the NebulaForge AI chat feature.
//
// Storage policy (plan section 7):
// - Profiles and non-secret preferences live in per-user editor config
//   (EditorPerProjectUserSettings) so secrets/preferences never enter
//   DefaultGame.ini or source control.
// - Raw API keys are NEVER stored here. Only an opaque secret-store
//   reference (ApiSecretReference) is persisted.
//
// Editor-only feature; do not reference from runtime targets.
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NebulaForgeAISettings.generated.h"

UENUM()
enum class ENebulaAIProfileKind : uint8
{
    OpenAI UMETA(DisplayName = "OpenAI"),
    OpenAICompatible UMETA(DisplayName = "OpenAI-compatible"),
    CodexResponses UMETA(DisplayName = "Codex / OpenAI Responses")
};

UENUM()
enum class ENebulaAIProfileProtocol : uint8
{
    ChatCompletions UMETA(DisplayName = "Chat Completions"),
    Responses UMETA(DisplayName = "Responses")
};

/** One provider profile. Raw secrets are referenced, not stored. */
USTRUCT()
struct FNebulaAIProviderProfile
{
    GENERATED_BODY()

    /** Stable UUID identifying the profile. */
    UPROPERTY(EditAnywhere, Category = "Provider")
    FString Id;

    UPROPERTY(EditAnywhere, Category = "Provider")
    FString DisplayName;

    UPROPERTY(EditAnywhere, Category = "Provider")
    ENebulaAIProfileKind Kind = ENebulaAIProfileKind::OpenAICompatible;

    /** Base URL without the endpoint path, e.g. https://api.openai.com/v1 */
    UPROPERTY(EditAnywhere, Category = "Provider")
    FString BaseUrl;

    /** Opaque credential-store reference. Never a raw key. */
    UPROPERTY(EditAnywhere, Category = "Provider")
    FString ApiSecretReference;

    UPROPERTY(EditAnywhere, Category = "Provider")
    FString OrganizationId;

    UPROPERTY(EditAnywhere, Category = "Provider")
    FString ProjectId;

    UPROPERTY(EditAnywhere, Category = "Model")
    FString DefaultModel;

    UPROPERTY(EditAnywhere, Category = "Model")
    ENebulaAIProfileProtocol Protocol = ENebulaAIProfileProtocol::ChatCompletions;

    /** JSON object of extra headers, e.g. {"X-Custom":"value"}. */
    UPROPERTY(EditAnywhere, Category = "Provider")
    FString CustomHeadersJson;

    /** Prefer max_completion_tokens over max_tokens (newer OpenAI behavior). */
    UPROPERTY(EditAnywhere, Category = "Model")
    bool bPreferMaxCompletionTokens = false;

    UPROPERTY(EditAnywhere, Category = "Provider")
    bool bEnabled = true;

    bool IsValid() const
    {
        return !Id.IsEmpty() && !DisplayName.IsEmpty() && !BaseUrl.IsEmpty();
    }
};

/** Default model/request settings applied on top of the active profile. */
USTRUCT()
struct FNebulaAIModelSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Model", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float Temperature = -1.0f;

    UPROPERTY(EditAnywhere, Category = "Model", meta = (ClampMin = "0"))
    int32 MaxOutputTokens = 0;

    UPROPERTY(EditAnywhere, Category = "Model")
    bool bStream = true;

    /** empty, "low", "medium", or "high"; only sent when supported. */
    UPROPERTY(EditAnywhere, Category = "Model")
    FString ReasoningEffort;

    UPROPERTY(EditAnywhere, Category = "Model", meta = (ClampMin = "5.0", ClampMax = "600.0"))
    float RequestTimeoutSeconds = 120.0f;

    /** Warn when estimated context tokens exceed this (0 = off). */
    UPROPERTY(EditAnywhere, Category = "Model", meta = (ClampMin = "0"))
    int32 ContextWindowWarningTokens = 0;

    UPROPERTY(EditAnywhere, Category = "Model")
    FString SystemInstructions;
};

/** Explicit Unreal permission switches. Mutating capabilities default off. */
USTRUCT()
struct FNebulaAIPermissionSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Read")
    bool bReadProjectMetadata = true;
    UPROPERTY(EditAnywhere, Category = "Read")
    bool bReadSelection = true;
    UPROPERTY(EditAnywhere, Category = "Read")
    bool bReadAssetsAndBlueprints = true;
    UPROPERTY(EditAnywhere, Category = "Read")
    bool bReadOutputLog = true;
    UPROPERTY(EditAnywhere, Category = "Read")
    bool bReadViewportScreenshot = false;

    UPROPERTY(EditAnywhere, Category = "Tools")
    bool bProposeToolCalls = true;
    UPROPERTY(EditAnywhere, Category = "Tools")
    bool bExecuteApprovedNonDestructiveTools = true;
    UPROPERTY(EditAnywhere, Category = "Tools|Dangerous")
    bool bExecuteApprovedMutatingTools = false;
    UPROPERTY(EditAnywhere, Category = "Tools|Dangerous")
    bool bExecuteConsoleCommands = false;
    UPROPERTY(EditAnywhere, Category = "Tools|Dangerous")
    bool bExecutePython = false;
    UPROPERTY(EditAnywhere, Category = "Tools|Dangerous")
    bool bWriteFilesOrAssets = false;

    /** Second confirmation required before dangerous switches take effect. */
    UPROPERTY(EditAnywhere, Category = "Tools|Dangerous")
    bool bDangerousCapabilitiesAcknowledged = false;
};

/** Privacy controls for what project data may leave the editor. */
USTRUCT()
struct FNebulaAIPrivacySettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Privacy")
    bool bSendProjectContext = true;
    UPROPERTY(EditAnywhere, Category = "Privacy")
    bool bSendSelectedAssetContents = false;
    UPROPERTY(EditAnywhere, Category = "Privacy")
    bool bSendScreenshots = false;
    UPROPERTY(EditAnywhere, Category = "Privacy")
    bool bIncludeOutputLog = false;
    UPROPERTY(EditAnywhere, Category = "Storage")
    bool bStoreConversationsLocally = true;
    /** Off by default; includes redacted request/response payloads. */
    UPROPERTY(EditAnywhere, Category = "Diagnostics")
    bool bStoreRequestDiagnostics = false;
};

UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "NebulaForge AI Chat"))
class NEBULAFORGEBRIDGE_API UNebulaForgeAISettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UNebulaForgeAISettings();

    virtual FName GetContainerName() const override { return TEXT("Project"); }
    virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

    /** Saved provider profiles (secrets referenced only). */
    UPROPERTY(EditAnywhere, Config, Category = "Providers")
    TArray<FNebulaAIProviderProfile> ProviderProfiles;

    /** Active profile id; empty means setup has not been completed. */
    UPROPERTY(EditAnywhere, Config, Category = "Providers")
    FString ActiveProfileId;

    UPROPERTY(EditAnywhere, Config, Category = "Model")
    FNebulaAIModelSettings Model;

    UPROPERTY(EditAnywhere, Config, Category = "Permissions")
    FNebulaAIPermissionSettings Permissions;

    UPROPERTY(EditAnywhere, Config, Category = "Privacy")
    FNebulaAIPrivacySettings Privacy;

    /** Show an optional toolbar button in the level editor. */
    UPROPERTY(EditAnywhere, Config, Category = "UI")
    bool bShowToolbarButton = false;

    /** Enter sends the composer message; Ctrl+Enter sends otherwise. */
    UPROPERTY(EditAnywhere, Config, Category = "UI")
    bool bEnterToSend = true;

    /** Dismiss the first-run setup notification. */
    UPROPERTY(EditAnywhere, Config, Category = "UI")
    bool bFirstRunComplete = false;

    /** Look up a profile by id. */
    const FNebulaAIProviderProfile* FindProfile(const FString& ProfileId) const;
    FNebulaAIProviderProfile* FindMutableProfile(const FString& ProfileId);
    /** Returns the active profile or nullptr when none is selected. */
    const FNebulaAIProviderProfile* GetActiveProfile() const;
};
