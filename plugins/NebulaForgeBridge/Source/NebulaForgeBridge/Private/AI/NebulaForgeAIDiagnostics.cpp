#include "AI/NebulaForgeAIDiagnostics.h"

bool FNebulaAIDiagnostics::IsSensitiveHeaderName(const FString& HeaderName)
{
    static const TArray<FString> SensitiveNames = {
        TEXT("authorization"), TEXT("x-api-key"), TEXT("api-key"),
        TEXT("x-auth-token"), TEXT("x-mcp-capability-token"),
        TEXT("cookie"), TEXT("proxy-authorization"), TEXT("openai-organization")
    };
    FString Lower = HeaderName.ToLower();
    Lower.TrimStartAndEnd();
    return SensitiveNames.Contains(Lower) || Lower.Contains(TEXT("token")) || Lower.Contains(TEXT("secret"));
}

FString FNebulaAIDiagnostics::RedactText(const FString& Input)
{
    FString Out = Input;

    // Mask bearer authorization headers of any case.
    int32 Search = 0;
    while (true)
    {
        const int32 BearerIdx = Out.Find(TEXT("Bearer "), ESearchCase::IgnoreCase, ESearchDir::FromStart, Search);
        if (BearerIdx == INDEX_NONE) break;
        int32 End = BearerIdx + 7;
        while (End < Out.Len() && !FChar::IsWhitespace(Out[End])) ++End;
        Out = Out.Left(BearerIdx) + TEXT("Bearer [REDACTED]") + Out.Mid(End);
        Search = BearerIdx + 17;
    }

    // Mask common API key shapes (sk-...).
    int32 Idx = 0;
    while (Idx < Out.Len())
    {
        const int32 KeyStart = Out.Find(TEXT("sk-"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Idx);
        if (KeyStart == INDEX_NONE) break;
        int32 End = KeyStart + 3;
        while (End < Out.Len() && (FChar::IsAlnum(Out[End]) || Out[End] == TEXT('_') || Out[End] == TEXT('-'))) ++End;
        if (End - KeyStart > 12)
        {
            Out = Out.Left(KeyStart) + TEXT("sk-[REDACTED]") + Out.Mid(End);
            Idx = KeyStart + 13;
        }
        else
        {
            Idx = KeyStart + 3;
        }
    }

    return Out;
}

FString FNebulaAIDiagnostics::MaskSecret(const FString& Secret)
{
    if (Secret.Len() <= 8)
    {
        return TEXT("********");
    }
    return Secret.Left(3) + TEXT("********") + Secret.Right(2);
}

int32 FNebulaAIDiagnostics::EstimateTokens(const FString& Text)
{
    // Rough heuristic: ~4 characters per token for English/code text.
    return (Text.Len() + 3) / 4;
}

FNebulaAIOutputLogRingBuffer::FNebulaAIOutputLogRingBuffer()
{
    Lines.Reserve(MaxLines);
}

void FNebulaAIOutputLogRingBuffer::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
{
    Serialize(V, Verbosity, Category, 0.0);
}

void FNebulaAIOutputLogRingBuffer::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, const double Time)
{
    if (Verbosity == ELogVerbosity::NoLogging || Verbosity == ELogVerbosity::All)
    {
        return;
    }
    FString Line(V);
    if (Line.Len() > MaxLineLength)
    {
        Line = Line.Left(MaxLineLength) + TEXT("...");
    }
    Line = FNebulaAIDiagnostics::RedactText(Line);

    FScopeLock Lock(&Mutex);
    if (Lines.Num() >= MaxLines)
    {
        Lines.RemoveAt(0);
    }
    Lines.Add(TPair<ELogVerbosity::Type, FString>(Verbosity, MoveTemp(Line)));
}

TArray<FString> FNebulaAIOutputLogRingBuffer::GetRecentLines(int32 InMaxLines, ELogVerbosity::Type MinVerbosity) const
{
    TArray<FString> Out;
    FScopeLock Lock(&Mutex);
    for (int32 Index = Lines.Num() - 1; Index >= 0 && Out.Num() < InMaxLines; --Index)
    {
        // Lower verbosity values are more severe in ELogVerbosity.
        if (Lines[Index].Key <= MinVerbosity)
        {
            Out.Insert(Lines[Index].Value, 0);
        }
    }
    return Out;
}
