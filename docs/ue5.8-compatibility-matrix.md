# NebulaForge UE 5.8 Compatibility Matrix

Audit date: 2026-08-28  
Target: Unreal Engine 5.8 Editor and UE 5.8 projects  
Scope: the 23 consolidated MCP tools, their exposed feature groups, and the
Landscape/PIE/screenshot capabilities recently added to the bridge.

## Status meanings

| Status | Meaning |
|---|---|
| **COMPATIBLE** | The feature has a UE 5.8 implementation path and no known engine-version blocker. |
| **CONDITIONAL** | Supported in UE 5.8 only when the required editor build, plugin/module, project setup, asset type, or runtime mode is present. |
| **PARTIAL** | The request surface exists, but one or more documented UE 5.8 modes or guarantees are intentionally not implemented. |
| **NOT COMPATIBLE** | The feature must not be reported as supported for UE 5.8. The bridge should return an explicit error or PARTIAL result. |

## Consolidated feature inventory

This is the complete top-level feature inventory exposed by
`src/tools/consolidated-tool-definitions.ts`. Individual actions are grouped
under their parent tool in the source of truth.

| Parent tool | Feature coverage | UE 5.8 status | Notes |
|---|---|---:|---|
| `manage_tools` | Dynamic tool listing, categories, enable/disable, status | COMPATIBLE | MCP/server capability; no UE API dependency. Protected actions remain available. |
| `manage_asset` | Asset create/load/save/delete/duplicate/import/export and inspection | CONDITIONAL | Requires editor build and valid project/content roots; asset behavior remains type-dependent. |
| `manage_blueprint` | Blueprint creation, graphs, nodes, pins, SCS/components, compilation | CONDITIONAL | Editor-only. Subobject Data paths are version guarded; compilation can be asynchronous. |
| `control_actor` | Spawn, transform, components, properties, collision, selection, debug drawing | CONDITIONAL | Editor-world operations require `WITH_EDITOR`; runtime operations require the correct PIE world. |
| `control_editor` | PIE, camera, viewport, input, runtime probes, screenshots, bounded playtests | PARTIAL | In-process viewport PIE is fully instrumented. Standalone cannot provide in-process probes/input and standalone-window capture is rejected. |
| `manage_level` | Maps, level loading, level actors, streaming, save/reload | CONDITIONAL | Requires saved `/Game` maps for persistence-sensitive operations. |
| `build_environment` | Landscapes, edit layers, heightmaps, materials, foliage, splines, water, weather, procedural building | CONDITIONAL | Landscape core and edit layers are UE 5.8-compatible; optional plugins/assets and World Partition setup affect individual actions. |
| `animation_physics` | Animation assets, montages, physics assets, constraints, control/retarget operations | CONDITIONAL | Asset/editor module availability and asset class determine support. |
| `system_control` | Console/CVar control, profiling, screenshots, widgets, UBT/tests, logs | CONDITIONAL | Editor/runtime split applies. Console commands are allow/deny-list validated. |
| `manage_sequence` | Level Sequence tracks, bindings, sections, keyframes, playback | CONDITIONAL | Editor-only authoring; runtime playback requires a valid sequence/world. |
| `inspect` | Actor, component, asset, world, runtime and diagnostic inspection | CONDITIONAL | PIE-only probes reject editor-world fallbacks by design. |
| `manage_audio` | Sounds, attenuation, mixes, MetaSounds, audio components and playback | CONDITIONAL | Runtime audio requires an audio-capable world/device; MetaSound authoring requires its plugin/module. |
| `manage_geometry` | Geometry collections, modeling/mesh operations and inspection | CONDITIONAL | Requires the corresponding UE editor plugins and compatible asset types. |
| `manage_pcg` | PCG graphs, nodes, pins, generation, data layers and World Partition | CONDITIONAL | PCG and World Partition/Data Layer features require the UE 5.8 PCG/editor modules. |
| `manage_effect` | Niagara and gameplay effect-style effect operations | CONDITIONAL | Niagara/plugin and editor/runtime context determine which actions can execute. |
| `manage_gas` | Gameplay Ability System abilities, effects, attributes and cues | CONDITIONAL | Requires the project to include/configure GAS; the bridge does not install project modules. |
| `manage_character` | Character/pawn setup, movement, camera, input and player configuration | CONDITIONAL | Requires valid project classes and a PIE/runtime world for behavioral checks. |
| `manage_combat` | Combat, damage, targeting, hit reactions and related runtime systems | CONDITIONAL | Project gameplay classes/interfaces are prerequisites. |
| `manage_ai` | AI controllers, behavior trees, perception, EQS and navigation | CONDITIONAL | Requires NavigationSystem/AI modules and project-authored assets. |
| `manage_inventory` | Inventory, items, pickups, recipes and loot structures | CONDITIONAL | Project gameplay data/classes are prerequisites; no stock UE inventory model exists. |
| `manage_interaction` | Interactable actors, prompts, traces and interaction rules | CONDITIONAL | Requires project interfaces/classes and a valid runtime player/controller. |
| `manage_networking` | Replication, RPCs, sessions, LAN, split-screen and voice settings | CONDITIONAL | Requires the selected online subsystem/platform and valid network session configuration. |
| `manage_level_structure` | Levels, streaming, World Partition, Data Layers, HLOD, level instances and volumes | CONDITIONAL | World Partition/Data Layers are UE 5.8-compatible but require a partitioned map, generated data, and editor subsystems. |

## Requested capability audit

| Capability | UE 5.8 result | Implementation/evidence |
|---|---:|---|
| Create Landscape Edit Layer | COMPATIBLE | `create_landscape_edit_layer`; calls UE 5.8 `ALandscape::CreateLayer(FName, TSubclassOf<ULandscapeEditLayerBase>, bool)` with a null class for the standard `ULandscapeEditLayer`. |
| List Landscape Edit Layers | COMPATIBLE | `list_landscape_edit_layers`; enumerates `ALandscape::GetLayersConst()` and returns names, GUIDs, active state, and count. |
| Verify Landscape Edit Layers | CONDITIONAL | `verify_landscape_edit_layers`; checks requested names, layer count, saved package existence, and returns explicit `PASS`/`PARTIAL`/`FAIL` evidence. A live unload/reopen cycle still requires an integration test against a UE 5.8 Editor process. |
| Save/reload persistence | CONDITIONAL | Saves through project safe-save wrappers and verifies package existence and layer metadata. It must not claim a full reload proof unless the test actually closes/reloads the map or asset. |
| World Partition preparation before PIE capture | CONDITIONAL | `prepare_pie_capture` makes Data Layers visible/loaded in the editor and flushes level streaming before capture. A partitioned map and usable Data Layer editor subsystem are required. |
| Runtime Data Layer activation | CONDITIONAL | `prepare_pie_capture` applies `Activated` through the UE 5.8 World Data Layers API when a PIE world and runtime Data Layer instances are available. Server/client load-filter rules still apply. |
| Wait for texture streaming | CONDITIONAL | The capture path requests `IStreamingManager::StreamAllResources` and flushes rendering commands. Actual completion still depends on resource availability and platform/RHI behavior. |
| Wait for shader compilation | CONDITIONAL | The capture path uses `FShaderCompilingManager` when available and verifies no compilation remains. Asset compilation and shader compilation are asynchronous in UE 5.8. |
| Reject black screenshots | COMPATIBLE | Capture retries and returns `BLACK_FRAME` when no visible pixels remain. |
| Reject empty screenshots | COMPATIBLE | Low-variation frames return `EMPTY_FRAME`. |
| Reject grid/checker placeholders | CONDITIONAL | Heuristic detection returns `GRID_ONLY_FRAME`; it is intentionally evidence validation, not semantic scene understanding. |
| Bounded PIE test | COMPATIBLE | Client-side hard deadline is 60 seconds, with `finally` cleanup and stop-state verification. |
| Clean PIE shutdown | COMPATIBLE | Cleanup calls stop and polls `get_pie_state`; failure to stop is reported rather than hidden. |
| Truthful results | COMPATIBLE | Playtests and verification return explicit `status`, `success`, step evidence, cleanup evidence, and failure codes. `PARTIAL` is not converted to `PASS`. |

## UE 5.8-supported but conditional modes

### Landscape

UE 5.8 documents Edit Layers as non-destructive heightmap/weightmap containers,
automatically enabled for new landscapes, with a configurable maximum layer
count and a default limit of eight. The bridge must preserve at least one
layer and should not silently exceed the project limit. Specialized Spline and
Patch edit layers are procedural and must not be treated as ordinary sculpt or
paint layers.

The current generic create action creates the standard layer type. It does not
claim to create a specialized Patch/Spline edit-layer class.

### World Partition and Data Layers

World Partition operations are conditional on a partitioned map and generated
streaming data. Data Layer preparation can make layers visible and loaded in
the editor before PIE, but runtime activation must be verified in the actual
PIE world. Empty cell results, unavailable generated data, or missing editor
subsystems are evidence for PARTIAL/FAIL, not PASS.

### PIE and capture modes

The UE 5.8 documentation distinguishes viewport PIE, new-window PIE, and
standalone game. The bridge provides the strongest evidence guarantees for
in-process viewport PIE. Standalone is a separate process: in-process actor
probes and input delivery are not supported, and standalone-window capture is
not reported as successful by the editor viewport path.

### Streaming and compilation

Texture streaming and shader compilation are asynchronous. A draw call alone
is not proof that all resources are ready. Capture evidence therefore includes
`streamingReady`, `shadersReady`, retry count, non-black pixel count, viewport
world, and camera metadata when available.

## Not compatible / must remain rejected

These modes must not be reported as full UE 5.8 support:

- Standalone PIE runtime probes that require `GEditor->PlayWorld`.
- Standalone-window screenshots through the in-process viewport readback path.
- Full runtime Data Layer state coverage when the map has no World Partition,
  generated streaming data, or Data Layer subsystem.
- Specialized Landscape Patch/Spline edit-layer creation through the generic
  standard-layer action.
- Persistence claims based only on `MarkPackageDirty()` or an in-memory object;
  a save/package check or an actual reload is required.
- Screenshot PASS results when the frame is black, empty, grid-only, unreadable,
  or captured from the wrong world.

## Official Unreal Engine references

- [Landscape Edit Layers](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-edit-layers-in-unreal-engine?lang=en-US)
- [`ALandscape::CreateLayer` API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Landscape/ALandscape/CreateLayer)
- [Creating Landscapes](https://dev.epicgames.com/documentation/en-us/unreal-engine/creating-landscapes-in-unreal-engine)
- [Importing and Exporting Landscape Heightmaps](https://dev.epicgames.com/documentation/en-us/unreal-engine/importing-and-exporting-landscape-heightmaps-in-unreal-engine)
- [Landscape Patch System](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-patch-system)
- [In-Editor Testing (Play and Simulate)](https://dev.epicgames.com/documentation/en-us/unreal-engine/ineditor-testing-play-and-simulate-in-unreal-engine?lang=en-US)
- [Playing and Simulating](https://dev.epicgames.com/documentation/unreal-engine/playing-and-simulating-in-unreal-engine)
- [Texture Streaming Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/texture-streaming-overview-for-unreal-engine)
- [Texture Streaming Configuration](https://dev.epicgames.com/documentation/en-us/unreal-engine/texture-streaming-configuration-in-unreal-engine)
- [`FShaderCompilingManager`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FShaderCompilingManager)
- [`FAssetCompilingManager`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FAssetCompilingManager?lang=en-US)
- [Using PCG with World Partition](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-with-world-partition-in-unreal-engine)
- [World Partition](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine?lang=en-US)
- [Set Data Layer Runtime State](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/DataLayers/SetDataLayerRuntimeState)
- [`AWorldDataLayers::SetDataLayerRuntimeState`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/AWorldDataLayers)

## Verification boundary

This document is a source/API compatibility audit. TypeScript tests can verify
contract routing and result classification, but only a UE 5.8 Editor process
can prove native compilation, World Partition cell readiness, shader/resource
completion, and actual map unload/reload persistence. Those checks belong in
the UE 5.8 integration suite and should retain their screenshot/log/package
artifacts as evidence.
