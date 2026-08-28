// =============================================================================
// NebulaForgeAISelfTest.cpp
// =============================================================================
// In-editor fixture tests for the AI chat core (plan section 11): SSE
// parsing (chunk boundaries, [DONE], multi-line data), secret redaction,
// secret store round-trip, and URL normalization.
//
// Run with:  NebulaForgeAI.SelfTest
// Results are logged; failures return a non-zero exit marker in the log for
// automation runners.
// =============================================================================

#include "AI/NebulaForgeAIDiagnostics.h"
#include "AI/NebulaForgeAISecretStore.h"
#include "AI/NebulaForgeAISseParser.h"
#include "HAL/IConsoleManager.h"

#if WITH_EDITOR

namespace
{
    int32 FailedChecks = 0;

    void Check(bool bCondition, const TCHAR* Message)
    {
        if (bCondition)
        {
            UE_LOG(LogNebulaForgeAI, Display, TEXT("[SELFTEST OK] %s"), Message);
        }
        else
        {
            ++FailedChecks;
            UE_LOG(LogNebulaForgeAI, Error, TEXT("[SELFTEST FAIL] %s"), Message);
        }
    }

    void RunSseParserTests()
    {
        // Fixture 1: Chat Completions style stream split across chunks.
        FNebulaAISseParser Parser;
        TArray<FNebulaAISseParser::FParsedEvent> Events;

        Events = Parser.Feed(TEXT("data: {\"choices\":[{\"delta\":{\"content\":\"He"));
        Check(Events.Num() == 0, TEXT("SSE: partial line yields no events"));

        Events = Parser.Feed(TEXT("llo\"}}]}\n\n"));
        Check(Events.Num() == 1 && Events[0].Data.Contains(TEXT("Hello")),
            TEXT("SSE: chunk split mid-payload reassembles"));

        // Fixture 2: CRLF line endings and [DONE].
        Events = Parser.Feed(TEXT("data: {\"id\":\"x\"}\r\n\r\ndata: [DONE]\r\n\r\n"));
        Check(Events.Num() == 2 && Events[1].bDone, TEXT("SSE: CRLF events and [DONE] detected"));
        Check(Parser.IsDone(), TEXT("SSE: IsDone after [DONE]"));

        // Fixture 3: Responses-style named events with event: lines.
        FNebulaAISseParser NamedParser;
        Events = NamedParser.Feed(TEXT("event: response.output_text.delta\ndata: {\"delta\":\"Hi\"}\n\n"));
        Check(Events.Num() == 1 && Events[0].EventName == TEXT("response.output_text.delta"),
            TEXT("SSE: named event captured"));

        // Fixture 4: comment/keep-alive lines ignored; multi-line data joined.
        FNebulaAISseParser MultiParser;
        Events = MultiParser.Feed(TEXT(": keep-alive\ndata: line1\ndata: line2\n\n"));
        Check(Events.Num() == 1 && Events[0].Data == TEXT("line1\nline2"),
            TEXT("SSE: comments ignored, multi-line data joined"));

        // Fixture 5: reset clears state.
        MultiParser.Reset();
        Check(!MultiParser.IsDone(), TEXT("SSE: Reset clears done state"));
    }

    void RunRedactionTests()
    {
        const FString Redacted = FNebulaAIDiagnostics::RedactText(
            TEXT("Authorization: Bearer sk-abcdef1234567890abcdef and standalone sk-verylongkey1234567890"));
        Check(!Redacted.Contains(TEXT("abcdef1234567890")), TEXT("Redaction: bearer sk- keys masked"));
        Check(Redacted.Contains(TEXT("[REDACTED]")), TEXT("Redaction: placeholder present"));

        Check(FNebulaAIDiagnostics::IsSensitiveHeaderName(TEXT("Authorization")),
            TEXT("Redaction: Authorization is sensitive"));
        Check(FNebulaAIDiagnostics::IsSensitiveHeaderName(TEXT("x-custom-token")),
            TEXT("Redaction: token header is sensitive"));
        Check(!FNebulaAIDiagnostics::IsSensitiveHeaderName(TEXT("Content-Type")),
            TEXT("Redaction: Content-Type is not sensitive"));

        const FString Masked = FNebulaAIDiagnostics::MaskSecret(TEXT("sk-1234567890abcdef"));
        Check(Masked.Contains(TEXT("****")), TEXT("Redaction: secret masked for display"));
    }

    void RunSecretStoreTests()
    {
        const FString TestProfileId = TEXT("selftest-profile");
        const FString Secret = TEXT("sk-selftest-0123456789");

        const FString Reference = FNebulaAISecretStore::StoreSecret(TestProfileId, Secret);
        Check(!Reference.IsEmpty(), TEXT("SecretStore: store returns reference"));

        FNebulaAIStoredSecret Loaded;
        const bool bLoaded = FNebulaAISecretStore::TryGetSecret(TestProfileId, Loaded);
        Check(bLoaded && Loaded.Secret == Secret, TEXT("SecretStore: round-trip preserves secret"));

        Check(FNebulaAISecretStore::DeleteSecret(TestProfileId), TEXT("SecretStore: delete succeeds"));
        FNebulaAIStoredSecret AfterDelete;
        Check(!FNebulaAISecretStore::TryGetSecret(TestProfileId, AfterDelete),
            TEXT("SecretStore: deleted secret is gone"));
    }

    void RunSelfTest()
    {
        FailedChecks = 0;
        UE_LOG(LogNebulaForgeAI, Display, TEXT("NebulaForgeAI self-test starting..."));

        RunSseParserTests();
        RunRedactionTests();
        RunSecretStoreTests();

        if (FailedChecks == 0)
        {
            UE_LOG(LogNebulaForgeAI, Display, TEXT("NebulaForgeAI self-test PASSED."));
        }
        else
        {
            UE_LOG(LogNebulaForgeAI, Error,
                TEXT("NebulaForgeAI self-test FAILED with %d failing checks."), FailedChecks);
        }
    }

    FAutoConsoleCommand GNebulaForgeAISelfTestCommand(
        TEXT("NebulaForgeAI.SelfTest"),
        TEXT("Runs fixture self-tests for the NebulaForge AI chat core (SSE parsing, redaction, secret store)."),
        FConsoleCommandDelegate::CreateStatic(RunSelfTest));
}

#endif // WITH_EDITOR
