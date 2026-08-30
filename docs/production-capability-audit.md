# NebulaForge Production Game-Building Capability Audit

Audit date: 2026-08-30  
Scope: MCP tools, TypeScript handlers, Unreal native handlers, documentation, and integration tests.  
Method: Static repository audit; runtime behavior still requires a live Unreal Editor/project verification pass.

Legend: ✅ available or substantially covered · ❌ missing, incomplete, or production-blocking · ⚠️ conditional

## Summary

NebulaForge provides broad Unreal Editor automation, but it is not yet a complete production game-building platform. The largest gaps are build/deployment, release validation, persistence/data systems, online services, cinematics/rendering, and project-specific gameplay architecture. The `inspect` tool's `production_capabilities` action exposes the same static boundary to clients.

## Production pipeline

| Capability | Status | Gap |
|---|:---:|---|
| Build and deployment tool | ❌ | No first-class `manage_build` tool. |
| Cook content | ❌ | `cook_content` remains a roadmap item. |
| Package/stage/archive project | ❌ | No complete shipping-build workflow. |
| Platform builds and signing | ❌ | Windows, Linux, Mac, iOS, Android, and console workflows are not implemented. |
| Plugin enable/disable management | ❌ | No complete project dependency/plugin management workflow. |
| Asset chunking, compression, encryption, PAK creation | ❌ | Release packaging controls are absent. |

Evidence: [Roadmap.md](./Roadmap.md#phase-32-build--deployment)

## Testing and release validation

| Capability | Status | Gap |
|---|:---:|---|
| Functional test authoring | ❌ | No production functional-test creation workflow. |
| Automation test execution/results | ❌ | No complete MCP-driven automation test pipeline. |
| Data Validation integration | ❌ | No project-wide data validation gate. |
| Blueprint validation sweep | ❌ | No comprehensive compile/error validation across a project. |
| Map error validation | ❌ | No complete map validation gate. |
| Cook/package smoke tests | ❌ | No integrated release smoke test. |
| Unreal Insights integration | ❌ | Trace capture and analysis remain incomplete. |
| Memory/network/visual profiling | ❌ | No complete production profiling workflow. |

Evidence: [Roadmap.md](./Roadmap.md#phase-33-testing--quality)

## Persistence and game data

| Capability | Status | Gap |
|---|:---:|---|
| SaveGame class authoring | ❌ | Not implemented. |
| Save/load/delete slots | ❌ | Not implemented. |
| Gameplay Tags authoring | ❌ | Not implemented. |
| Config hierarchy management | ❌ | Not implemented. |
| Data Assets and Primary Data Assets | ❌ | No complete authoring workflow. |
| DataTables and CurveTables | ❌ | Creation and row-management actions remain roadmap items. |
| Asset persistence verification | ⚠️ | Some systems verify package state; many operations only mark packages dirty or return completion without reload verification. |

Evidence: [Roadmap.md](./Roadmap.md#phase-31-data--persistence)

## Cinematics, rendering, and media

| Capability | Status | Gap |
|---|:---:|---|
| Master/subsequence/shot workflow | ❌ | Not implemented as a complete production workflow. |
| Camera cuts, fades, events, material tracks | ❌ | Missing from the expanded cinematic roadmap. |
| Movie Render Queue jobs | ❌ | No render-job, pass, burn-in, or queue automation. |
| Media Framework | ❌ | Media players, sources, textures, playlists, and playback are absent. |
| Take Recorder | ❌ | No recording workflow. |
| Demo/replay system | ❌ | No replay or killcam automation. |
| Lighting and post-processing controls | ✅ | Broad authoring coverage exists, but engine/project configuration remains conditional. |
| Scene captures and reflection controls | ✅ | Implemented with renderer-dependent limitations. |

Evidence: [Roadmap.md](./Roadmap.md#phase-30-cinematics--media)

## Blueprint and gameplay authoring

| Capability | Status | Gap |
|---|:---:|---|
| Basic Blueprint creation | ✅ | Native editor path exists. |
| Blueprint graph editing | ⚠️ | Coverage depends on available K2 schemas, headers, and node types. |
| Arbitrary K2 node support | ❌ | No complete generic node coverage. |
| Blueprint compilation | ⚠️ | Can be asynchronous and is not always proven complete. |
| Project-specific gameplay architecture | ❌ | Tools create primitives but do not generate a complete tested game architecture. |
| Gameplay error handling and validation | ❌ | No project-wide gameplay validation gate. |

Evidence: [BlueprintHandlers.cpp](../plugins/NebulaForgeBridge/Source/NebulaForgeBridge/Private/NebulaForgeBridge_BlueprintHandlers.cpp#L4248)

## Animation and character systems

| Capability | Status | Gap |
|---|:---:|---|
| Basic animation asset creation | ✅ | Several asset authoring paths exist. |
| Bone tracks and bone keys | ⚠️ | Unsupported paths exist by engine/build configuration. |
| Curve key authoring | ⚠️ | Not consistently available across supported engine versions. |
| Montage section editing | ⚠️ | Some timing/editing APIs are unavailable. |
| Control Rig graph editing | ❌ | Controls, rig units, and pin connections are unsupported in some builds. |
| IK Rig chain editing | ❌ | Requires manual IK Rig editor authoring. |
| Ragdoll setup/activation | ⚠️ | Editor-only limitations remain. |
| Animation preview/compression validation | ❌ | No complete production validation workflow. |

Evidence: [AnimationHandlers.cpp](../plugins/NebulaForgeBridge/Source/NebulaForgeBridge/Private/NebulaForgeBridge_AnimationHandlers.cpp#L4260)

## Niagara and effects

| Capability | Status | Gap |
|---|:---:|---|
| Niagara authoring | ⚠️ | Depends on Niagara/plugin/editor availability. |
| Niagara system/emitter creation | ⚠️ | Native non-editor paths return `NOT_IMPLEMENTED`. |
| Niagara spawning and parameter mutation | ⚠️ | Multiple actions are editor-only or have unsupported paths. |
| Ribbon/trail authoring | ⚠️ | Conditional implementation. |
| Effect presets | ❌ | Legacy effect actions remain stubbed. |
| Attachment/lifespan/pooling | ❌ | No complete effect lifecycle management. |
| VFX performance validation | ❌ | No production VFX budget or profiling gate. |

Evidence: [native-automation-progress.md](./native-automation-progress.md#niagara--effect-handlers), [NiagaraHandlers.cpp](../plugins/NebulaForgeBridge/Source/NebulaForgeBridge/Private/NebulaForgeBridge_NiagaraHandlers.cpp#L247)

## Audio

| Capability | Status | Gap |
|---|:---:|---|
| Basic audio authoring | ✅ | Sound, attenuation, mix, and component paths exist. |
| MetaSound authoring | ⚠️ | Requires the MetaSound module/plugin. |
| Dialogue system | ⚠️ | Conditional on project support. |
| Reverb authoring | ⚠️ | Can return `REVERB_NOT_AVAILABLE`. |
| Localization/subtitle/voice-bank workflow | ❌ | No complete production audio pipeline. |
| Audio validation | ❌ | No automated runtime/audio mix validation. |

Evidence: [AudioAuthoringHandlers.cpp](../plugins/NebulaForgeBridge/Source/NebulaForgeBridge/Private/NebulaForgeBridge_AudioAuthoringHandlers.cpp#L885)

## Multiplayer and online services

| Capability | Status | Gap |
|---|:---:|---|
| Replication primitives | ✅ | Property replication, RPC, ownership, relevancy, and prediction controls exist. |
| Local/LAN sessions | ✅ | LAN hosting/joining and local multiplayer are covered. |
| Matchmaking and lobbies | ❌ | Online Services phase remains unimplemented. |
| Presence/account/platform identity | ❌ | Not covered. |
| Dedicated-server build/deploy | ❌ | No production server packaging/deployment pipeline. |
| Multiplayer soak/latency/reconnect tests | ❌ | No complete automated network test suite. |
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
| PCG graph authoring | ⚠️ | Native graph/node authoring exists; the PCG plugin/editor module is required and generation remains asynchronous. Capability availability is reported before authoring. |
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
| Unified asynchronous job system | ❌ | PCG, HLOD, shader, texture, and asset jobs use separate polling/timeout patterns. |
| Completion proof | ⚠️ | Several actions report “started” without proving final state. |
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

1. Implement cook/package/deploy and platform configuration workflows.
2. Add project-wide validation, functional tests, profiling, and release gates.
3. Implement SaveGame, Data Assets, DataTables, Gameplay Tags, and config management.
4. Remove MCP contract/native-handler mismatches and replace stubs.
5. Complete Sequencer, Movie Render Queue, media, replay, and Take Recorder support.
6. Add online services, dedicated-server packaging, and multiplayer soak testing.
7. Add unified asynchronous jobs, completion verification, transactions, and rollback.
