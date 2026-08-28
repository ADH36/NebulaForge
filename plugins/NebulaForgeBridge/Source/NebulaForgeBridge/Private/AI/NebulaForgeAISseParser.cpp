#include "AI/NebulaForgeAISseParser.h"

TArray<FNebulaAISseParser::FParsedEvent> FNebulaAISseParser::Feed(const FString& Chunk)
{
    TArray<FParsedEvent> Events;
    if (bSawDone)
    {
        return Events;
    }

    LineBuffer.Append(Chunk);

    // Split on any complete line terminator (\n or \r\n).
    int32 NewlineIndex = INDEX_NONE;
    while (LineBuffer.FindChar(TEXT('\n'), NewlineIndex))
    {
        FString Line = LineBuffer.Left(NewlineIndex);
        if (Line.EndsWith(TEXT("\r")))
        {
            Line.LeftChopInline(1);
        }
        LineBuffer = LineBuffer.Mid(NewlineIndex + 1);
        ConsumeLine(Line, Events);
    }
    return Events;
}

void FNebulaAISseParser::Reset()
{
    LineBuffer.Reset();
    PendingEventName.Reset();
    PendingDataLines.Reset();
    bSawDone = false;
}

void FNebulaAISseParser::ConsumeLine(const FString& Line, TArray<FParsedEvent>& OutEvents)
{
    if (Line.IsEmpty())
    {
        // Blank line terminates the current event.
        if (!PendingDataLines.IsEmpty() || !PendingEventName.IsEmpty())
        {
            FParsedEvent Event;
            Event.Data = PendingDataLines;
            Event.EventName = PendingEventName;
            Event.bDone = PendingDataLines.TrimStartAndEnd().Equals(TEXT("[DONE]"), ESearchCase::IgnoreCase);
            bSawDone = bSawDone || Event.bDone;
            OutEvents.Add(MoveTemp(Event));
        }
        PendingEventName.Reset();
        PendingDataLines.Reset();
        return;
    }

    if (Line.StartsWith(TEXT(":"), ESearchCase::CaseSensitive))
    {
        // Comment/keep-alive line; ignore.
        return;
    }

    const int32 ColonIndex = Line.Find(TEXT(":"), ESearchCase::CaseSensitive);
    FString Field;
    FString Value;
    if (ColonIndex == INDEX_NONE)
    {
        Field = Line;
    }
    else
    {
        Field = Line.Left(ColonIndex);
        Value = Line.Mid(ColonIndex + 1);
        // A single optional leading space after the colon is stripped.
        if (Value.StartsWith(TEXT(" "), ESearchCase::CaseSensitive))
        {
            Value = Value.Mid(1);
        }
    }

    if (Field == TEXT("data"))
    {
        if (!PendingDataLines.IsEmpty())
        {
            PendingDataLines.Append(TEXT("\n"));
        }
        PendingDataLines.Append(Value);
    }
    else if (Field == TEXT("event"))
    {
        PendingEventName = Value;
    }
    // id:/retry: fields are not needed for the OpenAI transports.
}
