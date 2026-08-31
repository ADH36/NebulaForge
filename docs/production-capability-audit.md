# NebulaForge Production Game-Building Capability Audit

Audit date: 2026-08-30  
Scope: MCP tools, TypeScript handlers, Unreal native handlers, documentation, and integration tests.  
Method: Static repository audit; runtime behavior still requires a live Unreal Editor/project verification pass.

Current implementation update (2026-08-30): host project validation and declared-plugin management are now available through `system_control`; Movie Render Queue PNG/JPG/BMP/EXR submission, stable status/cancellation, deterministic overrides, and immutable primary-config preset copying are available through `manage_sequence`. These host workflows remain intentionally explicit about native `/mcp` limitations.

Media update: guarded Media Framework asset creation (`UMediaPlayer`, file/stream sources, `UMediaTexture`, and playlists), player open/play/pause/seek controls, demo replay controls, and Take Recorder start/stop/status lifecycle are now exposed. Media plugin availability, source-specific capture configuration, platform codecs, and packaged-runtime verification remain project-dependent.

Persistence update: `manage_asset.list_primary_assets` and `manage_asset.get_primary_asset` now expose registered Unreal Asset Manager primary IDs, paths, pagination, and loaded state. DataTable authoring now supports transactional reflected row modification and deletion in addition to creation, insertion, and readback. `system_control.get_runtime_gameplay_tag` and `control_actor.get_gameplay_tags`/`add_gameplay_tag`/`remove_gameplay_tag` verify and safely mutate supported actor-owned Gameplay Tag containers. SaveGame slot save/load now supports managed async lifecycle IDs and completion events in editor and packaged/runtime builds; terminal async records are bounded and pruned after retention to prevent long-session state growth, and terminal operations cannot be retroactively cancelled. Project-specific primary-asset registration/rules/bundles and SaveGame schema/version orchestration remain project-dependent.

Platform update: `system_control.inspect_platform_capabilities` now reports host/target platform support, discovered UAT/UBT paths, signing-tool categories, and Android/Apple deployment prerequisites. `deploy_package` provides bounded local desktop staging for Win64/Linux/Mac plus Android ADB and iOS/tvOS simulator installation with dry-run and managed-job modes; non-process filesystem tasks use the same poll/cancel/timeout job registry, while external stores, hosting, and device provisioning remain out of scope.

Platform readiness update: capability discovery now probes Android (`adb`, `sdkmanager`) and Apple (`xcodebuild`) deployment prerequisites and returns per-target readiness instead of treating broad target enumeration as deployability.

Profiling update: native `system_control` now exposes `start_session`, `get_session_status`, and `stop_session` for Unreal trace sessions. Status and stop operations use guarded `FTraceAuxiliary` APIs where supported by the engine version; trace-file export, analysis, and persisted report generation remain follow-up work.

Animation validation update: `animation_physics.validate_animation_asset` now validates that an asset exists and is a supported animation sequence or montage, reports duration and skeleton identity, and applies optional duration/skeleton gates with structured diagnostics.

VFX validation update: `manage_effect.validate_niagara_system` now reports emitter/renderer counts and can fail explicit `maxEmitters` and `maxRenderers` budgets instead of treating every valid Niagara asset as production-ready.

Profiling gate update: native `system_control.start_memory_report` requests a full Unreal memory report, `configure_stat_commands` applies only bounded stat names with structured rejection reporting, `capture_insights_trace` starts a confined file-backed `.utrace` capture, `start_network_profiler` controls `.nprof` recording, and Visual Logger recording/text markers are available. Host `analyze_trace` launches UnrealInsights in bounded headless analysis mode and reports its terminal result; metric export remains follow-up work.

Automation update: `system_control.create_automation_test` generates a safe compile-ready C++ automation-test skeleton; the stdio host path persists a bounded terminal JSON report under `Saved/AutomationReports`, while the native `/mcp` path now returns a managed `asyncId`, captures individual test failures, emits a terminal completion event, and exposes the result through `get_async_action`. `system_control.get_test_results` retrieves a confined host report or terminal job evidence with explicit pass state. `system_control.enable_gameplay_debugger` safely enables or disables a Gameplay Debugger category through the existing native module/config/replicator path. Reports include job state, exit code, command, stdout, stderr, and truncation metadata; `get_job_status` exposes `completionPending`/`completionError` while asynchronous report persistence finishes.

The report also includes a conservative, explicitly non-authoritative `testSummary` heuristic; raw output and process exit state remain the source of truth.

Release update: `system_control.validate_release` can optionally verify an in-archive SHA-256 manifest, in addition to required files and `.pak` presence, so build output can be integrity-gated before signing or distribution.

Release-gate update: host `system_control.release_gate` composes release-artifact checks with optional static project validation and declared-plugin dependency validation, returning per-check results and passed/failed check names. Archive requirements use `requiredFiles`/`requiredDirectories`; source-project requirements use `projectRequiredFiles`/`projectRequiredDirectories` to prevent scope conflation.

Packaging update: host `system_control.run_uat` now exposes explicit BuildCookRun controls for compressed output, encrypted INI files, encrypted PAK indexes, and platform prerequisites; project key management and platform-specific encryption policy remain required for a valid release.

Map validation update: editor `system_control.check_map_errors` executes Unreal's Map Check against the loaded editor world and returns structured error/warning counts from the MapCheck log; it intentionally reports an unavailable-world error when no map is loaded.

Functional-test update: editor `system_control.create_functional_test` creates a FunctionalTesting actor when the plugin is enabled, returns its object path, and leaves assertion/configuration policy to subsequent authoring actions.

Capability-report update: `inspect.production_capabilities` now advertises the automation-report, SHA-256 release-manifest, release-gate, Map Check, Functional Testing, animation validation, VFX budget, and online identity additions so AI clients do not have to infer them from documentation.

Blueprint validation update: `system_control.validate_blueprints` now performs a bounded editor sweep over `/Game` or explicitly selected asset paths, compiles each Blueprint, returns compiler messages and counts, and optionally saves only successful compilations.

Online update: `manage_networking.get_online_session_status` reports the provider, session existence, and native Online Subsystem lifecycle state for a named session, allowing automation to verify asynchronous create/join/destroy progress.

Online identity update: `manage_networking.get_online_identity_status` reports the active provider, local-user login state, nickname, and unique net ID when the configured Online Subsystem exposes `IOnlineIdentity`; provider-specific login/account linking remains an explicit integration boundary.

Online capability update: `get_online_capabilities` now reports achievement, leaderboard, stats, and external-UI interface availability alongside sessions, identity, friends, and presence.

Online reliability update: OnlineSubsystem create/find/join/destroy delegates now have bounded `timeoutMs` deadlines and exactly-once terminal response handling, so a provider that never completes cannot leave an MCP request hanging indefinitely.

Network-test update: editor `manage_networking.configure_network_conditions` safely applies bounded packet lag, loss, duplication, and ordering conditions through Unreal's network test console controls, with an explicit reset operation for soak/reconnect scenarios.

Release gate update: host `system_control.release_gate` can now include an optional Unreal automation-test run, waits for the managed test job to reach a terminal state, and fails the gate on timeout, cancellation, or nonzero exit alongside archive, project, and plugin checks. This is a real execution gate, but project-authored test coverage and platform-specific certification remain required.

Network soak update: host `system_control.run_network_soak` launches a bounded packaged server and configurable packaged-client set with managed job IDs, a validated managed port argument, argument validation, terminal outcomes, independently bounded server and client startup timeouts, optional server startup-pattern gating, optional per-client readiness-pattern gating, and cleanup when startup fails. Project-specific replication assertions, reconnect triggers, provider matchmaking, and external hosting remain explicit integration responsibilities.

The provider-specific session stack still requires a configured Online Subsystem and live multiplayer project; this action provides controlled fault injection, not a claim that a universal backend-independent soak harness exists.

Plugin update: `system_control.manage_project_plugin` now supports a read-only `validate` operation that checks declared project plugins against local `.uplugin` descriptors and reports unresolved local dependencies before build/cook.

Legend: ✅ available or substantially covered · ❌ missing, incomplete, or production-blocking · ⚠️ conditional

## Summary

NebulaForge provides broad Unreal Editor automation, but it is not yet a complete production game-building platform. The largest gaps are build/deployment, release validation, persistence/data systems, online services, cinematics/rendering, and project-specific gameplay architecture. The `inspect` tool's `production_capabilities` action exposes the same static boundary to clients.

## Production pipeline

| Capability | Status | Gap |
|---|:---:|---|
| Build and deployment tool | ⚠️ | TypeScript `system_control` provides validated `run_uat` BuildCookRun operations, controlled signing, bounded local packaged launch, Win64/Linux/Mac local staging, Android/iOS/tvOS local deployment, and host job polling; external stores and hosting remain absent, while native `/mcp` reports host-only actions explicitly. |
| Cook content | ⚠️ | `run_uat` supports cook through BuildCookRun, but live Unreal verification is still required. |
| Package/stage/archive project | ⚠️ | BuildCookRun package/archive operations, `validate_release`, controlled signing, bounded local packaged launch, local device deployment, and optional architecture-manifest release gating exist; external stores and hosting remain absent. |
| Platform builds and signing | ⚠️ | Host signing is supported for Win64, Mac/iOS, and Android when tools and credentials are supplied; Linux and console signing/provider deployment remain project/toolchain dependent. |
| Plugin enable/disable management | ⚠️ | `manage_project_plugin` lists, validates, enables, and disables declared project plugins; dependency resolution and live project reload remain project-dependent. |
| Asset chunking, compression, encryption, PAK creation | ⚠️ | `run_uat` exposes bounded packaging, compression, encrypted INI, encrypted PAK index, and prerequisite controls; chunk rules and key management remain project/platform dependent. |

Evidence: [Roadmap.md](./Roadmap.md#phase-32-build--deployment)

## Testing and release validation

| Capability | Status | Gap |
|---|:---:|---|
| Functional test authoring | ⚠️ | `create_functional_test` creates a FunctionalTesting actor when the plugin is available; test-specific steps and assertions remain project-authored. |
| Automation test authoring | ⚠️ | `create_automation_test` generates a safe compile-ready C++ automation-test skeleton; project assertions and module/build wiring remain project-authored. |
| Automation test execution/results | ⚠️ | Stdio `run_tests` launches filtered or full Unreal automation tests through UnrealEditor-Cmd as a managed job; native `/mcp` launches in-editor tests through a managed `asyncId`, captures test-end failures, emits completion, and supports `get_async_action` polling. `release_gate` waits for host completion and fails on timeout/nonzero exit; project assertion quality remains project-authored. |
| Data Validation integration | ⚠️ | `validate_project` can launch UnrealEditor-Cmd's DataValidation commandlet as a bounded managed job; the engine/project must provide UnrealEditor-Cmd and the result must be polled to terminal state. |
| Blueprint validation sweep | ⚠️ | Project-wide static validation and the live DataValidation commandlet gate are available; Blueprint compilation/error coverage still depends on the project's commandlet/plugin setup. |
| Map error validation | ⚠️ | `check_map_errors` runs Map Check against the loaded editor world; map availability and project-specific validation policy remain prerequisites. |
| Cook/package smoke tests | ⚠️ | `run_packaged` provides bounded local launch and managed process results; automated cook/package smoke policy and live Unreal verification remain project-dependent. |
| Unreal Insights integration | ⚠️ | Trace sessions, file-backed capture, bounded UnrealInsights analysis, network profiler, memory report, stat commands, and Visual Logger controls are available; metric export remains incomplete. |
| Memory/network/visual profiling | ⚠️ | Native profiling controls and bounded capture workflows exist; final budget thresholds and report aggregation remain project-specific. |

Evidence: [Roadmap.md](./Roadmap.md#phase-33-testing--quality)

## Persistence and game data

| Capability | Status | Gap |
|---|:---:|---|
| SaveGame class authoring | ⚠️ | `generate_save_game_class` generates constrained compile-ready C++ SaveGame classes and can emit a versioned `SaveSchemaVersion` field plus a project-confined migration manifest with validated version transitions; runtime migration logic and project-specific serialization remain a project integration boundary. |
| Save/load/delete slots | ⚠️ | Editor and packaged/runtime slot save/load/delete/existence actions are implemented for `USaveGame` objects, including managed async save/load lifecycle IDs and completion events. Runtime `list_save_game_slots` now enumerates local `Saved/SaveGames/*.sav` records with size and user-index metadata; cloud/provider slot enumeration remains unavailable through a universal UE API. Higher-level slot serialization orchestration remains a project integration boundary. |
| Gameplay Tags authoring | ⚠️ | Project Gameplay Tag config add/list/remove plus runtime registration, actor-container queries, and guarded actor-container mutation are implemented; native tag registration, tag-table workflows, and arbitrary subsystem container mutation remain incomplete. |
| Config hierarchy management | ⚠️ | Config layer discovery, validated section/key reads, atomic writes, explicit reload summaries, file flush, and project/platform layer inventory are implemented for project `.ini` files; full engine merge semantics and generated platform overrides still require live project verification. |
| Data Assets and Primary Data Assets | ⚠️ | Generic and project-defined `UDataAsset` creation plus reflected property read/write are implemented; Asset Manager primary-asset listing and inspection are available, while project-specific registration/rules and bundle orchestration remain incomplete. |
| DataTables and CurveTables | ⚠️ | DataTable creation, transactional reflected row insertion/modification/deletion, and row readback are implemented; CurveTable rich-row authoring now supports interpolation, tangent, and tangent-weight attributes with readback, plus CSV import/export; simple float and linear-color curve assets can be created with initial keys. Post-creation simple-curve editing remains incomplete. |
| Asset persistence verification | ⚠️ | `verify_asset_persistence` checks a confined asset's loaded object, on-disk package existence, and dirty state with explicit `requireClean`; opt-in `verifyReload` safely unloads/reloads the package and compares class/clean state, while per-asset semantic diffs remain engine/project dependent. |

Evidence: [Roadmap.md](./Roadmap.md#phase-31-data--persistence)

## Cinematics, rendering, and media

| Capability | Status | Gap |
|---|:---:|---|
| Master/subsequence/shot workflow | ⚠️ | Level Sequence creation, binding, tracks/sections, playback, metadata, and MRQ output are available; master/subsequence/shot orchestration still requires a project-specific sequence layout. |
| Camera cuts, fades, events, material tracks | ⚠️ | Generic track/section creation and discovered track types are available; specialized camera-cut/fade/event/material authoring remains engine/build dependent. |
| Movie Render Queue jobs | ⚠️ | Guarded MRQ PNG/JPG/BMP/EXR image-sequence submission, stable job IDs (`jobId`/`mrqJobId`), executor callback-backed completed/failed status, bounded `WIDTHxHEIGHT` resolution and frame-range overrides, project-relative output paths, status polling, cancellation, ordered `render_sequence_queue` orchestration, and immutable-copy application of `UMoviePipelinePrimaryConfig` presets are available; burn-ins, video codecs, and transactional multi-job recovery remain incomplete. |
| Media Framework | ⚠️ | Media players, sources, textures, playlists, and guarded playback controls are available; codec/provider availability remains project dependent. |
| Take Recorder | ⚠️ | Start/stop/status lifecycle is available when Take Recorder is compiled; track policy and capture-device configuration remain project dependent. |
| Demo/replay system | ⚠️ | Replay controls and status are available where the replay subsystem is enabled; project recording configuration and killcam presentation remain project dependent. |
| Lighting and post-processing controls | ✅ | Broad authoring coverage exists, but engine/project configuration remains conditional. |
| Scene captures and reflection controls | ✅ | Implemented with renderer-dependent limitations. |

Evidence: [Roadmap.md](./Roadmap.md#phase-30-cinematics--media)

## Blueprint and gameplay authoring

| Capability | Status | Gap |
|---|:---:|---|
| Basic Blueprint creation | ✅ | Native editor path exists. |
| Blueprint graph editing | ⚠️ | Coverage depends on available K2 schemas, headers, and node types. |
| Arbitrary K2 node support | ⚠️ | Generic reflected `nodeClass` creation now supports project/plugin `UEdGraphNode` classes when loaded; node-specific pin configuration and unsupported editor-only classes still require project/engine validation. |
| Blueprint compilation | ⚠️ | `validate_blueprints` synchronously invokes Unreal's compiler for each discovered Blueprint, records compiler errors/warnings, and verifies an up-to-date status; project/plugin discovery and live asset availability remain prerequisites. |
| Project-specific gameplay architecture | ⚠️ | Host-managed architecture manifests now let AI declare required modules, assets, files, directories, and tests and validate their presence; generated gameplay classes, wiring, and behavioral proof remain project-specific. |
| Gameplay error handling and validation | ⚠️ | `release_gate` can run project Unreal automation tests and fail on test failure; project-authored gameplay assertions and coverage remain required. |

Evidence: [BlueprintHandlers.cpp](../plugins/NebulaForgeBridge/Source/NebulaForgeBridge/Private/NebulaForgeBridge_BlueprintHandlers.cpp#L4248)

## Animation and character systems

| Capability | Status | Gap |
|---|:---:|---|
| Basic animation asset creation | ✅ | Several asset authoring paths exist. |
| Bone tracks and bone keys | ⚠️ | Unsupported paths exist by engine/build configuration. |
| Curve key authoring | ⚠️ | Not consistently available across supported engine versions. |
| Montage section editing | ⚠️ | Some timing/editing APIs are unavailable. |
| Control Rig graph editing | ⚠️ | Editor builds now author controls, RigVM units, and pin links; requires ControlRig/RigVM editor modules and remains editor-only. |
| IK Rig chain editing | ⚠️ | Editor builds now author retarget chains through UIKRigController; requires IKRigEditor and remains editor-only. |
| Ragdoll setup/activation | ⚠️ | Editor-only limitations remain. |
| Animation preview/compression validation | ⚠️ | `validate_animation_asset` provides existence, supported-class, duration, and expected-skeleton gates with structured diagnostics; preview rendering and compression-budget analysis remain project/engine dependent. |

Evidence: [AnimationHandlers.cpp](../plugins/NebulaForgeBridge/Source/NebulaForgeBridge/Private/NebulaForgeBridge_AnimationHandlers.cpp#L4260)

## Niagara and effects

| Capability | Status | Gap |
|---|:---:|---|
| Niagara authoring | ⚠️ | Depends on Niagara/plugin/editor availability. |
| Niagara system/emitter creation | ⚠️ | Native non-editor paths return `NOT_IMPLEMENTED`. |
| Niagara spawning and parameter mutation | ⚠️ | Multiple actions are editor-only or have unsupported paths. |
| Ribbon/trail authoring | ⚠️ | Conditional implementation. |
| Effect presets | ⚠️ | Host-managed, project-confined JSON presets can be created, validated, and applied as ordered MCP effect actions; native Niagara graph semantics and transactional rollback remain engine/project dependent. |
| Attachment/lifespan/pooling | ⚠️ | Explicit actor attachment, bounded lifespan, and destruction are available; reusable effect pooling and project-specific ownership policies remain project-authored. |
| VFX performance validation | ⚠️ | `validate_niagara_system` provides explicit emitter/renderer budget gates; GPU/CPU cost, memory, and frame-time budgets still require project-specific profiling captures. |

Evidence: [native-automation-progress.md](./native-automation-progress.md#niagara--effect-handlers), [NiagaraHandlers.cpp](../plugins/NebulaForgeBridge/Source/NebulaForgeBridge/Private/NebulaForgeBridge_NiagaraHandlers.cpp#L247)

## Audio

| Capability | Status | Gap |
|---|:---:|---|
| Basic audio authoring | ✅ | Sound, attenuation, mix, and component paths exist. |
| MetaSound authoring | ⚠️ | Requires the MetaSound module/plugin. |
| Dialogue system | ⚠️ | Conditional on project support. |
| Reverb authoring | ⚠️ | Can return `REVERB_NOT_AVAILABLE`. |
| Localization/subtitle/voice-bank workflow | ⚠️ | Host-managed, project-confined localization manifests now support culture declarations, stable keys, translations, voice-asset mappings, entry updates, and completeness validation; Unreal localization target cooking, subtitle widgets, and external voice-generation/import remain project/platform dependent. |
| Audio validation | ⚠️ | `manage_audio.validate_audio_asset` gates asset type, duration, sample rate, and channel count; runtime mix loudness and packaged codec validation remain project-dependent. |

Evidence: [AudioAuthoringHandlers.cpp](../plugins/NebulaForgeBridge/Source/NebulaForgeBridge/Private/NebulaForgeBridge_AudioAuthoringHandlers.cpp#L885)

## Multiplayer and online services

| Capability | Status | Gap |
|---|:---:|---|
| Replication primitives | ✅ | Property replication, RPC, ownership, relevancy, and prediction controls exist. |
| Local/LAN sessions | ✅ | LAN hosting/joining and local multiplayer are covered. |
| Matchmaking and lobbies | ⚠️ | Provider-agnostic Online Subsystem session create/find/join/destroy lifecycle is implemented; provider-specific lobbies and matchmaking policies remain incomplete. |
| Presence/account/platform identity | ⚠️ | Capability discovery plus `get_online_identity_status`, `get_online_presence`, and `set_online_presence` report and update provider presence when the active identity/presence interfaces support it; provider-specific account, friends, and platform UI flows remain incomplete. |
| Dedicated-server build/deploy | ⚠️ | `run_uat` now supports server/no-client BuildCookRun variants with a separate server configuration; deployment and platform hosting remain absent. |
| Multiplayer soak/latency/reconnect tests | ⚠️ | Packaged server/client soak orchestration now has bounded server and per-client readiness gates plus cleanup; replication assertions, latency thresholds, reconnect scenarios, and provider matchmaking remain project-authored. |
| Platform-grade voice integration | ⚠️ | Basic voice controls exist, but platform/backend integration is incomplete. |

## AI and runtime gameplay

| Capability | Status | Gap |
|---|:---:|---|
| Behavior Trees and EQS | ✅ | Authoring and runtime query actions exist; runtime execution requires a PIE world and project-authored assets. |
| StateTree, Smart Objects, Mass AI | ⚠️ | Native authoring paths exist and report module availability; each requires its relevant plugin/module. |
| Navigation authoring | ✅ | Nav bounds, modifiers, links, path queries, validation, and rebuild actions exist. |
| AI behavior visualization | ⚠️ | Gameplay Debugger availability is reported and debug runtime actions exist; project categories and live PIE state remain prerequisites. |
| Large-crowd performance validation | ⚠️ | Mass AI authoring is available when enabled, but no universal production-scale soak/benchmark policy is assumed. |
| Complete game-specific AI architecture | ⚠️ | Controllers, perception, behavior, navigation, StateTree, Smart Object, and Mass primitives exist; project classes/assets and game rules remain required. |
| Inventory and interaction primitives | ✅ | Broad component, pickup, equipment, crafting, interaction, and UI authoring exists. |
| Save-backed inventory progression | ⚠️ | Inventory authoring exists, but SaveGame slot persistence remains a project integration boundary. |

AI/module and persistence readiness can be queried with `inspect_ai_capabilities` before authoring.

## Assets and content management

| Capability | Status | Gap |
|---|:---:|---|
| Basic asset create/load/save/delete | ✅ | Broad editor coverage exists and is exposed through the asset workflow handler. |
| Source-control checkout/submit | ⚠️ | Native checkout/submit/state actions are implemented; success depends on an enabled editor source-control provider and workspace state. |
| Generic asset tagging/metadata | ✅ | Generic tag and package-metadata set/get/find actions are implemented with normalized asset paths. |
| Nanite rebuild workflow | ✅ | `nanite_rebuild_mesh` is implemented through the render handler for static meshes, subject to Nanite/engine support. |
| Cube/volume/array texture generation | ⚠️ | Source-backed import/assembly is supported; arbitrary generated cube, volume, and array sources remain intentionally unsupported. |
| Dependency-aware migration | ⚠️ | Dependency inspection plus duplicate/move operations exist; transactional dependency closure and release migration are not automatic. |
| Redirector cleanup | ✅ | `fixup_redirectors` performs editor/content-browser cleanup and reports the affected scope. |
| Asset validation and audit gates | ⚠️ | Per-asset validation and `generate_report` are implemented; a project-wide cook/package release gate remains outside the asset handler. |

Evidence: [AssetWorkflowHandlers.cpp](../plugins/NebulaForgeBridge/Source/NebulaForgeBridge/Private/NebulaForgeBridge_AssetWorkflowHandlers.cpp#L2736), [TextureHandlers.cpp](../plugins/NebulaForgeBridge/Source/NebulaForgeBridge/Private/NebulaForgeBridge_TextureHandlers.cpp#L3417), `inspect_asset_capabilities`

## World building

| Capability | Status | Gap |
|---|:---:|---|
| Landscape creation/editing | ⚠️ | Core creation/editing plus advertised heightmap, erosion, regional sculpt, rule-paint, inspection, deletion, and foliage actions route to native handlers; topology resize remains an explicit heightmap-reimport boundary. |
| Procedural heightmap/erosion workflow | ✅ | Native deterministic heightmap generation supports terrain features, seeded frequency/scale, optional source height data, and bounded thermal erosion iterations; writes use the existing persistence-aware landscape heightmap path. |
| Foliage authoring/scattering | ✅ | Native foliage types, instances, deterministic HISM scattering, inspection, regeneration, and generated-only clearing are implemented; valid assets and editor/world prerequisites are reported by `inspect_world_building_capabilities`. |
| PCG graph authoring | ⚠️ | Native graph/node authoring exists; the PCG plugin/editor module is required and generation remains asynchronous. Waited generation can now use the shared managed native async registry with cancellation, timeout, completion events, and `get_async_action` polling; the `async` contract is exposed on both TS and native PCG schemas. Capability availability is reported before authoring. |
| World Partition conversion | ⚠️ | New World Partition levels and configuration are supported; existing non-World-Partition maps return an explicit `editor_conversion_required` capability instead of claiming conversion succeeded. |
| Scoped HLOD rebuilds | ⚠️ | Whole-map and HLOD-layer commandlet rebuilds are supported; UE 5.8 does not expose cell/Data Layer scopes through the commandlet, and the limitation is reported explicitly. |
| Water systems | ⚠️ | Require the Water plugin and available classes. |
| Road/river spline authoring | ✅ | Spline and mesh scattering primitives exist. |
| Full traffic/road infrastructure | ⚠️ | Road/river spline and mesh primitives are native; lane logic, traffic simulation, terrain cutting, and project-specific decals require project gameplay/assets and are surfaced as unavailable by the capability report. |
| Production biome pipeline | ⚠️ | The constituent landscape, procedural heightmap/erosion, material, foliage, water, navigation, spline, and streaming actions are available; a universal project-independent recipe is intentionally not synthesized without project assets and validation policy. |

Evidence: [UE 5.8 compatibility matrix](./ue5.8-compatibility-matrix.md), [EnvironmentHandlers.cpp](../plugins/NebulaForgeBridge/Source/NebulaForgeBridge/Private/NebulaForgeBridge_EnvironmentHandlers.cpp#L2418), [LevelStructureHandlers.cpp](../plugins/NebulaForgeBridge/Source/NebulaForgeBridge/Private/NebulaForgeBridge_LevelStructureHandlers.cpp#L1253)

## Editor, runtime, and transport limitations

| Capability | Status | Gap |
|---|:---:|---|
| Editor automation | ✅ | Strongest supported execution mode. |
| Packaged plugin loading | ❌ | The shipped module is intentionally `Type: Editor` and the bridge subsystem derives from `UEditorSubsystem`; packaged targets do not load the MCP bridge. |
| In-process PIE runtime authoring | ⚠️ | Actor, environment, effect, input, and inspection actions can target an editor-owned PIE world. This is runtime-world mutation for verification, not packaged-game authoring or persistent asset creation. |
| Packaged/runtime asset authoring | ❌ | Blueprint, material, landscape, Niagara graph, sequence, import, save, and other asset-authoring actions depend on editor-only APIs and are not available in packaged builds. |
| In-process viewport PIE | ✅ | Best-supported runtime verification mode. |
| Standalone PIE probes/input | ❌ | In-process probes and input delivery are not supported because standalone runs in another process. |
| Standalone-window screenshots | ⚠️ | Direct external-window capture remains unsupported, but `mode: standalone_window` now safely reads a PNG written by the standalone game under `Saved/Screenshots` through `screenshotPath`. |
| Unified asynchronous job system | ⚠️ | Host processes, bounded filesystem tasks, waited PCG generation, and HLOD commandlets expose managed lifecycle IDs; host jobs provide output caps, while native PCG and HLOD provide cancellation, timeout, completion events, and polling. Shader/asset jobs still use separate Unreal-side state machines. |
| Completion proof | ⚠️ | Host jobs expose `wait_for_job`, and native managed async actions expose `wait_for_async_action` with timeout/cancel/success state reporting; editor-native PCG and HLOD now feed that registry, while shader/asset jobs still use separate Unreal-side state machines. |
| Undo/redo transactions | ⚠️ | Undo/redo now report success only when the editor accepts the command; transactional coverage remains incomplete across authoring handlers. |

The WebSocket handshake now advertises `executionMode: editor`, `pieRuntimeWorld: true`, and `packagedRuntimeAuthoring: false` so clients can select a supported execution path explicitly. A true packaged authoring implementation still requires a separate Runtime module, runtime-safe handler set, and packaged transport; changing `WITH_EDITOR` guards alone would be unsafe.

Evidence: [UE 5.8 compatibility matrix](./ue5.8-compatibility-matrix.md#pie-and-capture-modes), [NebulaForgeBridge.uplugin](../plugins/NebulaForgeBridge/NebulaForgeBridge.uplugin#L21), [NebulaForgeBridgeSubsystem.h](../plugins/NebulaForgeBridge/Source/NebulaForgeBridge/Public/NebulaForgeBridgeSubsystem.h#L108), [native-automation-progress.md](./native-automation-progress.md#niagara--effect-handlers)

## Documentation and test-quality gaps

- ❌ The roadmap marks several systems complete despite native `NOT_IMPLEMENTED` and `NOT_AVAILABLE` branches.
- ❌ Integration expectations often accept `success|not found`, which can conceal missing implementations.
- ⚠️ This document combines implementation status, persistence verification, runtime behavior, packaging, and release validation at static-audit level; live Unreal/project verification is still required.
- ❌ Most integration tests are feature-focused rather than full production-game workflows.
- ⚠️ Native compilation, World Partition readiness, shader completion, and true map reload persistence require a live UE Editor integration run.

Evidence: [Roadmap.md](./Roadmap.md), [testing-guide.md](./testing-guide.md), [UE 5.8 compatibility matrix](./ue5.8-compatibility-matrix.md#verification-boundary)

## Priority order for production readiness

1. Add signing/deployment, platform configuration, and dedicated-server packaging.
2. Add project-wide validation, functional tests, profiling, and release smoke gates.
3. Complete config hierarchy, Primary Assets, runtime Gameplay Tags, and async SaveGame orchestration.
4. Replace remaining editor-only stubs with verified native handlers where UE APIs support them.
5. Complete Sequencer, Movie Render Queue, media, replay, and Take Recorder support.
6. Add online services and multiplayer soak/reconnect testing.
7. Add unified asynchronous jobs, completion verification, transactions, and rollback.
### Latest implementation note (2026-08-30)

The host pipeline now supports controlled `sign_release` execution for Win64, Mac/iOS, and Android when the platform signing tool and credentials are explicitly supplied, plus `run_packaged` for bounded local packaged-runtime launch and job polling. Both validate artifact boundaries and support dry-run inspection. Deployment/upload to external stores or hosting providers remains intentionally unimplemented.

Audio validation update: `manage_audio.validate_audio_asset` now reuses the native audio inspection path and can gate known asset type, duration range, sample rate, and channel count with structured validation results.

Cinematics update: `manage_sequence.add_subsequence` now creates a real `MovieSceneSubTrack` section with bounded timing, duplicate/self-reference checks, and persisted parent-package mutation; `create_master_sequence` and `create_cine_camera_actor` remain canonical aliases for existing safe primitives.

### Status corrections

The historical capability tables above contain several stale gap markers. The current implementation also includes project/plugin validation, release archive and SHA-256 manifest gates, functional-test actor authoring, map checks, blueprint compile sweeps, redirector fixup, Insights session and file-trace capture, headless trace launch, memory/stat/network/visual profiling controls, platform toolchain inspection, local signing, packaged launch, Android/iOS simulator deployment, and bounded packaged server/client soak orchestration. These are still conditional on the required UE editor, optional plugins, platform SDKs, and project-specific assertions being available; native compilation and live project verification remain required release gates.

UE 5.8 packaging verification: the current bridge source compiled successfully through all 142 UnrealBuildTool actions and produced `NebulaForgeBridge-v0.5.30-UE5.8-Win64.zip` with the short-path package workflow on 2026-08-31. The plugin manifest now declares the optional `MovieRenderPipeline` dependency so MRQ module loading is explicit. This validates native compilation and packaging only; it does not replace live project, editor, PIE, packaged-game, or platform-service verification.

Host dispatch correction: consolidated `system_control` now routes `deploy_package`, `run_network_soak`, `analyze_trace`, and `release_gate` to their implemented host pipeline handlers. Previously these actions could fall through to the native bridge and be rejected as host-only even though the host implementations were available.
