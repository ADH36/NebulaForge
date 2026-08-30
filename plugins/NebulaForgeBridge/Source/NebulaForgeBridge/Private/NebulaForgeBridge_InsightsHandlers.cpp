// =============================================================================
// NebulaForgeBridge_InsightsHandlers.cpp
// =============================================================================
// NebulaForge Bridge - Profiling & Insights Handlers
//
// UE Version Support: 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7
//
// Handler Summary:
// -----------------------------------------------------------------------------
// Action: manage_insights
//   - start_session: Start Unreal Insights trace session with optional channels
//   - stop_session: Stop the active trace session
//   - get_session_status: Return active trace state and destination
//
// Dependencies:
//   - Core: NebulaForgeBridgeSubsystem, NebulaForgeBridgeHelpers
//   - Engine: Trace system (built-in)
//
// Notes:
//   - Uses console command "Trace.Start [channels]" for compatibility
//   - Channels are optional; default trace starts without specific channels
//   - Trace data sent to Unreal Insights application
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "NebulaForgeBridgeSubsystem.h"
#include "NebulaForgeBridgeHelpers.h"
#include "NebulaForgeBridgeGlobals.h"
#include "McpHandlerUtils.h"
#include "ProfilingDebugging/TraceAuxiliary.h"

// -----------------------------------------------------------------------------
// Engine Includes
// -----------------------------------------------------------------------------
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

// =============================================================================
// Handler Implementation
// =============================================================================

bool UNebulaForgeBridgeSubsystem::HandleInsightsAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Validate action
    if (Action != TEXT("manage_insights"))
    {
        return false;
    }

    // Validate payload
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // Extract subaction. Native MCP clients send "action"; TS bridge paths send
    // "subAction".
    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SubAction = GetJsonStringField(Payload, TEXT("action"));
    }

    // -------------------------------------------------------------------------
    // stop_session: Stop the active trace session.
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("stop_session"))
    {
        bool bTraceActive = false;
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
        bTraceActive = FTraceAuxiliary::IsConnected();
#endif
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
        const FTraceAuxiliary::ETraceSystemStatus TraceStatus = FTraceAuxiliary::GetTraceSystemStatus();
        bTraceActive = bTraceActive ||
            TraceStatus == FTraceAuxiliary::ETraceSystemStatus::TracingToServer ||
            TraceStatus == FTraceAuxiliary::ETraceSystemStatus::TracingToFile;
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
        bTraceActive = bTraceActive ||
            TraceStatus == FTraceAuxiliary::ETraceSystemStatus::TracingToCustomRelay;
#endif
#endif
        if (bTraceActive)
        {
            FTraceAuxiliary::Stop();
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("action"), TEXT("manage_insights"));
        Result->SetStringField(TEXT("subAction"), TEXT("stop_session"));
        Result->SetStringField(TEXT("traceAction"), TEXT("stop_trace"));
        Result->SetStringField(TEXT("status"), bTraceActive ? TEXT("stopped") : TEXT("already_stopped"));
        SendAutomationResponse(RequestingSocket, RequestId, true,
            bTraceActive ? TEXT("Trace session stopped.") : TEXT("No active trace session."), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // get_session_status: Return the current trace state.
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("get_session_status"))
    {
        bool bTraceActive = false;
        FString Status = TEXT("inactive");
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
        bTraceActive = FTraceAuxiliary::IsConnected();
#endif
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
        const FTraceAuxiliary::ETraceSystemStatus TraceStatus = FTraceAuxiliary::GetTraceSystemStatus();
        switch (TraceStatus)
        {
        case FTraceAuxiliary::ETraceSystemStatus::TracingToServer:
            Status = TEXT("tracing_to_server");
            bTraceActive = true;
            break;
        case FTraceAuxiliary::ETraceSystemStatus::TracingToFile:
            Status = TEXT("tracing_to_file");
            bTraceActive = true;
            break;
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
        case FTraceAuxiliary::ETraceSystemStatus::TracingToCustomRelay:
            Status = TEXT("tracing_to_custom_relay");
            bTraceActive = true;
            break;
#endif
        default:
            break;
        }
#endif
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("action"), TEXT("manage_insights"));
        Result->SetStringField(TEXT("subAction"), TEXT("get_session_status"));
        Result->SetBoolField(TEXT("active"), bTraceActive);
        Result->SetStringField(TEXT("status"), Status);
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
        Result->SetStringField(TEXT("destination"), FTraceAuxiliary::GetTraceDestinationString());
#endif
        SendAutomationResponse(RequestingSocket, RequestId, true,
            bTraceActive ? TEXT("Trace session is active.") : TEXT("Trace session is inactive."), Result);
        return true;
    }

    if (SubAction == TEXT("capture_insights_trace"))
    {
        FString FileName;
        Payload->TryGetStringField(TEXT("tracePath"), FileName);
        FileName.TrimStartAndEndInline();
        if (FileName.IsEmpty()) FileName = FString::Printf(TEXT("MCP_%s.utrace"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
        const bool bSafeFileName = FileName.Len() <= 128 && FileName.EndsWith(TEXT(".utrace"), ESearchCase::IgnoreCase) &&
            !FileName.Contains(TEXT("/")) && !FileName.Contains(TEXT("\\")) && !FileName.Contains(TEXT("..")) && !McpContainsUnsafeCommandSeparator(FileName);
        if (!bSafeFileName)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("tracePath must be a .utrace filename without directories or traversal."), TEXT("INVALID_TRACE_PATH"));
            return true;
        }
        FString Channels;
        const bool bHasChannels = Payload->TryGetStringField(TEXT("channels"), Channels) && !Channels.TrimStartAndEnd().IsEmpty();
        if (bHasChannels)
        {
            Channels.TrimStartAndEndInline();
            for (int32 Index = 0; Index < Channels.Len(); ++Index)
            {
                const TCHAR Character = Channels[Index];
                if (!(FChar::IsAlnum(Character) || Character == TEXT('_') || Character == TEXT('-') || Character == TEXT(',') || Character == TEXT(' ')))
                {
                    SendAutomationError(RequestingSocket, RequestId, TEXT("Trace channels contain unsupported characters."), TEXT("INVALID_CHANNELS"));
                    return true;
                }
            }
        }
        const FString TraceDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Profiling"), TEXT("Traces"));
        IFileManager::Get().MakeDirectory(*TraceDirectory, true);
        const FString TracePath = FPaths::Combine(TraceDirectory, FileName);
        const bool bStarted = FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::File, *TracePath, bHasChannels ? *Channels : nullptr, nullptr, LogNebulaForgeBridgeSubsystem);
        if (!bStarted)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to start file-backed trace; an active trace may already exist or the trace module is unavailable."), TEXT("TRACE_START_FAILED"));
            return true;
        }
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("action"), TEXT("manage_insights"));
        Result->SetStringField(TEXT("subAction"), TEXT("capture_insights_trace"));
        Result->SetStringField(TEXT("status"), TEXT("started"));
        Result->SetStringField(TEXT("tracePath"), TracePath);
        if (bHasChannels) Result->SetStringField(TEXT("channels"), Channels);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("File-backed trace capture started."), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // start_session: Start trace session with optional channels
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("start_session"))
    {
        FString Channels;
        const bool bHasChannels = Payload->TryGetStringField(TEXT("channels"), Channels)
            && !Channels.TrimStartAndEnd().IsEmpty();

        if (bHasChannels)
        {
            Channels.TrimStartAndEndInline();
            bool bChannelsSafe = !McpContainsUnsafeCommandSeparator(Channels);
            for (int32 Index = 0; bChannelsSafe && Index < Channels.Len(); ++Index)
            {
                const TCHAR Ch = Channels[Index];
                bChannelsSafe = FChar::IsAlnum(Ch) || Ch == TEXT('_') ||
                                Ch == TEXT('-') || Ch == TEXT(',') || Ch == TEXT(' ');
            }

            if (!bChannelsSafe)
            {
                SendAutomationError(RequestingSocket, RequestId,
                    TEXT("Trace channels contain unsupported characters."),
                    TEXT("INVALID_CHANNELS"));
                return true;
            }
        }

        bool bTraceAlreadyActive = false;
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
        bTraceAlreadyActive = FTraceAuxiliary::IsConnected();
#endif
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
        const FTraceAuxiliary::ETraceSystemStatus TraceStatus = FTraceAuxiliary::GetTraceSystemStatus();
        bTraceAlreadyActive = bTraceAlreadyActive ||
            TraceStatus == FTraceAuxiliary::ETraceSystemStatus::TracingToServer ||
            TraceStatus == FTraceAuxiliary::ETraceSystemStatus::TracingToFile;
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
        bTraceAlreadyActive = bTraceAlreadyActive ||
            TraceStatus == FTraceAuxiliary::ETraceSystemStatus::TracingToCustomRelay;
#endif
#endif
        if (bTraceAlreadyActive)
        {
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("action"), TEXT("manage_insights"));
            Result->SetStringField(TEXT("subAction"), TEXT("start_session"));
            Result->SetStringField(TEXT("traceAction"), TEXT("start_trace"));
            Result->SetStringField(TEXT("status"), TEXT("already_started"));
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
            Result->SetStringField(TEXT("destination"), FTraceAuxiliary::GetTraceDestinationString());
#endif
            if (bHasChannels)
            {
                Result->SetStringField(TEXT("channels"), Channels);
            }

            SendAutomationResponse(RequestingSocket, RequestId, true,
                TEXT("Trace session already active."), Result);
            return true;
        }

        // Guard GEngine before Exec call
        if (!GEngine)
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Engine is not available."), TEXT("ENGINE_UNAVAILABLE"));
            return true;
        }

        // Execute trace start via console command
        // This is the standard way to control trace from editor
        bool bCommandExecuted = false;
        if (bHasChannels)
        {
            bCommandExecuted = GEngine->Exec(nullptr, *FString::Printf(TEXT("Trace.Start %s"), *Channels));
        }
        else
        {
            bCommandExecuted = GEngine->Exec(nullptr, TEXT("Trace.Start"));
        }

        // Check if command was executed successfully
        if (!bCommandExecuted)
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Failed to start trace session. Trace module may not be available."),
                TEXT("COMMAND_FAILED"));
            return true;
        }

        // Build response
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("action"), TEXT("manage_insights"));
        Result->SetStringField(TEXT("subAction"), TEXT("start_session"));
        Result->SetStringField(TEXT("traceAction"), TEXT("start_trace"));
        Result->SetStringField(TEXT("status"), TEXT("started"));
        if (bHasChannels)
        {
            Result->SetStringField(TEXT("channels"), Channels);
        }

        SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Trace session started."), Result);
        return true;
    }

    // Unknown subaction
    SendAutomationError(RequestingSocket, RequestId,
        TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
