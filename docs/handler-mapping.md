# Handler Mappings

This document maps the TypeScript tool definitions to their corresponding C++ handlers in the Unreal Engine plugin.

> **Note:** The TypeScript bridge exposes 23 canonical MCP tools. Former child tool names are not exposed or accepted as direct MCP tool names; their actions live on the canonical parent tools shown below.

## Asset Manager (`manage_asset`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `list` | `NebulaForgeBridge_AssetQueryHandlers.cpp` | `HandleAssetQueryAction` | |
| `search_assets` | `NebulaForgeBridge_AssetQueryHandlers.cpp` | `HandleAssetQueryAction` | |
| `import` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `duplicate` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `rename` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `move` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `delete` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `delete_assets` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleBulkDeleteAssets` | |
| `create_folder` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `get_asset` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleGetAsset` | |
| `get_dependencies` | `NebulaForgeBridge_AssetQueryHandlers.cpp` | `HandleGetAssetDependencies` | |
| `get_source_control_state` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleSourceControlCheckout` | |
| `analyze_graph` | `NebulaForgeBridge_AssetQueryHandlers.cpp` | `HandleGetAssetReferences` | |
| `create_thumbnail` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleGenerateThumbnail` | |
| `set_tags` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `validate` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `fixup_redirectors` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleFixupRedirectors` | |
| `generate_report` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `set_view_settings` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleContentBrowserAction` | Uses official Content Browser instance settings; unsupported view-type fields are reported explicitly |
| `navigate_to_path` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleContentBrowserAction` | Selects a Content Browser folder |
| `sync_to_asset` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleContentBrowserAction` | Syncs one or more assets through the editor browser |
| `sync_to_folder` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleContentBrowserAction` | Syncs a folder with lock/focus/new-browser options |
| `create_collection` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleContentBrowserAction` | Supports local/private/shared and static/dynamic collections |
| `add_to_collection` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleContentBrowserAction` | Adds resolved asset soft paths to a collection |
| `set_asset_color` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleContentBrowserAction` | Persists the documented Content Browser folder-path color |
| `show_in_explorer` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleContentBrowserAction` | Reveals the asset package directory on disk |
| `set_search_text` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleContentBrowserAction` | Sets the Content Browser search text |
| `create_material` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `create_material_instance` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandleAssetAction` | |
| `create_physical_material` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandlePhysicalMaterialAction` | Creates and configures a `UPhysicalMaterial` asset |
| `set_friction` / `set_restitution` / `set_density` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandlePhysicalMaterialAction` | Updates validated physical-material surface/object properties |
| `configure_physical_material` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandlePhysicalMaterialAction` | Batch-updates material properties and combine modes |
| `get_physical_material` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandlePhysicalMaterialAction` | Returns structured physical-material properties |
| `configure_surface_type` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandlePhysicalMaterialAction` | Adds/replaces a project physical surface and optionally flushes config |
| `assign_physical_material` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandlePhysicalMaterialAction` | Applies an override to all or one primitive component |
| `clear_physical_material_override` | `NebulaForgeBridge_AssetWorkflowHandlers.cpp` | `HandlePhysicalMaterialAction` | Clears the component physical-material override |
| `create_render_target` | `NebulaForgeBridge_RenderHandlers.cpp` | `HandleRenderAction` | |
| `nanite_rebuild_mesh` | `NebulaForgeBridge_RenderHandlers.cpp` | `HandleRenderAction` | |
| `add_material_node` | `NebulaForgeBridge_MaterialGraphHandlers.cpp` | `HandleAddMaterialExpression` | |
| `connect_material_pins` | `NebulaForgeBridge_MaterialGraphHandlers.cpp` | `HandleCreateMaterialNodes` | |
| `remove_material_node` | `NebulaForgeBridge_MaterialGraphHandlers.cpp` | `HandleCreateMaterialNodes` | |
| `add_bt_node` | `NebulaForgeBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | |
| `connect_bt_nodes` | `NebulaForgeBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | |

## Blueprint Manager (`manage_blueprint`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create` | `NebulaForgeBridge_BlueprintCreationHandlers.cpp` | `HandleBlueprintAction` | |
| `get_blueprint` | `NebulaForgeBridge_BlueprintHandlers.cpp` | `HandleBlueprintAction` | |
| `compile` | `NebulaForgeBridge_BlueprintHandlers.cpp` | `HandleBlueprintAction` | |
| `add_component` | `NebulaForgeBridge_SCSHandlers.cpp` | `HandleBlueprintAction` | Uses `SubobjectData` in UE 5.7+ |
| `set_default` | `NebulaForgeBridge_BlueprintHandlers.cpp` | `HandleBlueprintAction` | |
| `modify_scs` | `NebulaForgeBridge_SCSHandlers.cpp` | `HandleBlueprintAction` | |
| `get_scs` | `NebulaForgeBridge_SCSHandlers.cpp` | `HandleBlueprintAction` | |
| `create_node` | `NebulaForgeBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | |
| `delete_node` | `NebulaForgeBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | |
| `connect_pins` | `NebulaForgeBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | |
| `break_pin_links` | `NebulaForgeBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | |
| `set_node_property` | `NebulaForgeBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | |

## Input Manager (`manage_networking`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_input_action` | `NebulaForgeBridge_InputHandlers.cpp` | `HandleInputAction` | |
| `create_input_mapping_context` | `NebulaForgeBridge_InputHandlers.cpp` | `HandleInputAction` | |
| `add_mapping` | `NebulaForgeBridge_InputHandlers.cpp` | `HandleInputAction` | |
| `remove_mapping` | `NebulaForgeBridge_InputHandlers.cpp` | `HandleInputAction` | |
| `add_variable` | `NebulaForgeBridge_BlueprintHandlers.cpp` | `HandleBlueprintAction` | |
| `add_function` | `NebulaForgeBridge_BlueprintHandlers.cpp` | `HandleBlueprintAction` | |
| `add_event` | `NebulaForgeBridge_BlueprintHandlers.cpp` | `HandleBlueprintAction` | Supports custom & standard events |

## Actor Control (`control_actor`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `spawn` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `spawn_blueprint` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `delete` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `delete_by_tag` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `duplicate` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `apply_force` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `set_transform` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `get_transform` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `set_visibility` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `add_component` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorAction` | Runtime component addition |
| `add_tag` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `find_by_tag` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `select_actor` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorSelection` | Selects exact actor labels, names, or paths |
| `select_actors_by_class` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorSelection` | Optional derived-class matching |
| `select_actors_by_tag` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorSelection` | Selects actors carrying a gameplay tag |
| `select_actors_in_volume` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorSelection` | Uses actor bounds against an `AVolume` |
| `deselect_all`, `get_selected_actors` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorSelection` | Clear or inspect editor actor selection |
| `group_actors`, `ungroup_actors` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorSelection` | Uses `UActorGroupingUtils` |
| `select_all`, `invert_selection`, `select_children` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorSelection` | Official editor subsystem selection utilities |
| `remove_selected_from_group`, `lock_selected_groups`, `unlock_selected_groups` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorSelection` | Group maintenance operations |
| `create_collision_channel`, `create_collision_profile` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorCollision` | CollisionProfile config entries with optional DefaultEngine.ini persistence |
| `configure_channel_responses`, `configure_object_type`, `configure_trace_channel` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorCollision` | Official UPrimitiveComponent collision channel/object response APIs |
| `set_actor_collision_profile`, `set_component_collision_profile` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorCollision` | Apply a validated named profile to primitive components |
| `get_actor_collision`, `get_component_collision`, `validate_collision_profile` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorCollision` | Collision state readback and UCollisionProfile validation |
| `list` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `attach` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `detach` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |

## Editor Control (`control_editor`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `play` | `NebulaForgeBridge_EditorFunctionHandlers.cpp` | `HandleControlEditorAction` | |
| `stop` | `NebulaForgeBridge_EditorFunctionHandlers.cpp` | `HandleControlEditorAction` | |
| `set_camera` | `NebulaForgeBridge_EditorFunctionHandlers.cpp` | `HandleControlEditorAction` | |
| `console_command` | `NebulaForgeBridge_EditorFunctionHandlers.cpp` | `HandleConsoleCommandAction` | |
| `screenshot` | `NebulaForgeBridge_EditorFunctionHandlers.cpp` | `HandleControlEditorAction` | |
| `simulate_input` | `NebulaForgeBridge_UiHandlers.cpp` | `HandleUiAction` | |
| `create_bookmark` | `NebulaForgeBridge_EditorFunctionHandlers.cpp` | `HandleControlEditorAction` | |
| `set_editor_mode` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlEditorSetEditorMode` | Selects a registered editor mode through the UE editor command path |
| `configure_editor_preferences` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlEditorSetPreferences` | Applies supported console variables and editor preferences |
| `set_grid_settings` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlEditorSetGridSettings` | Updates `ULevelEditorViewportSettings` translation grid settings |
| `set_snap_settings` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlEditorSetSnapSettings` | Updates translation, rotation, scale, actor, and surface snapping |
| `manage_editor_layouts` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlEditorManageLayouts` | Save/load/remove/reset/import/export named layouts through editor commands |
| `create_custom_editor_mode` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlEditorCreateCustomMode` | Validates a mode descriptor; runtime `UEdMode` registration requires a compiled editor module |

## Level Manager (`manage_level`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `load` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Alias for `load_level` |
| `load_level` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Opens a map package |
| `save` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Alias for `save_level` |
| `save_level` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Saves the current level |
| `save_as` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Alias for `save_level_as` |
| `save_level_as` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Saves current level to a new path |
| `stream` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Loads/updates a streaming level |
| `unload` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Alias for `unload_level` |
| `unload_level` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Forces streaming unload |
| `create_level` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Creates and opens a new level |
| `create_light` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Spawns a level light |
| `build_lighting` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Starts editor lighting build |
| `set_metadata` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Delegates to asset metadata handling |
| `export_level` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | |
| `import_level` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | |
| `list_levels` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Lists current world levels and map assets |
| `get_summary` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Maps to level info summary |
| `delete` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Alias for `delete_level` |
| `delete_level` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Deletes map file, built data, and external sidecars |
| `validate_level` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Checks package/file existence |
| `add_sublevel` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Adds a streaming sublevel |
| `rename_level` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Copies destination then deletes source and sidecars |
| `duplicate_level` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Copies map, built data, and external sidecars |
| `get_current_level` | `NebulaForgeBridge_LevelHandlers.cpp` | `HandleLevelAction` | Returns current editor level identity |

## Lighting Manager (`build_environment`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `spawn_light` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `spawn_sky_light` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `build_lighting` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `ensure_single_sky_light` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `create_lightmass_volume` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `setup_volumetric_fog` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `create_lighting_enabled_level` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `setup_global_illumination` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `configure_shadows` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `set_exposure` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `set_ambient_occlusion` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | |
| `list_light_types` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Discovery: Returns all `ALight` subclasses |
| `configure_ray_traced_shadows` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.1; applies supported ray-traced shadow CVars |
| `configure_ray_traced_gi` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.1; applies supported RTGI CVars |
| `configure_ray_traced_reflections` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.1; applies supported ray-traced reflection CVars |
| `configure_ray_traced_ao` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.1; applies supported ray-traced AO CVars |
| `configure_path_tracing` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.1; applies supported path-tracing CVars |
| `configure_ray_traced_translucency` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.1; configures translucency/refraction and Post Process Volume overrides |
| `configure_ray_tracing_quality` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.1; configures scene culling, geometry inclusion, residency, and update budgets |
| `set_light_channel` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.2; configures a light's three lighting channels |
| `set_actor_light_channel` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.2; applies channels to primitive components, with optional component targeting |
| `get_light_channels` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.2; reads back light or actor component channel state for verification |
| `configure_lightmass_settings` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.3; updates world-level CPU Lightmass settings and returns a complete readback |
| `build_lighting_quality` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.3; starts a Preview/Medium/High/Production lighting build |
| `configure_indirect_lighting_cache` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.3; configures cache enablement, update cadence, and allocation CVars |
| `configure_volumetric_lightmaps` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.3; configures volumetric lightmap and sparse-volume settings |
| `configure_lightmass_ambient_occlusion` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.3; configures baked AO and optional precomputed AO material masks |
| `inspect_lightmass_settings` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.3; returns current Lightmass and volumetric-lightmap settings |
| `create_sphere_reflection_capture` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.4; creates and configures a sphere reflection capture |
| `create_box_reflection_capture` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.4; creates and configures a box reflection capture |
| `configure_capture_resolution` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.4; sets the global reflection capture cubemap resolution |
| `configure_capture_offset` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.4; updates a capture's world-space offset |
| `recapture_scene` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.4; queues targeted or all static/runtime captures for refresh |
| `create_planar_reflection` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.4; creates a realtime planar reflection actor |
| `configure_planar_reflection` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.4; configures planar fade, distortion, filtering, and performance settings |
| `configure_ssr_settings` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.4; applies supported SSR CVars |
| `configure_lumen_reflection_settings` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.4; applies supported Lumen reflection CVars |
| `inspect_reflection_captures` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.4; lists capture and planar reflection actors with readback |
| `create_post_process_volume` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; creates a configurable Post Process Volume |
| `configure_pp_blend` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; configures volume extent, priority, blend radius, weight, and unbound state |
| `set_pp_white_balance` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; sets temperature and tint |
| `set_pp_color_grading` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; sets saturation, contrast, gamma, gain, and offset vectors |
| `set_pp_lut` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; assigns a safe `/Game` or `/Engine` LUT texture |
| `configure_tonemapper` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; configures tone curve, gamut, and film clips |
| `set_tonemapper_type` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; applies the renderer tonemapper CVar when available |
| `configure_bloom` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; configures bloom method, intensity, threshold, and size |
| `set_bloom_intensity` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; updates bloom intensity |
| `set_bloom_threshold` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; updates bloom threshold |
| `configure_lens_flare` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; configures lens flare intensity, bokeh size, and threshold |
| `configure_dof` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; configures depth of field focus, aperture, blur, and bokeh |
| `set_dof_method` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; selects the available DOF method enum |
| `set_focal_distance` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; updates DOF focal distance |
| `set_aperture` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; updates DOF F-stop |
| `configure_bokeh` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; updates DOF blade/minimum aperture settings |
| `configure_motion_blur` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; configures motion blur amount, maximum, target FPS, and object size |
| `set_motion_blur_amount` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; updates motion blur amount |
| `set_motion_blur_max` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; updates motion blur maximum |
| `configure_exposure` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; configures metering, compensation, range, and adaptation speeds |
| `set_exposure_method` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; selects histogram, basic, or manual exposure |
| `set_exposure_compensation` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; updates exposure compensation |
| `set_exposure_min_max` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; updates exposure adaptation limits |
| `configure_ssao` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; configures ambient occlusion volume settings |
| `configure_gtao` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; configures AO settings and optional GTAO renderer CVar |
| `configure_vignette` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; configures vignette intensity |
| `configure_chromatic_aberration` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; configures scene fringe/chromatic aberration |
| `configure_grain` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; configures film grain intensity |
| `configure_screen_percentage` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; applies renderer screen percentage CVar |
| `inspect_post_process_volume` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.5; lists volumes and representative post-process settings |
| `create_scene_capture_2d` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.6; creates a 2D scene capture actor |
| `create_scene_capture_cube` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.6; creates a six-face cube scene capture actor |
| `create_render_target_cube` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.6; creates a persistent cube render-target asset |
| `configure_scene_capture` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.6; configures cadence, persistence, projection, FOV, priority, source, and post-process blend |
| `configure_scene_capture_resolution` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.6; resizes 2D targets or reinitializes cube targets |
| `configure_capture_source` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.6; resolves and applies `ESceneCaptureSource` |
| `assign_render_target` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.6; assigns a typed 2D/cube render target |
| `capture_scene` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.6; captures immediately or deferred |
| `inspect_scene_captures` | `NebulaForgeBridge_LightingHandlers.cpp` | `HandleLightingAction` | Phase 29.6; lists scene captures and key settings |

## Performance Manager (`system_control`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `generate_memory_report` | `NebulaForgeBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `configure_texture_streaming` | `NebulaForgeBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `merge_actors` | `NebulaForgeBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `start_profiling` | `NebulaForgeBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `stop_profiling` | `NebulaForgeBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `show_fps` | `NebulaForgeBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `show_stats` | `NebulaForgeBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `set_scalability` | `NebulaForgeBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `set_resolution_scale` | `NebulaForgeBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `set_vsync` | `NebulaForgeBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `set_frame_rate_limit` | `NebulaForgeBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `configure_nanite` | `NebulaForgeBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |
| `configure_lod` | `NebulaForgeBridge_PerformanceHandlers.cpp` | `HandlePerformanceAction` | |

## Animation & Physics (`animation_physics`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_animation_bp` | `NebulaForgeBridge_AnimationHandlers.cpp` | `HandleCreateAnimBlueprint` | |
| `play_montage` | `NebulaForgeBridge_AnimationHandlers.cpp` | `HandlePlayAnimMontage` | |
| `setup_ragdoll` | `NebulaForgeBridge_AnimationHandlers.cpp` | `HandleSetupRagdoll` | |
| `configure_vehicle` | `NebulaForgeBridge_AnimationHandlers.cpp` | `HandleAnimationPhysicsAction` | Supports custom vehicle type passthrough |

## Effects Manager (`manage_effect`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `niagara` | `NebulaForgeBridge_EffectHandlers.cpp` | `HandleEffectAction` | |
| `spawn_niagara` | `NebulaForgeBridge_EffectHandlers.cpp` | `HandleSpawnNiagaraActor` | |
| `debug_shape` | `NebulaForgeBridge_EffectHandlers.cpp` | `HandleEffectAction` | |
| `create_niagara_system` | `NebulaForgeBridge_EffectHandlers.cpp` | `HandleCreateNiagaraSystem` | |
| `create_niagara_emitter` | `NebulaForgeBridge_EffectHandlers.cpp` | `HandleCreateNiagaraEmitter` | |
| `add_niagara_module` | `NebulaForgeBridge_NiagaraGraphHandlers.cpp` | `HandleNiagaraGraphAction` | |
| `list_debug_shapes` | `NebulaForgeBridge_EffectHandlers.cpp` | `HandleEffectAction` | Discovery: Returns all debug shape types |
| `clear_debug_shapes` | `NebulaForgeBridge_EffectHandlers.cpp` | `HandleEffectAction` | Clears persistent debug shapes |

## Environment Builder (`build_environment`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_landscape` | `NebulaForgeBridge_LandscapeHandlers.cpp` | `HandleCreateLandscape` | |
| `sculpt` | `NebulaForgeBridge_LandscapeHandlers.cpp` | `HandleSculptLandscape` | |
| `paint_foliage` | `NebulaForgeBridge_FoliageHandlers.cpp` | `HandlePaintFoliage` | |
| `add_foliage_instances` | `NebulaForgeBridge_FoliageHandlers.cpp` | `HandleAddFoliageInstances` | |
| `get_foliage_instances` | `NebulaForgeBridge_FoliageHandlers.cpp` | `HandleGetFoliageInstances` | |
| `remove_foliage` | `NebulaForgeBridge_FoliageHandlers.cpp` | `HandleRemoveFoliage` | |
| `create_procedural_terrain` | `NebulaForgeBridge_EnvironmentHandlers.cpp` | `HandleCreateProceduralTerrain` | |

## System Control (`system_control`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `execute_command` | `NebulaForgeBridge_EditorFunctionHandlers.cpp` | `HandleExecuteEditorFunction` | |
| `console_command` | `NebulaForgeBridge_EditorFunctionHandlers.cpp` | `HandleConsoleCommandAction` | |
| `run_ubt` | `NebulaForgeBridge_PipelineHandlers.cpp` | `HandlePipelineAction` | TypeScript runs local UBT when discoverable, otherwise falls back to native `manage_pipeline` execution. |
| `run_tests` | `NebulaForgeBridge_TestHandlers.cpp` | `HandleTestAction` | |
| `subscribe` | `NebulaForgeBridge_LogHandlers.cpp` | `HandleLogAction` | |
| `unsubscribe` | `NebulaForgeBridge_LogHandlers.cpp` | `HandleLogAction` | |
| `spawn_category` | `NebulaForgeBridge_DebugHandlers.cpp` | `HandleDebugAction` | |
| `start_session` | `NebulaForgeBridge_InsightsHandlers.cpp` | `HandleInsightsAction` | |
| `lumen_update_scene` | `NebulaForgeBridge_RenderHandlers.cpp` | `HandleRenderAction` | |
| `set_project_setting` | `NebulaForgeBridge_EnvironmentHandlers.cpp` | `HandleSystemControlAction` | |
| `execute_python` | `NebulaForgeBridge_SystemControlHandlers.cpp` | `HandleSystemControlAction` | Requires Python Editor Script Plugin. Max 1 MB code. Async timeout warning at 60s. |
| `create_game_instance_subsystem` / `create_world_subsystem` | `NebulaForgeBridge_SystemControlHandlers.cpp` | `HandleSubsystemAction` | Resolves Unreal-managed game-instance or world subsystem instances |
| `create_local_player_subsystem` / `create_engine_subsystem` | `NebulaForgeBridge_SystemControlHandlers.cpp` | `HandleSubsystemAction` | Resolves local-player or engine subsystem instances |
| `get_subsystem` / `inspect_subsystem` | `NebulaForgeBridge_SystemControlHandlers.cpp` | `HandleSubsystemAction` | Returns subsystem class, object, owner, scope, and tick metadata |
| `list_subsystems` | `NebulaForgeBridge_SystemControlHandlers.cpp` | `HandleSubsystemAction` | Enumerates live subsystem instances with optional scope/class filters |
| `configure_subsystem_tick` | `NebulaForgeBridge_SystemControlHandlers.cpp` | `HandleSubsystemAction` | Configures tick mode for `UTickableWorldSubsystem` |
| `set_timer` / `clear_timer` / `pause_timer` / `resume_timer` | `NebulaForgeBridge_SystemControlHandlers.cpp` | `HandleAsyncTimerAction` | Manages world `FTimerManager` timers |
| `get_timer` / `list_timers` | `NebulaForgeBridge_SystemControlHandlers.cpp` | `HandleAsyncTimerAction` | Reports timer active, paused, pending, elapsed, and remaining state |
| `create_latent_action` / `clear_latent_action` | `NebulaForgeBridge_SystemControlHandlers.cpp` | `HandleAsyncTimerAction` | Registers UUID-backed latent delays with optional callback links |
| `get_latent_action` / `list_latent_actions` | `NebulaForgeBridge_SystemControlHandlers.cpp` | `HandleAsyncTimerAction` | Inspects bridge-owned latent actions |
| `create_async_action` / `cancel_async_action` | `NebulaForgeBridge_SystemControlHandlers.cpp` | `HandleAsyncTimerAction` | Runs cooperative work through Unreal `Async` execution modes |
| `get_async_action` / `list_async_actions` | `NebulaForgeBridge_SystemControlHandlers.cpp` | `HandleAsyncTimerAction` | Reports async execution and completion state |
| `create_gameplay_task` / `end_gameplay_task` | `NebulaForgeBridge_SystemControlHandlers.cpp` | `HandleAsyncTimerAction` | Creates and ends managed generic gameplay tasks |
| `get_gameplay_task` / `list_gameplay_tasks` / `configure_task_priority` | `NebulaForgeBridge_SystemControlHandlers.cpp` | `HandleAsyncTimerAction` | Inspects task state and recreates inactive managed tasks with a new priority |
| `create_event_dispatcher` / `create_delegate` | `NebulaForgeBridge_DelegateInterfaceHandlers.cpp` | `HandleDelegateInterfaceAction` | Creates Blueprint multicast dispatcher or single/multicast delegate variables |
| `bind_to_event` / `unbind_from_event` / `bind_delegate` | `NebulaForgeBridge_DelegateInterfaceHandlers.cpp` | `HandleDelegateInterfaceAction` | Binds loaded `UObject` callback functions through reflected delegate properties |
| `broadcast_event` | `NebulaForgeBridge_DelegateInterfaceHandlers.cpp` | `HandleDelegateInterfaceAction` | Broadcasts multicast delegates using reflected `parameterValues` |
| `inspect_delegate` / `list_delegate_bindings` | `NebulaForgeBridge_DelegateInterfaceHandlers.cpp` | `HandleDelegateInterfaceAction` | Returns delegate signature, binding state, and bound-object metadata |
| `create_blueprint_interface` / `add_interface_function` | `NebulaForgeBridge_DelegateInterfaceHandlers.cpp` | `HandleDelegateInterfaceAction` | Creates Blueprint Interface assets and function graphs |
| `implement_interface` | `NebulaForgeBridge_DelegateInterfaceHandlers.cpp` | `HandleDelegateInterfaceAction` | Uses `FBlueprintEditorUtils::ImplementNewInterface` |
| `get_interface_info` / `call_interface_function` | `NebulaForgeBridge_DelegateInterfaceHandlers.cpp` | `HandleDelegateInterfaceAction` | Enumerates implemented interfaces and invokes reflected interface functions |
| `create_hud` | `NebulaForgeBridge_UiHandlers.cpp` | `HandleUiAction` | Sub-action of `system_control` |
| `set_widget_text` | `NebulaForgeBridge_UiHandlers.cpp` | `HandleUiAction` | Sub-action of `system_control` |
| `set_widget_image` | `NebulaForgeBridge_UiHandlers.cpp` | `HandleUiAction` | Sub-action of `system_control` |
| `set_widget_visibility` | `NebulaForgeBridge_UiHandlers.cpp` | `HandleUiAction` | Sub-action of `system_control` |
| `remove_widget_from_viewport` | `NebulaForgeBridge_UiHandlers.cpp` | `HandleUiAction` | Sub-action of `system_control` |

## Sequencer (`manage_sequence`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create` | `NebulaForgeBridge_SequenceHandlers.cpp` | `HandleSequenceAction` | |
| `add_actor` | `NebulaForgeBridge_SequenceHandlers.cpp` | `HandleSequenceAction` | |
| `play` | `NebulaForgeBridge_SequenceHandlers.cpp` | `HandleSequenceAction` | |
| `add_keyframe` | `NebulaForgeBridge_SequencerHandlers.cpp` | `HandleAddSequencerKeyframe` | |
| `add_camera` | `NebulaForgeBridge_SequenceHandlers.cpp` | `HandleAddCameraTrack` | |
| `add_track` | `NebulaForgeBridge_SequenceHandlers.cpp` | `HandleSequenceAction` | Dynamic track class resolution |
| `list_track_types` | `NebulaForgeBridge_SequenceHandlers.cpp` | `HandleSequenceAction` | Discovery: Returns all `UMovieSceneTrack` subclasses |

## Introspection (`inspect`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `inspect_object` | `NebulaForgeBridge_PropertyHandlers.cpp` | `HandleInspectAction` | |
| `inspect_class` | `NebulaForgeBridge_EnvironmentHandlers.cpp` | `HandleInspectAction` | Global action, no objectPath required |
| `inspect_cdo` | `NebulaForgeBridge_PropertyHandlers.cpp` | `HandleInspectCdoAction` | Inspect any Blueprint CDO without spawning an actor. CDO properties via reflection; for Actor BPs enumerates CDO components with effective overrides. |
| `set_property` | `NebulaForgeBridge_PropertyHandlers.cpp` | `HandleSetObjectProperty` | |
| `get_property` | `NebulaForgeBridge_PropertyHandlers.cpp` | `HandleGetObjectProperty` | |
| `get_components` | `NebulaForgeBridge_ControlHandlers.cpp` | `HandleControlActorAction` | |
| `list_objects` | `NebulaForgeBridge_PropertyHandlers.cpp` | `HandleInspectAction` | |

## Audio Manager (`manage_audio`)

`manage_audio` exposes 50 public actions. TypeScript and native MCP route the 30 graph/asset-authoring actions through the internal native `manage_audio_authoring` action while regular playback and runtime configuration continue through `HandleAudioAction`.

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_sound_cue`, `create_sound_class`, `create_sound_mix` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Creates base SoundCue/SoundClass/SoundMix assets without modal editor dialogs |
| `play_sound_at_location`, `play_sound_2d`, `play_sound_attached`, `spawn_sound_at_location`, `create_ambient_sound`, `prime_sound` | `NebulaForgeBridge_AudioHandlers.cpp` | `HandleAudioAction` | Playback, attachment, component spawning, and priming |
| `create_audio_component`, `create_reverb_zone` | `NebulaForgeBridge_AudioHandlers.cpp` | `HandleAudioAction` | Editor actor/component creation |
| `push_sound_mix`, `pop_sound_mix`, `set_sound_mix_class_override`, `clear_sound_mix_class_override`, `set_base_sound_mix` | `NebulaForgeBridge_AudioHandlers.cpp` | `HandleAudioAction` | Runtime SoundMix control |
| `set_sound_attenuation`, `set_doppler_effect`, `set_audio_occlusion`, `enable_audio_analysis`, `fade_sound`, `fade_sound_in`, `fade_sound_out` | `NebulaForgeBridge_AudioHandlers.cpp` | `HandleAudioAction` | Runtime/configuration actions |

## Behavior Tree Manager (`manage_ai`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `add_node` | `NebulaForgeBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | |
| `connect_nodes` | `NebulaForgeBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | |
| `remove_node` | `NebulaForgeBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | |
| `break_connections` | `NebulaForgeBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | |
| `set_node_properties` | `NebulaForgeBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | Handles `FBlackboardKeySelector` struct properties via `SelectedKeyName` + `ResolveSelectedKey` |
| `add_subnode` | `NebulaForgeBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | Add decorator/service as subnode attached to a parent graph node (`"root"` sentinel for root-level decorators) |
| `get_tree` | `NebulaForgeBridge_BehaviorTreeHandlers.cpp` | `HandleBehaviorTreeAction` | Read-only. Returns the navigable BT hierarchy (recursive `rootNode`, per-edge `entryDecorators`/`entryDecoratorOpsRaw`, `services`, `rootDecorators`/`rootDecoratorOpsRaw`, per-node `keyProperties`). Runtime-only walk of `BT->RootNode`; no BehaviorTreeEditor module required. |

## Blueprint Graph Actions (`manage_blueprint`)

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_node` | `NebulaForgeBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | Dynamic node class resolution |
| `delete_node` | `NebulaForgeBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | |
| `connect_pins` | `NebulaForgeBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | |
| `break_pin_links` | `NebulaForgeBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | |
| `set_node_property` | `NebulaForgeBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | |
| `list_node_types` | `NebulaForgeBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | Lists all UK2Node subclasses |
| `set_pin_default_value` | `NebulaForgeBridge_BlueprintGraphHandlers.cpp` | `HandleBlueprintGraphAction` | Sets default value on input pins |

## Geometry Manager (`manage_geometry`) - Phase 6

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_box` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates DynamicMesh box primitive |
| `create_sphere` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates DynamicMesh sphere primitive |
| `create_cylinder` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates DynamicMesh cylinder primitive |
| `create_cone` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates DynamicMesh cone primitive |
| `create_capsule` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates DynamicMesh capsule primitive |
| `create_torus` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates DynamicMesh torus primitive |
| `create_plane` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates DynamicMesh plane primitive |
| `create_disc` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates DynamicMesh disc primitive |
| `create_stairs` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates linear stairs with configurable steps |
| `create_spiral_stairs` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates curved/spiral stairs with inner radius |
| `create_ring` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates ring (disc with hole) using inner/outer radius |
| `boolean_union` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Boolean union of two DynamicMesh actors |
| `boolean_subtract` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Boolean subtraction of two DynamicMesh actors |
| `boolean_intersection` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Boolean intersection of two DynamicMesh actors |
| `extrude` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Linear extrude faces along direction |
| `inset` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Inset faces (shrink inward) |
| `outset` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Outset faces (expand outward) |
| `bevel` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Bevel edges/polygroups with subdivisions |
| `offset_faces` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Offset faces along normals |
| `shell` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Solidify mesh (add thickness) |
| `bend` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Bend deformer with angle and extent |
| `twist` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Twist deformer with angle and extent |
| `taper` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Taper/flare deformer with XY percentages |
| `noise_deform` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Perlin noise displacement with magnitude/frequency |
| `smooth` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Iterative smoothing with iterations and alpha |
| `simplify_mesh` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Reduces triangle count via QEM simplification |
| `subdivide` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Subdivides mesh via PN tessellation |
| `remesh_uniform` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Uniform remesh to target triangle count |
| `weld_vertices` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Weld nearby vertices within tolerance |
| `fill_holes` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Automatically fill mesh holes |
| `remove_degenerates` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Remove degenerate triangles/edges |
| `auto_uv` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Auto-generates UVs using XAtlas |
| `recalculate_normals` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Recalculates mesh normals |
| `flip_normals` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Flips mesh normals |
| `generate_collision` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Generate collision (convex, box, sphere, capsule, decomposition) |
| `convert_to_static_mesh` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Converts DynamicMesh to StaticMesh asset |
| `get_mesh_info` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Returns vertex/triangle counts, UV/normal info |
| `create_arch` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates partial torus (arch) with angle parameter |
| `create_pipe` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates hollow cylinder (boolean subtract inner) |
| `create_ramp` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Creates extruded right triangle polygon |
| `mirror` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Mirror mesh across axis (X/Y/Z), optionally weld seam |
| `array_linear` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Linear array with count and offset vector |
| `array_radial` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Radial array around center with count and angle |
| `triangulate` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Convert quads/n-gons to triangles |
| `poke` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Poke face centers, subdivide with offset |
| `relax` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Relaxation smoothing (Laplacian with strength) |
| `project_uv` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | UV projection (box, planar, cylindrical) |
| `recompute_tangents` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Recompute tangent space using MikkT |
| `revolve` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Revolve 2D profile around axis to create solid |
| `stretch` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Stretch mesh along axis with factor |
| `spherify` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Transform vertices toward spherical shape |
| `cylindrify` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Transform vertices toward cylindrical shape |
| `chamfer` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Chamfer edges with distance and steps |
| `merge_vertices` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Merge duplicate vertices within tolerance |
| `transform_uvs` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Transform UVs (translate, scale, rotate) |
| `boolean_trim` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Boolean trim with keepInside option |
| `self_union` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Self-union for watertight mesh |
| `extrude_along_spline` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Extrude mesh profile along spline path |
| `bridge` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Bridge gaps between edge groups |
| `loft` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Loft surface between cross-sections |
| `sweep` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Sweep profile along path with twist/scale |
| `duplicate_along_spline` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Duplicate meshes along spline path |
| `loop_cut` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Insert edge loop cuts |
| `edge_split` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Split edges based on angle threshold |
| `quadrangulate` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Convert triangles to quads |
| `remesh_voxel` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Voxel-based remesh for watertight mesh |
| `unwrap_uv` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Automatic UV unwrap with XAtlas |
| `pack_uv_islands` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Pack UV islands for optimal space usage |
| `generate_complex_collision` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Generate complex collision (mesh-based) |
| `simplify_collision` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Simplify collision with convex decomposition |
| `generate_lods` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Generate LOD levels for static mesh |
| `set_lod_settings` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Configure individual LOD settings |
| `set_lod_screen_sizes` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Set screen size thresholds for all LODs |
| `convert_to_nanite` | `NebulaForgeBridge_GeometryHandlers.cpp` | `HandleGeometryAction` | Enable Nanite on static mesh (UE5+) |

## Skeleton Manager (`animation_physics`) - Phase 7

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `get_skeleton_info` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleGetSkeletonInfo` | Returns bone count, virtual bone count, socket count |
| `list_bones` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleListBones` | Lists all bones with index, parent, reference pose |
| `list_sockets` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleListSockets` | Lists all sockets with bone, location, rotation, scale |
| `create_socket` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleCreateSocket` | Creates socket on skeleton with attachment bone |
| `configure_socket` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleConfigureSocket` | Modifies existing socket properties |
| `create_virtual_bone` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleCreateVirtualBone` | Creates virtual bone between source and target bones |
| `create_physics_asset` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleCreatePhysicsAsset` | Creates physics asset linked to skeletal mesh |
| `list_physics_bodies` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleListPhysicsBodies` | Lists physics bodies and constraints |
| `add_physics_body` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleAddPhysicsBody` | Adds capsule/sphere/box bodies to physics asset |
| `configure_physics_body` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleConfigurePhysicsBody` | Configures mass, damping, collision |
| `add_physics_constraint` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleAddPhysicsConstraint` | Creates joint between two bodies |
| `configure_constraint_limits` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleConfigureConstraintLimits` | Sets angular/linear limits |
| `rename_bone` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleRenameBone` | Renames virtual bones (regular bones require reimport) |
| `set_bone_transform` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleSetBoneTransform` | Sets reference pose transform |
| `create_morph_target` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleCreateMorphTarget` | Creates new UMorphTarget on mesh |
| `set_morph_target_deltas` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleSetMorphTargetDeltas` | Sets vertex deltas for morph target |
| `import_morph_targets` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleImportMorphTargets` | Lists morph targets (FBX import via asset pipeline) |
| `normalize_weights` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleNormalizeWeights` | Rebuilds mesh with normalized weights |
| `prune_weights` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandlePruneWeights` | Rebuilds mesh with pruned weights |
| `bind_cloth_to_skeletal_mesh` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleBindClothToSkeletalMesh` | Prepares cloth binding |
| `assign_cloth_asset_to_mesh` | `NebulaForgeBridge_SkeletonHandlers.cpp` | `HandleAssignClothAssetToMesh` | Lists/assigns cloth assets |
| `create_skeleton` | `NebulaForgeBridge_SkeletonHandlers.cpp` | - | **Stub** - Requires FBX import |
| `add_bone` | `NebulaForgeBridge_SkeletonHandlers.cpp` | - | **Stub** - Requires FReferenceSkeletonModifier |
| `remove_bone` | `NebulaForgeBridge_SkeletonHandlers.cpp` | - | **Stub** - Requires FReferenceSkeletonModifier |
| `set_bone_parent` | `NebulaForgeBridge_SkeletonHandlers.cpp` | - | **Stub** - Requires hierarchy rebuild |
| `auto_skin_weights` | `NebulaForgeBridge_SkeletonHandlers.cpp` | - | **Stub** - Use Skeletal Mesh Editor |
| `set_vertex_weights` | `NebulaForgeBridge_SkeletonHandlers.cpp` | - | **Stub** - Requires vertex buffer access |
| `copy_weights` | `NebulaForgeBridge_SkeletonHandlers.cpp` | - | **Stub** - Use Skeletal Mesh Editor |
| `mirror_weights` | `NebulaForgeBridge_SkeletonHandlers.cpp` | - | **Stub** - Use Skeletal Mesh Editor |

## Material Authoring Manager (`manage_asset`) - Phase 8

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_material` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Creates new UMaterial asset |
| `set_blend_mode` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Sets blend mode (opaque, masked, translucent, etc.) |
| `set_shading_model` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Sets shading model (default_lit, unlit, subsurface, etc.) |
| `set_material_domain` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Sets domain (surface, deferred_decal, light_function, etc.) |
| `add_texture_sample` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionTextureSample node |
| `add_texture_coordinate` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionTextureCoordinate node |
| `add_scalar_parameter` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionScalarParameter node |
| `add_vector_parameter` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionVectorParameter node |
| `add_static_switch_parameter` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionStaticSwitchParameter node |
| `add_math_node` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds math nodes (add, multiply, divide, power, lerp, etc.) |
| `add_world_position` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionWorldPosition node |
| `add_vertex_normal` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionVertexNormalWS node |
| `add_pixel_depth` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionPixelDepth node |
| `add_fresnel` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionFresnel node |
| `add_reflection_vector` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionReflectionVectorWS node |
| `add_panner` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionPanner node |
| `add_rotator` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionRotator node |
| `add_noise` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionNoise node |
| `add_voronoi` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionVoronoiNoise node |
| `add_if` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionIf node |
| `add_switch` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionSwitch node |
| `add_custom_expression` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionCustom node (HLSL code) |
| `connect_nodes` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Connects material expression pins |
| `disconnect_nodes` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Disconnects material expression pins |
| `create_material_function` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Creates UMaterialFunction asset |
| `add_function_input` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionFunctionInput node |
| `add_function_output` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionFunctionOutput node |
| `use_material_function` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionMaterialFunctionCall node |
| `create_material_instance` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Creates UMaterialInstanceConstant asset |
| `set_scalar_parameter_value` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Sets scalar parameter on material instance |
| `set_vector_parameter_value` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Sets vector parameter on material instance |
| `set_texture_parameter_value` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Sets texture parameter on material instance |
| `create_landscape_material` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Creates landscape-ready material |
| `create_decal_material` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Creates deferred decal material |
| `create_post_process_material` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Creates post process material |
| `add_landscape_layer` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Adds UMaterialExpressionLandscapeLayerBlend node |
| `configure_layer_blend` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Configures layer blend settings |
| `compile_material` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Compiles material and reports errors |
| `get_material_info` | `NebulaForgeBridge_MaterialAuthoringHandlers.cpp` | `HandleManageMaterialAuthoringAction` | Returns material properties and node info |

## Texture Manager (`manage_asset`) - Phase 9

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| `create_noise_texture` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Creates procedural noise texture (Perlin, Simplex, Worley, Voronoi) |
| `create_gradient_texture` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Creates gradient texture (Linear, Radial, Angular) |
| `create_pattern_texture` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Creates pattern texture (Checker, Grid, Brick, Dots, Stripes) |
| `create_normal_from_height` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Converts height map to normal map using Sobel/Prewitt |
| `create_ao_from_mesh` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Bakes AO from mesh (placeholder - requires GPU) |
| `resize_texture` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `adjust_levels` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `adjust_curves` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `blur` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `sharpen` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `invert` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `desaturate` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `channel_pack` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `channel_extract` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `combine_textures` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | **Stub** - Requires GPU processing |
| `set_compression_settings` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Sets texture compression (TC_Default, TC_Normalmap, etc.) |
| `set_texture_group` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Sets LOD group (TEXTUREGROUP_World, etc.) |
| `set_lod_bias` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Sets LOD bias value |
| `configure_virtual_texture` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Enables/disables virtual texture streaming |
| `set_streaming_priority` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Sets streaming priority and NeverStream flag |
| `get_texture_info` | `NebulaForgeBridge_TextureHandlers.cpp` | `HandleManageTextureAction` | Returns texture dimensions, format, compression, mip count |

## Animation Authoring Manager (`animation_physics`) - Phase 10

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Animation Sequences** | | | |
| `create_animation_sequence` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UAnimSequence with specified skeleton, frames, framerate |
| `set_sequence_length` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Sets sequence duration via IAnimationDataController |
| `add_bone_track` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds bone curve to sequence |
| `set_bone_key` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Sets transform keyframe at frame |
| `set_curve_key` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Sets curve value keyframe |
| `add_notify` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds UAnimNotify at time/frame |
| `add_notify_state` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds UAnimNotifyState with duration |
| `add_sync_marker` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds FAnimSyncMarker |
| `set_root_motion_settings` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Configures root motion, root lock |
| `set_additive_settings` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Configures additive type, base pose |
| **Animation Montages** | | | |
| `create_montage` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UAnimMontage with skeleton |
| `add_montage_section` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds FCompositeSection at time |
| `add_montage_slot` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds animation to slot track |
| `set_section_timing` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Updates section start time |
| `add_montage_notify` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds notify to montage |
| `set_blend_in` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Configures blend in time/curve |
| `set_blend_out` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Configures blend out time/curve |
| `link_sections` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Links montage sections |
| **Blend Spaces** | | | |
| `create_blend_space_1d` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UBlendSpace1D |
| `create_blend_space_2d` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UBlendSpace |
| `add_blend_sample` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds animation sample at position |
| `set_axis_settings` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Configures axis name, min, max, grid |
| `set_interpolation_settings` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Sets target weight interpolation speed |
| `create_aim_offset` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UAimOffsetBlendSpace |
| `add_aim_offset_sample` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds sample at yaw/pitch |
| **Animation Blueprints** | | | |
| `create_anim_blueprint` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UAnimBlueprint |
| `add_state_machine` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds state machine to anim graph |
| `add_state` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds state to state machine |
| `add_transition` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds transition between states |
| `set_transition_rules` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Configures blend time, logic type |
| `add_blend_node` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds blend node to anim graph |
| `add_cached_pose` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds cached pose node |
| `add_slot_node` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds slot node for montages |
| `add_layered_blend_per_bone` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds layered blend per bone |
| `set_anim_graph_node_value` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Sets node property value |
| **Control Rig** | | | |
| `create_control_rig` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UControlRigBlueprint (if available) |
| `add_control` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds control to rig |
| `add_rig_unit` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds IK/FK solver unit |
| `connect_rig_elements` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Connects rig elements |
| `create_pose_library` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UPoseAsset |
| **Retargeting** | | | |
| `create_ik_rig` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UIKRigDefinition |
| `add_ik_chain` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Adds IK chain to rig |
| `create_ik_retargeter` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Creates UIKRetargeter |
| `set_retarget_chain_mapping` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Maps source to target chain |
| **Utility** | | | |
| `get_animation_info` | `NebulaForgeBridge_AnimationAuthoringHandlers.cpp` | `HandleManageAnimationAuthoringAction` | Returns animation asset properties |

## Audio Authoring Manager (`manage_audio`) - Phase 11

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Sound Cues** | | | |
| `add_cue_node` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Adds wave_player, mixer, random, modulator, looping, etc. |
| `connect_cue_nodes` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Connects sound cue nodes as parent-child |
| `set_cue_attenuation` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets attenuation settings on sound cue |
| `set_cue_concurrency` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets concurrency settings on sound cue |
| **MetaSounds** | | | |
| `create_metasound` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Creates MetaSound asset (UE 5.0+) |
| `add_metasound_node` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Adds node to MetaSound graph |
| `connect_metasound_nodes` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Connects MetaSound nodes |
| `add_metasound_input` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Adds input parameter to MetaSound |
| `add_metasound_output` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Adds output to MetaSound |
| `set_metasound_default` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets default value for MetaSound input |
| **Sound Classes & Mixes** | | | |
| `set_class_properties` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets volume, pitch, LPF, stereo bleed, etc. |
| `set_class_parent` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets parent sound class for hierarchy |
| `add_mix_modifier` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Adds FSoundClassAdjuster to sound mix |
| `configure_mix_eq` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Configures EQ settings on sound mix |
| **Attenuation & Spatialization** | | | |
| `create_attenuation_settings` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Creates USoundAttenuation asset |
| `configure_distance_attenuation` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets inner radius, falloff, algorithm |
| `configure_spatialization` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets spatialization algorithm (Panner/HRTF) |
| `configure_occlusion` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets occlusion LPF, volume, interpolation |
| `configure_reverb_send` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets reverb wet levels and distances |
| **Dialogue System** | | | |
| `create_dialogue_voice` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Creates UDialogueVoice with gender/plurality |
| `create_dialogue_wave` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Creates UDialogueWave with spoken text |
| `set_dialogue_context` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Sets dialogue context mappings |
| **Effects** | | | |
| `create_reverb_effect` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Creates UReverbEffect with density, decay, etc. |
| `create_source_effect_chain` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Creates source effect chain (AudioMixer) |
| `add_source_effect` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Adds effect to source effect chain |
| `create_submix_effect` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Creates submix effect preset |
| **Utility** | | | |
| `get_audio_info` | `NebulaForgeBridge_AudioAuthoringHandlers.cpp` | `HandleManageAudioAuthoringAction` | Returns audio asset properties (type-specific) |

## Niagara Authoring Manager (`manage_effect`) - Phase 12

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Systems & Emitters** | | | |
| `create_niagara_system` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Creates UNiagaraSystem asset |
| `create_niagara_emitter` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Creates UNiagaraEmitter asset |
| `add_emitter_to_system` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds emitter to system |
| `set_emitter_properties` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Sets enabled, local space, sim target |
| **Spawn Modules** | | | |
| `add_spawn_rate_module` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Configures spawn rate (particles/sec) |
| `add_spawn_burst_module` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Configures burst spawn (count, time) |
| `add_spawn_per_unit_module` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Configures spawn per unit distance |
| **Particle Modules** | | | |
| `add_initialize_particle_module` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Sets lifetime, mass, initial size |
| `add_particle_state_module` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Configures particle state behavior |
| `add_force_module` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds gravity, drag, vortex, curl noise, etc. |
| `add_velocity_module` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Sets initial velocity (linear, cone, point) |
| `add_acceleration_module` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Sets particle acceleration vector |
| **Appearance Modules** | | | |
| `add_size_module` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Configures uniform/non-uniform size |
| `add_color_module` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Sets color, color mode, color curves |
| **Renderer Modules** | | | |
| `add_sprite_renderer_module` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds UNiagaraSpriteRendererProperties |
| `add_mesh_renderer_module` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds UNiagaraMeshRendererProperties |
| `add_ribbon_renderer_module` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds UNiagaraRibbonRendererProperties |
| `add_light_renderer_module` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds UNiagaraLightRendererProperties |
| **Behavior Modules** | | | |
| `add_collision_module` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Configures collision mode, restitution, friction |
| `add_kill_particles_module` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Configures kill conditions (age, box, sphere) |
| `add_camera_offset_module` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Sets camera offset for particles |
| **Parameters** | | | |
| `add_user_parameter` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds exposed user parameter |
| `set_parameter_value` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Sets parameter value |
| `bind_parameter_to_source` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Binds parameter to source |
| **Data Interfaces** | | | |
| `add_skeletal_mesh_data_interface` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds skeletal mesh sampling DI |
| `add_static_mesh_data_interface` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds static mesh sampling DI |
| `add_spline_data_interface` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds spline data interface |
| `add_audio_spectrum_data_interface` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds audio spectrum reactive DI |
| `add_collision_query_data_interface` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds collision query DI |
| **Events** | | | |
| `add_event_generator` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds event generator to emitter |
| `add_event_receiver` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds event receiver with optional spawn |
| `configure_event_payload` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Configures event payload attributes |
| **GPU Simulation** | | | |
| `enable_gpu_simulation` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Enables GPU compute simulation |
| `add_simulation_stage` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Adds custom simulation stage |
| **Utility** | | | |
| `get_niagara_info` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Returns system/emitter info, parameters, renderers |
| `validate_niagara_system` | `NebulaForgeBridge_NiagaraAuthoringHandlers.cpp` | `HandleManageNiagaraAuthoringAction` | Validates system and returns errors/warnings |

## GAS Manager (`manage_gas`) - Phase 13

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Components & Attributes** | | | |
| `add_ability_system_component` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Adds ASC via SCS to blueprint |
| `configure_asc` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets replication mode on ASC |
| `create_attribute_set` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Creates UAttributeSet blueprint |
| `add_attribute` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Adds FGameplayAttributeData member |
| `set_attribute_base_value` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Configures attribute base value |
| `set_attribute_clamping` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Configures min/max clamping |
| **Gameplay Abilities** | | | |
| `create_gameplay_ability` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Creates UGameplayAbility blueprint |
| `set_ability_tags` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets ability/cancel/block tags |
| `set_ability_costs` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets cost GameplayEffect class |
| `set_ability_cooldown` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets cooldown GameplayEffect class |
| `set_ability_targeting` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Configures targeting type |
| `add_ability_task` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Reference for AbilityTask usage |
| `set_activation_policy` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets net execution policy |
| `set_instancing_policy` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets instancing policy |
| **Gameplay Effects** | | | |
| `create_gameplay_effect` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Creates UGameplayEffect blueprint |
| `set_effect_duration` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets duration policy and time |
| `add_effect_modifier` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Adds modifier with operation/magnitude |
| `set_modifier_magnitude` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets magnitude on existing modifier |
| `add_effect_execution_calculation` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Adds execution calculation reference |
| `add_effect_cue` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Adds gameplay cue tag to effect |
| `set_effect_stacking` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets stacking type and limit |
| `set_effect_tags` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Sets granted/application/removal tags |
| **Gameplay Cues** | | | |
| `create_gameplay_cue_notify` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Creates static or actor cue notify |
| `configure_cue_trigger` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Configures trigger type |
| `set_cue_effects` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Configures particle/sound/shake refs |
| `add_tag_to_asset` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Adds gameplay tag to asset |
| **Utility** | | | |
| `get_gas_info` | `NebulaForgeBridge_GASHandlers.cpp` | `HandleManageGASAction` | Returns GAS asset info and properties |

## Character Manager (`manage_character`) - Phase 14

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Character Creation** | | | |
| `create_character_blueprint` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Creates ACharacter blueprint with capsule, mesh, movement |
| `configure_capsule_component` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Sets radius, half-height, collision |
| `configure_mesh_component` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Configures skeletal mesh, animation BP |
| `configure_camera_component` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Sets up camera boom and camera component |
| **Movement Component** | | | |
| `configure_movement_speeds` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Walk, run, sprint, crouch, swim, fly speeds |
| `configure_jump` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Jump height, air control, double jump |
| `configure_rotation` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Orient to movement, use controller rotation |
| `add_custom_movement_mode` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Adds custom movement mode enum |
| `configure_nav_movement` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | NavMesh agent settings |
| **Advanced Movement** | | | |
| `setup_mantling` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Configures mantling system (trace, animation) |
| `setup_vaulting` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Configures vaulting system |
| `setup_climbing` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Configures climbing system |
| `setup_sliding` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Configures sliding system |
| `setup_wall_running` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Configures wall running system |
| `setup_grappling` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Configures grappling hook system |
| **Footsteps System** | | | |
| `setup_footstep_system` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Creates footstep audio system |
| `map_surface_to_sound` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Maps physical surface to sound |
| `configure_footstep_fx` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Configures footstep VFX (dust, splashes) |
| **Utility** | | | |
| `get_character_info` | `NebulaForgeBridge_CharacterHandlers.cpp` | `HandleManageCharacterAction` | Returns character blueprint info |

## Combat Manager (`manage_combat`) - Phase 15

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Weapon Base** | | | |
| `create_weapon_blueprint` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Creates AActor weapon blueprint with components |
| `configure_weapon_mesh` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Sets skeletal/static mesh on weapon |
| `configure_weapon_sockets` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Configures muzzle, grip, attachment sockets |
| `set_weapon_stats` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Sets damage, fire rate, range, spread |
| **Firing Modes** | | | |
| `configure_hitscan` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Configures hitscan trace settings |
| `configure_projectile` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Sets projectile class, spawn settings |
| `configure_spread_pattern` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Configures pellet spread, pattern type |
| `configure_recoil_pattern` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Sets recoil curve, recovery settings |
| `configure_aim_down_sights` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | ADS zoom, FOV, camera offset |
| **Projectiles** | | | |
| `create_projectile_blueprint` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Creates AActor projectile blueprint |
| `configure_projectile_movement` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Sets speed, gravity, rotation following |
| `configure_projectile_collision` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Collision channels, ignore actors |
| `configure_projectile_homing` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Homing acceleration, target type |
| **Damage System** | | | |
| `create_damage_type` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Creates UDamageType blueprint |
| `configure_damage_execution` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Sets damage multipliers, falloff |
| `setup_hitbox_component` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Adds hitbox with damage multiplier |
| **Weapon Features** | | | |
| `setup_reload_system` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Creates reload variables, montage slot |
| `setup_ammo_system` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Magazine size, reserve ammo, ammo types |
| `setup_attachment_system` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Attachment slots, stat modifiers |
| `setup_weapon_switching` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Weapon slots, switch timing |
| **Effects** | | | |
| `configure_muzzle_flash` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Particle system, light flash settings |
| `configure_tracer` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Tracer particle, frequency settings |
| `configure_impact_effects` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Surface-based impact particles, decals |
| `configure_shell_ejection` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Shell mesh, ejection socket, physics |
| **Melee Combat** | | | |
| `create_melee_trace` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Creates trace function, socket setup |
| `configure_combo_system` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Combo chain, timing windows |
| `create_hit_pause` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Hitstop duration, time dilation |
| `configure_hit_reaction` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Hit reaction montages, stagger |
| `setup_parry_block_system` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Parry window, block angle, stamina |
| `configure_weapon_trails` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Trail particle, socket binding |
| **Utility** | | | |
| `get_combat_info` | `NebulaForgeBridge_CombatHandlers.cpp` | `HandleManageCombatAction` | Returns weapon/combat component info |

## AI Manager (`manage_ai`) - Phase 16

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **AI Controller** | | | |
| `create_ai_controller` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Creates AAIController blueprint |
| `assign_behavior_tree` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Sets behavior tree on controller |
| `assign_blackboard` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Sets blackboard asset on controller |
| **Blackboard** | | | |
| `create_blackboard_asset` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Creates UBlackboardData asset |
| `add_blackboard_key` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds key (bool, int, float, vector, rotator, object, class, enum, name, string) |
| `set_key_instance_synced` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Sets instance sync flag on key |
| **Behavior Tree** | | | |
| `create_behavior_tree` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Creates UBehaviorTree asset |
| `add_composite_node` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds selector, sequence, parallel, simple_parallel |
| `add_task_node` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds move_to, rotate_to_face, wait, play_animation, play_sound, run_eqs_query, etc. |
| `add_decorator` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds blackboard, cooldown, cone_check, loop, time_limit, force_success, etc. |
| `add_service` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds default_focus, run_eqs services |
| `configure_bt_node` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Sets node properties |
| **EQS** | | | |
| `create_eqs_query` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Creates UEnvQuery asset |
| `add_eqs_generator` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds actors_of_class, current_location, donut, grid, on_circle, pathing_grid, points |
| `add_eqs_context` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds querier, item, target contexts |
| `add_eqs_test` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds distance, dot, overlap, pathfinding, project, random, trace, gameplay_tags tests |
| `configure_test_scoring` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Configures test scoring settings |
| **Perception** | | | |
| `add_ai_perception_component` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds UAIPerceptionComponent to blueprint |
| `configure_sight_config` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Sets sight radius, angle, age, detection by affiliation |
| `configure_hearing_config` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Sets hearing radius |
| `configure_damage_sense_config` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Configures damage sensing |
| `set_perception_team` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Sets AI perception team ID |
| **State Trees (UE5.3+)** | | | |
| `create_state_tree` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Creates UStateTree asset |
| `add_state_tree_state` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds state to state tree |
| `add_state_tree_transition` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds transition between states |
| `configure_state_tree_task` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Configures state task and conditions |
| **Smart Objects** | | | |
| `create_smart_object_definition` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Creates USmartObjectDefinition asset |
| `add_smart_object_slot` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds slot to smart object definition |
| `configure_slot_behavior` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Configures slot behavior settings |
| `add_smart_object_component` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds USmartObjectComponent to blueprint |
| **Mass AI (Crowds)** | | | |
| `create_mass_entity_config` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Creates mass entity config data asset |
| `configure_mass_entity` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Configures mass entity traits and fragments |
| `add_mass_spawner` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Adds AMassSpawner to level |
| **Utility** | | | |
| `get_ai_info` | `NebulaForgeBridge_AIHandlers.cpp` | `HandleManageAIAction` | Returns AI asset info and configuration |

## Inventory Manager (`manage_inventory`) - Phase 17

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Data Assets** | | | |
| `create_item_data_asset` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Creates generic item data asset |
| `set_item_properties` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Applies fields supplied in the `properties` object |
| `create_item_category` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Creates category data asset |
| `assign_item_category` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Assigns item to category |
| **Inventory Component** | | | |
| `create_inventory_component` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Adds UActorComponent for inventory |
| `configure_inventory_slots` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Sets inventory slot count |
| `add_inventory_functions` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Adds Add/Remove/Has item functions |
| `configure_inventory_events` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Configures OnItemAdded/Removed events |
| `set_inventory_replication` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Sets replication mode for multiplayer |
| **Pickups** | | | |
| `create_pickup_actor` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Creates pickup actor blueprint |
| `configure_pickup_interaction` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Sets interaction type and prompt |
| `configure_pickup_respawn` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Configures respawn timing |
| `configure_pickup_effects` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Configures pickup bobbing, rotation, and glow flags |
| **Equipment** | | | |
| `create_equipment_component` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Adds equipment management component |
| `define_equipment_slots` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Defines slot types (head, body, weapon, etc.) |
| `configure_equipment_effects` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Toggles stat modifier, ability grant, and passive effect support |
| `add_equipment_functions` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Adds Equip/Unequip functions |
| `configure_equipment_visuals` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Sets mesh attachment on equip |
| **Loot System** | | | |
| `create_loot_table` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Creates loot table data asset |
| `add_loot_entry` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Adds item to loot table with weight |
| `configure_loot_drop` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Sets drop count, radius, and drop-on-death flag |
| `set_loot_quality_tiers` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Configures quality tier names and weights |
| **Crafting** | | | |
| `create_crafting_recipe` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Creates recipe data asset |
| `configure_recipe_requirements` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Sets required level and station |
| `create_crafting_station` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Creates crafting station actor |
| `add_crafting_component` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Adds crafting functionality component |
| **Additional Actions** | | | |
| `configure_item_stacking` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Configures stackable, max stack size, and unique-item flags |
| `set_item_icon` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Stores an item icon path |
| `add_recipe_ingredient` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Adds an ingredient item path and quantity to a recipe |
| `remove_loot_entry` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Removes an array-backed loot entry by index when supported |
| `configure_inventory_weight` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Adds weight and encumberance support variables |
| `configure_station_recipes` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Configures station recipe paths, type, and crafting speed |
| **Utility** | | | |
| `get_inventory_info` | `NebulaForgeBridge_InventoryHandlers.cpp` | `HandleManageInventoryAction` | Returns inventory/equipment info |

## Interaction Manager (`manage_interaction`) - Phase 18

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Interaction Component** | | | |
| `create_interaction_component` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Adds interaction component to blueprint with trace settings |
| `configure_interaction_trace` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Configures trace type, channel, distance, frequency |
| `configure_interaction_widget` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets widget class, offset, prompt text format |
| `add_interaction_events` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Creates OnInteractionStart/End/Found/Lost event dispatchers |
| **Interactables** | | | |
| `create_interactable_interface` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Creates IInteractable UInterface with Interact/CanInteract functions |
| `create_door_actor` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Creates door blueprint with pivot, rotation animation, sounds |
| `configure_door_properties` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets open angle, time, direction, locked state, key item |
| `create_switch_actor` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Creates switch/button/lever with on/off states |
| `configure_switch_properties` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets switch type, toggleable, one-shot, target actors |
| `create_chest_actor` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Creates chest/container with lid animation, loot integration |
| `configure_chest_properties` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets loot table, locked state, respawn settings |
| `create_lever_actor` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Creates lever with rotation/translation animation |
| **Destructibles** | | | |
| `setup_destructible_mesh` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets up GeometryCollection with fracture mode, piece count |
| `configure_destruction_levels` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Configures damage thresholds, mesh indices, physics |
| `configure_destruction_effects` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets destroy sound, particle, debris settings |
| `configure_destruction_damage` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets max health, damage thresholds, multipliers |
| `add_destruction_component` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Adds destruction management component |
| **Trigger System** | | | |
| `create_trigger_actor` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Creates trigger volume actor (box, sphere, capsule) |
| `configure_trigger_events` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets onEnter, onExit, onStay events |
| `configure_trigger_filter` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets class, tag, interface filters |
| `configure_trigger_response` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Sets response type, cooldown, max activations |
| **Utility** | | | |
| `get_interaction_info` | `NebulaForgeBridge_InteractionHandlers.cpp` | `HandleManageInteractionAction` | Returns interaction component/actor properties |

## Widget Authoring Manager (`manage_blueprint`) - Phase 19

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Widget Creation** | | | |
| `create_widget_blueprint` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates UUserWidget blueprint asset |
| `set_widget_parent_class` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets parent class for widget |
| **Layout Panels** | | | |
| `add_canvas_panel` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UCanvasPanel container |
| `add_horizontal_box` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UHorizontalBox layout |
| `add_vertical_box` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UVerticalBox layout |
| `add_overlay` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UOverlay container |
| `add_grid_panel` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UGridPanel container |
| `add_uniform_grid` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UUniformGridPanel |
| `add_wrap_box` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UWrapBox container |
| `add_scroll_box` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UScrollBox container |
| `add_size_box` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds USizeBox constraint |
| `add_scale_box` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UScaleBox container |
| `add_border` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UBorder container |
| **Common Widgets** | | | |
| `add_text_block` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UTextBlock widget |
| `add_rich_text_block` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds URichTextBlock widget |
| `add_image` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UImage widget |
| `add_button` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UButton widget |
| `add_check_box` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UCheckBox widget |
| `add_slider` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds USlider widget |
| `add_progress_bar` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UProgressBar widget |
| `add_text_input` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds editable text widget |
| `add_combo_box` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UComboBoxString widget |
| `add_spin_box` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds USpinBox widget |
| `add_list_view` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UListView widget |
| `add_tree_view` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds UTreeView widget |
| **Layout & Styling** | | | |
| `set_anchor` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets widget anchors |
| `set_alignment` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets widget alignment |
| `set_position` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets widget position |
| `set_size` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets widget size |
| `set_padding` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets widget padding |
| `set_z_order` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets widget z-order |
| `set_render_transform` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets render transform |
| `set_visibility` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets widget visibility |
| `set_style` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets widget styling |
| `set_clipping` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Sets widget clipping |
| **Bindings & Events** | | | |
| `create_property_binding` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates property binding |
| `bind_text` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Binds text property |
| `bind_visibility` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Binds visibility |
| `bind_color` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Binds color/opacity |
| `bind_enabled` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Binds enabled state |
| `bind_on_clicked` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Binds click event |
| `bind_on_hovered` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Binds hover events |
| `bind_on_value_changed` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Binds value change event |
| **Widget Animations** | | | |
| `create_widget_animation` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates UWidgetAnimation |
| `add_animation_track` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds animation track |
| `add_animation_keyframe` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds keyframe to track |
| `set_animation_loop` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Configures animation loop |
| **UI Templates** | | | |
| `create_main_menu` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates main menu template |
| `create_pause_menu` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates pause menu template |
| `create_settings_menu` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates settings menu |
| `create_loading_screen` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates loading screen |
| `create_hud_widget` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates HUD template |
| `add_health_bar` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds health bar element |
| `add_ammo_counter` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds ammo counter |
| `add_minimap` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds minimap element |
| `add_crosshair` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds crosshair element |
| `add_compass` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds compass element |
| `add_interaction_prompt` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds interaction prompt |
| `add_objective_tracker` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds objective tracker |
| `add_damage_indicator` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Adds damage indicator |
| `create_inventory_ui` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates inventory UI |
| `create_dialog_widget` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates dialog widget |
| `create_radial_menu` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Creates radial menu |
| **Utility** | | | |
| `get_widget_info` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Returns widget blueprint info |
| `preview_widget` | `NebulaForgeBridge_WidgetAuthoringHandlers.cpp` | `HandleManageWidgetAuthoringAction` | Opens widget in preview |

## Networking Manager (`manage_networking`) - Phase 20

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Replication** | | | |
| `set_property_replicated` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets property replication flag |
| `set_replication_condition` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets COND_* replication condition |
| `configure_net_update_frequency` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Configures update frequency (Hz) |
| `configure_net_priority` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets network bandwidth priority |
| `set_net_dormancy` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets DORM_* dormancy mode |
| `configure_replication_graph` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Configures replication graph settings |
| **RPCs** | | | |
| `create_rpc_function` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Creates Server/Client/NetMulticast RPC |
| `configure_rpc_validation` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Enables RPC validation |
| `set_rpc_reliability` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets reliable/unreliable RPC |
| **Authority & Ownership** | | | |
| `set_owner` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets actor owner at runtime |
| `set_autonomous_proxy` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Configures autonomous proxy role |
| `check_has_authority` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Checks if local has authority |
| `check_is_locally_controlled` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Checks local control for Pawns |
| **Network Relevancy** | | | |
| `configure_net_cull_distance` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets network culling distance |
| `set_always_relevant` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets bAlwaysRelevant flag |
| `set_only_relevant_to_owner` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets bOnlyRelevantToOwner flag |
| **Net Serialization** | | | |
| `configure_net_serialization` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Configures custom NetSerialize |
| `set_replicated_using` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets ReplicatedUsing RepNotify |
| `configure_push_model` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Enables push-model replication |
| **Network Prediction** | | | |
| `configure_client_prediction` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Enables client-side prediction |
| `configure_server_correction` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Configures server reconciliation |
| `add_network_prediction_data` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Adds prediction data structure |
| `configure_movement_prediction` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Configures CMC network smoothing |
| **Connection & Session** | | | |
| `configure_net_driver` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Configures net driver settings |
| `set_net_role` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Sets initial net role |
| `configure_replicated_movement` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Configures movement replication |
| **Utility** | | | |
| `get_networking_info` | `NebulaForgeBridge_NetworkingHandlers.cpp` | `HandleManageNetworkingAction` | Returns networking configuration |

## Game Framework Manager (`manage_networking`) - Phase 21

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Core Class Creation** | | | |
| `create_game_mode` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Creates GameMode blueprint (AGameModeBase or AGameMode) |
| `create_game_state` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Creates GameState blueprint |
| `create_player_controller` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Creates PlayerController blueprint |
| `create_player_state` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Creates PlayerState blueprint |
| `create_game_instance` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Creates GameInstance blueprint |
| `create_hud_class` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Creates HUD blueprint |
| **Game Mode Configuration** | | | |
| `set_default_pawn_class` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Sets DefaultPawnClass on GameMode |
| `set_player_controller_class` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Sets PlayerControllerClass on GameMode |
| `set_game_state_class` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Sets GameStateClass on GameMode |
| `set_player_state_class` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Sets PlayerStateClass on GameMode |
| `configure_game_rules` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Configures game rules (min players, ready up, time limits) |
| **Match Flow** | | | |
| `setup_match_states` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Defines match state machine (waiting, warmup, in_progress, etc.) |
| `configure_round_system` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Configures round-based gameplay |
| `configure_team_system` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Sets up teams with colors and friendly fire |
| `configure_scoring_system` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Defines scoring rules and limits |
| `configure_spawn_system` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Configures spawn selection strategy |
| **Player Management** | | | |
| `configure_player_start` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Configures PlayerStart actor properties |
| `set_respawn_rules` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Sets respawn delay and location rules |
| `configure_spectating` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Configures spectator mode options |
| **Utility** | | | |
| `get_game_framework_info` | `NebulaForgeBridge_GameFrameworkHandlers.cpp` | `HandleManageGameFrameworkAction` | Queries GameMode class configuration |

## Sessions Manager (`manage_networking`) - Phase 22

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Session Management** | | | |
| `configure_local_session_settings` | `NebulaForgeBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Configures max players, session name, private/public |
| `configure_session_interface` | `NebulaForgeBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Configures Online Subsystem session interface |
| **Local Multiplayer** | | | |
| `configure_split_screen` | `NebulaForgeBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Enables/disables split-screen mode |
| `set_split_screen_type` | `NebulaForgeBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Sets split type (horizontal, vertical, quadrant) |
| `add_local_player` | `NebulaForgeBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Adds local player to session |
| `remove_local_player` | `NebulaForgeBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Removes local player from session |
| **LAN** | | | |
| `configure_lan_play` | `NebulaForgeBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Configures LAN broadcast/discovery settings |
| `host_lan_server` | `NebulaForgeBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Hosts LAN server on specified port |
| `join_lan_server` | `NebulaForgeBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Joins LAN server by IP/port |
| **Voice Chat** | | | |
| `enable_voice_chat` | `NebulaForgeBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Enables/disables voice chat |
| `configure_voice_settings` | `NebulaForgeBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Configures voice input/output settings |
| `set_voice_channel` | `NebulaForgeBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Sets player voice channel |
| `mute_player` | `NebulaForgeBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Mutes/unmutes specific player |
| `set_voice_attenuation` | `NebulaForgeBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Configures 3D voice attenuation |
| `configure_push_to_talk` | `NebulaForgeBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Configures push-to-talk settings |
| **Utility** | | | |
| `get_sessions_info` | `NebulaForgeBridge_SessionsHandlers.cpp` | `HandleManageSessionsAction` | Returns current session configuration info |

## Level Structure Manager (`manage_level_structure`) - Phase 23

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Levels** | | | |
| `create_level` | `NebulaForgeBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Creates new level asset |
| `create_sublevel` | `NebulaForgeBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Creates sublevel in current world |
| `configure_level_streaming` | `NebulaForgeBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Configures streaming method (Blueprint, AlwaysLoaded, etc.) |
| `set_streaming_distance` | `NebulaForgeBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Sets streaming distance thresholds |
| `configure_level_bounds` | `NebulaForgeBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Sets level bounds for streaming/culling |
| **World Partition** | | | |
| `enable_world_partition` | `NebulaForgeBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Enables World Partition on level |
| `configure_grid_size` | `NebulaForgeBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Sets World Partition grid cell size |
| `create_data_layer` | `NebulaForgeBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Creates UDataLayerAsset |
| `assign_actor_to_data_layer` | `NebulaForgeBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Assigns actor to data layer |
| `configure_hlod_layer` | `NebulaForgeBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Configures HLOD layer settings |
| `create_minimap_volume` | `NebulaForgeBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Creates minimap bounds volume |
| **Level Blueprint** | | | |
| `open_level_blueprint` | `NebulaForgeBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Opens Level Blueprint in editor |
| `add_level_blueprint_node` | `NebulaForgeBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Adds node to Level Blueprint |
| `connect_level_blueprint_nodes` | `NebulaForgeBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Connects pins between nodes |
| **Level Instances** | | | |
| `create_level_instance` | `NebulaForgeBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Creates ALevelInstance actor |
| `create_packed_level_actor` | `NebulaForgeBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Creates APackedLevelActor |
| **Utility** | | | |
| `get_level_structure_info` | `NebulaForgeBridge_LevelStructureHandlers.cpp` | `HandleManageLevelStructureAction` | Returns level structure information |

## Volumes Manager (`manage_level_structure`) - Phase 24

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Trigger Volumes** | | | |
| `create_trigger_volume` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ATriggerVolume |
| `create_trigger_box` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ATriggerBox |
| `create_trigger_sphere` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ATriggerSphere |
| `create_trigger_capsule` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ATriggerCapsule |
| **Gameplay Volumes** | | | |
| `create_blocking_volume` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ABlockingVolume |
| `create_kill_z_volume` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates AKillZVolume |
| `create_pain_causing_volume` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates APainCausingVolume with damage settings |
| `create_physics_volume` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates APhysicsVolume with gravity/friction |
| **Audio Volumes** | | | |
| `create_audio_volume` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates AAudioVolume |
| `create_reverb_volume` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates AAudioVolume with reverb settings |
| **Rendering Volumes** | | | |
| `create_cull_distance_volume` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ACullDistanceVolume |
| `create_precomputed_visibility_volume` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates APrecomputedVisibilityVolume |
| `create_lightmass_importance_volume` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ALightmassImportanceVolume |
| **Navigation Volumes** | | | |
| `create_nav_mesh_bounds_volume` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ANavMeshBoundsVolume |
| `create_nav_modifier_volume` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ANavModifierVolume |
| `create_camera_blocking_volume` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Creates ACameraBlockingVolume |
| **Configuration** | | | |
| `set_volume_extent` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Sets volume brush extent (X, Y, Z) |
| `set_volume_properties` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Sets volume-specific properties |
| **Utility** | | | |
| `get_volumes_info` | `NebulaForgeBridge_VolumeHandlers.cpp` | `HandleManageVolumesAction` | Returns volume information |

## Navigation Manager (`manage_ai`) - Phase 25

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **NavMesh Configuration** | | | |
| `configure_nav_mesh_settings` | `NebulaForgeBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Sets TileSizeUU, MinRegionArea, NavMeshResolutionParams (UE 5.7+) |
| `set_nav_agent_properties` | `NebulaForgeBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Sets AgentRadius, AgentHeight, AgentMaxSlope |
| `rebuild_navigation` | `NebulaForgeBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Triggers NavSys->Build() |
| **Nav Modifiers** | | | |
| `create_nav_modifier_component` | `NebulaForgeBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Creates UNavModifierComponent via SCS |
| `set_nav_area_class` | `NebulaForgeBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Sets area class on modifier component |
| `configure_nav_area_cost` | `NebulaForgeBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Configures DefaultCost on area CDO |
| **Nav Links** | | | |
| `create_nav_link_proxy` | `NebulaForgeBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Spawns ANavLinkProxy with FNavigationLink |
| `configure_nav_link` | `NebulaForgeBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Updates link start/end, direction, snap radius |
| `set_nav_link_type` | `NebulaForgeBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Toggles bSmartLinkIsRelevant (simple/smart) |
| `create_smart_link` | `NebulaForgeBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Spawns NavLinkProxy with smart link enabled |
| `configure_smart_link_behavior` | `NebulaForgeBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Configures UNavLinkCustomComponent settings |
| **Utility** | | | |
| `get_navigation_info` | `NebulaForgeBridge_NavigationHandlers.cpp` | `HandleManageNavigationAction` | Returns NavMesh stats, agent properties, link counts |

## Splines Manager (`build_environment`) - Phase 26

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Spline Creation** | | | |
| `create_spline_actor` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Creates ASplineActor with USplineComponent |
| `add_spline_point` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Adds point at index with position/tangent |
| `remove_spline_point` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Removes point at specified index |
| `set_spline_point_position` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets point location in world/local space |
| `set_spline_point_tangents` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets arrive/leave tangents |
| `set_spline_point_rotation` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets point rotation |
| `set_spline_point_scale` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets point scale |
| `set_spline_type` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets type (linear, curve, constant, clamped_curve) |
| **Spline Mesh** | | | |
| `create_spline_mesh_component` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Creates USplineMeshComponent on actor |
| `set_spline_mesh_asset` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets static mesh asset on spline mesh |
| `configure_spline_mesh_axis` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets forward axis (X, Y, Z) |
| `set_spline_mesh_material` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets material on spline mesh |
| **Mesh Scattering** | | | |
| `scatter_meshes_along_spline` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Spawns mesh instances along spline |
| `configure_mesh_spacing` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets spacing mode (distance, count) |
| `configure_mesh_randomization` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Sets random offset, rotation, scale |
| **Quick Templates** | | | |
| `create_road_spline` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Creates road with configurable width, lanes |
| `create_river_spline` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Creates river with water material |
| `create_fence_spline` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Creates fence with posts and rails |
| `create_wall_spline` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Creates wall with height and thickness |
| `create_cable_spline` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Creates hanging cable with sag |
| `create_pipe_spline` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Creates pipe with radius and segments |
| **Utility** | | | |
| `get_splines_info` | `NebulaForgeBridge_SplineHandlers.cpp` | `HandleManageSplinesAction` | Returns spline info (points, length, closed) |

## PCG Manager (`manage_pcg`) - Phase 27

| Action | C++ Handler File | C++ Function | Notes |
| :--- | :--- | :--- | :--- |
| **Graph Assets** | | | |
| `create_pcg_graph` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Creates or reuses a PCG graph asset |
| `create_pcg_subgraph` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Creates a PCG subgraph and can insert a subgraph node in a parent graph |
| **Graph Editing** | | | |
| `add_pcg_node` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds a PCG settings node by class name or alias |
| `connect_pcg_pins` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Connects PCG node pins by label, defaulting to actual source output and target input pins (for graph I/O nodes: `In` -> `Out`) |
| `set_pcg_node_settings` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Applies reflected PCG settings properties and node metadata |
| **Input Nodes** | | | |
| `add_landscape_data_node` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGGetLandscapeSettings` |
| `add_spline_data_node` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGGetSplineSettings` |
| `add_volume_data_node` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGGetVolumeSettings` |
| `add_actor_data_node` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGDataFromActorSettings` |
| `add_texture_data_node` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGTextureSamplerSettings`; `texturePath` maps to `Texture` |
| **Point Operations** | | | |
| `add_surface_sampler` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGSurfaceSamplerSettings` |
| `add_mesh_sampler` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGPointFromMeshSettings`; `meshPath` maps to `Mesh` |
| `add_spline_sampler` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGSplineSamplerSettings` |
| `add_volume_sampler` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGVolumeSamplerSettings` |
| `add_bounds_modifier` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGBoundsModifierSettings` |
| `add_density_filter` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGDensityFilterSettings` |
| `add_height_filter` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGAttributeFilteringRangeSettings` for reflected height-range configuration |
| `add_slope_filter` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGNormalToDensitySettings` |
| `add_distance_filter` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGDistanceSettings` |
| `add_bounds_filter` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGCullPointsOutsideActorBoundsSettings` |
| `add_self_pruning` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGSelfPruningSettings` |
| `add_transform_points` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGTransformPointsSettings` |
| `add_project_to_surface` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGProjectionSettings` |
| `add_copy_points` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGCopyPointsSettings` |
| `add_merge_points` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGMergeSettings` |
| **Spawning** | | | |
| `add_static_mesh_spawner` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGStaticMeshSpawnerSettings` |
| `add_actor_spawner` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGSpawnActorSettings`; `actorClass`/`classPath` maps to `TemplateActorClass` |
| `add_spline_spawner` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Adds `PCGSpawnSplineSettings` |
| **Execution** | | | |
| `execute_pcg_graph` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Assigns a graph to a resolved/created `UPCGComponent` and starts local generation |
| `set_pcg_partition_grid_size` | `NebulaForgeBridge_PCGHandlers.cpp` | `HandleManagePCGAction` | Updates world-scoped `APCGWorldActor::PartitionGridSize` or component-scoped generation grid size |
