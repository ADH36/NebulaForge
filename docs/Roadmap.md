# Roadmap for Unreal Engine MCP Server

This roadmap outlines the comprehensive development plan for expanding the Unreal Engine Model Context Protocol (MCP) server into a **complete automation platform** capable of building full production projects (games, films, archviz, VR experiences, virtual production, etc.).

**Target**: ~2,855 actions covering all Unreal Engine subsystems and major plugin integrations.

---

## Phase 1: Architecture & Foundation (Completed)

- [x] **Native C++ Bridge**: Replace Python-based bridge with native C++ WebSocket plugin.
- [x] **Consolidated Tools**: Unify disparate tools into cohesive domains (`manage_asset`, `control_actor`, etc.).
- [x] **Modular Server**: Refactor monolithic `index.ts` into specialized subsystems (`ServerSetup`, `HealthMonitor`, `ResourceHandler`).
- [x] **Performance Optimization**: Pure TypeScript implementation for all math/JSON operations (WASM removed - FFI overhead made it 4-33x slower).
- [x] **Offline Capabilities**: Implement file-based fallback for project settings (`DefaultEngine.ini`).

## Phase 2: Graph & Logic Automation (Completed)

- [x] **Blueprint Graphs**: Add/remove nodes, pins, and properties (`manage_blueprint` graph actions).
- [x] **Material Graphs**: Edit material expressions and connections (`manage_asset` material actions).
- [x] **Niagara Graphs**: Edit emitters, modules, and parameters (`manage_effect` actions).
- [x] **Behavior Trees**: Edit AI behavior trees, tasks, and decorators (`manage_ai`).
- [x] **Environment**: Unified environment builder (`build_environment`) for landscape, foliage, and proc-gen.

## Phase 3: Cinematic & Visual Automation (Completed)

- [x] **Sequencer**: Full control over Level Sequences (create, play, tracks, keys, bindings).
- [x] **Audio**: Create SoundCues, play sounds, set mixes (`create_sound_cue`, `play_sound_at_location`).
- [x] **Landscape**: Sculpting, painting layers, modifying heightmaps (`sculpt_landscape`, `paint_landscape_layer`).
- [x] **Foliage**: Painting foliage instances and procedural spawning (`paint_foliage`).

## Phase 4: System & Developer Experience (Completed)

- [x] **Pipeline Integration**: Direct UBT execution with output streaming.
- [x] **Documentation**: Comprehensive handler mappings and API references.
- [x] **Metrics Dashboard**: `ue://health` view backed by bridge/server metrics.
- [x] **UE 5.7 Support**: Full compatibility with Unreal Engine 5.7 (Control Rig, Subobject Data).
- [x] **Enhanced Input Authoring**: Create Input Actions and Mapping Contexts, configure runtime context priority, key mappings, triggers, modifiers, legacy mappings, action value types, and inspect input assets through the consolidated `manage_input` surface. This covers Epic's documented Enhanced Input concepts for UE 5.8.

## Phase 5: Infrastructure Improvements (Current)

- [x] **Real-time Streaming**: Native MCP Streamable HTTP/SSE transport delivers progress and final results; `system_control.subscribe` streams editor logs and test jobs expose lifecycle polling.
- [x] **Extensibility Framework**: Dynamic handler registry via project-local JSON aliases and public custom C++ handler registration.
- [x] **Remote Profiling**: Trace sessions, file-backed `.utrace` capture, status, and host-side trace analysis are available through `system_control`/`manage_insights`.

## Context Reduction Initiative (Completed Workstream)

**Goal**: Reduce AI context overhead from ~78,000 tokens to ~25,000 tokens through multiple optimization strategies.

---

### CRI-1: Schema Pruning (Complete)

**Issue**: [#106](https://github.com/ADH36/NebulaForge/issues/106)

**Results**: ~23,000 token reduction

- [x] Remove "Supported actions:" lists from all tool descriptions (redundant with enum)
- [x] Remove "Use it when you need to:" bullet lists
- [x] Condense all descriptions to 1-2 sentences max
- [x] Remove parameter descriptions that just restate the parameter name
- [x] Audit and trim `manage_geometry`, `manage_asset`, `manage_effect`

---

### CRI-2: Common Schema Extraction (Complete)

**Issue**: [#108](https://github.com/ADH36/NebulaForge/issues/108)

**Results**: ~8,000 token reduction

- [x] Move repeated parameters to `commonSchemas` in `tool-definition-utils.ts`
- [x] Extract: `assetPath`, `actorName`, `location`, `rotation`, `scale`, `save`, `overwrite`
- [x] Define `standardResponse` schema for `success` and `message`
- [x] Update all tools to reference `commonSchemas`

---

### CRI-3: Dynamic Tool Loading (Complete)

**Issue**: [#109](https://github.com/ADH36/NebulaForge/issues/109)

**Results**: ~50,000 token reduction (when using category filtering)

- [x] Add `listChanged: true` to server capabilities
- [x] Add `manage_tools` tool with `list_tools`, `list_categories`, enable/disable, status, and reset actions
- [x] Define tool categories: `core`, `world`, `gameplay`, `utility`, `all`
- [x] Filter tools by category in `ListToolsRequestSchema` handler
- [x] Client capability detection via `mcp-client-capabilities` package
- [x] Backward compatibility: clients without `listChanged` support get ALL tools

---

### CRI-4: Strategic Tool Merging (Complete)

**Issue**: [#111](https://github.com/ADH36/NebulaForge/issues/111)

**Results**: Consolidated the public MCP surface to 23 canonical tools while keeping domain actions on parent tools.

#### Tool Consolidations
- [x] Blueprint graph and widget authoring actions live on `manage_blueprint`
- [x] Material authoring and texture actions live on `manage_asset`
- [x] Behavior Tree and navigation actions live on `manage_ai`
- [x] Sessions, game framework, and input actions live on `manage_networking`
- [x] Volumes live on `manage_level_structure`
- [x] Performance, UBT, tests, logs, and Python actions live on `system_control`
- [x] Skeleton and animation authoring actions live on `animation_physics`

#### Public Surface
- [x] Former child tool names are no longer exposed or accepted as direct MCP tool names
- [x] Action routing resolves consolidated actions on their canonical parent tools

---

# Advanced Capabilities Roadmap

The following phases represent the comprehensive expansion to enable **full project creation** - from simple prototypes to AAA-quality games, animated films, and virtual production pipelines.

---

## Phase 6: Geometry & Mesh Creation (Complete)

**Goal**: Enable AI to CREATE actual 3D geometry, not just place existing meshes.

**Tool**: `manage_geometry`

**Status**: All primitives, booleans, modeling operations, deformers, topology, mesh processing, mesh repair, UV operations, collision, LOD, and Nanite conversion implemented.

### 6.1 Primitives
- [x] `create_box`, `create_sphere`, `create_cylinder`, `create_cone`
- [x] `create_torus`, `create_capsule`, `create_plane`, `create_disc`
- [x] `create_stairs`, `create_spiral_stairs`, `create_ring`
- [x] `create_ramp`, `create_arch`, `create_pipe`

### 6.2 Boolean/CSG Operations
- [x] `boolean_union`, `boolean_subtract`, `boolean_intersection`
- [x] `boolean_trim`, `self_union`

### 6.3 Modeling Operations
- [x] `extrude`, `inset`, `outset`
- [x] `bevel`, `offset_faces`, `shell`
- [x] `extrude_along_spline`
- [x] `chamfer`
- [x] `bridge`, `loft`, `sweep`
- [x] `revolve`
- [x] `mirror`, `symmetrize`
- [x] `array_linear`, `array_radial`
- [x] `duplicate_along_spline`
- [x] `loop_cut`, `edge_split`
- [x] `poke`, `triangulate`
- [x] `quadrangulate`

### 6.4 Deformers
- [x] `bend`, `twist`, `taper`
- [x] `noise_deform`, `smooth`
- [x] `stretch`
- [x] `lattice_deform`
- [x] `spherify`, `cylindrify`
- [x] `relax`
- [x] `displace_by_texture`

### 6.5 Mesh Processing
- [x] `subdivide` (PN tessellation)
- [x] `simplify_mesh` (QEM decimation)
- [x] `remesh_uniform`
- [x] `weld_vertices`
- [x] `remove_degenerates`, `fill_holes`
- [x] `flip_normals`, `recalculate_normals`
- [x] `remesh_voxel`
- [x] `merge_vertices`
- [x] `recompute_tangents`

### 6.6 UV Operations
- [x] `auto_uv` (XAtlas)
- [x] `project_uv` (box, planar, cylindrical)
- [x] `unwrap_uv`, `pack_uv_islands`
- [x] `transform_uvs`, `scale_uvs`, `rotate_uvs`

### 6.7 Collision Generation
- [x] `generate_collision` (convex, box, sphere, capsule, decomposition)
- [x] `generate_complex_collision`
- [x] `simplify_collision`

### 6.8 LOD & Output
- [x] `generate_lods`, `set_lod_settings`
- [x] `set_lod_screen_sizes`
- [x] `convert_to_static_mesh` (save DynamicMesh as StaticMesh asset)
- [x] `convert_to_nanite`

### 6.9 Mesh Query
- [x] `get_mesh_info` (vertex/triangle count, UV/normal status)
- [x] `get_mesh_info` also reports official GeometryScript surface area and volume
- [x] `get_mesh_info` also reports official GeometryScript center of mass
- [x] `get_mesh_info` also reports official GeometryScript diagnostic info string
- [x] `get_mesh_info` also reports official GeometryScript attribute-set state
- [x] `get_mesh_info` also reports official GeometryScript topology state and connectivity counts on UE 5.5+ (`isClosed`, `isDense`, open-border loops/edges, connected islands)
- [x] `get_mesh_info` also reports official GeometryScript vertex/triangle ID-gap diagnostics on UE 5.5+ (`hasVertexIDGaps`, `hasTriangleIDGaps`)
- [x] `get_mesh_info` also reports the official GeometryScript local bounding box (`bounds.min`, `bounds.max`, `bounds.center`, `bounds.extent`)
- [x] `get_uv_set_bounds` (official GeometryScript UV-channel bounds query)
- [x] `get_uv_set_bounds` also returns `uvArea` and valid UV-triangle count (official GeometryScript UV-area query semantics)
- [x] `get_num_uv_islands` (official GeometryScript query; requires UE 5.5+)

> **Note**: Geometry Collection for destruction is in Phase 46.1 (Chaos Destruction).

---

## Phase 7: Skeletal Mesh & Rigging (Complete)

**Goal**: Enable creation and editing of animated characters with proper rigs.

**Tool**: `animation_physics`

**Status**: All 29 actions fully implemented in TypeScript and C++.

### 7.1 Skeleton Creation
- [x] `get_skeleton_info`, `list_bones`, `list_sockets` (query operations)
- [x] `create_socket`, `configure_socket`
- [x] `create_virtual_bone`
- [x] `rename_bone` (virtual bones only; regular bones require reimport)
- [x] `set_bone_transform` (reference pose modification)
- [x] `create_skeleton` (uses FReferenceSkeletonModifier)
- [x] `add_bone`, `remove_bone`, `set_bone_parent` (uses FReferenceSkeletonModifier)

### 7.2 Skin Weights
- [x] `normalize_weights` (rebuilds mesh)
- [x] `prune_weights` (rebuilds mesh with threshold)
- [x] `auto_skin_weights` (triggers mesh rebuild)
- [x] `set_vertex_weights` (uses FSkinWeightProfileData)
- [x] `copy_weights`, `mirror_weights` (skin weight profile operations)

### 7.3 Physics Asset
- [x] `create_physics_asset`, `list_physics_bodies`
- [x] `add_physics_body` (capsule, sphere, box, convex)
- [x] `configure_physics_body` (mass, damping, collision)
- [x] `add_physics_constraint`
- [x] `configure_constraint_limits`

### 7.4 Cloth Setup (Basic)
> **Note**: Full cloth simulation configuration is in Phase 46.3 (Chaos Cloth). This section covers skeletal mesh cloth binding only.

- [x] `bind_cloth_to_skeletal_mesh`
- [x] `assign_cloth_asset_to_mesh`

### 7.5 Morph Targets
- [x] `create_morph_target`
- [x] `set_morph_target_deltas`
- [x] `import_morph_targets` (lists targets; FBX import via asset pipeline)

---

## Phase 8: Advanced Material Authoring (Complete)

**Goal**: Full material creation and shader authoring capabilities.

**Tool**: `manage_asset`

**Status**: All 39 actions fully implemented in TypeScript and C++.

### 8.1 Material Creation
- [x] `create_material` (surface, deferred_decal, light_function, post_process, UI, volume)
- [x] `set_blend_mode` (opaque, masked, translucent, additive, modulate)
- [x] `set_shading_model` (default_lit, unlit, subsurface, clear_coat, two_sided_foliage, hair, eye, cloth)
- [x] `set_material_domain` (surface, deferred_decal, light_function, post_process, UI, volume)

### 8.2 Material Expressions (Node Graph)
- [x] `add_texture_sample`, `add_texture_coordinate`
- [x] `add_scalar_parameter`, `add_vector_parameter`
- [x] `add_static_switch_parameter`
- [x] `add_math_node` (add, multiply, divide, power, lerp, clamp, etc.)
- [x] `add_world_position`, `add_vertex_normal`, `add_pixel_depth`
- [x] `add_fresnel`, `add_reflection_vector`
- [x] `add_panner`, `add_rotator`
- [x] `add_noise`, `add_voronoi`
- [x] `add_if`, `add_switch`
- [x] `add_custom_expression` (HLSL code)
- [x] `connect_nodes`, `disconnect_nodes`

### 8.3 Material Functions & Layers
- [x] `create_material_function`
- [x] `add_function_input`, `add_function_output`
- [x] `use_material_function`

### 8.4 Material Instances
- [x] `create_material_instance`
- [x] `set_scalar_parameter_value`
- [x] `set_vector_parameter_value`
- [x] `set_texture_parameter_value`

### 8.5 Specialized Materials
- [x] `create_landscape_material`, `add_landscape_layer`, `configure_layer_blend`
- [x] `create_decal_material`
- [x] `create_post_process_material`

### 8.6 Utilities
- [x] `compile_material`
- [x] `get_material_info`

---

## Phase 9: Texture Generation & Processing (Complete)

**Goal**: Procedural texture creation and processing.

**Tool**: `manage_asset`

**Status**: All 21 actions fully implemented in TypeScript and C++.

### 9.1 Procedural Generation
- [x] `create_noise_texture` (perlin, simplex, worley, voronoi)
- [x] `create_gradient_texture` (linear, radial, angular)
- [x] `create_pattern_texture` (checker, grid, brick, tile, dots, stripes)
- [x] `create_normal_from_height` (Sobel, Prewitt, Scharr algorithms)
- [x] `create_ao_from_mesh`

### 9.2 Texture Processing
- [x] `resize_texture`
- [x] `adjust_levels`, `adjust_curves` (LUT-based curve adjustment)
- [x] `blur`, `sharpen`
- [x] `invert`, `desaturate`
- [x] `channel_pack`, `channel_extract` (extract R/G/B/A to grayscale)
- [x] `combine_textures` (blend modes)

### 9.3 Texture Settings
- [x] `set_compression_settings` (TC_Default, TC_Normalmap, TC_Masks, etc.)
- [x] `set_texture_group` (TEXTUREGROUP_World, Character, UI, etc.)
- [x] `set_lod_bias`
- [x] `configure_virtual_texture`
- [x] `set_streaming_priority`

### 9.4 Utility
- [x] `get_texture_info` (width, height, format, compression, mip count, sRGB, etc.)

---

## Phase 10: Complete Animation System (Complete)

**Goal**: Full animation authoring from keyframes to state machines.

**Tool**: `animation_physics`

**Status**: All listed actions fully implemented in TypeScript and C++.

### 10.1 Animation Sequences
- [x] `create_animation_sequence`
- [x] `set_sequence_length`
- [x] `add_bone_track`
- [x] `set_bone_key` (location, rotation, scale at time)
- [x] `set_curve_key` (float curve at time)
- [x] `add_notify`, `add_notify_state`
- [x] `add_sync_marker`
- [x] `set_root_motion_settings`
- [x] `set_additive_settings`
- [x] `validate_animation_asset` (asset type, duration, and skeleton gates)

### 10.2 Animation Montages
- [x] `create_montage`
- [x] `add_montage_section`
- [x] `add_montage_slot`
- [x] `set_section_timing`
- [x] `add_montage_notify`
- [x] `set_blend_in`, `set_blend_out`
- [x] `link_sections`

### 10.3 Blend Spaces
- [x] `create_blend_space_1d`
- [x] `create_blend_space_2d`
- [x] `add_blend_sample`
- [x] `set_axis_settings`
- [x] `set_interpolation_settings`
- [x] `create_aim_offset`, `add_aim_offset_sample`

### 10.4 Animation Blueprints
- [x] `create_anim_blueprint`
- [x] `add_state_machine`
- [x] `add_state`, `add_transition`
- [x] `set_transition_rules`
- [x] `add_blend_node`
- [x] `add_cached_pose`
- [x] `add_slot_node`
- [x] `add_layered_blend_per_bone`
- [x] `set_anim_graph_node_value`

### 10.5 Control Rig
- [x] `create_control_rig`
- [x] `add_control`
- [x] `add_rig_unit` (FKIK, aim, basic_ik, etc.)
- [x] `connect_rig_elements`
- [x] `create_pose_library`

### 10.6 Retargeting
- [x] `create_ik_rig`
- [x] `add_ik_chain`
- [x] `create_ik_retargeter`
- [x] `set_retarget_chain_mapping`

### 10.7 Utility
- [x] `get_animation_info`

---

## Phase 11: Complete Audio System (Complete)

**Goal**: Full audio authoring including MetaSounds.

**Tool**: `manage_audio`

### 11.1 Sound Cues (Expanded)
- [x] `create_sound_cue`
- [x] `add_cue_node` (wave_player, mixer, random, modulator, etc.)
- [x] `connect_cue_nodes`
- [x] `set_cue_attenuation`, `set_cue_concurrency`

### 11.2 MetaSounds
- [x] `create_metasound`
- [x] `add_metasound_node`
- [x] `connect_metasound_nodes`
- [x] `add_metasound_input`, `add_metasound_output`
- [x] `set_metasound_default`

### 11.3 Sound Classes & Mixes
- [x] `create_sound_class`, `set_class_properties`, `set_class_parent`
- [x] `create_sound_mix`, `add_mix_modifier`, `configure_mix_eq`

### 11.4 Attenuation & Spatialization
- [x] `create_attenuation_settings`
- [x] `configure_distance_attenuation`
- [x] `configure_spatialization`
- [x] `configure_occlusion`
- [x] `configure_reverb_send`

### 11.5 Dialogue System
- [x] `create_dialogue_voice`
- [x] `create_dialogue_wave`
- [x] `set_dialogue_context`
- [x] Host-managed localization manifests with translation/voice-bank completeness validation

### 11.6 Effects
- [x] `create_reverb_effect`
- [x] `create_source_effect_chain`
- [x] `add_source_effect` (filter, eq, chorus, delay, etc.)
- [x] `create_submix_effect`

### 11.7 Utility
- [x] `get_audio_info`

---

## Phase 12: Complete Niagara VFX System (Complete)

**Goal**: Full Niagara system authoring.

**Tool**: `manage_effect`

**Status**: All listed actions fully implemented in TypeScript and C++.

### 12.1 Systems & Emitters
- [x] `create_niagara_system`
- [x] `create_niagara_emitter`
- [x] `add_emitter_to_system`
- [x] `set_emitter_properties`

### 12.2 Module Library
- [x] `add_spawn_rate_module`, `add_spawn_burst_module`, `add_spawn_per_unit_module`
- [x] `add_initialize_particle_module`
- [x] `add_particle_state_module`
- [x] `add_force_module` (gravity, drag, vortex, point_attraction, curl_noise)
- [x] `add_velocity_module`, `add_acceleration_module`
- [x] `add_size_module`, `add_color_module`
- [x] `add_sprite_renderer_module`
- [x] `add_mesh_renderer_module`
- [x] `add_ribbon_renderer_module`
- [x] `add_light_renderer_module`

### 12.3 Runtime Lifecycle
- [x] `set_lifespan` with finite bounded duration validation (`0` means infinite)
- [x] `destroy_effect` with explicit actor lookup and destruction result
- [x] Reusable effect pooling and project-specific ownership policies (prewarmed Niagara actor pools with pool, level, and persistent ownership modes)
- [x] `add_collision_module`
- [x] `add_kill_particles_module`
- [x] `add_camera_offset_module`

### 12.3 Parameters & Data Interfaces
- [x] `add_user_parameter` (float, vector, color, texture, mesh, etc.)
- [x] `set_parameter_value`, `bind_parameter_to_source`
- [x] Quaternion runtime parameter values via the official `UNiagaraComponent::SetNiagaraVariableQuat` API (`set_niagara_parameter`, `parameterType: Quaternion` or `Quat`); this is additional UE API coverage beyond the original roadmap list.
- [x] Integer runtime parameter values via the official `UNiagaraComponent::SetNiagaraVariableInt` API (`set_niagara_parameter`, `parameterType: Int`, `Int32`, or `Integer`); this is additional UE API coverage beyond the original roadmap list.
- [x] Vector2, Vector4, and Position runtime values via Epic's typed `UNiagaraComponent` setters (`set_niagara_parameter`, `parameterType: Vector2`/`Vec2`, `Vector4`/`Vec4`, or `Position`); this is additional UE API coverage beyond the original roadmap list.
- [x] Texture and Static Mesh runtime parameter values via Epic's `UNiagaraComponent::SetVariableTexture` and `SetVariableStaticMesh` APIs (`set_niagara_parameter`, `texturePath`/`meshPath`); asset paths are normalized and restricted to valid Unreal object roots.
- [x] Actor runtime parameter values via Epic's `UNiagaraComponent::SetVariableActor` API (`set_niagara_parameter`, `parameterType: Actor`, `targetActorName`); target actors resolve by editor label or object name.
- [x] Matrix runtime parameter values via Epic's `UNiagaraComponent::SetVariableMatrix` API (`set_niagara_parameter`, `parameterType: Matrix`, 16-value row-major array); non-finite matrix values are rejected.
- [x] Material runtime parameter values via Epic's `UNiagaraComponent::SetVariableMaterial` API (`set_niagara_parameter`, `parameterType: Material`, `materialPath`); material paths are normalized and restricted to valid Unreal object roots.
- [x] Texture Render Target runtime parameter values via Epic's `UNiagaraComponent::SetVariableTextureRenderTarget` API (`set_niagara_parameter`, `parameterType: TextureRenderTarget` or `RenderTarget`, `renderTargetPath`); paths are normalized and restricted to valid Unreal object roots.
- [x] Generic UObject runtime parameter values via Epic's `UNiagaraComponent::SetVariableObject` API (`set_niagara_parameter`, `parameterType: Object`, `objectPath`); paths are normalized and restricted to valid Unreal object roots.
- [x] Skeletal mesh component runtime parameter overrides via Epic's `UNiagaraFunctionLibrary::OverrideSystemUserVariableSkeletalMeshComponent` API (`set_niagara_parameter`, `parameterType: SkeletalMeshComponent` or `SkeletalMesh`, optional `componentName`).
- [x] Static mesh component runtime parameter overrides via Epic's `UNiagaraFunctionLibrary::OverrideSystemUserVariableStaticMeshComponent` API (`set_niagara_parameter`, `parameterType: StaticMeshComponent`, optional `componentName`).
- [x] Texture-object, 2D-array texture, and volume-texture runtime overrides via Epic's `UNiagaraFunctionLibrary::SetTextureObject`, `SetTexture2DArrayObject`, and `SetVolumeTextureObject` APIs (`set_niagara_parameter`, `parameterType: TextureObject`, `Texture2DArray`, or `VolumeTexture`, `texturePath`).
- [x] Skeletal data-interface filtered bones, filtered sockets, and sampling-region overrides via Epic's `UNiagaraFunctionLibrary` APIs (`set_niagara_parameter`, `parameterType: FilteredBones`, `FilteredSockets`, or `SamplingRegions`, with `boneNames`, `socketNames`, or `samplingRegions`).
- [x] Niagara parameter-collection override cleanup via Epic's `UNiagaraComponent::ClearParameterCollectionOverrides` API (`set_niagara_parameter`, `parameterType: ClearParameterCollectionOverrides` or `ClearCollectionOverrides`); this avoids claiming asset-path loading can create a valid collection instance.
- [x] Niagara float, int32, vector, vector2D, vector4, and quaternion array values via Epic's `UNiagaraDataInterfaceArrayFunctionLibrary` typed setters (`set_niagara_parameter`, `parameterType: FloatArray`, `Int32Array`, `VectorArray`, `Vector2Array`, `Vector4Array`, or `QuaternionArray`); malformed and non-finite entries are rejected.
- [x] Niagara matrix array values via Epic's `UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayMatrix` API (`set_niagara_parameter`, `parameterType: MatrixArray`, nested 16-value row-major matrices); malformed and non-finite entries are rejected, with optional `applyLwcRebase` (default `true`) matching Epic's LWC translation-rebasing flag.
- [x] Niagara linear-color array values via Epic's `UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayColor` API (`set_niagara_parameter`, `parameterType: ColorArray`, nested RGB/RGBA values); malformed and non-finite entries are rejected.
- [x] Niagara UInt8 array values via Epic's `UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayUInt8` API (`set_niagara_parameter`, `parameterType: UInt8Array`); values are restricted to integral 0–255 entries.
- [x] Niagara boolean array values via Epic's `UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayBool` API (`set_niagara_parameter`, `parameterType: BoolArray`); entries must be JSON booleans.
- [x] Indexed Niagara float and boolean array values via Epic's `SetNiagaraArrayFloatValue` and `SetNiagaraArrayBoolValue` APIs (`set_niagara_parameter`, `parameterType: FloatArrayValue` or `BoolArrayValue`, `arrayIndex`, `arrayValue`, optional `sizeToFit`).
- [x] Indexed Niagara int32 and UInt8 array values via Epic's `SetNiagaraArrayInt32Value` and `SetNiagaraArrayUInt8Value` APIs (`set_niagara_parameter`, `parameterType: Int32ArrayValue` or `UInt8ArrayValue`); integral/range validation is enforced.
- [x] Indexed Niagara vector, vector2D, vector4, quaternion, and color array values via Epic's typed `SetNiagaraArray*Value` APIs (`set_niagara_parameter`, `parameterType: VectorArrayValue`, `Vector2ArrayValue`, `Vector4ArrayValue`, `QuaternionArrayValue`, or `ColorArrayValue`).
- [x] Indexed Niagara matrix array values via Epic's `SetNiagaraArrayMatrixValue` API (`set_niagara_parameter`, `parameterType: MatrixArrayValue`, `arrayIndex`, `arrayValue`, optional `sizeToFit` and `applyLwcRebase`).
- [x] Niagara Position/LWC array values via Epic's `SetNiagaraArrayPosition` and `SetNiagaraArrayPositionValue` APIs (`set_niagara_parameter`, `parameterType: PositionArray` or `PositionArrayValue`); position arrays use FVector values distinct from ordinary vector arrays.
- [x] Additional official Niagara array readback via `UNiagaraDataInterfaceArrayFunctionLibrary` getters (`get_niagara_array_parameter`, `parameterType: FloatArray`, `Int32Array`, `IntArray`, `UInt8Array`, `BoolArray`, `VectorArray`, `Vector2Array`, `Vector4Array`, `QuaternionArray`, `QuatArray`, `ColorArray`, `PositionArray`, or `MatrixArray`); nested vector-like values and row-major 16-value matrices are returned as JSON arrays, with matrix LWC rebasing configurable and defaulting to `true`. Optional `arrayIndex` returns one validated element.
- [x] `add_skeletal_mesh_data_interface`
- [x] `add_static_mesh_data_interface`
- [x] `add_spline_data_interface`
- [x] `add_audio_spectrum_data_interface`
- [x] `add_collision_query_data_interface`

### 12.4 Events & GPU
- [x] `add_event_generator`, `add_event_receiver`
- [x] `configure_event_payload`
- [x] `enable_gpu_simulation`
- [x] `add_simulation_stage`

### 12.5 Utility
- [x] `get_niagara_info`
- [x] `validate_niagara_system`

---

## Phase 13: Gameplay Ability System (GAS) (Complete)

**Goal**: Complete GAS implementation for abilities, effects, and attributes.

**Tool**: `manage_gas`

### 13.1 Components & Attributes
- [x] `add_ability_system_component`
- [x] `configure_asc` (replication_mode, owner)
- [x] `create_attribute_set`
- [x] `add_attribute` (health, mana, stamina, damage, armor, etc.)
- [x] `set_attribute_base_value`, `set_attribute_clamping`

### 13.2 Gameplay Abilities
- [x] `create_gameplay_ability`
- [x] `set_ability_tags`
- [x] `set_ability_costs`, `set_ability_cooldown`
- [x] `set_ability_targeting`
- [x] `add_ability_task`
- [x] `set_activation_policy`, `set_instancing_policy`

### 13.3 Gameplay Effects
- [x] `create_gameplay_effect`
- [x] `set_effect_duration` (instant, infinite, duration)
- [x] `add_effect_modifier` (attribute, operation, magnitude)
- [x] `set_modifier_magnitude` (scalable, attribute_based, set_by_caller)
- [x] `add_effect_execution_calculation`
- [x] `add_effect_cue`
- [x] `set_effect_stacking`
- [x] `set_effect_tags` (granted, application, removal)

### 13.4 Gameplay Cues
- [x] `create_gameplay_cue_notify` (static, actor)
- [x] `configure_cue_trigger`
- [x] `set_cue_effects` (particles, sounds, camera_shake)
- [x] `add_tag_to_asset`

### 13.5 Utility
- [x] `get_gas_info`

> **Note**: Gameplay Tag creation and management is in Phase 31.3 (Data & Persistence).

---

## Phase 14: Character & Movement System (Complete)

**Goal**: Complete character setup with advanced movement.

**Tool**: `manage_character`

**Status**: All 19 actions fully implemented in TypeScript and C++.

### 14.1 Character Creation
- [x] `create_character_blueprint`
- [x] `configure_capsule_component`
- [x] `configure_mesh_component`
- [x] `configure_camera_component`

### 14.2 Movement Component
- [x] `configure_movement_speeds` (walk, run, sprint, crouch, swim, fly)
- [x] `configure_jump` (height, air_control, double_jump)
- [x] `configure_rotation` (orient_to_movement, use_controller_rotation)
- [x] `add_custom_movement_mode`
- [x] `configure_nav_movement`

### 14.3 Advanced Movement
- [x] `setup_mantling`
- [x] `setup_vaulting`
- [x] `setup_climbing`
- [x] `setup_sliding`
- [x] `setup_wall_running`
- [x] `setup_grappling`

### 14.4 Footsteps System
> **Note**: Physical Material creation is in Phase 34.5 (Physics Materials).

- [x] `setup_footstep_system`
- [x] `map_surface_to_sound`
- [x] `configure_footstep_fx`

### 14.5 Utility
- [x] `get_character_info`

---

## Phase 15: Combat & Weapons System (Complete)

**Goal**: Complete combat implementation.

**Tool**: `manage_combat`

**Status**: All 31 actions fully implemented in TypeScript and C++.

### 15.1 Weapon Base
- [x] `create_weapon_blueprint`
- [x] `configure_weapon_mesh`, `configure_weapon_sockets`
- [x] `set_weapon_stats` (damage, fire_rate, range, spread)

### 15.2 Firing Modes
- [x] `configure_hitscan`
- [x] `configure_projectile`
- [x] `configure_spread_pattern`
- [x] `configure_recoil_pattern`
- [x] `configure_aim_down_sights`

### 15.3 Projectiles
- [x] `create_projectile_blueprint`
- [x] `configure_projectile_movement`
- [x] `configure_projectile_collision`
- [x] `configure_projectile_homing`

### 15.4 Damage System
- [x] `create_damage_type`
- [x] `configure_damage_execution`
- [x] `setup_hitbox_component`

### 15.5 Weapon Features
- [x] `setup_reload_system`
- [x] `setup_ammo_system`
- [x] `setup_attachment_system`
- [x] `setup_weapon_switching`

### 15.6 Effects
- [x] `configure_muzzle_flash`
- [x] `configure_tracer`
- [x] `configure_impact_effects`
- [x] `configure_shell_ejection`

### 15.7 Melee Combat
- [x] `create_melee_trace`
- [x] `configure_combo_system`
- [x] `create_hit_pause` (hitstop)
- [x] `configure_hit_reaction`
- [x] `setup_parry_block_system`
- [x] `configure_weapon_trails`

### 15.8 Utility
- [x] `get_combat_info`

---

## Phase 16: Complete AI System (Complete)

**Goal**: Full AI pipeline with EQS, perception, and smart objects.

**Tool**: `manage_ai`

**Status**: All listed actions fully implemented in TypeScript and C++.

### 16.1 AI Controller
- [x] `create_ai_controller`
- [x] `assign_behavior_tree`, `assign_blackboard`

### 16.2 Blackboard
- [x] `create_blackboard_asset`
- [x] `add_blackboard_key` (bool, int, float, vector, rotator, object, class, enum, name, string)
- [x] `set_key_instance_synced`

### 16.3 Behavior Tree (Expanded)
- [x] `create_behavior_tree`
- [x] `add_composite_node` (selector, sequence, parallel, simple_parallel)
- [x] `add_task_node` (move_to, rotate_to_face, wait, play_animation, play_sound, run_eqs_query, etc.)
- [x] `add_decorator` (blackboard, cooldown, cone_check, does_path_exist, is_at_location, loop, time_limit, force_success)
- [x] `add_service` (default_focus, run_eqs)
- [x] `configure_bt_node`

### 16.4 Environment Query System (EQS)
- [x] `create_eqs_query`
- [x] `add_eqs_generator` (actors_of_class, current_location, donut, grid, on_circle, pathing_grid, points)
- [x] `add_eqs_context` (querier, item, target)
- [x] `add_eqs_test` (distance, dot, overlap, pathfinding, project, random, trace, gameplay_tags)
- [x] `configure_test_scoring`

### 16.5 Perception System
- [x] `add_ai_perception_component`
- [x] `configure_sight_config` (radius, angle, age, detection_by_affiliation)
- [x] `configure_hearing_config` (radius)
- [x] `configure_damage_sense_config`
- [x] `set_perception_team`

### 16.6 State Trees (UE5.3+)
- [x] `create_state_tree`
- [x] `add_state_tree_state`
- [x] `add_state_tree_transition`
- [x] `configure_state_tree_task`

### 16.7 Smart Objects
- [x] `create_smart_object_definition`
- [x] `add_smart_object_slot`
- [x] `configure_slot_behavior`
- [x] `add_smart_object_component`

### 16.8 Mass AI (Crowds)
- [x] `create_mass_entity_config`
- [x] `configure_mass_entity`
- [x] `add_mass_spawner`

### 16.9 Utility
- [x] `get_ai_info`

---

## Phase 17: Inventory & Items System (Complete)

**Goal**: Complete inventory and item management.

**Tool**: `manage_inventory`

**Status**: All 33 actions fully implemented in TypeScript and C++.

### 17.1 Data Assets
- [x] `create_item_data_asset`
- [x] `set_item_properties` (properties object)
- [x] `create_item_category`
- [x] `assign_item_category`

### 17.2 Inventory Component
- [x] `create_inventory_component`
- [x] `configure_inventory_slots`
- [x] `add_inventory_functions`
- [x] `configure_inventory_events`
- [x] `set_inventory_replication`

### 17.3 Pickups
- [x] `create_pickup_actor`
- [x] `configure_pickup_interaction`
- [x] `configure_pickup_respawn`
- [x] `configure_pickup_effects`

### 17.4 Equipment
- [x] `create_equipment_component`
- [x] `define_equipment_slots`
- [x] `configure_equipment_effects`
- [x] `add_equipment_functions`
- [x] `configure_equipment_visuals`

### 17.5 Loot System
- [x] `create_loot_table`
- [x] `add_loot_entry`
- [x] `configure_loot_drop`
- [x] `set_loot_quality_tiers`

### 17.6 Crafting
- [x] `create_crafting_recipe`
- [x] `configure_recipe_requirements`
- [x] `create_crafting_station`
- [x] `add_crafting_component`

### 17.7 Additional Actions
- [x] `configure_item_stacking`
- [x] `set_item_icon`
- [x] `add_recipe_ingredient`
- [x] `remove_loot_entry`
- [x] `configure_inventory_weight`
- [x] `configure_station_recipes`

### 17.8 Utility
- [x] `get_inventory_info`

---

## Phase 18: Interaction System (Complete)

**Goal**: Complete interaction framework.

**Tool**: `manage_interaction`

**Status**: All 22 actions fully implemented in TypeScript and C++.

### 18.1 Interaction Component
- [x] `create_interaction_component`
- [x] `configure_interaction_trace`
- [x] `configure_interaction_widget`
- [x] `add_interaction_events`

### 18.2 Interactables
- [x] `create_interactable_interface`
- [x] `create_door_actor`
- [x] `configure_door_properties`
- [x] `create_switch_actor`
- [x] `configure_switch_properties`
- [x] `create_chest_actor`
- [x] `configure_chest_properties`
- [x] `create_lever_actor`

> **Note**: Pickup actors are in Phase 17.3 (Inventory - Pickups).

### 18.3 Destructibles
- [x] `setup_destructible_mesh`
- [x] `configure_destruction_levels`
- [x] `configure_destruction_effects`
- [x] `configure_destruction_damage`
- [x] `add_destruction_component`

### 18.4 Trigger System
- [x] `create_trigger_actor`
- [x] `configure_trigger_events`
- [x] `configure_trigger_filter`
- [x] `configure_trigger_response`

### 18.5 Utility
- [x] `get_interaction_info`

---

## Phase 19: Complete UI/UX System (Complete)

**Goal**: Full UMG widget authoring capabilities.

**Tool**: `manage_blueprint`

**Status**: All 65 actions fully implemented in TypeScript and C++.

### 19.1 Widget Creation
- [x] `create_widget_blueprint`
- [x] `set_widget_parent_class`

### 19.2 Layout Panels
- [x] `add_canvas_panel`
- [x] `add_horizontal_box`, `add_vertical_box`
- [x] `add_overlay`, `add_grid_panel`, `add_uniform_grid`
- [x] `add_wrap_box`, `add_scroll_box`
- [x] `add_size_box`, `add_scale_box`
- [x] `add_border`

### 19.3 Common Widgets
- [x] `add_text_block`, `add_rich_text_block`
- [x] `add_image`, `add_button`
- [x] `add_check_box`, `add_slider`
- [x] `add_progress_bar`
- [x] `add_text_input` (editable_text, editable_text_box)
- [x] `add_combo_box`, `add_spin_box`
- [x] `add_list_view`, `add_tree_view`

### 19.4 Layout & Styling
- [x] `set_anchor`, `set_alignment`
- [x] `set_position`, `set_size`
- [x] `set_padding`, `set_z_order`
- [x] `set_render_transform`, `set_clipping`
- [x] `set_visibility`, `set_style`

### 19.5 Bindings & Events
- [x] `create_property_binding`
- [x] `bind_text`, `bind_visibility`, `bind_color`, `bind_enabled`
- [x] `bind_on_clicked`, `bind_on_hovered`, `bind_on_value_changed`

### 19.6 Widget Animations
- [x] `create_widget_animation`
- [x] `add_animation_track` (transform, color, opacity, material)
- [x] `add_animation_keyframe`
- [x] `set_animation_loop`

### 19.7 UI Templates
- [x] `create_main_menu`, `create_pause_menu`
- [x] `create_settings_menu` (video, audio, controls, gameplay)
- [x] `create_loading_screen`
- [x] `create_hud_widget`
- [x] `add_health_bar`, `add_ammo_counter`, `add_minimap`
- [x] `add_crosshair`, `add_compass`
- [x] `add_interaction_prompt`, `add_objective_tracker`
- [x] `add_damage_indicator`
- [x] `create_inventory_ui`
- [x] `create_dialog_widget`, `create_radial_menu`

### 19.8 Utility
- [x] `get_widget_info`
- [x] `preview_widget`

---

## Phase 20: Networking & Multiplayer (Complete)

**Goal**: Complete networking and replication system.

**Tool**: `manage_networking`

**Status**: All 27 actions fully implemented in TypeScript and C++.

### 20.1 Replication
- [x] `set_property_replicated`
- [x] `set_replication_condition` (COND_None, COND_OwnerOnly, COND_SkipOwner, COND_SimulatedOnly, etc.)
- [x] `configure_net_update_frequency`
- [x] `configure_net_priority`
- [x] `set_net_dormancy`
- [x] `configure_replication_graph`

### 20.2 RPCs
- [x] `create_rpc_function` (Server, Client, NetMulticast)
- [x] `configure_rpc_validation`
- [x] `set_rpc_reliability`

### 20.3 Authority & Ownership
- [x] `set_owner`
- [x] `set_autonomous_proxy`
- [x] `check_has_authority`
- [x] `check_is_locally_controlled`

### 20.4 Network Relevancy
- [x] `configure_net_cull_distance`
- [x] `set_always_relevant`
- [x] `set_only_relevant_to_owner`

### 20.5 Net Serialization
- [x] `configure_net_serialization`
- [x] `set_replicated_using`
- [x] `configure_push_model`

### 20.6 Network Prediction
- [x] `configure_client_prediction`
- [x] `configure_server_correction`
- [x] `add_network_prediction_data`
- [x] `configure_movement_prediction`

### 20.7 Connection & Session
- [x] `configure_net_driver`
- [x] `set_net_role`
- [x] `configure_replicated_movement`

### 20.8 Utility
- [x] `get_networking_info`

---

## Phase 21: Game Framework (Complete)

**Goal**: Complete game mode and session management.

**Tool**: `manage_networking`

**Status**: All 20 actions fully implemented in TypeScript and C++.

### 21.1 Core Classes
- [x] `create_game_mode`
- [x] `create_game_state`
- [x] `create_player_controller`
- [x] `create_player_state`
- [x] `create_game_instance`
- [x] `create_hud_class`

### 21.2 Game Mode Configuration
- [x] `set_default_pawn_class`
- [x] `set_player_controller_class`
- [x] `set_game_state_class`
- [x] `set_player_state_class`
- [x] `configure_game_rules`

### 21.3 Match Flow
- [x] `setup_match_states` (waiting, warmup, in_progress, post_match)
- [x] `configure_round_system`
- [x] `configure_team_system`
- [x] `configure_scoring_system`
- [x] `configure_spawn_system`

### 21.4 Player Management
- [x] `configure_player_start`
- [x] `set_respawn_rules`
- [x] `configure_spectating`

### 21.5 Utility
- [x] `get_game_framework_info`

---

## Phase 22: Sessions & Local Multiplayer ✅

**Goal**: Session management and split-screen support.

**Tool**: `manage_networking`

### 22.1 Session Management (Local/LAN)
> **Note**: Online session management (matchmaking, lobbies) is in Phase 43 (Online Services). This section covers local/LAN sessions only.

- [x] `configure_local_session_settings`
- [x] `configure_session_interface`

### 22.2 Local Multiplayer
- [x] `configure_split_screen`
- [x] `set_split_screen_type` (horizontal, vertical, grid)
- [x] `add_local_player`
- [x] `remove_local_player`

### 22.3 LAN
- [x] `configure_lan_play`
- [x] `host_lan_server`
- [x] `join_lan_server`

### 22.4 Voice Chat
- [x] `enable_voice_chat` (conditional; waits for connection completion and requires a VoiceChat implementation)
- [x] `configure_voice_settings` (conditional; reports unavailable when no supported VoiceChat settings API is loaded)
- [x] `set_voice_channel`
- [x] `mute_player`
- [x] `set_voice_attenuation`
- [x] `configure_push_to_talk`

### 22.5 Utility
- [x] `get_sessions_info`

---

## Phase 23: World & Level Structure ✅

**Goal**: Complete level and world management.

**Tool**: `manage_level_structure`

**Status**: All 17 actions fully implemented in TypeScript and C++.

### 23.1 Levels
- [x] `create_level`, `create_sublevel`
- [x] `configure_level_streaming`
- [x] `set_streaming_distance`
- [x] `configure_level_bounds`

### 23.2 World Partition (Expanded)
- [x] `enable_world_partition`
- [x] `configure_grid_size`
- [x] `create_data_layer`
- [x] `assign_actor_to_data_layer`
- [x] `configure_hlod_layer`
- [x] `create_minimap_volume`

### 23.3 Level Blueprint
- [x] `open_level_blueprint`
- [x] `add_level_blueprint_node`
- [x] `connect_level_blueprint_nodes`

### 23.4 Level Instances
- [x] `create_level_instance`
- [x] `create_packed_level_actor`

### 23.5 Utility
- [x] `get_level_structure_info`

---

## Phase 24: Volumes & Zones ✅

**Goal**: Complete volume and trigger system.

**Tool**: `manage_level_structure`

**Status**: All 18 actions fully implemented in TypeScript and C++.

### 24.1 Trigger Volumes
- [x] `create_trigger_volume`
- [x] `create_trigger_box`, `create_trigger_sphere`, `create_trigger_capsule`

### 24.2 Gameplay Volumes
- [x] `create_blocking_volume`
- [x] `create_kill_z_volume`
- [x] `create_pain_causing_volume`
- [x] `create_physics_volume`
- [x] `create_audio_volume`, `create_reverb_volume`
- [x] `create_cull_distance_volume`
- [x] `create_precomputed_visibility_volume`
- [x] `create_lightmass_importance_volume`
- [x] `create_nav_mesh_bounds_volume`
- [x] `create_nav_modifier_volume`
- [x] `create_camera_blocking_volume`

> **Note**: Post Process Volume configuration is in Phase 29.5 (Post Processing).

### 24.3 Volume Configuration
- [x] `set_volume_extent`
- [x] `set_volume_properties`

### 24.4 Utility
- [x] `get_volumes_info`

---

## Phase 25: Navigation System (Complete)

**Goal**: Complete navigation mesh and pathfinding.

**Tool**: `manage_ai`

### 25.1 NavMesh
- [x] `configure_nav_mesh_settings`
- [x] `set_nav_agent_properties` (radius, height, step_height)
- [x] `rebuild_navigation`

### 25.2 Nav Modifiers
- [x] `create_nav_modifier_component`
- [x] `set_nav_area_class`
- [x] `configure_nav_area_cost`

### 25.3 Nav Links
- [x] `create_nav_link_proxy`
- [x] `configure_nav_link` (start, end, direction, snap_radius)
- [x] `set_nav_link_type` (simple, smart)
- [x] `create_smart_link`
- [x] `configure_smart_link_behavior`

---

## Phase 26: Spline System (Complete)

**Goal**: Complete spline-based content creation.

**Tool**: `build_environment`

**Status**: All 21 actions fully implemented in TypeScript and C++.

### 26.1 Spline Creation
- [x] `create_spline_actor`
- [x] `add_spline_point`, `remove_spline_point`
- [x] `set_spline_point_position`
- [x] `set_spline_point_tangents`
- [x] `set_spline_point_rotation`, `set_spline_point_scale`
- [x] `set_spline_type` (linear, curve, constant, clamped_curve)

### 26.2 Spline Mesh
- [x] `create_spline_mesh_component`
- [x] `set_spline_mesh_asset`
- [x] `configure_spline_mesh_axis`
- [x] `set_spline_mesh_material`

### 26.3 Spline Mesh Array
- [x] `scatter_meshes_along_spline`
- [x] `configure_mesh_spacing`
- [x] `configure_mesh_randomization`

### 26.4 Quick Templates
- [x] `create_road_spline`
- [x] `create_river_spline`
- [x] `create_fence_spline`
- [x] `create_wall_spline`
- [x] `create_cable_spline`
- [x] `create_pipe_spline`

### 26.5 Utility
- [x] `get_splines_info`

---

## Phase 27: PCG Framework

**Goal**: Complete procedural content generation.

**Tool**: `manage_pcg`

### 27.1 Graph Management
- [x] `create_pcg_graph`, `create_pcg_subgraph`
- [x] `add_pcg_node`
- [x] `connect_pcg_pins`
- [x] `set_pcg_node_settings`

### 27.2 Input Nodes
- [x] `add_landscape_data_node`
- [x] `add_spline_data_node`
- [x] `add_volume_data_node`
- [x] `add_actor_data_node`
- [x] `add_texture_data_node`

### 27.3 Point Operations
- [x] `add_surface_sampler`, `add_mesh_sampler`
- [x] `add_spline_sampler`, `add_volume_sampler`
- [x] `add_bounds_modifier`
- [x] `add_density_filter`, `add_height_filter`
- [x] `add_slope_filter`, `add_distance_filter`
- [x] `add_bounds_filter`, `add_self_pruning`
- [x] `add_transform_points`
- [x] `add_project_to_surface`
- [x] `add_copy_points`, `add_merge_points`

### 27.4 Spawning
- [x] `add_static_mesh_spawner`
- [x] `add_actor_spawner`
- [x] `add_spline_spawner`

### 27.5 Execution
- [x] `execute_pcg_graph`
- [x] `set_pcg_partition_grid_size`

---

## Phase 28: Environment Systems (Complete)

**Goal**: Complete environment (sky, weather, water).

**Tool**: `build_environment`

**Status**: All 42 listed actions fully implemented in TypeScript and C++.

### 28.1 Landscape (Expanded)
- [x] `create_landscape`
- [x] `import_heightmap`, `export_heightmap`
- [x] `sculpt_landscape` (raise, lower, smooth, flatten, erosion)
- [x] `paint_landscape_layer`
- [x] `create_landscape_layer_info`
- [x] `configure_landscape_material`
- [x] `create_landscape_grass_type`
- [x] `configure_landscape_splines`
- [x] `configure_landscape_lod`
- [x] `create_landscape_streaming_proxy`

### 28.2 Foliage (Expanded)
- [x] `create_foliage_type`
- [x] `configure_foliage_mesh`
- [x] `configure_foliage_placement` (density, scale, rotation, align)
- [x] `configure_foliage_lod`, `configure_foliage_collision`, `configure_foliage_culling`
- [x] `paint_foliage_instances`, `remove_foliage_instances`

### 28.3 Sky & Atmosphere
- [x] `configure_sky_atmosphere`
- [x] `configure_sky_light`
- [x] `configure_directional_light_atmosphere`
- [x] `configure_exponential_height_fog`
- [x] `configure_volumetric_cloud`
- [x] `create_sky_sphere`

### 28.4 Weather
- [x] `create_weather_system`
- [x] `configure_rain_particles`
- [x] `configure_snow_particles`
- [x] `configure_wind`
- [x] `configure_lightning`

### 28.5 Time of Day
- [x] `create_time_of_day_system`
- [x] `configure_sun_position`
- [x] `configure_light_color_curve`
- [x] `configure_sky_color_curve`

### 28.6 Water (Water Plugin)
- [x] `create_water_body_ocean`
- [x] `create_water_body_lake`
- [x] `create_water_body_river`
- [x] `create_water_body_custom`
- [x] `configure_water_waves`
- [x] `configure_water_material`
- [x] `configure_water_collision`
- [x] `create_buoyancy_component`

---

## Phase 29: Advanced Lighting & Rendering

**Goal**: Complete lighting and post-processing.

**Tool**: `build_environment` (Expanded) + `manage_post_process`

### 29.1 Ray Tracing
- [x] `configure_ray_traced_shadows`
- [x] `configure_ray_traced_gi`
- [x] `configure_ray_traced_reflections`
- [x] `configure_ray_traced_ao`
- [x] `configure_path_tracing`
- [x] `configure_ray_traced_translucency` (translucency, refraction, roughness, and per-volume overrides)
- [x] `configure_ray_tracing_quality` (scene culling, geometry participation, TLAS/BLAS performance controls)

### 29.2 Light Channels
- [x] `set_light_channel`
- [x] `set_actor_light_channel`
- [x] `get_light_channels` (extra readback/verification action)

### 29.3 Lightmass
- [x] `configure_lightmass_settings` (world-level Lightmass tuning: level scale, direct/sky bounces, indirect quality/smoothness, environment/diffuse/emissive boosts, compression, and safe readback)
- [x] `build_lighting_quality` (Preview, Medium, High, and Production quality presets)
- [x] `configure_indirect_lighting_cache` (enable/disable cache, update cadence, cache dimension, movable-object allocation size)
- [x] `configure_volumetric_lightmaps` (Volumetric Lightmap or Sparse Volume Lighting Samples, detail-cell size, brick memory, loading-cell size when supported, placement scale, and spherical-harmonic smoothing)
- [x] `configure_lightmass_ambient_occlusion` (baked AO enablement, material mask generation, direct/indirect occlusion fractions, exponent, full-occlusion fraction, and max distance)
- [x] `inspect_lightmass_settings` (complete settings readback plus engine-version support for optional World Partition fields)

**Implementation notes**: Lightmass settings are persisted on the active `WorldSettings` and returned after every mutation. The handler is compatible with UE 5.0-5.8; `VolumetricLightmapLoadingCellSize` is applied through reflection when the active engine exposes it. The implementation follows Epic's documented CPU Lightmass, volumetric lightmap, indirect lighting cache, and static-light mobility settings.

**Research references**: [Global Illumination](https://dev.epicgames.com/documentation/en-us/unreal-engine/global-illumination-in-unreal-engine), [Static Light Mobility](https://dev.epicgames.com/documentation/en-us/unreal-engine/static-light-mobility-in-unreal-engine), [Volumetric Lightmaps](https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-lightmaps-in-unreal-engine), and [Indirect Lighting Cache](https://dev.epicgames.com/documentation/en-us/unreal-engine/indirect-lighting-cache-in-unreal-engine).

### 29.4 Reflections
- [x] `create_sphere_reflection_capture` (influence radius, brightness, capture offset, source cubemap angle, runtime capture, and max view distance)
- [x] `create_box_reflection_capture` (box size, transition distance, brightness, capture offset, source cubemap angle, runtime capture, and max view distance)
- [x] `configure_capture_resolution` (power-of-two global reflection capture cubemap resolution)
- [x] `configure_capture_offset` (world-space offset for a named capture)
- [x] `recapture_scene` (targeted or all captures, with fast-render and smooth-blend options for runtime captures)
- [x] `create_planar_reflection`
- [x] `configure_planar_reflection` (normal distortion, prefilter roughness, distance/angle fade, screen percentage, extra FOV, two-sided rendering, preview plane)
- [x] `configure_ssr_settings` (enablement, intensity, quality, and maximum roughness)
- [x] `configure_lumen_reflection_settings` (enablement, quality, roughness threshold, bounces, downsampling, screen traces, and checkerboard mode)
- [x] `inspect_reflection_captures` (readback for sphere, box, and planar reflection actors)

**Implementation notes**: Reflection captures are static cubemap probes with optional runtime refresh. Sphere and box captures use their native influence/transition geometry; planar reflections use the native scene-capture component. Global SSR and Lumen controls report unsupported CVars when an engine version or rendering path does not expose them.

**Research references**: [Reflection Captures](https://dev.epicgames.com/documentation/en-us/unreal-engine/reflections-captures-in-unreal-engine), [Planar Reflections](https://dev.epicgames.com/documentation/en-us/unreal-engine/planar-reflections-in-unreal-engine), [Reflections Environment](https://dev.epicgames.com/documentation/en-us/unreal-engine/reflections-environment-in-unreal-engine), [Rendering Settings](https://dev.epicgames.com/documentation/en-us/unreal-engine/rendering-settings-in-the-unreal-engine-project-settings), and [Lumen Reflections](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-global-illumination-and-reflections-in-unreal-engine).

### 29.5 Post Processing
- [x] `create_post_process_volume` (location, rotation, extent, enabled, and named-volume targeting)
- [x] `configure_pp_blend` (infinite extent/unbound, priority, blend radius, blend weight)
- [x] Color Grading: `set_pp_white_balance`, `set_pp_color_grading`, `set_pp_lut` (temperature, tint, saturation, contrast, gamma, gain, offset, LUT intensity)
- [x] Tonemapper: `configure_tonemapper`, `set_tonemapper_type` (tone curve, gamut, film clips, engine CVar fallback)
- [x] Bloom/Lens: `configure_bloom`, `set_bloom_intensity`, `set_bloom_threshold`, `configure_lens_flare` (size and threshold controls)
- [x] DOF: `configure_dof`, `set_dof_method`, `set_focal_distance`, `set_aperture`, `configure_bokeh` (focus, F-stop, transitions, blur, blades)
- [x] Motion Blur: `configure_motion_blur`, `set_motion_blur_amount`, `set_motion_blur_max` (amount, max, target FPS, per-object size)
- [x] Exposure: `configure_exposure`, `set_exposure_method`, `set_exposure_compensation`, `set_exposure_min_max` (histogram/basic/manual, limits, adaptation speeds)
- [x] AO: `configure_ssao`, `configure_gtao` (volume AO properties plus optional GTAO CVar)
- [x] Effects: `configure_vignette`, `configure_chromatic_aberration`, `configure_grain`, `configure_screen_percentage`
- [x] `inspect_post_process_volume` (volume/blend and representative settings readback)

**Implementation notes**: The implementation uses `APostProcessVolume` and reflection-backed `FPostProcessSettings` writes so optional properties can report as unsupported on older UE5 versions instead of breaking compilation or silently changing unrelated settings. Post-process override flags are enabled for every applied setting; unsupported engine CVars are returned in `unsupportedSettings`. LUT paths are restricted to `/Game` and `/Engine` assets, and screen percentage/tonemapper/GTAO controls use CVars when exposed by the active renderer.

**Research references**: [Post Process Effects](https://dev.epicgames.com/documentation/en-us/unreal-engine/post-process-effects-in-unreal-engine), [FPostProcessSettings API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FPostProcessSettings), [Post Process Volume API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/APostProcessVolume), [Color Grading and Filmic Tonemapper](https://dev.epicgames.com/documentation/en-us/unreal-engine/color-grading-and-the-filmic-tonemapper-in-unreal-engine), and [Rendering Features Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/rendering-features-reference).

### 29.6 Scene Capture
- [x] `create_scene_capture_2d`, `create_scene_capture_cube`
- [x] `configure_scene_capture_resolution`, `configure_capture_source`
- [x] `create_render_target` (existing 2D `manage_asset` action), `create_render_target_cube`
- [x] `assign_render_target`, `capture_scene`, `inspect_scene_captures`
- [x] Additional UE capture controls: projection mode, FOV/orthographic width, capture cadence, persisted rendering state, capture priority, post-process blend weight, linear-gamma/HDR target options, and capture filtering fields.

**Implementation notes**: Scene Capture actors are created as `ASceneCapture2D` or `ASceneCaptureCube` and configured through their `USceneCaptureComponent` instances. 2D captures resize `UTextureRenderTarget2D`; cube captures initialize `UTextureRenderTargetCube`. Capture source names are resolved against the engine enum so version-specific enum values remain discoverable. Existing `manage_asset.create_render_target` remains the 2D render-target creator; Phase 29.6 adds the cube counterpart and explicit assignment/capture/inspection actions.

**Research references**: [Rendering Components](https://dev.epicgames.com/documentation/en-us/unreal-engine/rendering-components-in-unreal-engine), [USceneCaptureComponent](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/USceneCaptureComponent?lang=en-US), [USceneCaptureComponent2D](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/USceneCaptureComponent2D?lang=en-US), [USceneCaptureComponentCube](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/USceneCaptureComponentCube?lang=en-US), [ESceneCaptureSource](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/ESceneCaptureSource), [UTextureRenderTarget2D](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UTextureRenderTarget2D?lang=en-US), and [UTextureRenderTargetCube](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UTextureRenderTargetCube).

---

## Phase 30: Cinematics & Media

**Goal**: Complete sequencer and media capabilities.

**Tool**: `manage_sequencer` (Expanded) + `manage_movie_render` + `manage_media`

### 30.1 Sequencer (Expanded)
- [x] Additional official Niagara Sequencer coverage: `inspect_niagara_parameter_track` reads typed parameter metadata and section counts; `add_niagara_integer_parameter_key` authors typed integer keys through `FMovieSceneIntegerChannel::AddKeys`; `add_niagara_bool_parameter_key` authors typed boolean keys through `FMovieSceneBoolChannel::AddKeys`; `add_niagara_vector_parameter_key` authors XYZ keys through typed float channels; `add_niagara_color_parameter_key` authors RGBA keys through typed float channels.
- [x] `create_master_sequence` (safe alias of Level Sequence creation)
- [x] `add_subsequence` (native MovieSceneSubTrack section authoring)
- [x] `add_shot_track` (native cinematic-shot track); [x] `configure_shot_settings` (official cinematic shot display, thumbnail offset, and frame-range setters)
- [x] `inspect_shot_settings` (official cinematic shot metadata and section-range readback)
- [x] `create_cine_camera_actor` (safe alias of camera binding authoring)
- [x] `configure_camera_settings` (filmback, lens, focus via `control_actor` and reflected `UCineCameraComponent` settings)
- [x] `add_camera_cut_track`; [x] `add_camera_shake_track` (native `UMovieSceneCameraShakeTrack::AddNewCameraShake` authoring)
- [x] `configure_camera_rig_rail`, `configure_camera_rig_crane` (validated actor settings for UE `ACameraRig_Rail` and `ACameraRig_Crane`)
- [x] Additional generic tracks: `add_fade_track`, `add_level_visibility_track`, `add_skeletal_animation_track`, `add_transform_track`, `add_event_track`, `add_property_track`; [x] `add_audio_track` (official `UMovieSceneAudioTrack::AddNewSoundOnRow` authoring with sound asset, frame range, row, looping, and play-until-finished settings); [x] `add_material_parameter_track` (official `UMovieSceneMaterialTrack::AddScalarParameterKey` authoring on an actor binding); [x] `add_material_color_track` (official `UMovieSceneMaterialTrack::AddColorParameterKey` authoring with RGBA channels); [x] `add_custom_primitive_data_track` (official `UMovieSceneCustomPrimitiveDataTrack::AddScalarParameterKey` authoring by primitive-data index); [x] `add_niagara_system_track` (optional Niagara plugin; official `UMovieSceneNiagaraSystemTrack` and spawn-section authoring); [x] `create_niagara_float_parameter_track` (optional Niagara plugin; official `UMovieSceneNiagaraParameterTrack::SetParameter` with a typed float track); [x] `add_niagara_float_parameter_key` (public `FMovieSceneFloatChannel::AddLinearKey` insertion on the typed Niagara section); [x] Niagara vector/color/bool/integer value-key authoring

### 30.2 Movie Render Queue
- [x] `create_render_job` (implemented as `manage_sequence.render_sequence_mrq`)
- [x] `configure_output_settings` (MRQ preset, project-relative output path, resolution, frame range, and image format)
- [x] `add_render_pass` (beauty and optional `object_id`; depth, normal, motion_vector, and custom_stencil require project-specific deferred-pass material/plugin configuration)
- [x] `configure_anti_aliasing` (spatial and temporal sample counts on the transient MRQ job)
- [x] `configure_console_variables` (validated batch updates via UE `IConsoleManager`/`IConsoleVariable`)
- [x] `configure_burn_ins` (transient MRQ burn-in setting and widget class)
- [x] `queue_render`, `start_render` (implemented as `manage_sequence.render_sequence_mrq` and `render_sequence_queue`)

**Implemented gap:** `system_control.configure_console_variables` now performs validated batch CVar updates through UE's `IConsoleManager`/`IConsoleVariable` API and returns per-variable results.

**Render-pass note:** `manage_sequence.render_sequence_mrq` accepts `renderPass: "beauty" | "object_id"`; Object ID requires Epic's Movie Render Queue Additional Render Passes plugin and EXR output. The remaining AOVs are intentionally not claimed without a stable cross-version UE API for the required deferred-pass materials.

### 30.3 Media Framework
- [x] `create_media_player`
- [x] `create_media_source` (file, stream)
- [x] `create_media_texture`
- [x] `create_media_sound_component` (implemented through `control_actor`, binds a `UMediaPlayer` to `UMediaSoundComponent`)
- [x] `create_media_playlist`
- [x] `play_media`, `pause_media`, `seek_media`

**Implementation note:** Media asset creation is guarded by Media Framework availability and uses the native `UMediaPlayer`, `UMediaSource`, `UMediaTexture`, `UMediaPlaylist`, and `UMediaSoundComponent` asset/component types. Platform-specific capture sources remain conditional.

### 30.4 Take Recorder
- [x] `create_take_recorder_panel` (opens Epic's registered Take Recorder tab through ITakeRecorderModule::TakeRecorderTabName; optional plugin)
- [x] `configure_take_sources` (UE Take Recorder actor sources)
- [x] `start_recording`, `stop_recording` (Take Recorder lifecycle via `control_editor.start_take_recording` / `stop_take_recording`)
- [x] `configure_recorded_tracks` (UTakeRecorderSources settings)

**Implementation note:** The lifecycle actions use UE's optional `UTakeRecorderSubsystem`, support recording into an existing Level Sequence, and expose `get_take_recording_status`. `configure_take_sources` adds resolved world actors through the official subsystem API, while `configure_recorded_tracks` applies the public `UTakeRecorderSources` policy settings. `create_take_recorder_panel` remains editor-UI dependent because Epic documents the panel object and operations but not a stable public factory for creating the editor tab from this bridge.

### 30.5 Demo/Replay System
- [x] `start_demo_recording`, `stop_demo_recording` (DemoRec/DemoStop aliases)
- [x] `configure_demo_settings` (guarded documented `demo.*` replay setting authoring)
- [x] `play_demo`, `pause_demo`, `seek_demo`
- [x] `set_demo_playback_speed`
- [x] `configure_killcam_duration`, `start_killcam` (bounded replay seek/playback window with automatic pause)

**Implementation note:** Replay playback controls route through `control_editor` and the UE `DemoNetDriver` console/API path. Killcam start accepts an explicit replay filename and start time, then pauses after the configured bounded window; streamer configuration remains project-specific.

---

## Phase 31: Data & Persistence

**Goal**: Complete data management and save systems.

**Tools**: `manage_data_assets`, `manage_save_system`, `manage_gameplay_tags`, `manage_config`

### 31.1 Data Assets
- [x] `create_data_asset`, `create_primary_data_asset` (primary action requires a concrete `UPrimaryDataAsset` subclass)
- [x] `create_data_table`, `add_data_table_row`, `modify_data_table_row`, `delete_data_table_row`
- [x] `import_data_table_csv`, `export_data_table_csv` (native `UDataTable` CSV APIs with row-struct validation and import diagnostics)
- [x] `create_curve_table`, `create_curve_float`, `create_curve_linear_color`

**Implementation note:** `manage_asset` also exposes the unlisted `get_data_asset_properties` and `set_data_asset_properties` actions for reflected `UDataAsset` properties.

### 31.2 Save System
- [x] `create_save_game_class`, `add_save_variable` (compile-ready `USaveGame` class generator accepts serialized variable declarations)
- [x] `save_game_to_slot`, `load_game_from_slot`
- [x] `delete_save_slot`, `check_save_slot_exists`
- [x] `get_save_slot_names`
- [x] `configure_async_save_load`

**Implementation note:** `system_control` maps the roadmap names to `save_game_to_slot`, `load_game_from_slot`, `delete_save_game_slot`, `check_save_game_slot`, and `list_save_game_slots`; save/load support synchronous and bounded managed async paths.

### 31.3 Gameplay Tags
- [x] `create_gameplay_tag`
- [x] `create_tag_container`
- [x] `add_tag_to_container`, `remove_tag_from_container`
- [x] `check_tag_match` (any/all and exact variants through UE `FGameplayTagContainer`)
- [x] `register_native_tag` (runtime `UGameplayTagsManager::AddNativeGameplayTag`)
- [x] `create_tag_table` (DataTable using `FGameplayTagTableRow`)

**Implementation note:** `create_gameplay_tag` safely authors the project `DefaultGameplayTags.ini` dictionary; container actions are transient request-scoped operations over registered tags. `register_native_tag` adds a runtime native tag through `UGameplayTagsManager` and is intentionally process-only; `create_tag_table` creates a persisted `UDataTable` with the official `FGameplayTagTableRow` schema.

### 31.4 Config System
- [x] `read_config_value`, `write_config_value`
- [x] `get_section`, `create_config_section`
- [x] `flush_config`, `reload_config`
- [x] `get_config_hierarchy`

**Implementation note:** The roadmap aliases map to the safe project config service, which confines filenames to `Config/*.ini`, rejects unsafe section/key/value input, and preserves optional backups for writes. Unreal's live `FConfigCacheIni` merge behavior remains a separate runtime concern.

---

## Phase 32: Build & Deployment

**Goal**: Complete build pipeline and packaging.

**Tool**: `manage_build`

### 32.1 Build Pipeline
- [x] `run_ubt` (expanded)
- [x] `generate_project_files` (managed UBT `ProjectFiles` mode)
- [x] `compile_shaders` (validated `recompileshaders changed|all` console alias)
- [x] `cook_content` (platform)
- [x] `package_project` (platform)
- [x] `configure_build_settings` (safe project INI setting authoring)
- [x] `create_build_target` (safe compile-ready `.Target.cs` generator)

**Implementation note:** `cook_content` and `package_project` are dedicated aliases over the validated RunUAT `BuildCookRun` pipeline, selecting the corresponding operation while preserving platform, configuration, async job, and packaging option handling.

`run_ubt` validates target, platform, configuration, and bounded extra arguments before launching the discovered UnrealBuildTool executable through the managed job lifecycle.

### 32.2 Platform Builds
- [x] `configure_windows_build`, `configure_linux_build`, `configure_mac_build` (safe platform target-settings INI authoring)
- [x] `configure_ios_build` (safe iOS target-settings INI authoring)
- [x] `configure_ios_signing` (writes Epic-documented Bundle Identifier, Mobile Provision, and Signing Certificate settings to `IOSRuntimeSettings`; Apple credentials, Xcode, and SDK remain external prerequisites)
- [x] `configure_android_build` (safe Android target-settings INI authoring)
- [x] `configure_android_signing` (writes Epic-documented Android package/signing settings to `AndroidRuntimeSettings`; keystore generation, credentials, and SDK remain external prerequisites)

> **Note**: Console builds (PlayStation, Xbox, Switch) require external SDK installation and platform portal registration BEFORE the project can be opened. Once SDKs are installed, MCP can configure build settings within the project.

### 32.3 Asset Management
- [x] `validate_assets`
- [x] `audit_assets` (Asset Registry inventory with dependency/referencer counts)
- [x] `size_map_analysis` (Asset Registry inventory with on-disk package sizes)
- [x] `reference_viewer` (bidirectional Asset Registry graph with configurable traversal depth)
- [x] `configure_chunking` (ProjectPackagingSettings chunk policy authoring)
- [x] `create_pak_file` (RunUAT package/PAK alias)
- [x] `configure_asset_encryption` (RunUAT INI/PAK encryption options)
- [x] `configure_compression` (RunUAT compressed packaging option)

### 32.4 Plugins
- [x] `list_plugins`
- [x] `enable_plugin`, `disable_plugin`
- [x] `get_plugin_status`
- [x] `configure_plugin_settings` (safe project config authoring for plugin DeveloperSettings sections)

**Implementation note:** Dedicated plugin actions delegate to the existing safe project-plugin service; status reports descriptor validation and project enablement, while enable/disable update the project descriptor with backups.

---

## Phase 33: Testing & Quality

**Goal**: Complete testing and profiling infrastructure.

**Tools**: `manage_testing`, `manage_profiling`, `manage_validation`

### 33.1 Automation Testing
- [x] `create_functional_test`
- [x] `create_automation_test` (safe compile-ready C++ test skeleton generator; project test logic remains authored by the caller)
- [x] `run_automation_tests` (managed job/report path)
- [x] `get_test_results` (bounded report or terminal-job inspection)
- [x] `get_test_results`
- [x] `create_test_level` (validated alias to UE level creation)
- [x] `configure_test_settings` (guarded project config authoring)
- [x] `run_gauntlet_test` (managed RunUAT RunUnreal job)

### 33.2 Profiling
- [x] `start_unreal_insights` (via `system_control.start_session`)
- [x] `stop_unreal_insights` / `get_unreal_insights_status` (via `system_control`)
- [x] `capture_insights_trace` (confined file-backed `.utrace` capture)
- [x] `analyze_trace` (bounded UnrealInsights headless analysis job)
- [x] `start_memory_report`
- [x] `start_network_profiler` (bounded enable/disable control)
- [x] `enable_visual_logger`
- [x] `add_visual_log_entry`
- [x] `enable_gameplay_debugger` (safe category enable/disable through the Gameplay Debugger module)
- [x] `configure_stat_commands`

### 33.3 Validation
- [x] `create_asset_validator`
- [x] `run_data_validation`
- [x] `check_for_errors` (bounded message-log category error gate)
- [x] `fix_redirectors`
- [x] `check_map_errors`
- [x] `validate_blueprints` (bounded project or path sweep with compiler diagnostics)

**Implementation note:** `run_data_validation` is a dedicated alias for the managed `UnrealEditor-Cmd.exe -run=DataValidation` path; custom validator authoring remains project-specific.

---

## Phase 34: Editor Utilities

**Goal**: Complete editor automation.

**Tools**: Various editor management tools

### 34.1 Editor Modes
- [x] `set_editor_mode` (place, paint, landscape, foliage, mesh_paint)
- [x] `configure_editor_preferences`
- [x] `set_grid_settings`, `set_snap_settings`
- [x] `manage_editor_layouts`
- [x] `create_custom_editor_mode` (descriptor validation; compiled editor module/plugin required for runtime `UEdMode` registration)

**Implemented feature coverage**: editor-mode selection; supported preference/CVar updates; translation, rotation, scale, actor-distance, actor-scale, power-of-two, and surface snapping; named layout save/load/remove/reset/import/export command routing; and safe custom-mode descriptors.

**Official Unreal references**: [`ULevelEditorViewportSettings`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/UnrealEd/ULevelEditorViewportSettings), [`FSnapToSurfaceSettings`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/UnrealEd/FSnapToSurfaceSettings), [`FEditorViewportCommands`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/UnrealEd/FEditorViewportCommands), [`UEdMode`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/UnrealEd/UEdMode), and [Layout Customization](https://dev.epicgames.com/documentation/en-us/unreal-engine/layout-customization?application_version=4.27).

### 34.2 Content Browser
- [x] `set_view_settings`
- [x] `navigate_to_path`
- [x] `sync_to_asset`, `sync_to_folder`
- [x] `create_collection`, `add_to_collection`
- [x] `set_asset_color`
- [x] `show_in_explorer`
- [x] `set_search_text`

**Implemented feature coverage**: Content Browser boolean view/search settings with optional persistence; folder navigation and folder/asset synchronization; local, private, and shared static/dynamic collections; folder-path color persistence and refresh; Explorer reveal; and direct search-text control. `viewType` and `thumbnailSize` are accepted for forward compatibility and reported as unsupported when no mutable public singleton setter is available for an existing browser instance.

**Official Unreal references**: [`FContentBrowserModule`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/ContentBrowser/FContentBrowserModule), [`IContentBrowserSingleton`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/ContentBrowser/IContentBrowserSingleton), [`SetSelectedPaths`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/ContentBrowser/IContentBrowserSingleton/SetSelectedPaths?application_version=5.5), [`SyncBrowserTo`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/ContentBrowser/IContentBrowserSingleton/SyncBrowserTo?application_version=5.5), [`ICollectionManager`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Developer/CollectionManager/ICollectionManager), [`AddToCollection`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Developer/CollectionManager/ICollectionManager/AddToCollection), and [Working with Assets](https://dev.epicgames.com/documentation/en-us/unreal-engine/working-with-assets-in-unreal-engine).

### 34.3 Selection
- [x] `select_actor`
- [x] `select_actors_by_class`
- [x] `select_actors_by_tag`
- [x] `select_actors_in_volume`
- [x] `deselect_all`
- [x] `get_selected_actors`
- [x] `group_actors`, `ungroup_actors`
- [x] `select_all`, `invert_selection`, `select_children`
- [x] `remove_selected_from_group`, `lock_selected_groups`, `unlock_selected_groups`

**Implemented feature coverage**: exact actor selection by label/name/path; class selection with optional derived classes; gameplay-tag selection; volume-intersection selection using actor bounds; clear/read/select-all/invert/child-hierarchy selection; and group, ungroup, remove, lock, and unlock operations. Selection responses include actor labels, object names, paths, classes, hidden state, and counts.

**Official Unreal references**: [`UEditorEngine::SelectActor`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/UnrealEd/Editor/UEditorEngine/SelectActor?application_version=5.5), [`UEditorActorSubsystem`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/UnrealEd/Subsystems/UEditorActorSubsystem?application_version=5.5), [`UEditorActorSubsystem::GetSelectedLevelActors`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/UnrealEd/UEditorActorSubsystem/GetSelectedLevelActors?lang=en-US), [`AVolume::EncompassesPoint`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/AVolume?application_version=5.5), and [`UActorGroupingUtils`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/UnrealEd/UActorGroupingUtils).

### 34.4 Collision
- [x] `create_collision_channel`
- [x] `create_collision_profile`
- [x] `configure_channel_responses`
- [x] `configure_object_type`
- [x] `configure_trace_channel`
- [x] `set_actor_collision_profile`
- [x] `set_component_collision_profile`
- [x] `get_actor_collision`, `get_component_collision`, `validate_collision_profile`

**Implemented feature coverage**: configuration-backed custom object/trace channels with slot validation, default responses, optional `DefaultEngine.ini` persistence, profile creation with collision mode/object type/custom responses, runtime profile assignment to every primitive component on an actor or to one component, per-channel response updates, object-type changes, trace-channel response changes, and structured collision readback/validation.

**Official Unreal references**: [`UCollisionProfile`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UCollisionProfile), [`UCollisionProfile::LoadProfileConfig`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UCollisionProfile/LoadProfileConfig?application_version=5.7), [`FCollisionResponseContainer`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FCollisionResponseContainer), [`UPrimitiveComponent`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UPrimitiveComponent), [`SetCollisionProfileName`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UPrimitiveComponent/SetCollisionProfileName), [`SetCollisionObjectType`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UPrimitiveComponent/SetCollisionObjectType), [`SetCollisionResponseToChannel`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UPrimitiveComponent/SetCollisionResponseToChannel), and [`ECollisionChannel`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/ECollisionChannel).

### 34.5 Physics Materials
- [x] `create_physical_material`
- [x] `set_friction`, `set_restitution`, `set_density`
- [x] `configure_surface_type`
- [x] `assign_physical_material`
- [x] `configure_physical_material`, `get_physical_material`, `clear_physical_material_override`

**Implemented feature coverage**: editor-only `UPhysicalMaterial` asset creation through `UPhysicalMaterialFactoryNew`; validated friction, static friction, restitution, density, surface type, friction/restitution combine modes, and combine-mode override flags; structured physical-material readback; project `PhysicalSurfaces` configuration with optional `DefaultEngine.ini` persistence; assignment to every primitive component or one named component on an actor; and explicit clearing of component physical-material overrides.

**Official Unreal references**: [`UPhysicalMaterial`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/PhysicsCore/PhysicalMaterials/UPhysicalMaterial), [`UPhysicalMaterialFactoryNew`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/UnrealEd/Factories/UPhysicalMaterialFactoryNew?application_version=5.5), [`UPrimitiveComponent::SetPhysMaterialOverride`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UPrimitiveComponent/SetPhysMaterialOverride), [Physical Materials Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/physical-materials-reference-for-unreal-engine), [Add a Surface Type](https://dev.epicgames.com/documentation/en-us/unreal-engine/add-a-surface-type-in-unreal-engine?lang=en-US), and [Physics Settings](https://dev.epicgames.com/documentation/en-us/unreal-engine/physics-settings-in-the-unreal-engine-project-settings?lang=en-US).

### 34.6 Subsystems
- [x] `create_game_instance_subsystem`
- [x] `create_world_subsystem`
- [x] `create_local_player_subsystem`
- [x] `create_engine_subsystem`
- [x] `configure_subsystem_tick`
- [x] `get_subsystem`, `inspect_subsystem`, `list_subsystems`

**Implemented feature coverage**: managed resolution of engine, game-instance, world, local-player, and editor subsystems through Unreal's subsystem collections; context-aware PIE/editor world selection; local-player index selection; live subsystem enumeration and structured class/object/outer metadata; tickability inspection; and conditional/always/never tick-mode configuration for `UTickableWorldSubsystem` instances. The create actions acquire Unreal-managed instances and never bypass subsystem lifecycle initialization with `NewObject`.

**Official Unreal references**: [`USubsystem`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/USubsystem), [`UGameInstanceSubsystem`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UGameInstanceSubsystem), [`UWorldSubsystem`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UWorldSubsystem), [`ULocalPlayerSubsystem`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/ULocalPlayerSubsystem), [`UEngine::GetEngineSubsystemBase`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UEngine), [`UWorld::GetSubsystemBase`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/Engine/UWorld?application_version=5.5), [`FTickableGameObject::SetTickableTickType`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FTickableGameObject/SetTickableTickType), and [`ETickableTickType`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/ETickableTickType?lang=en-US).

### 34.7 Async & Timers
- [x] `set_timer`, `clear_timer`, `pause_timer`, `resume_timer`, `get_timer`, `list_timers`
- [x] `create_latent_action`, `clear_latent_action`, `get_latent_action`, `list_latent_actions`
- [x] `create_async_action`, `cancel_async_action`, `get_async_action`, `list_async_actions`
- [x] `create_gameplay_task`, `end_gameplay_task`, `get_gameplay_task`, `list_gameplay_tasks`
- [x] `configure_task_priority`

**Implemented feature coverage**: world-scoped `FTimerManager` scheduling with looping, first-delay, pause/resume, clear, and elapsed/remaining/active/pending inspection; UUID-backed `FLatentActionManager` delay actions with optional zero-argument callback links and cooperative cancellation; asynchronous work on Unreal task-graph, thread, and thread-pool execution modes with lifecycle events and cooperative cancellation; and managed generic `UGameplayTask` creation, activation, inspection, ending, and priority reconfiguration by task recreation when inactive. Operation identifiers make each managed item independently inspectable and safe to clean up when the editor subsystem deinitializes.

**Official Unreal references**: [`FTimerManager`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FTimerManager?lang=en-US), [Gameplay Timers](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-timers-in-unreal-engine?lang=en-US), [`FLatentActionManager`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FLatentActionManager?lang=en-US), [`FPendingLatentAction`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FPendingLatentAction), [`Async`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/Async?lang=en-US), [`EAsyncExecution`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/EAsyncExecution), [`UGameplayTask`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/GameplayTasks/UGameplayTask?lang=en-US), and [`UGameplayTasksComponent`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/GameplayTasks/UGameplayTasksComponent).

### 34.8 Delegates & Interfaces
- [x] `create_event_dispatcher`
- [x] `bind_to_event`, `unbind_from_event`
- [x] `broadcast_event`
- [x] `create_delegate`, `bind_delegate`
- [x] `inspect_delegate`, `list_delegate_bindings`
- [x] `create_blueprint_interface`, `add_interface_function`
- [x] `implement_interface`
- [x] `get_interface_info`, `call_interface_function`

**Implemented feature coverage**: editor-safe creation of single-cast and multicast Blueprint delegate variables; event-dispatcher authoring; reflection-backed binding, unbinding, inspection, and broadcasting; Blueprint Interface asset creation and function-graph authoring; interface implementation through `FBlueprintEditorUtils::ImplementNewInterface`; interface enumeration; and reflected interface calls with JSON-marshaled parameters and return values. Runtime delegate/object references are intentionally resolved only from loaded Unreal objects, and authored assets are saved through the bridge safe-save wrapper.

**Additional coverage**: delegate signatures and bound-object metadata are returned by inspection; multicast invocation supports reflected parameter values through `parameterValues`; interface calls expose the interface class, target, function, and return value; and delegate/function-graph authoring reports idempotent/existing states where Unreal permits them.

**Official Unreal references**: [`FDelegateProperty`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/CoreUObject/FDelegateProperty), [`FMulticastDelegateProperty`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/CoreUObject/FMulticastDelegateProperty), [`TScriptDelegate`](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/TScriptDelegate?lang=en-US), [`TMulticastScriptDelegate`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/TMulticastScriptDelegate), [`UEdGraphSchema_K2::PC_Delegate`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/BlueprintGraph/UEdGraphSchema_K2), [`FBlueprintEditorUtils::ImplementNewInterface`](https://dev.epicgames.com/documentation/unreal-engine/API/Editor/UnrealEd/FBlueprintEditorUtils/ImplementNewInterface?lang=en-US), [`FBlueprintEditorUtils::CreateFunctionGraph`](https://dev.epicgames.com/documentation/unreal-engine/API/Editor/UnrealEd/FBlueprintEditorUtils/CreateFunctionGraph), [`UInterface`](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/CoreUObject/UInterface?lang=en-US), and [Unreal Interfaces](https://dev.epicgames.com/documentation/unreal-engine/interfaces-in-unreal-engine).

---

## Phase 35: Additional Gameplay Systems

**Goal**: Common gameplay patterns and systems.

### 35.1 Targeting System
- [ ] `create_targeting_component`
- [ ] `configure_lock_on_target`
- [ ] `set_target_priority`
- [ ] `configure_target_switching`
- [ ] `configure_soft_lock`
- [ ] `configure_aim_assist`

### 35.2 Checkpoint System
- [ ] `create_checkpoint_actor`
- [x] `configure_checkpoint_data` (named façade over the validated SaveGame class generator; authors a project-confined `USaveGame` subclass and optional schema/migration metadata for checkpoint payloads)
- [x] `save_checkpoint_async`, `load_checkpoint_async` (API-derived aliases over Epic's `UGameplayStatics::AsyncSaveGameToSlot`/`AsyncLoadGameFromSlot`, returning the bridge-managed `asyncId` lifecycle)
- [x] `save_checkpoint`, `load_checkpoint` (aliases over the existing UE `UGameplayStatics` SaveGame slot pipeline, defaulting to the `Checkpoint` slot while accepting an explicit `slotName`)
- [ ] `configure_checkpoint_respawn`

### 35.3 Objective System
- [ ] `create_objective`
- [ ] `set_objective_state` (locked, active, completed, failed)
- [ ] `configure_objective_markers`
- [ ] `create_objective_tracker_widget`
- [ ] `configure_objective_progression`

### 35.4 World Markers/Ping System
- [ ] `create_world_marker`
- [ ] `create_ping_system`
- [ ] `configure_marker_widget`
- [ ] `configure_marker_3d_2d`
- [ ] `configure_marker_distance`
- [ ] `configure_marker_occlusion`

### 35.5 Photo Mode
- [ ] `enable_photo_mode`
- [ ] `configure_photo_mode_camera`
- [ ] `configure_photo_mode_filters`
- [ ] `configure_photo_mode_poses`
- [x] `take_photo_mode_screenshot` (named alias over the validated screenshot pipeline, including editor/game/full-window modes and optional metadata)
- [ ] `configure_photo_mode_ui`

### 35.6 Quest/Dialogue System
- [ ] `create_quest_data_asset`
- [ ] `create_quest_manager`
- [ ] `start_quest`, `complete_quest_objective`, `track_quest`
- [ ] `create_dialogue_tree`
- [ ] `add_dialogue_node`, `add_dialogue_choice`
- [ ] `configure_dialogue_conditions`
- [ ] `play_dialogue`

### 35.7 Instancing & HLOD
- [x] `create_instanced_static_mesh_component` (native actor component authoring)
- [x] `create_hierarchical_instanced_static_mesh` (native actor component authoring)
- [x] `add_instance`, `remove_instance`, `update_instance_transform` (ISM/HISM component operations)
- [x] `configure_instance_culling`, `configure_instance_lod` (ISM/HISM culling and LOD properties)
- [x] `create_hlod_layer`
- [x] `configure_hlod_settings` (via `configure_hlod_layer`)
- [x] `add_actors_to_hlod`, `build_hlod` (via `assign_hlod_layer` and `build_hlods`)
- [x] `configure_hlod_transition` (HLOD layer loading-range/cell-size alias)

**Implementation note:** World Partition HLOD layer creation, actor assignment, configuration, inspection, and managed rebuild/delete lifecycle are available through `manage_level_structure`; `configure_hlod_transition` aliases the layer's engine-backed loading range and cell size settings.

### 35.8 Localization
- [x] `create_string_table`
- [x] `add_string_entry`
- [x] `configure_localization_target` (guarded GatherText INI authoring)
- [x] `import_localization`, `export_localization` (managed GatherText commandlet jobs)
- [x] `set_culture`
- [x] `get_localized_string`

**Implementation note:** These actions manage transient process-level string tables through UE's `FStringTableRegistry`; `set_culture` uses `FInternationalization::SetCurrentCulture` and returns the active IETF culture name. `configure_localization_target` writes a validated GatherText target config under `Config/Localization`; import/export launch managed `GatherText` commandlet jobs against a validated target config.

**Additional API coverage:** `system_control.set_language_and_locale` and `set_locale` expose Epic's narrower language/locale setters for callers that must avoid changing all active culture state.

### 35.9 Scalability
- [x] `create_device_profile` (safe `DefaultDeviceProfiles.ini` authoring)
- [x] `configure_scalability_group` (validated group-to-scalability-CVar mapping)
- [x] `set_cvar_for_profile` (safe profile CVar override authoring)
- [x] `configure_platform_settings` (safe platform INI setting authoring)
- [x] `set_quality_level` (overall 0-4 scalability level via `Scalability::FQualityLevels`)

---

# Plugin Integration Phases

---

## Phase 36: Character & Avatar Plugins

### 36.1 MetaHuman
- [ ] `import_metahuman`, `list_available_metahumans`, `spawn_metahuman_actor`
- [ ] `configure_metahuman_component`, `set_lod_settings`, `configure_body_type`
- [ ] Face: `get_face_parameters`, `set_face_parameter`, `set_skin_tone`, `set_eye_color`, `set_hair_style`, `set_hair_color`, `set_eyebrow_style`, `set_teeth_configuration`, `set_makeup`
- [ ] Body: `set_body_proportions`, `set_body_type`, `set_height`
- [ ] DNA/Rig: `export_metahuman_dna`, `create_custom_rig_logic`, `configure_control_rig_for_metahuman`
- [ ] LOD: `configure_metahuman_lod_bias`, `enable_disable_features_for_performance`

### 36.2 Ready Player Me
- [ ] `load_avatar_from_url`, `load_avatar_from_glb`
- [ ] `configure_avatar_component`
- [ ] `setup_avatar_skeleton_mapping`
- [ ] `apply_avatar_to_character`
- [ ] `configure_avatar_lod`

### 36.3 Mutable (Character Customization)
- [ ] `create_customizable_object`, `add_component_mesh`, `add_component_parameter`
- [ ] `create_customizable_instance`
- [ ] `set_int_parameter`, `set_float_parameter`, `set_color_parameter`, `set_projector_parameter`
- [ ] `update_skeletal_mesh`
- [ ] `bake_customizable_instance`, `configure_bake_settings`

### 36.4 Groom/Hair System
- [ ] `create_groom_asset`, `import_groom` (alembic, cache), `configure_groom_lod`
- [ ] `create_groom_binding`, `bind_groom_to_skeletal_mesh`
- [ ] `configure_hair_simulation`, `set_hair_stiffness`, `set_hair_damping`, `configure_hair_collision`
- [ ] `configure_hair_material`, `set_hair_color`, `set_hair_roughness`

---

## Phase 37: Asset & Content Plugins

### 37.1 Quixel Bridge / Megascans / Fab
- [ ] `connect_to_bridge`, `list_bridge_assets`, `list_downloaded_assets`
- [ ] `import_megascan_surface`, `import_megascan_3d_asset`, `import_megascan_3d_plant`, `import_megascan_decal`, `import_megascan_atlas`, `import_megascan_brush`
- [ ] `configure_import_settings`, `set_lod_generation`, `set_material_blend_mode`, `configure_nanite_import`
- [x] `configure_virtual_texture` (shared texture authoring handler)
- [ ] `search_megascan_library`, `filter_by_category`, `filter_by_biome`, `download_asset`
- [ ] Fab: `browse_fab_assets`, `download_fab_asset`, `import_fab_asset`

### 37.2 Interchange Framework
- [ ] `create_interchange_pipeline`, `add_pipeline_step`, `configure_pipeline_settings`
- [ ] `register_custom_translator`, `configure_translator_settings`
- [ ] `import_with_interchange`, `configure_import_asset_type`, `set_reimport_strategy`
- [ ] `import_fbx_interchange`, `import_gltf_interchange`, `import_usd_interchange`, `import_obj_interchange`

### 37.3 USD
- [ ] Stage: `create_usd_stage`, `open_usd_stage`, `save_usd_stage`, `close_usd_stage`
- [ ] Prims: `create_usd_prim`, `set_prim_transform`, `set_prim_attribute`, `add_prim_reference`, `add_prim_payload`
- [ ] Layers: `create_usd_layer`, `add_sublayer`, `set_edit_target`, `mute_layer`
- [ ] Export: `export_level_to_usd`, `export_actors_to_usd`, `configure_usd_export_options`
- [ ] Import: `import_usd_to_level`, `configure_usd_import_options`, `import_usd_animations`
- [ ] Live: `enable_usd_live_edit`, `configure_usd_sync`

### 37.4 Alembic
- [ ] `import_alembic_file`, `configure_alembic_import` (geometry, groom, animation)
- [ ] `set_alembic_sampling`, `set_alembic_frame_range`
- [ ] `create_geometry_cache_track`, `configure_cache_playback`
- [ ] `import_alembic_groom`, `bind_groom_to_mesh`

### 37.5 glTF
- [ ] `import_gltf`, `import_glb`, `configure_gltf_import_options`, `set_gltf_material_import`
- [ ] `export_to_gltf`, `export_to_glb`, `configure_gltf_export_options`, `set_draco_compression`

### 37.6 Substance Plugin
- [ ] `import_sbsar_file`, `create_substance_graph_instance`
- [ ] `get_substance_parameters`, `set_substance_parameter`, `randomize_substance_seed`
- [ ] `render_substance_textures`, `configure_output_size`, `export_substance_outputs`
- [ ] `create_material_from_substance`, `update_substance_material`

### 37.7 Houdini Engine
- [ ] `import_hda`, `instantiate_hda`, `list_hda_parameters`
- [ ] Parameters: `set_hda_float_parameter`, `set_hda_int_parameter`, `set_hda_string_parameter`, `set_hda_toggle_parameter`, `set_hda_ramp_parameter`
- [ ] Input: `set_hda_geometry_input`, `set_hda_curve_input`, `set_hda_world_input`
- [ ] Cooking: `cook_hda`, `recook_hda`, `configure_cook_options`
- [ ] Output: `get_hda_output_meshes`, `get_hda_output_instances`, `bake_hda_to_actors`, `bake_hda_to_blueprint`
- [ ] Sessions: `start_houdini_session`, `stop_houdini_session`, `configure_session_type`

### 37.8 SpeedTree
- [ ] `import_speedtree_model`, `configure_speedtree_import`
- [ ] `configure_speedtree_wind`, `set_wind_parameters`
- [ ] `configure_speedtree_lod`, `set_billboard_settings`
- [ ] `configure_speedtree_material`, `set_subsurface_color`

### 37.9 Datasmith/CAD
- [ ] `import_datasmith_file`, `configure_datasmith_options`
- [ ] `import_cad_file`, `configure_tessellation`
- [ ] `import_revit`, `import_sketchup`

---

## Phase 38: Audio Middleware Plugins

### 38.1 Wwise
- [ ] Project: `connect_wwise_project`, `refresh_wwise_project`, `generate_sound_banks`
- [ ] Events: `list_wwise_events`, `post_wwise_event`, `post_wwise_event_at_location`, `post_wwise_event_attached`, `stop_wwise_event`
- [ ] Game Syncs: `set_rtpc_value`, `set_wwise_switch`, `set_wwise_state`, `post_wwise_trigger`
- [ ] Spatial: `create_ak_spatial_audio_volume`, `configure_room`, `configure_portal`, `set_room_reverb`, `configure_geometry`
- [ ] Components: `add_ak_component`, `configure_ak_component`, `set_ak_listener`
- [ ] Environment: `set_ak_environment`, `add_ak_reverb_volume`
- [ ] Banks: `load_soundbank`, `unload_soundbank`, `configure_auto_load`
- [ ] Profiling: `start_wwise_profiler_capture`, `stop_wwise_profiler_capture`

### 38.2 FMOD
- [ ] Project: `connect_fmod_project`, `refresh_fmod_banks`
- [ ] Events: `list_fmod_events`, `play_fmod_event`, `play_fmod_event_at_location`, `play_fmod_event_attached`, `stop_fmod_event`, `stop_all_fmod_events`
- [ ] Parameters: `set_fmod_global_parameter`, `set_fmod_event_parameter`, `get_fmod_parameter_value`
- [ ] Snapshots: `start_fmod_snapshot`, `stop_fmod_snapshot`
- [ ] Buses: `set_fmod_bus_volume`, `set_fmod_bus_paused`, `set_fmod_bus_muted`, `set_fmod_vca_volume`
- [ ] Banks: `load_fmod_bank`, `unload_fmod_bank`, `get_bank_loading_state`
- [ ] Components: `add_fmod_audio_component`, `configure_fmod_component`

### 38.3 Bink Video
- [ ] `create_bink_media_player`, `open_bink_video`
- [ ] `play_bink`, `pause_bink`, `stop_bink`, `seek_bink`
- [ ] `set_bink_loop`, `set_bink_audio_tracks`, `configure_bink_texture`

---

## Phase 39: Motion Capture & Live Link Plugins

### 39.1 Live Link (Core)
- [ ] Sources: `add_livelink_source`, `remove_livelink_source`, `list_livelink_sources`, `configure_livelink_source`
- [ ] Presets: `create_livelink_preset`, `apply_livelink_preset`, `save_livelink_preset`
- [ ] Subjects: `list_livelink_subjects`, `get_subject_data`, `set_subject_enabled`, `configure_subject_settings`
- [ ] Roles: `set_livelink_role`, `configure_role_mapping`
- [ ] Retargeting: `create_livelink_retarget_asset`, `configure_retarget_bones`
- [ ] Timecode: `configure_livelink_timecode`, `synchronize_livelink_sources`

### 39.2 Live Link Face (iOS)
- [ ] `configure_livelink_face_source`, `set_face_source_ip`
- [ ] `configure_arkit_blendshape_mapping`, `map_blendshape_to_morph`
- [ ] `configure_head_rotation_mapping`, `configure_eye_tracking_mapping`
- [ ] `set_neutral_pose`, `configure_sensitivity_multiplier`

### 39.3 OptiTrack (Motive)
- [ ] `connect_optitrack_server`, `configure_optitrack_settings`
- [ ] `set_bone_naming_convention`, `configure_skeleton_mapping`, `configure_rigid_body_tracking`
- [ ] `list_optitrack_skeletons`, `list_optitrack_rigid_bodies`, `assign_optitrack_to_actor`

### 39.4 Vicon
- [ ] `connect_vicon_datastream`, `configure_vicon_settings`
- [ ] `list_vicon_subjects`, `get_vicon_subject_data`, `configure_vicon_retargeting`

### 39.5 Rokoko
- [ ] `connect_rokoko_studio`, `configure_rokoko_settings`
- [ ] `list_rokoko_actors`, `assign_rokoko_to_character`, `configure_rokoko_mapping`
- [ ] Props: `list_rokoko_props`, `assign_rokoko_prop`
- [ ] Face: `configure_rokoko_face`, `map_rokoko_blendshapes`

### 39.6 Xsens MVN
- [ ] `connect_xsens_mvn`, `configure_xsens_streaming`, `map_xsens_skeleton`, `assign_xsens_to_character`

---

## Phase 40: Virtual Production Plugins

### 40.1 nDisplay
- [ ] Cluster: `create_ndisplay_config`, `add_cluster_node`, `configure_cluster_node`, `set_primary_node`
- [ ] Viewports: `create_viewport`, `configure_viewport_region`, `set_viewport_camera`, `configure_viewport_projection`
- [ ] Projection: `set_projection_policy`, `configure_projection_mesh`, `configure_projection_frustum`, `import_mpcdi_file`
- [ ] ICVFX: `create_icvfx_camera`, `configure_inner_frustum`, `configure_outer_region`, `set_chromakey_color`, `configure_distortion_correction`
- [ ] Sync: `configure_genlock`, `configure_frame_sync`, `set_swap_sync_policy`
- [ ] Color: `configure_per_viewport_color_grading`, `set_viewport_ocio_config`
- [ ] Stage: `create_stage_actor`, `configure_stage_geometry`, `configure_light_cards`
- [ ] Runtime: `switch_ndisplay_config`, `set_viewport_enabled`

### 40.2 Composure
- [ ] `create_composure_actor`, `add_composure_layer`
- [x] `configure_layer_blend` (shared material/layer authoring handler)
- [ ] Elements: `add_cg_layer`, `add_media_layer`, `add_transform_pass`, `add_compositing_pass`
- [ ] Keying: `add_chroma_keyer`, `configure_chroma_key_color`, `configure_key_settings`
- [ ] Transform: `configure_layer_transform`, `add_distortion_correction`
- [ ] Output: `configure_composure_output`, `set_output_resolution`, `enable_output_to_render_target`

### 40.3 OpenColorIO (OCIO)
- [ ] `load_ocio_config`, `set_active_ocio_config`, `list_color_spaces`, `list_displays`, `list_views`
- [ ] `set_working_color_space`, `configure_viewport_display_transform`, `add_color_transform_to_viewport`
- [ ] `set_texture_color_space`, `configure_material_color_space`

### 40.4 Remote Control
- [ ] Presets: `create_remote_control_preset`, `add_property_to_preset`, `add_function_to_preset`, `expose_actor_properties`, `expose_blueprint_functions`
- [ ] Groups: `create_preset_group`, `add_property_to_group`
- [ ] Web: `start_web_remote_control_server`, `configure_web_server`, `set_cors_settings`
- [ ] API: `register_custom_route`, `configure_api_permissions`
- [ ] Bindings: `create_remote_control_binding`, `bind_to_protocol`

### 40.5 DMX
- [ ] Library: `create_dmx_library`, `add_dmx_universe`, `configure_universe_settings`
- [ ] Fixtures: `add_dmx_fixture_type`, `configure_fixture_modes`, `add_fixture_function`, `add_dmx_fixture_patch`
- [ ] Ports: `create_dmx_input_port`, `create_dmx_output_port`, `configure_port_protocol`, `set_port_ip_address`
- [ ] Components: `add_dmx_component`, `configure_dmx_receive`, `configure_dmx_transmit`
- [ ] GDTF/MVR: `import_gdtf_fixture`, `configure_gdtf_settings`, `import_mvr_scene`, `export_mvr_scene`

### 40.6 OSC
- [ ] Server: `create_osc_server`, `set_osc_server_port`, `start_osc_server`, `stop_osc_server`
- [ ] Client: `create_osc_client`, `set_osc_client_target`
- [ ] Messages: `send_osc_message`, `send_osc_bundle`, `register_osc_address_handler`
- [ ] Binding: `bind_osc_to_property`, `bind_osc_to_function`

### 40.7 MIDI
- [ ] Devices: `list_midi_devices`, `open_midi_input`, `open_midi_output`, `close_midi_device`
- [ ] Messages: `send_midi_note_on`, `send_midi_note_off`, `send_midi_control_change`, `send_midi_program_change`, `send_midi_pitch_bend`
- [ ] Events: `register_midi_note_handler`, `register_midi_cc_handler`
- [ ] Binding: `bind_midi_to_property`, `bind_midi_cc_to_float`

### 40.8 Timecode
- [ ] `set_timecode_provider`, `configure_timecode_framerate`
- [ ] `configure_genlock_source`, `set_custom_timecode`, `configure_timecode_offset`
- [ ] `configure_ltc_input`, `configure_ltc_output`

---

## Phase 41: XR Plugins (VR/AR/MR)

### 41.1 OpenXR
- [ ] System: `get_openxr_runtime_name`, `configure_openxr_settings`
- [ ] Tracking: `configure_tracking_origin`, `get_hmd_transform`, `get_controller_transform`, `configure_hand_tracking`
- [ ] Actions: `create_openxr_action_set`, `add_openxr_action`, `bind_action_to_controller`, `configure_action_binding`
- [ ] Extensions: `enable_openxr_extension`, `configure_passthrough`, `configure_hand_mesh`
- [ ] Haptics: `trigger_haptic_feedback`, `configure_haptic_settings`

### 41.2 Meta Quest
- [ ] Platform: `configure_quest_settings`, `set_cpu_gpu_level`, `configure_foveated_rendering`, `configure_compositor_layers`
- [ ] Passthrough: `enable_passthrough`, `configure_passthrough_style`, `create_passthrough_layer`
- [ ] Anchors: `create_spatial_anchor`, `save_spatial_anchor`, `load_spatial_anchors`, `share_spatial_anchor`
- [ ] Scene: `enable_scene_capture`, `get_room_layout`, `get_scene_planes`, `get_scene_volumes`
- [ ] Hand: `enable_quest_hand_tracking`, `get_hand_skeleton`, `get_hand_gestures`, `configure_hand_mesh`
- [ ] Body: `enable_quest_body_tracking`, `get_body_skeleton`
- [ ] Face: `enable_quest_face_tracking`, `get_face_expressions`
- [ ] Eye: `enable_quest_eye_tracking`, `get_eye_gaze`

### 41.3 SteamVR
- [ ] System: `configure_steamvr_settings`, `get_steamvr_runtime_version`
- [ ] Chaperone: `configure_chaperone_bounds`, `get_chaperone_bounds`, `set_chaperone_color`
- [ ] Controllers: `configure_controller_bindings`, `get_controller_model`
- [ ] Overlays: `create_steamvr_overlay`, `set_overlay_transform`, `set_overlay_texture`
- [ ] Skeletal: `configure_skeletal_tracking`, `get_skeletal_data`

### 41.4 Apple ARKit
- [ ] Session: `configure_arkit_session`, `start_arkit_session`, `pause_arkit_session`
- [ ] Tracking: `configure_world_tracking`, `configure_image_tracking`, `configure_object_tracking`, `configure_body_tracking`, `configure_face_tracking`
- [ ] Anchors: `get_tracked_planes`, `create_arkit_anchor`, `get_tracked_images`, `get_tracked_objects`
- [ ] Occlusion: `enable_people_occlusion`, `enable_scene_depth`, `configure_lidar_meshing`
- [ ] Environment: `capture_environment_probe`, `get_light_estimation`
- [ ] Face: `get_face_geometry`, `get_face_blendshapes`, `get_eye_tracking`

### 41.5 Google ARCore
- [ ] Session: `configure_arcore_session`, `check_arcore_availability`
- [ ] Tracking: `configure_arcore_world_tracking`, `configure_arcore_image_tracking`, `configure_arcore_face_tracking`
- [ ] Planes: `get_arcore_planes`, `configure_plane_detection`
- [ ] Depth: `enable_arcore_depth`, `get_depth_image`
- [ ] Cloud: `host_cloud_anchor`, `resolve_cloud_anchor`
- [ ] Geospatial: `enable_geospatial`, `get_geospatial_pose`, `create_geospatial_anchor`

### 41.6 Varjo
- [ ] `configure_varjo_settings`, `set_varjo_foveated_rendering`
- [ ] MR: `enable_varjo_video_passthrough`, `configure_passthrough_blend`, `configure_depth_test`
- [ ] Eye: `enable_varjo_eye_tracking`, `get_eye_gaze_data`, `configure_gaze_data_output`

### 41.7 HoloLens
- [ ] `configure_hololens_settings`, `set_holographic_remoting`
- [ ] Spatial: `configure_spatial_mapping`, `get_spatial_mesh`, `create_spatial_anchor`
- [ ] QR: `enable_qr_tracking`, `get_tracked_qr_codes`
- [ ] Hand: `configure_hand_tracking`, `get_hand_joint_data`, `configure_hand_mesh`
- [ ] Voice: `register_voice_command`, `configure_voice_recognition`

---

## Phase 42: AI & NPC Plugins

### 42.1 Convai
- [ ] Characters: `create_convai_character`, `configure_character_backstory`, `set_character_personality`
- [ ] Conversation: `start_conversation`, `send_text_to_character`, `send_voice_to_character`, `get_character_response`
- [ ] Actions: `configure_character_actions`, `trigger_character_action`
- [ ] Lip Sync: `configure_convai_lipsync`, `map_visemes_to_morphs`

### 42.2 Inworld AI
- [ ] Characters: `create_inworld_character`, `configure_character_brain`, `set_character_goals`
- [ ] Integration: `connect_inworld_service`, `configure_inworld_settings`
- [ ] Conversation: `start_inworld_session`, `send_player_message`, `receive_character_response`
- [ ] Emotions: `get_character_emotion`, `configure_emotion_response`

### 42.3 NVIDIA ACE
- [ ] `configure_audio2face`
- [ ] `process_audio_to_blendshapes`
- [ ] `stream_audio_to_face`
- [ ] `configure_blendshape_mapping`
- [ ] `set_emotion_weights`

---

## Phase 43: Online Services Plugins

### 43.1 Online Subsystem (Core)
- [ ] `get_online_subsystem`, `configure_default_subsystem`
- [ ] Identity: `login`, `logout`, `get_player_nickname`, `get_unique_net_id`
- [ ] Sessions: `create_session`, `find_sessions`, `join_session`, `destroy_session`, `register_player`, `unregister_player`, `get_session_state`
- [ ] Friends: `get_friends_list`, `send_friend_invite`, `accept_friend_invite`, `get_friend_presence`
- [ ] Achievements: `get_achievements`, `unlock_achievement`, `get_achievement_progress`, `write_achievement_progress`
- [ ] Leaderboards: `read_leaderboard`, `write_leaderboard`, `get_leaderboard_entries`
- [ ] Stats: `read_stats`, `write_stats`
- [ ] Voice: `configure_voice_chat`, `mute_player`, `set_voice_volume`

### 43.2 Epic Online Services (EOS)
- [ ] Platform: `initialize_eos`, `configure_eos_settings`
- [ ] Auth: `login_eos`, `link_account`, `get_eos_product_user_id`, `login_with_connect`, `create_device_id`
- [ ] Lobby: `create_eos_lobby`, `find_eos_lobbies`, `join_eos_lobby`, `leave_eos_lobby`, `update_lobby_attributes`
- [ ] P2P: `configure_p2p`, `create_socket`, `send_p2p_message`, `receive_p2p_message`
- [ ] Voice: `join_eos_voice_room`, `leave_eos_voice_room`, `mute_eos_player`
- [ ] Achievements: `define_eos_achievements`, `unlock_eos_achievement`, `get_player_achievements`
- [ ] Stats: `ingest_eos_stat`, `query_player_stats`
- [ ] Leaderboards: `query_eos_leaderboard`, `submit_leaderboard_score`
- [ ] Storage: `write_player_storage`, `read_player_storage`, `delete_player_storage`, `query_title_storage`, `read_title_storage_file`
- [ ] Anti-Cheat: `configure_eos_anti_cheat`, `start_anti_cheat_session`

### 43.3 Steam
- [ ] Auth: `login_steam`, `get_steam_id`, `get_persona_name`
- [ ] Friends: `get_steam_friends`, `get_friend_avatar`, `get_friend_game_info`
- [ ] Achievements: `set_steam_achievement`, `clear_steam_achievement`, `indicate_achievement_progress`, `store_stats`
- [ ] Leaderboards: `find_steam_leaderboard`, `upload_leaderboard_score`, `download_leaderboard_entries`
- [ ] Workshop: `create_workshop_item`, `update_workshop_item`, `subscribe_workshop_item`, `get_subscribed_items`
- [ ] Cloud: `write_steam_cloud_file`, `read_steam_cloud_file`, `delete_steam_cloud_file`
- [ ] Rich Presence: `set_steam_rich_presence`, `clear_steam_rich_presence`
- [ ] Overlay: `activate_steam_overlay`, `activate_overlay_to_store`, `activate_overlay_to_user`
- [ ] Input: `configure_steam_input`, `get_steam_controller_state`
- [ ] Networking: `configure_steam_networking`, `send_steam_p2p_packet`

### 43.4 PlayStation Network (Requires External SDK)

> **Prerequisites**: PSN SDK must be installed and configured via PlayStation Partners portal BEFORE MCP can interact with these APIs. MCP can only configure/use these once the SDK is available in the project.

- [ ] `configure_psn_settings` (assumes SDK already installed)
- [ ] `login_psn`, `get_psn_user_id`
- [ ] `unlock_trophy`, `get_trophy_pack_info`
- [ ] `start_activity`, `end_activity`
- [ ] `create_player_session`, `join_player_session`
- [ ] `configure_psn_voice`

### 43.5 Xbox Live (Requires External SDK)

> **Prerequisites**: GDK must be installed and project registered in Partner Center BEFORE MCP can interact with these APIs.

- [ ] `configure_xbox_settings` (assumes GDK already installed)
- [ ] `login_xbox`, `get_xbox_user_id`, `get_gamertag`
- [ ] `unlock_xbox_achievement`, `get_achievement_status`
- [ ] `set_xbox_presence`, `get_friend_presence`
- [ ] `create_xbox_session`, `join_xbox_session`
- [ ] `write_connected_storage`, `read_connected_storage`

### 43.6 Nintendo Switch (Requires External SDK)

> **Prerequisites**: Nintendo SDK must be installed and project registered in Nintendo Developer Portal BEFORE MCP can interact with these APIs.

- [ ] `configure_switch_settings` (assumes SDK already installed)
- [ ] `get_switch_user`, `open_account_selector`
- [ ] `configure_nso`, `set_play_report`
- [ ] `configure_joycon`, `configure_hd_rumble`

---

## Phase 44: Streaming & Distribution Plugins

### 44.1 Pixel Streaming
- [ ] Server: `enable_pixel_streaming`, `configure_pixel_streaming_settings`
- [ ] Encoder: `set_encoder_type`, `set_target_bitrate`, `set_max_framerate`, `set_resolution`, `configure_quality_settings`
- [ ] Signaling: `configure_signaling_server`, `set_stun_server`, `set_turn_server`
- [ ] Input: `configure_input_handling`, `set_input_type`, `configure_touch_controller`
- [ ] Streaming: `start_streaming`, `stop_streaming`, `force_keyframe`
- [ ] Multi-Stream: `configure_multi_user`, `set_matchmaker_url`, `configure_sfu_connection`

### 44.2 Media Streaming
- [ ] RTSP: `create_rtsp_source`, `configure_rtsp_stream`
- [ ] NDI: `create_ndi_source`, `create_ndi_output`, `configure_ndi_settings`
- [ ] SRT: `create_srt_source`, `create_srt_output`, `configure_srt_settings`
- [ ] Blackmagic: `list_blackmagic_devices`, `create_blackmagic_input`, `create_blackmagic_output`, `configure_blackmagic_settings`
- [ ] AJA: `list_aja_devices`, `create_aja_input`, `create_aja_output`, `configure_aja_settings`

---

## Phase 45: Utility Plugins

### 45.1 Python Scripting
- [x] `execute_python_script`, `execute_python_string`, `execute_python_file` (secured aliases over `execute_python`, with explicit code/file mode validation, project-confined `.py` file checks, and a 1 MB limit; requires Epic's Python Editor Script Plugin)
- [x] `configure_python_paths` (adds validated paths to the live Python interpreter `sys.path`, matching Epic's documented runtime path configuration; restart-persistent Additional Paths remain project-settings controlled)
- [ ] `install_python_package`
- [x] `list_python_packages` (reports up to 500 installed distributions from Unreal's embedded Python interpreter)
- [x] `create_python_editor_utility` (alias for the official `EditorUtilityBlueprintFactory` creation path; covered by the system-control integration suite)
- [x] `register_python_command`, `unregister_python_command` (official `UICommandsScriptingSubsystem` and `ScriptingCommandInfo` APIs)

### 45.2 Editor Scripting Utilities
- [x] `create_editor_utility_widget` (creates and saves an `EditorUtilityWidgetBlueprint` with Epic's `EditorUtilityWidgetBlueprintFactory`; requires Blutility/Python Editor Script Plugin)
- [x] `create_editor_utility_blueprint` (creates and saves an `EditorUtilityBlueprint` with Epic's `EditorUtilityBlueprintFactory`; requires Blutility/Python Editor Script Plugin)
- [x] `run_editor_utility` (validates and invokes an existing utility asset through Epic's `EditorUtilitySubsystem::CanRun`/`TryRun` APIs)
- [x] `inspect_editor_utility` (additional official `EditorUtilitySubsystem` availability and Editor Utility Widget tab readback)
- [ ] `create_asset_action`
- [ ] `create_editor_mode`, `register_editor_mode`, `configure_mode_toolkit`
- [ ] Menus: `add_menu_entry`, `add_toolbar_button`, `create_submenu`, `register_context_menu`
- [ ] Commands: `register_editor_command`, `bind_command_to_action`, `execute_editor_command`

### 45.3 Modeling Tools Editor Mode
- [x] `activate_modeling_tool`, `deactivate_modeling_tool` (official `FEditorModeTools` lifecycle calls for `EM_ModelingToolsEditorMode`; requires the optional Modeling Tools Editor Mode plugin)
- [x] `inspect_modeling_mode` (additional official active-mode readback)
- [ ] PolyEdit: `select_mesh_elements`, `transform_selection`, `extrude_selection`, `inset_selection`, `bevel_selection`, `bridge_edges`, `fill_hole`, `weld_edges`, `split_edges`
- [x] PolyEdit: `triangulate`, `flip_normals` (shared GeometryScript mesh operations)
- [ ] Sculpt: `set_sculpt_brush`, `set_brush_size`, `set_brush_strength`, `set_brush_falloff`, `sculpt_stroke`
- [ ] Deform: `apply_lattice_deform`, `apply_bend_deform`, `apply_twist_deform`
- [ ] UV: `open_uv_editor`, `select_uv_islands` (interactive editor UV-tool selection remains distinct from GeometryScript mesh UV operations)
- [x] UV: `pack_uvs`, `unwrap_uvs` (explicit aliases to the existing GeometryScript `pack_uv_islands` and `unwrap_uv` operations)
- [x] UV: `transform_uvs` (GeometryScript-backed mesh UV translation/scale/rotation equivalent)
- [x] UV: `get_uv_set_bounds` (additional official GeometryScript UV-channel bounds readback)
- [x] Mesh Ops: `simplify_mesh_tool`, `remesh_tool`, `boolean_tool` (GeometryScript-backed equivalents with explicit operation routing)
- [ ] Mesh Ops: `merge_meshes_tool`, `separate_meshes_tool` (editor-mode topology behavior remains plugin/API dependent)

### 45.4 Common UI Plugin
- [ ] `configure_ui_input_config`, `set_input_mode`
- [x] `configure_analog_cursor` (CommonInput gamepad platform cursor CVar; optional CommonUI plugin)
- [x] `create_common_activatable_widget`, `create_common_button_base` (CommonUI widget blueprint factories; optional CommonUI plugin)
- [x] `create_common_tab_list` (CommonUI tab-list widget blueprint factory; optional CommonUI plugin)
- [x] `create_common_action_widget` (adds Epic's `UCommonActionWidget` as a child in an existing widget blueprint; optional CommonUI plugin)
- [x] `configure_navigation_rules` (official UWidget navigation rule setters with validated directions and explicit targets)
- [x] `set_focus_widget` (live editor/PIE widget lookup with official UWidget::SetKeyboardFocus)
- [x] `configure_back_action` (dispatches Epic's UCommonActivatableWidget::HandleBackAction on a live widget)
- [ ] `register_ui_action`, `bind_ui_action_to_input`

### 45.5 Paper2D
- [x] Sprites: `create_sprite` (official `UPaperSpriteFactory`; requires Paper2DEditor and an existing `UTexture2D`)
- [x] Sprites: `configure_sprite_collision` (official `UPaperSprite` collision properties plus `RebuildData`; reflection is used because Epic exposes no public collision-domain setter)
- [x] Sprites: `inspect_sprite_sockets` (additional official `UPaperSprite::QuerySupportedSockets` discovery)
- [x] Sprites: `get_sprite_socket_transform` (additional official `UPaperSprite::FindSocket`/`FPaperSpriteSocket` transform readback)
- [x] Sprites: `remove_sprite_socket` (additional official `UPaperSprite::RemoveSocket` with validated safe persistence)
- [x] Sprites: `validate_sprite_socket_names` (additional official `UPaperSprite::ValidateSocketNames` normalization with safe persistence)
- [x] Sprites: `find_sprite_texture_bounding_box` (additional official `UPaperSprite::FindTextureBoundingBox` alpha-bounds analysis)
- [x] Flipbooks: `inspect_flipbook` (additional official `UPaperFlipbook` playback metadata and keyframe readback)
- [x] Flipbooks: `get_flipbook_socket_transform` (additional official `UPaperFlipbook::FindSocket` frame-specific socket transform readback)
- [x] Flipbooks: `get_flipbook_sprite_at_time` (additional official `UPaperFlipbook::GetSpriteAtTime` time-based sprite resolution)
- [x] Flipbooks: `get_flipbook_sprite_at_frame` (additional official `UPaperFlipbook::GetSpriteAtFrame` direct frame-index lookup)
- [x] Sprites: `configure_sprite_source` (official `UPaperSprite::SetTrim` source-region editing with safe persistence)
- [x] Sprites: `set_sprite_pivot` (official `UPaperSprite::SetPivotMode` with documented modes and safe persistence)
- [x] Sprites: `inspect_sprite` (official source, UV, pivot, and collision metadata readback)
- [x] Flipbooks: `create_flipbook` (official `UPaperFlipbookFactory` keyframes from existing sprites; requires Paper2DEditor)
- [x] Flipbooks: `add_flipbook_keyframe`, `set_flipbook_framerate` (official `FScopedFlipbookMutator` edits with cache invalidation and safe persistence)
- [x] Flipbooks: `configure_flipbook_loop` (explicit alias to the validated `UPaperFlipbookComponent::SetLooping` handler)
- [x] Tile Maps: `create_tile_map` (official `UPaperTileMapFactory`; optional existing `UPaperTileSet` seed and safe persistence)
- [x] Tile Maps: `create_tile_set` (official `UPaperTileSetFactory`; optional initial texture and safe persistence)
- [x] Tile Maps: `inspect_tile_set` (additional official `UPaperTileSet` metadata readback)
- [x] Tile Maps: `configure_tile_set` (official `UPaperTileSet` texture, size, spacing, margin, drawing-offset, and background-color setters)
- [x] Tile Maps: `get_tile_set_tile_uv` (additional official `UPaperTileSet::GetTileUV` readback)
- [x] Tile Maps: `configure_tile_set_tile_metadata` (additional official `UPaperTileSet::GetMutableTileMetadata` authoring for public `UserDataName` tags)
- [x] Tile Maps: `get_tile_set_tile_xy` (additional official `UPaperTileSet::GetTileXYFromTextureUV` coordinate conversion)
- [x] Tile Maps: `inspect_tile_set_tile_metadata` (additional official per-tile metadata, terrain-membership, and collision readback)
- [x] Tile Maps: `get_tile_set_uv_from_xy` (additional official `UPaperTileSet::GetTileUVFromTileXY` coordinate conversion)
- [x] Tile Maps: `inspect_tile_map` (additional official `UPaperTileMap` dimensions, layers, scale, collision, and grid-color readback)
- [x] Tile Maps: `configure_tile_map_visuals` (additional official `UPaperTileMap` visual fields for colors, grid layout, scale, and layer/tile separation)
- [x] Tile Maps: `configure_tile_map_layer` (additional official `UPaperTileLayer` color, collision override, and editor-visibility setters)
- [x] Tile Maps: `inspect_tile_map_layer` (additional official `UPaperTileLayer` dimensions, occupancy, collision, visibility, and color readback)
- [ ] Tile Maps: `add_tile_to_set` (Epic models tiles as texture-derived cells; no public append API is available)
- [x] Tile Maps: `fill_tile_region` via `set_tile_map_cell` and `fill_tile_map_region` (official `UPaperTileLayer::SetCell`/`FPaperTileInfo` asset editing, including packed transform flags)
- [x] Tile Maps: `get_tile_map_cell` (additional official `UPaperTileLayer::GetCell`/`FPaperTileInfo` asset readback, including `GetTileTransform` flag decoding)
- [x] Tile Maps: `get_tile_map_tile_geometry` (additional official `UPaperTileMap` local-space position, center, and polygon readback)
- [x] Tile Maps: `get_tile_map_tile_coordinates` (additional official `UPaperTileMap` local-space to tile-grid coordinate conversion)
- [x] Tile Maps: `paint_paper_tile` (official `UPaperTileMapComponent::SetTile` and `FPaperTileInfo`; owned tile-map component and existing tile set required)
- [x] Tile Maps: `fill_paper_tile_region` (bounded batch of official `SetTile` calls with optional single collision rebuild)
- [x] Tile Maps: `configure_paper_tile_map_layer_collision` (official `UPaperTileMapComponent::SetLayerCollision`; requires an owned/editable tile map component and the optional Paper2D plugin)
- [x] Tile Maps: `configure_tile_map_collision` (official `UPaperTileMap::SetCollisionDomain`, `SetCollisionThickness`, and `RebuildCollision`; requires the optional Paper2D editor plugin)
- [x] Tile Maps: `resize_tile_map` (official `UPaperTileMap::ResizeMap`; bounded dimensions, optional force resize, collision rebuild, and safe persistence)
- [x] Actors: `spawn_paper_sprite_actor`, `spawn_paper_flipbook_actor` (official Paper2D actor/component APIs; requires the optional Paper2D plugin and a pre-existing sprite/flipbook asset)
- [x] Actors: `spawn_paper_tile_map_actor` (official `APaperTileMapActor::GetRenderComponent` and `UPaperTileMapComponent::SetTileMap`; requires the optional Paper2D plugin and a pre-existing tile-map asset)
- [x] Actors: `set_paper_tile_map_color` (official `UPaperTileMapComponent::SetTileMapColor`; requires the optional Paper2D plugin)
- [x] Actors: `set_paper_tile_map_layer_color` (official `UPaperTileMapComponent::SetLayerColor`; requires the optional Paper2D plugin)
- [x] Actors: `get_paper_tile_map_layer_color` (official `UPaperTileMapComponent::GetLayerColor`; bounded layer readback)
- [x] Actors: `get_paper_tile_map_info` (official `UPaperTileMapComponent::GetMapSize` and `GetRenderingStats`; requires the optional Paper2D plugin)
- [x] Actors: `get_paper_tile_map_parameters` (official `UPaperTileMap::GetTileToLocalParameters` and `GetLocalToTileParameters`; bidirectional tile-grid conversion basis readback)
- [x] Actors: `get_paper_tile` (official `UPaperTileMapComponent::GetTile`, `GetTileCenterPosition`, and `GetTileCornerPosition`; bounded cell readback with local/world-space positions)
- [x] Actors: `get_paper_tile_polygon` (official `UPaperTileMap::GetTilePolygon`; bounded cell polygon readback)
- [x] Actors: `resize_paper_tile_map` (official `UPaperTileMapComponent::ResizeMap`; bounded resize of owned tile maps with resulting dimensions read back)
- [x] Actors: `set_paper_tile_map_default_collision` (official `UPaperTileMapComponent::SetDefaultCollisionThickness`; bounded thickness with explicit rebuild control)
- [x] `set_paper_sprite_color`, `configure_paper_flipbook` (newly audited official Paper2D component color/playback APIs; requires the optional Paper2D plugin)
- [x] `configure_paper_character` (official `APaperCharacter`, `UPaperFlipbookComponent`, and `UCharacterMovementComponent` configuration; requires the optional Paper2D plugin)

### 45.6 Procedural Mesh Component
- [x] `create_procedural_mesh_component` (safe component creation; requires the optional Procedural Mesh Component plugin)
- [x] `create_mesh_section`, `update_mesh_section`, `clear_mesh_section`, `clear_all_mesh_sections` (official `UProceduralMeshComponent` section APIs; requires the optional Procedural Mesh Component plugin)
- [x] `set_mesh_section_visibility`, `get_mesh_section_visibility` (newly audited official section visibility APIs; requires the optional Procedural Mesh Component plugin)
- [x] `set_mesh_section_material` (newly audited `UMeshComponent::SetMaterial` section assignment; requires the optional Procedural Mesh Component plugin)
- [x] `get_mesh_section_data` (official `UProceduralMeshComponent::GetProcMeshSection` readback of vertices, topology, normals, UV0, colors, tangents, visibility, and collision flags)
- [x] `set_mesh_vertices` (official `UProceduralMeshComponent::UpdateMeshSection`; exact-count position updates preserve existing attributes)
- [x] `set_mesh_normals`, `set_mesh_uvs` (official `UProceduralMeshComponent::UpdateMeshSection`; exact-count attribute updates preserve other attributes)
- [x] `set_mesh_colors`, `set_mesh_tangents` (official `UProceduralMeshComponent::UpdateMeshSection`; exact-count color/tangent updates preserve other attributes)
- [x] `set_mesh_triangles` (official `UProceduralMeshComponent::CreateMeshSection`; bounded index validation preserves vertex attributes, collision, and visibility)
- [x] `get_mesh_section_bounds` (additional official `FProcMeshSection` vertex-bounds readback)
- [x] `set_collision_from_mesh`, `add_collision_convex_mesh`, `clear_collision_convex_meshes` (official `UProceduralMeshComponent` collision configuration and convex APIs; requires the optional Procedural Mesh Component plugin)
- [x] `convert_to_static_mesh` (shared GeometryScript DynamicMesh-to-StaticMesh conversion; see Phase 6.8)

### 45.7 Variant Manager
- [x] `create_variant_set` (official `VariantManagerLibrary.create_level_variant_sets_asset`, `VariantSet`, and `LevelVariantSets.add_variant_set`; requires VariantManagerContent and Python Editor Script Plugin)
- [x] `add_variant` (official `Variant`, `VariantSet.add_variant`, and `VariantSet.get_variant_by_name`; requires an existing LevelVariantSets asset)
- [x] `configure_variant_properties` (official `Variant.capture_property` and `VariantManagerLibrary.set_value_bool/int/float/string`; scalar values require an existing actor binding target and property path)
- [x] `set_variant_dependencies` (official `VariantDependency`, `Variant.is_valid_dependency`, and `VariantManagerLibrary.add_dependency`; cycle-safe named variant relationships)
- [ ] `set_exclusive_variants`
- [x] `capture_variant_thumbnail` (official `Variant.set_thumbnail_from_editor_viewport`)
- [x] `set_variant_thumbnail` (official `Variant.set_thumbnail_from_file` or `set_thumbnail_from_editor_viewport`; file inputs are project-confined)
- [x] `activate_variant` (official `Variant.switch_on()` activation by named Level Variant Set and Variant)
- [x] `get_active_variants` (official `Variant.is_active()` status readback across all sets and variants)
- [x] `export_variant_configuration` (official Variant Manager readback exported as project-confined JSON metadata, including sets, variants, active state, actor bindings, and captured-property display/type information)

---

## Phase 46: Physics & Destruction Plugins

### 46.1 Chaos Destruction
- [x] Collection: `create_geometry_collection` (official `GeometryCollectionFactory` creates a new Geometry Collection asset; source geometry composition remains dependent on the Fracture Editor/Dataflow workflow)
- [x] Collection: `configure_geometry_collection` (constrained official asset-property configuration for mass, clustering, Nanite, ray tracing, removal, and damage thresholds)
- [x] Collection: `inspect_geometry_collection` (readback of official Geometry Collection asset properties)
- [x] Collection: `add_geometry_to_collection`, `remove_geometry_from_collection` (official `GeometryCollection.geometry_source` / `GeometryCollectionSource` composition path)
- [ ] Fracturing: `apply_uniform_fracture`, `apply_clustered_fracture`, `apply_radial_fracture`, `apply_planar_fracture`, `apply_brick_fracture`, `apply_mesh_fracture`, `configure_fracture_settings`
- [ ] Clustering: `set_cluster_group`, `configure_cluster_level`, `auto_cluster`
- [ ] Damage: `configure_damage_threshold`, `enable_strain_damage`
- [x] Damage: `apply_damage` (shared combat authoring handler)
- [ ] Materials: `set_interior_material`, `configure_interior_uv_scale`
- [ ] Physics: `configure_rigid_body_settings`, `set_sleeping_threshold`, `configure_collision_filter`
- [x] Physics: `configure_geometry_collection_component` (documented component-level simulation, collision-damage, clustering, drag, and collision-profile settings)
- [ ] Fields: `add_anchor_field`, `remove_anchor_field`, `create_field_system`, `add_radial_falloff_field`, `add_uniform_vector_field`, `add_radial_vector_field`, `add_plane_falloff_field`, `add_noise_field`, `add_kill_field`

**Implementation note:** Epic exposes the Geometry Collection asset factory through Python, but the current Fracture Editor fracture tools are modal editor tools rather than a stable public factory API. This bridge therefore claims asset creation only until a version-stable Geometry Collection composition/fracture API is available.

**Implementation note:** Epic exposes Variant Manager asset/set creation through the official experimental Python API. Variant property capture and dependency authoring remain separate actions because they require actor bindings and property-path/value semantics.

### 46.2 Chaos Vehicles
- [ ] Setup: `create_chaos_wheeled_vehicle`, `create_chaos_hover_vehicle`
- [ ] Wheels: `add_wheel_setup`, `configure_wheel_physics`, `set_wheel_radius`, `set_wheel_width`, `configure_wheel_suspension`, `configure_wheel_friction`, `configure_wheel_brake`, `configure_anti_roll_bar`
- [ ] Engine: `configure_engine_torque_curve`, `set_max_rpm`, `set_engine_idle_rpm`, `configure_throttle_response`
- [ ] Transmission: `configure_transmission`, `set_gear_ratios`, `set_final_drive_ratio`, `configure_gear_change_time`
- [ ] Steering: `configure_steering_curve`, `set_max_steering_angle`, `configure_ackermann_steering`
- [ ] Aero: `configure_drag`, `configure_downforce`
- [ ] Effects: `configure_skid_marks`, `configure_tire_smoke`, `configure_engine_audio`, `configure_exhaust_vfx`

### 46.3 Chaos Cloth
- [ ] `create_cloth_asset`, `configure_cloth_config`
- [ ] Properties: `set_mass`, `set_edge_stiffness`, `set_bending_stiffness`, `set_area_stiffness`, `configure_damping`, `configure_collision_thickness`, `configure_friction`
- [ ] Painting: `paint_max_distance`, `paint_backstop_distance`, `paint_backstop_radius`
- [ ] LOD: `configure_cloth_lod`, `set_lod_transition`

### 46.4 Chaos Flesh
- [ ] `create_flesh_asset`, `configure_flesh_deformer`
- [ ] `configure_flesh_simulation`, `set_flesh_stiffness`, `set_flesh_damping`
- [ ] `configure_flesh_collision`

---

## Phase 47: Accessibility System

**Goal**: Complete accessibility features for inclusive game design.

**Tool**: `manage_accessibility`

### 47.1 Visual Accessibility
- [ ] `create_colorblind_filter` (protanopia, deuteranopia, tritanopia)
- [ ] `configure_colorblind_simulation`
- [x] `set_ui_scale` (official `UUserInterfaceSettings::ApplicationScale` with live `FSlateApplication::SetApplicationScale`)
- [ ] `set_font_scale` (UE exposes font DPI settings, but not a stable global font-scale setter across supported versions)
- [ ] `configure_high_contrast_mode`
- [x] `set_screen_reader_text` (live UMG widget object path; official `UWidget` accessibility properties)
- [ ] `configure_text_to_speech`
- [ ] `set_color_coding_alternatives` (shapes, patterns)

### 47.2 Subtitle System
- [ ] `create_subtitle_widget`
- [ ] `configure_subtitle_style` (font, size, color, background)
- [ ] `set_subtitle_position`
- [ ] `configure_speaker_identification`
- [ ] `configure_caption_timing`
- [ ] `add_directional_indicators` (sound direction)
- [ ] `configure_subtitle_presets` (small, medium, large, custom)

### 47.3 Audio Accessibility
- [ ] `configure_mono_audio`
- [ ] `configure_audio_visualization` (visual sound indicators)
- [ ] `create_sound_indicator_widget`
- [ ] `configure_haptic_audio_feedback`
- [ ] `set_audio_balance` (left/right)

### 47.4 Motor Accessibility
- [ ] `configure_control_remapping_ui`
- [ ] `configure_hold_vs_toggle`
- [ ] `configure_input_timing` (QTE timing, combo windows)
- [ ] `configure_auto_aim_strength`
- [ ] `configure_one_handed_mode`
- [ ] `configure_sticky_keys`
- [ ] `configure_input_buffering`

### 47.5 Cognitive Accessibility
- [ ] `configure_difficulty_presets`
- [ ] `configure_objective_reminders`
- [ ] `configure_navigation_assistance`
- [ ] `configure_puzzle_hints`
- [ ] `configure_content_warnings`
- [ ] `configure_motion_sickness_options` (FOV, camera shake, motion blur)

### 47.6 Accessibility Presets
- [ ] `create_accessibility_preset`
- [ ] `apply_accessibility_preset`
- [ ] `export_accessibility_settings`
- [ ] `import_accessibility_settings`
- [ ] `configure_accessibility_menu`

**API-derived addition:** `system_control.announce_accessible_string` uses Unreal's official `UGameplayStatics::AnnounceAccessibleString` API to send a platform accessibility announcement.

**API-derived addition:** `system_control.get_ui_scale` reads the official `UUserInterfaceSettings::ApplicationScale` value and reports whether the editor Slate application is initialized.

**API-derived addition:** `system_control.configure_screen_reader_support` applies the official `Accessibility.Enable` console variable documented by Epic for third-party screen-reader support, and reports `NOT_SUPPORTED` when the target build omits that CVar.

**API-derived addition:** `system_control.set_screen_reader_text` configures a live `UWidget` through Epic's public `AccessibleText`, `AccessibleSummaryText`, `AccessibleBehavior`, and `bOverrideAccessibleDefaults` properties. The caller must provide the widget's full UObject path; the bridge does not guess among similarly named widgets.

---

## Phase 48: Modding & UGC System

**Goal**: Enable mod support and user-generated content within Unreal Engine.

**Tool**: `manage_modding`

### 48.1 Pak/Mod Loading
- [ ] `configure_mod_loading_paths`
- [ ] `scan_for_mod_paks`
- [ ] `load_mod_pak`, `unload_mod_pak`
- [ ] `get_mod_info` (metadata, version, dependencies)
- [ ] `validate_mod_pak`
- [ ] `configure_mod_load_order`
- [ ] `configure_mod_priority`

### 48.2 Mod Discovery
- [ ] `create_mod_browser_widget`
- [ ] `list_installed_mods`
- [ ] `enable_mod`, `disable_mod`
- [ ] `get_mod_thumbnail`
- [ ] `get_mod_dependencies`
- [ ] `check_mod_compatibility`

### 48.3 Asset Replacement
- [ ] `configure_asset_override_paths`
- [ ] `register_mod_asset_redirect`
- [ ] `get_modded_asset_list`
- [ ] `restore_original_asset`

### 48.4 Mod SDK Generation
- [ ] `export_moddable_headers`
- [ ] `create_mod_template_project`
- [ ] `configure_exposed_classes`
- [ ] `configure_moddable_data_assets`
- [ ] `generate_mod_documentation`

### 48.5 Sandboxing & Security
- [ ] `configure_mod_sandbox`
- [ ] `set_allowed_mod_operations`
- [ ] `configure_mod_memory_limits`
- [ ] `log_mod_activity`
- [ ] `validate_mod_content` (check for exploits)

### 48.6 Steam Workshop Integration (via Steam OSS)
- [ ] `upload_to_workshop` (requires Steam SDK)
- [ ] `download_workshop_item`
- [ ] `get_subscribed_workshop_items`
- [ ] `configure_workshop_item_metadata`
- [ ] `update_workshop_item`

> **Note**: Steam Workshop integration requires Steam SDK to be configured (Phase 43.3). MCP handles the UE-side integration.

---

# Summary

## Statistics

| Category | Phases | Estimated Actions |
|----------|--------|-------------------|
| Completed Foundation (1-4) | 4 | ~160 |
| Infrastructure (5) | 1 | ~20 |
| Content Creation (6-12) | 7 | ~400 |
| Gameplay Systems (13-18) | 6 | ~300 |
| UI/UX (19) | 1 | ~80 |
| Networking & Framework (20-22) | 3 | ~100 |
| World & Environment (23-28) | 6 | ~200 |
| Rendering & Post (29) | 1 | ~80 |
| Cinematics & Media (30) | 1 | ~80 |
| Data & Persistence (31) | 1 | ~50 |
| Build & Deploy (32) | 1 | ~35 |
| Testing & Quality (33) | 1 | ~35 |
| Editor Utilities (34) | 1 | ~60 |
| Additional Systems (35) | 1 | ~80 |
| Plugin: Character (36) | 1 | ~60 |
| Plugin: Asset/Content (37) | 1 | ~150 |
| Plugin: Audio Middleware (38) | 1 | ~80 |
| Plugin: Motion Capture (39) | 1 | ~70 |
| Plugin: Virtual Production (40) | 1 | ~150 |
| Plugin: XR (41) | 1 | ~150 |
| Plugin: AI/NPC (42) | 1 | ~30 |
| Plugin: Online Services (43) | 1 | ~160 |
| Plugin: Streaming (44) | 1 | ~50 |
| Plugin: Utilities (45) | 1 | ~100 |
| Plugin: Physics/Destruction (46) | 1 | ~80 |
| Accessibility (47) | 1 | ~40 |
| Modding & UGC (48) | 1 | ~25 |
| **TOTAL** | **48** | **~2,855** |

## What This Enables

With ~2,855 actions covering all Unreal Engine systems and major plugins:

- **Complete Game Development**: Build any genre (FPS, RPG, Racing, Platformer, etc.)
- **Animated Films**: Full pipeline from character to final render
- **Virtual Production**: LED walls, motion capture, real-time compositing
- **VR/AR Experiences**: All major headsets and AR platforms
- **ArchViz**: CAD import, materials, lighting, walkthroughs
- **Live Events**: DMX, OSC, MIDI, timecode sync
- **Cloud Gaming**: Pixel streaming, multi-user sessions
- **Console Development**: PS5, Xbox, Switch support (once SDKs installed externally)
- **Mobile Development**: iOS ARKit, Android ARCore
- **Accessible Games**: Full accessibility support (colorblind, subtitles, motor, cognitive)
- **Mod-Friendly Games**: Pak loading, mod browser, asset override, workshop integration

---

## Legend

- [x] **Completed**: Feature is implemented and verified.
- [ ] **Planned**: Feature is scheduled for implementation.

---

## Contributing

This roadmap represents a massive undertaking. Contributions are welcome for any phase. See [CONTRIBUTING.md](../CONTRIBUTING.md) for guidelines.

Each new action requires:
1. TypeScript handler in `src/tools/handlers/`
2. Tool definition in `src/tools/consolidated-tool-definitions.ts`
3. C++ handler in `plugins/NebulaForgeBridge/Source/.../Private/`
4. Integration test
