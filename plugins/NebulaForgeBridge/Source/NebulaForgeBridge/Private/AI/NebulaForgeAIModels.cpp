#include "NebulaForgeAIModels.h"

const TCHAR* NebulaAIProviderKindToString(ENebulaAIProviderKind Kind)
{
    switch (Kind)
    {
    case ENebulaAIProviderKind::OpenAI:
        return TEXT("OpenAI");
    case ENebulaAIProviderKind::CodexResponses:
        return TEXT("CodexResponses");
    case ENebulaAIProviderKind::OpenAICompatible:
    default:
        return TEXT("OpenAICompatible");
    }
}

ENebulaAIProviderKind NebulaAIProviderKindFromString(const FString& InValue, ENebulaAIProviderKind Fallback)
{
    if (InValue == TEXT("OpenAI")) return ENebulaAIProviderKind::OpenAI;
    if (InValue == TEXT("CodexResponses")) return ENebulaAIProviderKind::CodexResponses;
    if (InValue == TEXT("OpenAICompatible")) return ENebulaAIProviderKind::OpenAICompatible;
    return Fallback;
}

const TCHAR* NebulaAIProtocolToString(ENebulaAIProtocol Protocol)
{
    return Protocol == ENebulaAIProtocol::Responses ? TEXT("Responses") : TEXT("ChatCompletions");
}

ENebulaAIProtocol NebulaAIProtocolFromString(const FString& InValue, ENebulaAIProtocol Fallback)
{
    if (InValue == TEXT("Responses")) return ENebulaAIProtocol::Responses;
    if (InValue == TEXT("ChatCompletions")) return ENebulaAIProtocol::ChatCompletions;
    return Fallback;
}

const TCHAR* NebulaAIToolRiskToString(ENebulaAIToolRisk Risk)
{
    switch (Risk)
    {
    case ENebulaAIToolRisk::Reversible: return TEXT("Reversible");
    case ENebulaAIToolRisk::Mutating: return TEXT("Mutating");
    case ENebulaAIToolRisk::Destructive: return TEXT("Destructive");
    case ENebulaAIToolRisk::ExternalProcess: return TEXT("External process");
    case ENebulaAIToolRisk::ReadOnly:
    default: return TEXT("Read-only");
    }
}

FString NebulaAIInteractionModeToString(ENebulaAIInteractionMode Mode)
{
    switch (Mode)
    {
    case ENebulaAIInteractionMode::Plan: return TEXT("Plan");
    case ENebulaAIInteractionMode::Act: return TEXT("Act");
    case ENebulaAIInteractionMode::Ask:
    default: return TEXT("Ask");
    }
}
