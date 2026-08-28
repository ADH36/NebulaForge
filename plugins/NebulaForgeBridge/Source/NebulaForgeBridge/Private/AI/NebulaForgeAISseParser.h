// =============================================================================
// NebulaForgeAISseParser.h
// =============================================================================
// Incremental server-sent-events (SSE) parser shared by the Chat Completions
// and Responses adapters.
//
// The parser is a pure state machine: feed arbitrary chunks from an HTTP
// progress callback and receive complete `data:` payloads. It tolerates
// CRLF/LF line endings, multi-line events, comment lines, and [DONE]
// sentinels, matching the broad compatibility baseline in plan section 5.2.
// =============================================================================

#pragma once

#include "CoreMinimal.h"

class FNebulaAISseParser
{
public:
    struct FParsedEvent
    {
        /** Raw `data:` payload (may be empty for keep-alive events). */
        FString Data;
        /** True when the payload was the [DONE] sentinel. */
        bool bDone = false;
        /** Optional event name from an `event:` line. */
        FString EventName;
    };

    /** Feed a raw text chunk; returns completed events. */
    TArray<FParsedEvent> Feed(const FString& Chunk);

    /** Reset parser state (e.g. after cancellation). */
    void Reset();

    /** True once [DONE] was observed. */
    bool IsDone() const { return bSawDone; }

private:
    /** Consume the buffered line currently in LineBuffer. */
    void ConsumeLine(TArray<FParsedEvent>& OutEvents);

    FString LineBuffer;
    FString PendingEventName;
    FString PendingDataLines;
    bool bSawDone = false;
};
