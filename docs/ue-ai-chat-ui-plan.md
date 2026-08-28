# NebulaForge UE AI Chat UI — Full Implementation Plan

**Status:** Implemented (v1) — see "Implementation status" at the end of this document
**Scope:** Unreal Editor plugin UI, OpenAI-compatible model providers, and Codex support
**Primary target:** UE 5.0–5.8 Preview, editor-only plugin
**Existing product:** `NebulaForgeBridge` v0.5.30

## 1. Product goal

Add a native Unreal Editor experience where a user can open NebulaForge, chat with an AI about the current project, and allow the AI to inspect or modify Unreal through the existing automation/MCP handler system.

The first release will support:

- A dockable Unreal Editor chat window.
- Conversations, streaming responses, cancellation, retry, copy, and clear-history actions.
- Current-project context such as map, selected actors, selected assets, editor mode, and recent output-log entries.
- A Settings page for API credentials, endpoints, models, request defaults, privacy, and permissions.
- OpenAI-compatible providers using the common HTTP API shape.
- A first-class OpenAI provider using the Responses API where required.
- Codex model support through the OpenAI Responses API adapter.
- Approval gates before the AI invokes mutating Unreal operations.

This is an editor productivity feature, not a runtime gameplay UI. It must not add API keys or outbound AI networking to packaged games.

## 2. Product decisions and non-goals

### Decisions

1. Build the UI with Slate and Unreal Editor modules so the plugin remains self-contained and dockable.
2. Keep AI transport separate from the existing MCP/WebSocket transport. The AI client talks to a provider; Unreal actions are exposed to that client through a controlled local tool bridge.
3. Reuse existing consolidated MCP handlers and safe-operation wrappers. The UI must never call handler implementation functions directly.
4. Store provider profiles and non-secret preferences in project/user config. Store API secrets in the platform credential store when available, with an explicit encrypted-config fallback documented to the user.
5. Default to read-only project context and confirmation for all writes, console commands, Python execution, file operations, and process operations.
6. Treat model capability as data returned by the provider or configured in the profile. Do not hard-code model-specific controls into the chat widget.

### Non-goals for v1

- Non-OpenAI provider-specific SDKs.
- Voice, image generation, realtime audio, or autonomous background agents.
- Shipping AI networking in packaged/cooked builds.
- Automatic execution of destructive Unreal operations.
- Full replacement of external MCP clients.
- Assuming every OpenAI-compatible endpoint implements the Responses API.

## 3. User experience

### 3.1 Entry points

Add these editor entry points:

- **Window → NebulaForge AI Chat**: opens or focuses the dockable tab.
- Existing NebulaForge status-bar indicator: add **Open AI Chat** and **Open Settings** actions.
- Optional toolbar button in the Level Editor, behind a plugin setting.
- First-run notification when the plugin is enabled and no provider profile exists.

The UI should use standard Unreal editor tab behavior: dock, float, restore layout, close, and reopen without losing the active conversation.

### 3.2 Main chat layout

```text
┌─────────────────────────────────────────────────────────────────────┐
│ NebulaForge AI                 Provider · Model        ⚙  ⋯          │
├───────────────┬─────────────────────────────────────────────────────┤
│ Conversations │ Context: Map / Selection / Tools / Privacy         │
│               ├─────────────────────────────────────────────────────┤
│ + New chat    │                                                     │
│ Today         │                  Message timeline                   │
│  • Build UI   │                                                     │
│  • Fix error  │                                                     │
│ Yesterday     │                                                     │
│  • Materials  │                                                     │
│               ├─────────────────────────────────────────────────────┤
│               │ Context chips · Attach screenshot · Attach asset    │
│               │ [ Ask NebulaForge…                         ] [Send] │
└───────────────┴─────────────────────────────────────────────────────┘
```

#### Header

- Provider and model selectors.
- Connection/credential status indicator.
- Token/request activity indicator while streaming.
- Settings button.
- Conversation actions: rename, export, delete, clear messages.

#### Conversation sidebar

- New conversation.
- Search conversations.
- Sort by recent activity.
- Rename inline.
- Delete with confirmation.
- Show provider/model used for each conversation.
- Keep the sidebar collapsible for narrow layouts.

#### Message timeline

- Separate user, assistant, tool-call, approval, and error message styles.
- Markdown rendering for headings, lists, links, tables, and fenced code.
- Unreal-aware code blocks with Copy and Insert/Apply actions where relevant.
- Streaming text updates without rebuilding the entire widget tree on every token.
- Collapsible reasoning/status area when the provider returns non-user-facing reasoning metadata. Never expose hidden chain-of-thought; show only safe status summaries.
- Tool calls show name, purpose, arguments summary, result, duration, and approval status.
- Error cards include retry, copy diagnostic, and open-settings actions.

#### Composer

- Multiline input with Enter-to-send configurable by preference.
- Stop button during streaming.
- Attach current screenshot, selected actors, selected assets, current level, or output-log excerpt.
- Context summary chips that can be removed before sending.
- Per-message mode: **Ask**, **Plan**, or **Act**. Ask and Plan are read-only by default; Act allows approved mutations.
- Disabled state with an actionable explanation when no valid provider/model is configured.

### 3.3 Settings page

Open from the chat header, status bar, Project Settings, and first-run setup.

#### Provider profiles

Each profile contains:

- Display name.
- Provider kind: `OpenAI`, `OpenAICompatible`, or `Codex/OpenAI Responses`.
- Base URL.
- API key reference; display only a masked value after saving.
- Organization/project headers where applicable.
- Default model.
- API protocol: `ChatCompletions` or `Responses`.
- Optional custom headers, with sensitive header names blocked from logs.
- Enabled/disabled state.
- Test connection action.
- Delete profile action with confirmation.

Preset profiles:

- **OpenAI**: official base URL and Responses-capable defaults.
- **OpenAI-compatible**: editable base URL and Chat Completions defaults.
- **Codex**: official OpenAI Responses configuration with a Codex model selector.

The provider UI must not imply that an arbitrary OpenAI-compatible server supports Codex. Codex is available only when the selected endpoint and account expose the required Responses-compatible model.

#### Model settings

- Model ID, refreshed from the provider when supported.
- Reasoning effort where supported.
- Temperature where supported.
- Maximum output tokens.
- Streaming toggle.
- Request timeout.
- Context window warning threshold.
- Optional system/developer instructions.

Unsupported fields must be hidden or marked unsupported rather than silently sent.

#### Unreal permissions

Provide explicit switches for:

- Read project metadata.
- Read current selection.
- Read assets and Blueprints.
- Read output log.
- Read viewport screenshot.
- Propose tool calls.
- Execute approved non-destructive tools.
- Execute approved mutating tools.
- Execute console commands.
- Execute Python.
- Write files or assets.

Mutating capabilities default off. Dangerous capabilities require a second confirmation in Settings and per-request approval in Chat.

#### Privacy and storage

- Send project context: off/on.
- Send selected asset contents: off/on.
- Send screenshots: off/on.
- Include output-log content: off/on.
- Store conversations locally: on/off.
- Store provider request/response diagnostics: off by default.
- Clear all local conversations.
- Clear all saved credentials.
- Link to the project privacy notice.

#### Advanced diagnostics

- Current plugin version.
- Provider health and last error.
- Last request ID, latency, HTTP status, and token usage when available.
- Export redacted diagnostics.
- Log level; never log raw API keys, Authorization headers, full prompts, or full responses by default.

### 3.4 First-run flow

1. User opens NebulaForge AI Chat.
2. Welcome state explains that API usage is billed by the selected provider and that keys remain local to the editor.
3. User chooses OpenAI, OpenAI-compatible, or Codex/OpenAI Responses.
4. User enters a key, selects a model, and presses **Test connection**.
5. User selects default privacy and tool-permission settings.
6. Plugin creates a first conversation with a short project-context summary.

If setup is skipped, the chat remains usable as a local planning surface but cannot send a request.

## 4. AI and Unreal architecture

### 4.1 High-level flow

```text
Slate Chat UI
    ↓ user message + selected context
Conversation service
    ↓ normalized request
Provider adapter (Chat Completions or Responses)
    ↓ streamed events
AI event router
    ↓ text / tool proposal / approval / error
Unreal tool gateway
    ↓ approved MCP action
Existing consolidated registry → executeAutomationRequest / native handlers
```

The provider adapter owns HTTP, authentication, streaming, retries, response normalization, and provider error mapping. The tool gateway owns permission checks, approval state, argument validation, and dispatch into the existing MCP registry path.

### 4.2 New plugin subsystems/classes

Proposed editor-only classes:

- `FNebulaForgeAIModule`: module startup/shutdown, menus, tab registration, style setup.
- `FNebulaForgeAITabManager`: dockable tab spawner and layout restoration.
- `SNebulaForgeAIChat`: root Slate widget.
- `SNebulaForgeAIConversationList`: conversation sidebar.
- `SNebulaForgeAIMessageView`: virtualized/rendered message timeline.
- `SNebulaForgeAIComposer`: input, attachments, send/stop controls.
- `SNebulaForgeAISettings`: settings pages and provider-profile editing.
- `UNebulaForgeAISettings`: Developer Settings/config model for non-secret preferences.
- `FNebulaForgeAIConversationService`: local conversation lifecycle and persistence.
- `FNebulaForgeAIProviderService`: profile management and provider discovery.
- `FNebulaForgeAIRequestCoordinator`: cancellation, streaming, retry, timeout, and concurrency.
- `FNebulaForgeAIContextCollector`: gathers opted-in Unreal context on the game/editor thread.
- `FNebulaForgeAIToolGateway`: permission checks and approved MCP tool execution.
- `FNebulaForgeAIProviderAdapter`: common provider interface.
- `FNebulaForgeOpenAIChatCompletionsAdapter`: OpenAI-compatible `/chat/completions` transport.
- `FNebulaForgeOpenAIResponsesAdapter`: official OpenAI Responses transport.
- `FNebulaForgeAISecretStore`: OS credential integration plus guarded fallback.

Keep UI classes thin. Services should be testable without Slate or a running Unreal world.

### 4.3 Build/module changes

Extend `NebulaForgeBridge.Build.cs` with only editor dependencies required by the UI and HTTP stack, likely:

- `Slate`, `SlateCore`, `ToolMenus`, `LevelEditor`, `EditorSubsystem`, `Projects`, `DeveloperSettings`.
- `HTTP`, `Json`, `JsonUtilities`, `SSL` where already available.
- `ApplicationCore` for clipboard and platform integration if required.

Do not make the new UI available to non-editor targets. Confirm module loading remains editor-only in the `.uplugin`.

## 5. Provider contract

### 5.1 Normalized request

```cpp
struct FNebulaForgeAIRequest
{
    FString ConversationId;
    FString Model;
    FString SystemInstructions;
    TArray<FNebulaForgeAIMessage> Messages;
    TArray<FNebulaForgeAIToolDefinition> Tools;
    FNebulaForgeAIRequestOptions Options;
    FNebulaForgeAIPrivacyOptions Privacy;
};
```

The normalized layer must support text deltas, usage updates, tool calls, tool results, response completion, cancellation, and structured errors.

### 5.2 OpenAI-compatible mode

Implement the broad compatibility baseline around:

- Configurable base URL.
- `POST /chat/completions`.
- Bearer authentication.
- `messages`, `model`, `stream`, token limit, temperature, and tool/function calling when supported.
- Server-sent event parsing with `[DONE]` handling.
- Provider-specific fields only through an opt-in JSON extension map.

The adapter must tolerate common variations in `max_tokens` versus `max_completion_tokens`, finish reasons, tool-call chunks, and usage reporting. Compatibility decisions should be covered by fixture tests.

### 5.3 Official OpenAI Responses mode

Use a separate adapter because Responses has a different input/output/event model and supports fields that should not be sent to a generic Chat Completions endpoint. Normalize its output into the same internal event stream.

Support in v1:

- Text input and streamed text output.
- Instructions/developer prompt.
- Conversation continuation using local history first; provider-side response/conversation IDs only when explicitly enabled.
- Function/tool calls for the Unreal tool gateway.
- Request cancellation and timeout.
- Usage and request ID display when returned.
- `store` default off unless the user opts in.

The official OpenAI documentation identifies Codex models such as GPT-5-Codex as optimized for agentic coding and available through the Responses API; the implementation must therefore route Codex profiles through `FNebulaForgeOpenAIResponsesAdapter` rather than assuming Chat Completions compatibility.

### 5.4 Codex support

Codex support in this plan means selectable OpenAI Codex models and Codex-oriented behavior inside the Unreal editor chat, not embedding the external Codex CLI.

Codex profile behavior:

- Provider kind is `Codex/OpenAI Responses`.
- Endpoint is the official OpenAI Responses endpoint unless the user explicitly edits it.
- Model is user-selectable from discovered models or manually entered.
- UI labels coding-oriented models clearly.
- Default system instructions emphasize Unreal project conventions, safe edits, explicit plans, and approval before mutations.
- Enable reasoning-effort controls only if the model reports/supports them.
- Preserve tool-call approval and output limits exactly as for other providers.
- Do not claim Codex availability for generic compatible endpoints.

### 5.5 Model discovery

Model discovery is best-effort. If a provider cannot list models:

- Allow manual model ID entry.
- Show the last successful model ID.
- Let the user test the exact model with a small request.
- Avoid maintaining a brittle hard-coded list as the source of truth.

## 6. Unreal context and tool execution

### 6.1 Context collector

Create a compact, explicit context envelope rather than dumping project data into every prompt:

- Project name and engine version.
- Current map/level and play/editor state.
- Selected actors: names, classes, transforms, tags, and component summaries.
- Selected assets: paths, classes, package dirty state, and relevant metadata.
- Current editor mode and viewport summary.
- Optional screenshot attachment.
- Optional recent output-log excerpt, capped and redacted.
- User-configured project instructions.

Every context item must show a UI chip and be removable before sending.

### 6.2 Tool catalog

Generate tool definitions from the existing canonical MCP registry/metadata where possible. Do not duplicate hundreds of action schemas in the UI.

Expose a curated default set:

- Read project/level/selection.
- Query assets and Blueprints.
- Inspect logs and performance.
- Create or modify approved assets/actors.
- Save operations through existing safe wrappers.

Keep console, Python, process, file, and destructive actions disabled unless explicitly enabled in Settings.

### 6.3 Approval UX

Before execution, show:

- Tool name and human-readable purpose.
- Target asset/actor/path.
- Changed fields or operation summary.
- Risk label: read-only, reversible, mutating, destructive, external process.
- Allow once, allow for this conversation, deny, or edit arguments.

Approved calls go through the registry path and existing validation/safety utilities. The UI must not bypass `toolRegistry.register()`, `handleConsolidatedToolCall()`, `executeAutomationRequest()`, path security, command validation, or safe save/load wrappers.

## 7. Persistence and secrets

### 7.1 Data model

Persist locally:

- Provider profiles without raw secrets.
- Active provider/model.
- UI layout and preferences.
- Conversation metadata and messages if enabled.
- Attachment metadata, not duplicated binary data unless explicitly requested.

Recommended identifiers:

- Provider profile: UUID.
- Conversation: UUID.
- Message: UUID plus monotonic sequence.
- Request: UUID and provider request ID when available.

### 7.2 Secret handling

- Never place raw API keys in `DefaultGame.ini`, source control, crash reports, or normal logs.
- Use Windows Credential Manager, macOS Keychain, and Linux Secret Service/libsecret where available.
- Store only a secret reference in Unreal config.
- If a platform credential backend is unavailable, use an encrypted local fallback with a clear warning and a migration path.
- Mask secrets in all widgets and diagnostic exports.
- Clear secret text from temporary buffers after use where practical.
- Do not send API keys to the TypeScript MCP server or Unreal automation bridge.

### 7.3 Conversation privacy

Conversation persistence must be opt-out at the project/user level and easy to delete. Add a redaction pass before diagnostics export for keys, bearer tokens, filesystem paths where possible, and large asset contents.

## 8. Error, loading, and offline states

Implement explicit states:

- No provider configured.
- Missing/invalid secret.
- Testing connection.
- Connected and ready.
- Streaming.
- Waiting for approval.
- Tool executing.
- Cancelled.
- Rate limited.
- Timeout.
- Provider unavailable.
- Invalid model/protocol.
- Context too large.
- Unreal operation failed.

Errors should include a user-facing summary, technical details behind an expander, and a recovery action. Retry must not duplicate a previously approved mutation without a fresh approval decision.

## 9. Implementation phases

### Phase 0 — Design and compatibility spike

- Confirm supported UE versions and module APIs.
- Prototype a dockable Slate tab and one HTTP streaming request.
- Confirm credential-store approach per platform.
- Build fixture payloads for Chat Completions and Responses streams.
- Decide whether provider networking runs on `FHttpModule` callbacks or a dedicated service thread; callbacks must marshal state changes safely to the game/editor thread.

**Exit criteria:** A test tab can send a mocked request and render streamed text without blocking the editor.

### Phase 1 — Plugin shell and settings

- Register module, menus, tab spawner, style, commands, and settings section.
- Add `UNebulaForgeAISettings` and provider-profile UI.
- Add secret-store abstraction.
- Add first-run setup and test-connection flow.
- Add redacted diagnostics.

**Exit criteria:** User can create, save, test, select, and delete a provider profile without secrets appearing in config or logs.

### Phase 2 — Conversation core

- Add conversation/message models.
- Add local persistence and deletion.
- Add sidebar, timeline, markdown/code rendering, copy, retry, stop, and export.
- Add composer and context chips.

**Exit criteria:** Mock provider supports multiple conversations, streaming, cancellation, retry, and restart persistence.

### Phase 3 — OpenAI-compatible transport

- Implement Chat Completions adapter.
- Add SSE parser and normalized event router.
- Add model discovery/manual model entry.
- Add request timeout, retry policy, rate-limit handling, and usage display.
- Add compatibility fixture matrix.

**Exit criteria:** A compatible endpoint can complete a chat, stream tokens, return tool calls, and surface protocol errors clearly.

### Phase 4 — Official OpenAI Responses and Codex

- Implement Responses adapter.
- Add Responses event normalization and cancellation.
- Add Codex profile/presets and model capability presentation.
- Add reasoning-effort and output-limit controls only when supported.
- Validate local-history and optional provider-side continuation behavior.

**Exit criteria:** A configured OpenAI Codex model can answer in the UE chat, stream output, request an Unreal tool, and wait for approval before execution.

### Phase 5 — Unreal context and safe tools

- Implement opted-in context collector.
- Generate tool definitions from canonical MCP metadata.
- Implement read-only tools first.
- Add approval drawer and tool execution timeline.
- Add mutating tools behind settings and per-call approval.

**Exit criteria:** The assistant can inspect the current level/selection and perform one approved safe mutation using the existing registry and wrappers.

### Phase 6 — Hardening and release

- Add automated tests, UE functional tests, and multi-version compile checks.
- Test plugin enable/disable and editor restart behavior.
- Test all secret redaction paths.
- Test network failures, malformed streams, cancellation, duplicate events, and stale approvals.
- Package plugin and update documentation, screenshots, and migration notes.

**Exit criteria:** Release checklist is green on Win64, Mac, and Linux editor targets supported by the plugin.

## 10. Proposed file layout

```text
plugins/NebulaForgeBridge/
  Source/NebulaForgeBridge/
    Public/
      NebulaForgeAISettings.h
      NebulaForgeAIService.h
      NebulaForgeAIModels.h
    Private/
      AI/
        NebulaForgeAIModule.cpp
        NebulaForgeAITabManager.*
        NebulaForgeAIProviderService.*
        NebulaForgeAIRequestCoordinator.*
        NebulaForgeAIConversationService.*
        NebulaForgeAIContextCollector.*
        NebulaForgeAIToolGateway.*
        NebulaForgeAISecretStore.*
        Providers/
          NebulaForgeAIProviderAdapter.*
          NebulaForgeOpenAIChatCompletionsAdapter.*
          NebulaForgeOpenAIResponsesAdapter.*
        UI/
          SNebulaForgeAIChat.*
          SNebulaForgeAIConversationList.*
          SNebulaForgeAIMessageView.*
          SNebulaForgeAIComposer.*
          SNebulaForgeAISettings.*
          SNebulaForgeAIToolApproval.*
          NebulaForgeAIStyle.*
```

The exact split may change after the Phase 0 spike. Avoid adding a second plugin module until compile time or dependency isolation requires it.

## 11. Testing plan

### Unit tests

- Provider profile validation and URL normalization.
- Secret reference handling and redaction.
- Chat Completions SSE parsing.
- Responses event parsing.
- Tool-call argument decoding.
- Conversation serialization and migration.
- Context size estimation and truncation.
- Permission matrix and approval expiry.
- Retry/cancellation state machine.

### Integration tests

- Mock OpenAI-compatible server with normal, streamed, malformed, rate-limited, and timeout responses.
- Mock Responses server with text deltas, tool calls, usage, cancellation, and error events.
- End-to-end tool proposal → approval → registry dispatch → normalized result.
- Editor restart restores settings and conversations without exposing secrets.
- No HTTP request is made when privacy/permission settings deny the required context.

### Unreal functional tests

- Open/focus/close/restore the dockable tab.
- Read current map and selected actor context.
- Render a long streamed response without editor hitching.
- Cancel during streaming.
- Approve a safe read operation.
- Deny a mutating operation.
- Save an approved asset through the project safe-save path.
- Verify the plugin is absent from packaged runtime targets.

### Manual acceptance checklist

- A new user can finish setup without reading documentation.
- The active provider/model is always visible.
- The user can tell what project information will be sent before sending.
- No mutation happens without an understandable approval step.
- Errors are recoverable and do not leave the composer locked.
- API keys never appear in config, logs, screenshots, or exported diagnostics.
- OpenAI-compatible models work without requiring Codex-specific fields.
- Codex models use the Responses path and are clearly labeled.
- The editor remains responsive during network activity and tool execution.

## 12. Risks and mitigations

| Risk | Mitigation |
|---|---|
| OpenAI-compatible APIs differ in small protocol details | Keep a conservative baseline, add provider capability flags, and use fixture-driven adapters. |
| Responses and Chat Completions payloads diverge | Maintain separate adapters with one normalized internal event model. |
| API keys leak into config/logs | Credential-store abstraction, redaction tests, and secret-aware diagnostics. |
| AI performs an unsafe Unreal change | Default read-only, per-call approval, existing command/path validators, and safe wrappers. |
| Large project context exceeds model limits | Explicit context chips, size estimation, truncation summaries, and user-visible warnings. |
| Network callbacks touch Slate from the wrong thread | Marshal all UI mutations to the editor/game thread and test cancellation races. |
| UE version differences break editor APIs | Keep UI dependencies in the editor module and compile-test UE 5.0–5.8 variants. |
| Conversation persistence exposes project IP | Local-only storage, opt-in diagnostics, deletion controls, and redaction. |

## 13. Definition of done for v1

The feature is ready when a user can install the NebulaForge plugin into an Unreal project, open a dockable AI Chat tab, configure an OpenAI-compatible or OpenAI/Codex profile, test the connection, send a prompt with selected Unreal context, receive a streamed answer, approve a tool call, and see the safe operation result—all without Node.js, without exposing credentials, and without affecting packaged runtime builds.

## 14. Official API assumptions to re-check before implementation

The implementation spike must revalidate current OpenAI API documentation before coding against request fields, model IDs, event names, or availability. In particular, the current official model documentation describes GPT-5-Codex as optimized for agentic coding and available through the Responses API, while the Responses create API exposes input, instructions, model, streaming, tool, reasoning, and output-limit controls. These are planning inputs, not a promise that every future model or account has identical capabilities.

- [GPT-5-Codex model documentation](https://developers.openai.com/api/docs/models/gpt-5-codex)
- [Responses create API reference](https://developers.openai.com/api/reference/cli/resources/responses/methods/create)

## 15. Implementation status (v1)

The feature is implemented inside the existing `NebulaForgeBridge` editor module
(no second module, per section 10's guidance).

### Delivered

| Area | Files | Notes |
|---|---|---|
| Data model | `Public/NebulaForgeAIModels.h`, `Private/AI/NebulaForgeAIModels.cpp` | Plain-C++ normalized messages/requests/events, tool risk classes, chat states |
| Settings | `Public/NebulaForgeAISettings.h`, `Private/AI/NebulaForgeAISettings.cpp` | `UNebulaForgeAISettings` in per-user editor config; provider profiles reference secrets only; permission + privacy + model defaults |
| Service facade | `Public/NebulaForgeAIService.h`, `Private/AI/NebulaForgeAIService.cpp` | Owns services, output-log ring buffer, game-thread ticker |
| Secret store | `Private/AI/NebulaForgeAISecretStore.*` | Windows Credential Manager primary, encrypted local fallback under `Saved/NebulaForgeAI/Secrets.bin`, masked everywhere |
| SSE parsing | `Private/AI/NebulaForgeAISseParser.*` | Chunk-safe, CRLF/LF, multi-line data, `[DONE]`, named events |
| Redaction/diagnostics | `Private/AI/NebulaForgeAIDiagnostics.*` | Bearer/sk- masking, sensitive-header detection, token estimation, log ring buffer |
| Chat Completions adapter | `Private/AI/Providers/NebulaForgeOpenAIChatCompletionsAdapter.*` | Streaming SSE, tool-call chunk accumulation, `max_tokens`/`max_completion_tokens` toggle, full-JSON fallback |
| Responses adapter | `Private/AI/Providers/NebulaForgeOpenAIResponsesAdapter.*` | Official + Codex path: `instructions`, `store:false`, reasoning effort, function calls, usage |
| Transport base | `Private/AI/Providers/NebulaForgeAIProviderAdapter.*` | URL normalization (`/v1` join), bearer/org/project/custom headers, timeouts; UE 5.8 `OnRequestProgress64` vs older progress delegate handled via `NEBULA_AI_HTTP_PROGRESS64` |
| Provider service | `Private/AI/NebulaForgeAIProviderService.*` | Presets (OpenAI / compatible / Codex), profile CRUD, Codex pinned to Responses, capability gating |
| Conversations | `Private/AI/NebulaForgeAIConversationService.*` | UUID ids + monotonic sequences, JSON persistence under `Saved/NebulaForgeAI/Conversations`, deletion, Markdown export (redacted), per-conversation tool approvals |
| Context collector | `Private/AI/NebulaForgeAIContextCollector.*` | Game-thread collection of project/level/selection/assets/editor-mode/log chips, privacy-gated |
| Tool gateway | `Private/AI/NebulaForgeAIToolGateway.*` | Curated catalog mapped to canonical subsystem actions, permission matrix, approval state machine (once / conversation / deny), dispatch via `ExecuteLocalAutomationRequest` only |
| Request coordinator | `Private/AI/NebulaForgeAIRequestCoordinator.*` | Event queue drained on game thread, streaming aggregation, timeout watchdog paused during approvals, cancel, tool round-trip continuation |
| Local execution | `UNebulaForgeBridgeSubsystem` | New `ERequestOrigin::LocalAI` + `ExecuteLocalAutomationRequest` routing responses through the existing handler registry and error capture |
| Slate UI | `Private/AI/UI/*` | `SNebulaForgeAIChat` root, conversation list, message view (role-colored rows, copy diagnostics), composer (mode Ask/Plan/Act, context toggles, stop), settings window (profiles, test connection, privacy actions), approval banner |
| Entry points | `Private/AI/UI/NebulaForgeAITabManager.*`, module cpp, status bar | Window menu entry, optional toolbar button, dockable nomad tab, status-bar menu with AI chat/settings |
| Self-test | `Private/AI/NebulaForgeAISelfTest.cpp` | `NebulaForgeAI.SelfTest` console command: SSE fixtures, redaction, secret round-trip |

### Known v1 limitations (mapped to future phases)

- Markdown rendering is plain text; code-fence Copy/Apply beyond tool-result copy is not implemented.
- Screenshot attachments are represented by a placeholder chip pending image upload support.
- Model discovery returns `/models` results but the settings UI relies on manual model entry plus test connection.
- Reasoning-effort and `stream_options.include_usage` follow profile/settings switches without per-model capability probing.
- Automated fixture tests run in-editor via `NebulaForgeAI.SelfTest`; CI-side C++ fixtures and UE functional tests are not yet wired.

### Build integration

`NebulaForgeBridge.Build.cs` gains editor-only private dependencies `HTTP`,
`WorkspaceMenu`, and `DesktopPlatform`. Everything ships in the existing
editor-only module; packaged/cooked targets are unaffected.

