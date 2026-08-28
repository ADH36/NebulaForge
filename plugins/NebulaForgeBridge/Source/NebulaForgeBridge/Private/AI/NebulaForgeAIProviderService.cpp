#include "AI/NebulaForgeAIProviderService.h"
#include "AI/NebulaForgeAISecretStore.h"
#include "AI/Providers/NebulaForgeOpenAIChatCompletionsAdapter.h"
#include "AI/Providers/NebulaForgeOpenAIResponsesAdapter.h"
#include "Misc/Guid.h"

namespace
{
    ENebulaAIProviderKind ToModelKind(ENebulaAIProfileKind Kind)
    {
        switch (Kind)
        {
        case ENebulaAIProfileKind::OpenAI:
            return ENebulaAIProviderKind::OpenAI;
        case ENebulaAIProfileKind::CodexResponses:
            return ENebulaAIProviderKind::CodexResponses;
        case ENebulaAIProfileKind::OpenAICompatible:
        default:
            return ENebulaAIProviderKind::OpenAICompatible;
        }
    }

    ENebulaAIProfileKind ToSettingsKind(ENebulaAIProviderKind Kind)
    {
        switch (Kind)
        {
        case ENebulaAIProviderKind::OpenAI:
            return ENebulaAIProfileKind::OpenAI;
        case ENebulaAIProviderKind::CodexResponses:
            return ENebulaAIProfileKind::CodexResponses;
        case ENebulaAIProviderKind::OpenAICompatible:
        default:
            return ENebulaAIProfileKind::OpenAICompatible;
        }
    }
}

FString FNebulaForgeAIProviderService::CreatePresetProfile(ENebulaAIProviderKind Kind)
{
    UNebulaForgeAISettings* Settings = GetMutableDefault<UNebulaForgeAISettings>();
    if (!Settings)
    {
        return FString();
    }

    FNebulaAIProviderProfile Profile;
    Profile.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    Profile.Kind = ToSettingsKind(Kind);
    Profile.bEnabled = true;
    Profile.bPreferMaxCompletionTokens = Kind != ENebulaAIProviderKind::OpenAICompatible;

    switch (Kind)
    {
    case ENebulaAIProviderKind::OpenAI:
        Profile.DisplayName = TEXT("OpenAI");
        Profile.BaseUrl = TEXT("https://api.openai.com/v1");
        Profile.Protocol = ENebulaAIProfileProtocol::Responses;
        Profile.DefaultModel = TEXT("gpt-5");
        break;
    case ENebulaAIProviderKind::CodexResponses:
        Profile.DisplayName = TEXT("Codex (OpenAI Responses)");
        Profile.BaseUrl = TEXT("https://api.openai.com/v1");
        Profile.Protocol = ENebulaAIProfileProtocol::Responses;
        Profile.DefaultModel = TEXT("gpt-5-codex");
        break;
    case ENebulaAIProviderKind::OpenAICompatible:
    default:
        Profile.DisplayName = TEXT("OpenAI-compatible");
        Profile.BaseUrl = TEXT("https://localhost:8000/v1");
        Profile.Protocol = ENebulaAIProfileProtocol::ChatCompletions;
        Profile.DefaultModel = FString();
        break;
    }

    Settings->ProviderProfiles.Add(Profile);
    if (Settings->ActiveProfileId.IsEmpty())
    {
        Settings->ActiveProfileId = Profile.Id;
    }
    Settings->SaveConfig();
    return Profile.Id;
}

bool FNebulaForgeAIProviderService::UpsertProfile(const FNebulaAIProviderProfile& Profile)
{
    UNebulaForgeAISettings* Settings = GetMutableDefault<UNebulaForgeAISettings>();
    if (!Settings || !Profile.IsValid())
    {
        return false;
    }

    // Codex profiles are pinned to the Responses protocol.
    FNebulaAIProviderProfile Stored = Profile;
    if (ToModelKind(Stored.Kind) == ENebulaAIProviderKind::CodexResponses)
    {
        Stored.Protocol = ENebulaAIProfileProtocol::Responses;
    }
    else if (Stored.Protocol == ENebulaAIProfileProtocol::Responses &&
             ToModelKind(Stored.Kind) == ENebulaAIProviderKind::OpenAICompatible)
    {
        // Allow explicit Responses on compatible endpoints only when the user
        // chose it; keep ChatCompletions as the default elsewhere.
    }

    if (FNebulaAIProviderProfile* Existing = Settings->FindMutableProfile(Stored.Id))
    {
        *Existing = Stored;
    }
    else
    {
        Settings->ProviderProfiles.Add(Stored);
    }
    Settings->SaveConfig();
    return true;
}

bool FNebulaForgeAIProviderService::DeleteProfile(const FString& ProfileId)
{
    UNebulaForgeAISettings* Settings = GetMutableDefault<UNebulaForgeAISettings>();
    if (!Settings)
    {
        return false;
    }

    bool bRemoved = false;
    for (int32 Index = Settings->ProviderProfiles.Num() - 1; Index >= 0; --Index)
    {
        if (Settings->ProviderProfiles[Index].Id == ProfileId)
        {
            Settings->ProviderProfiles.RemoveAtSwap(Index);
            bRemoved = true;
        }
    }
    if (Settings->ActiveProfileId == ProfileId)
    {
        Settings->ActiveProfileId =
            Settings->ProviderProfiles.Num() > 0 ? Settings->ProviderProfiles[0].Id : FString();
    }
    if (bRemoved)
    {
        FNebulaAISecretStore::DeleteSecret(ProfileId);
        Settings->SaveConfig();
    }
    return bRemoved;
}

void FNebulaForgeAIProviderService::SetActiveProfile(const FString& ProfileId)
{
    UNebulaForgeAISettings* Settings = GetMutableDefault<UNebulaForgeAISettings>();
    if (Settings)
    {
        Settings->ActiveProfileId = ProfileId;
        Settings->SaveConfig();
    }
}

ENebulaAIProtocol FNebulaForgeAIProviderService::ResolveProtocol(const FNebulaAIProviderProfile& Profile)
{
    // Codex models (e.g. GPT-5-Codex) are only exposed through the
    // Responses API; never route them through Chat Completions.
    if (ToModelKind(Profile.Kind) == ENebulaAIProviderKind::CodexResponses)
    {
        return ENebulaAIProtocol::Responses;
    }
    return Profile.Protocol == ENebulaAIProfileProtocol::Responses
        ? ENebulaAIProtocol::Responses
        : ENebulaAIProtocol::ChatCompletions;
}

TSharedPtr<INebulaAIProviderTransport> FNebulaForgeAIProviderService::CreateTransport(
    const FString& ProfileId, FString& OutError)
{
    const UNebulaForgeAISettings* Settings = GetDefault<UNebulaForgeAISettings>();
    const FNebulaAIProviderProfile* Profile = Settings ? Settings->FindProfile(ProfileId) : nullptr;
    if (!Profile)
    {
        OutError = TEXT("No provider profile is configured. Open Settings to add one.");
        return nullptr;
    }
    if (!Profile->bEnabled)
    {
        OutError = FString::Printf(TEXT("Provider profile '%s' is disabled."), *Profile->DisplayName);
        return nullptr;
    }
    if (Profile->DefaultModel.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Profile '%s' has no default model."), *Profile->DisplayName);
        return nullptr;
    }

    FNebulaAIStoredSecret Secret;
    if (!FNebulaAISecretStore::TryGetSecret(ProfileId, Secret) || !Secret.IsValid())
    {
        OutError = FString::Printf(
            TEXT("Missing API key for profile '%s'. Add the key in Settings; it is stored locally."),
            *Profile->DisplayName);
        return nullptr;
    }

    const ENebulaAIProtocol Protocol = ResolveProtocol(*Profile);
    if (Protocol == ENebulaAIProtocol::Responses)
    {
        return MakeShared<FNebulaOpenAIResponsesAdapter>(*Profile, Secret.Secret);
    }
    return MakeShared<FNebulaOpenAIChatCompletionsAdapter>(*Profile, Secret.Secret);
}

FString FNebulaForgeAIProviderService::GetDefaultSystemInstructions(ENebulaAIProviderKind Kind)
{
    if (Kind == ENebulaAIProviderKind::CodexResponses)
    {
        return TEXT(
            "You are NebulaForge AI, an assistant embedded in the Unreal Editor and backed by a Codex model. "
            "Follow Unreal project conventions, propose safe edits, and present an explicit plan before changes. "
            "You may request Unreal tool calls to inspect or modify the project; mutating operations require "
            "explicit user approval before execution. Prefer minimal, reversible changes.");
    }
    return TEXT(
        "You are NebulaForge AI, an assistant embedded in the Unreal Editor. Answer questions about the current "
        "project, propose changes, and use the provided Unreal tools when helpful. Mutating operations require "
        "explicit user approval. Never assume an operation succeeded unless the tool result says so.");
}

bool FNebulaForgeAIProviderService::SupportsReasoningEffort(ENebulaAIProviderKind Kind)
{
    // Reasoning-effort is a Responses-API control; do not advertise it for
    // generic Chat Completions compatible endpoints.
    return Kind == ENebulaAIProviderKind::CodexResponses || Kind == ENebulaAIProviderKind::OpenAI;
}
