// =============================================================================
// NebulaForgeAIDiagnostics.h
// =============================================================================
// Redaction helpers and diagnostics bookkeeping for the AI chat feature.
//
// Raw API keys, bearer tokens, and Authorization headers must never reach
// logs, exported diagnostics, or crash reports (plan section 7.2).
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "NebulaForgeAIModels.h"

class FNebulaAIDiagnostics
{
public:
    /** Header names whose values are always masked. */
    static bool IsSensitiveHeaderName(const FString& HeaderName);

    /** Mask common credential shapes (sk-..., Bearer ..., long tokens). */
    static FString RedactText(const FString& Input);

    /** Mask a secret for display: keep at most the first 3 and last 2 chars. */
    static FString MaskSecret(const FString& Secret);

    /** Rough token estimate used for context-size warnings. */
    static int32 EstimateTokens(const FString& Text);
};

/**
 * Small ring buffer of recent output-log lines captured for the optional
 * output-log context chip. Registered on the game thread at service startup.
 */
class FNebulaAIOutputLogRingBuffer : public FOutputDevice
{
public:
    static constexpr int32 MaxLines = 200;
    static constexpr int32 MaxLineLength = 512;

    FNebulaAIOutputLogRingBuffer();
    virtual ~FNebulaAIOutputLogRingBuffer() {}

    // FOutputDevice
    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override;
    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time) override;
    virtual bool IsMemoryOnly() const override { return true; }

    /** Returns up to MaxLines recent, redacted lines (oldest first). */
    TArray<FString> GetRecentLines(int32 InMaxLines, ELogVerbosity::Type MinVerbosity = ELogVerbosity::Warning) const;

private:
    mutable FCriticalSection Mutex;
    TArray<TPair<ELogVerbosity::Type, FString>> Lines;
};
