#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpConsolidatedActions
{
inline void AppendUniqueActions(TArray<FString>& Target, const TArray<FString>& Source)
{
	for (const FString& Action : Source)
	{
		Target.AddUnique(Action);
	}
}

inline FString GetPayloadSubAction(const TSharedPtr<FJsonObject>& Payload)
{
	FString SubAction;
	if (Payload.IsValid())
	{
		if (!Payload->TryGetStringField(TEXT("subAction"), SubAction) || SubAction.IsEmpty())
		{
			Payload->TryGetStringField(TEXT("action"), SubAction);
		}
	}
	SubAction = SubAction.ToLower();
	SubAction.ReplaceInline(TEXT("-"), TEXT("_"));
	SubAction.ReplaceInline(TEXT(" "), TEXT("_"));
	return SubAction;
}

inline TSharedPtr<FJsonObject> WithPayloadSubAction(const TSharedPtr<FJsonObject>& Payload, const FString& SubAction)
{
	if (!Payload.IsValid() || SubAction.IsEmpty())
	{
		return Payload;
	}

	TSharedPtr<FJsonObject> RoutedPayload = MakeShared<FJsonObject>();
	RoutedPayload->Values = Payload->Values;
	RoutedPayload->SetStringField(TEXT("action"), SubAction);
	RoutedPayload->SetStringField(TEXT("subAction"), SubAction);
	return RoutedPayload;
}

inline bool ContainsAction(const TArray<FString>& Actions, const FString& Action)
{
	return Actions.Contains(Action);
}

inline const TArray<FString>& ManageAssetCore()
{
	static const TArray<FString> Actions = {
		TEXT("list"), TEXT("import"), TEXT("duplicate"), TEXT("duplicate_asset"),
		TEXT("rename"), TEXT("rename_asset"), TEXT("move"), TEXT("move_asset"),
		TEXT("delete"), TEXT("delete_asset"), TEXT("delete_assets"),
		TEXT("create_folder"), TEXT("search_assets"), TEXT("get_dependencies"),
		TEXT("get_source_control_state"), TEXT("analyze_graph"),
        TEXT("get_asset_graph"), TEXT("reference_viewer"), TEXT("audit_assets"), TEXT("size_map_analysis"), TEXT("create_thumbnail"), TEXT("set_tags"),
		TEXT("get_metadata"), TEXT("set_metadata"), TEXT("validate"),
		TEXT("fixup_redirectors"), TEXT("find_by_tag"), TEXT("verify_asset_persistence"), TEXT("generate_report"),
		TEXT("inspect_asset_capabilities"),
		TEXT("set_view_settings"), TEXT("navigate_to_path"),
		TEXT("sync_to_asset"), TEXT("sync_to_folder"),
		TEXT("create_collection"), TEXT("add_to_collection"),
		TEXT("set_asset_color"), TEXT("show_in_explorer"),
		TEXT("set_search_text"),
		TEXT("create_material"), TEXT("create_material_instance"),
		TEXT("create_render_target"), TEXT("create_data_asset"), TEXT("create_primary_data_asset"),
		TEXT("get_data_asset_properties"), TEXT("set_data_asset_properties"),
		TEXT("list_primary_assets"), TEXT("get_primary_asset"),
        TEXT("create_media_player"), TEXT("create_media_source"), TEXT("create_media_texture"), TEXT("create_media_playlist"), TEXT("create_sprite"), TEXT("create_flipbook"), TEXT("create_tile_set"), TEXT("inspect_tile_set"), TEXT("configure_tile_set"), TEXT("get_tile_set_tile_uv"), TEXT("configure_tile_set_tile_metadata"), TEXT("inspect_tile_set_tile_metadata"), TEXT("get_tile_set_tile_xy"), TEXT("get_tile_set_uv_from_xy"), TEXT("create_tile_map"), TEXT("inspect_tile_map"), TEXT("configure_tile_map_visuals"), TEXT("configure_tile_map_layer"), TEXT("inspect_tile_map_layer"), TEXT("set_tile_map_cell"), TEXT("get_tile_map_cell"), TEXT("fill_tile_map_region"), TEXT("get_tile_map_tile_geometry"), TEXT("get_tile_map_tile_coordinates"), TEXT("resize_tile_map"), TEXT("set_sprite_pivot"), TEXT("inspect_sprite"), TEXT("configure_sprite_source"), TEXT("configure_tile_map_collision"), TEXT("add_flipbook_keyframe"), TEXT("set_flipbook_framerate"),
		TEXT("create_data_table"), TEXT("create_tag_table"), TEXT("add_data_table_row"), TEXT("modify_data_table_row"), TEXT("delete_data_table_row"), TEXT("get_data_table_rows"), TEXT("import_data_table_csv"), TEXT("export_data_table_csv"),
		TEXT("create_curve_table"), TEXT("create_curve_float"), TEXT("create_curve_linear_color"), TEXT("replace_curve_keys"), TEXT("add_curve_table_row"), TEXT("get_curve_table_rows"),
		TEXT("import_curve_table_csv"), TEXT("export_curve_table_csv"), TEXT("generate_lods"),
		TEXT("add_material_parameter"), TEXT("list_instances"),
		TEXT("reset_instance_parameters"), TEXT("exists"),
		TEXT("get_material_stats"), TEXT("nanite_rebuild_mesh"),
		TEXT("bulk_rename"), TEXT("bulk_delete"),
		TEXT("source_control_checkout"), TEXT("source_control_submit"),
		TEXT("add_material_node"), TEXT("connect_material_pins"),
		TEXT("remove_material_node"), TEXT("break_material_connections"),
		TEXT("get_material_node_details"), TEXT("rebuild_material"),
		TEXT("create_physical_material"), TEXT("set_friction"), TEXT("set_restitution"),
		TEXT("set_density"), TEXT("configure_surface_type"), TEXT("assign_physical_material"),
		TEXT("configure_physical_material"), TEXT("get_physical_material"),
		TEXT("clear_physical_material_override"), TEXT("configure_sprite_collision"), TEXT("inspect_sprite_sockets"), TEXT("get_sprite_socket_transform"), TEXT("remove_sprite_socket"), TEXT("validate_sprite_socket_names"), TEXT("find_sprite_texture_bounding_box"), TEXT("inspect_flipbook"), TEXT("get_flipbook_socket_transform"), TEXT("get_flipbook_sprite_at_time"), TEXT("get_flipbook_sprite_at_frame")
	};
	return Actions;
}

inline const TArray<FString>& MaterialAuthoring()
{
	static const TArray<FString> Actions = {
		TEXT("create_material"), TEXT("set_blend_mode"),
		TEXT("set_shading_model"), TEXT("set_material_domain"),
		TEXT("add_texture_sample"), TEXT("add_texture_coordinate"),
		TEXT("add_scalar_parameter"), TEXT("add_vector_parameter"),
		TEXT("add_static_switch_parameter"), TEXT("add_math_node"),
		TEXT("add_world_position"), TEXT("add_vertex_normal"),
		TEXT("add_pixel_depth"), TEXT("add_fresnel"),
		TEXT("add_reflection_vector"), TEXT("add_panner"), TEXT("add_rotator"),
		TEXT("add_noise"), TEXT("add_voronoi"), TEXT("add_if"),
		TEXT("add_switch"), TEXT("add_custom_expression"),
		TEXT("connect_nodes"), TEXT("connect_material_pins"),
		TEXT("disconnect_nodes"), TEXT("break_material_connections"),
		TEXT("create_material_function"), TEXT("add_function_input"),
		TEXT("add_function_output"), TEXT("use_material_function"),
		TEXT("get_material_function_info"), TEXT("create_material_instance"),
		TEXT("set_scalar_parameter_value"), TEXT("set_vector_parameter_value"),
		TEXT("set_texture_parameter_value"), TEXT("create_landscape_material"),
		TEXT("create_decal_material"), TEXT("create_post_process_material"),
		TEXT("add_landscape_layer"), TEXT("configure_layer_blend"),
		TEXT("compile_material"), TEXT("get_material_info"), TEXT("find_node"),
		TEXT("get_node_connections"), TEXT("get_node_properties"),
		TEXT("set_static_switch_parameter_value"), TEXT("delete_node"),
		TEXT("update_custom_expression"), TEXT("get_node_chain"),
		TEXT("get_connected_subgraph"), TEXT("add_material_node"),
		TEXT("rebuild_material"), TEXT("set_material_parameter"),
		TEXT("get_material_node_details"), TEXT("remove_material_node"),
		TEXT("set_two_sided")
	};
	return Actions;
}

inline const TArray<FString>& Texture()
{
	static const TArray<FString> Actions = {
		TEXT("create_noise_texture"), TEXT("create_gradient_texture"),
		TEXT("create_pattern_texture"), TEXT("create_normal_from_height"),
		TEXT("create_ao_from_mesh"), TEXT("resize_texture"),
		TEXT("adjust_levels"), TEXT("adjust_curves"), TEXT("blur"),
		TEXT("sharpen"), TEXT("invert"), TEXT("desaturate"),
		TEXT("channel_pack"), TEXT("channel_extract"), TEXT("combine_textures"),
		TEXT("set_compression_settings"), TEXT("set_texture_group"),
		TEXT("set_lod_bias"), TEXT("configure_virtual_texture"),
		TEXT("set_streaming_priority"), TEXT("get_texture_info")
	};
	return Actions;
}

inline TArray<FString> ManageAsset()
{
	TArray<FString> Actions = ManageAssetCore();
	AppendUniqueActions(Actions, MaterialAuthoring());
	AppendUniqueActions(Actions, Texture());
	return Actions;
}

inline const TArray<FString>& ManageBlueprintCore()
{
	static const TArray<FString> Actions = {
		TEXT("create"), TEXT("create_blueprint"), TEXT("get_blueprint"),
		TEXT("get"), TEXT("compile"), TEXT("add_component"),
		TEXT("set_default"), TEXT("modify_scs"), TEXT("get_scs"),
		TEXT("add_scs_component"),
		TEXT("remove_scs_component"), TEXT("reparent_scs_component"),
		TEXT("set_scs_transform"), TEXT("set_scs_property"),
		TEXT("ensure_exists"), TEXT("probe_handle"), TEXT("add_variable"),
		TEXT("remove_variable"), TEXT("rename_variable"), TEXT("add_function"),
		TEXT("add_event"), TEXT("remove_event"),
		TEXT("add_construction_script"), TEXT("set_variable_metadata"),
		TEXT("set_metadata"), TEXT("create_node"), TEXT("add_node"),
		TEXT("delete_node"), TEXT("connect_pins"), TEXT("break_pin_links"),
		TEXT("set_node_property"), TEXT("create_reroute_node"),
		TEXT("get_node_details"), TEXT("get_graph_details"),
		TEXT("get_pin_details"), TEXT("list_node_types"),
		TEXT("set_pin_default_value"),
		// UE 5.8 Blueprint graph authoring.  These remain aliases of the
		// established graph operations so existing MCP calls stay valid.
		TEXT("create_event_graph"), TEXT("find_event_graph"),
		TEXT("create_function_graph"), TEXT("find_function_graph"),
		TEXT("add_begin_play"), TEXT("add_tick"), TEXT("add_input_event"),
		TEXT("add_custom_event"), TEXT("add_variable_get"),
		TEXT("add_variable_set"), TEXT("add_function_call"),
		TEXT("add_branch"), TEXT("add_sequence"), TEXT("add_cast"),
		TEXT("add_arithmetic"), TEXT("add_component_reference"),
		TEXT("add_self_reference"), TEXT("disconnect_pins"),
		TEXT("inspect_graph"), TEXT("get_nodes"), TEXT("get_connections"),
		TEXT("register_mapping_context_begin_play"), TEXT("add_enhanced_input_event"),
		TEXT("bind_input_action_event"), TEXT("inspect_input_bindings")
	};
	return Actions;
}

inline const TArray<FString>& WidgetAuthoring()
{
	static const TArray<FString> Actions = {
		TEXT("create_widget_blueprint"), TEXT("set_widget_parent_class"),
		TEXT("add_canvas_panel"), TEXT("add_horizontal_box"),
		TEXT("add_vertical_box"), TEXT("add_overlay"), TEXT("add_grid_panel"),
		TEXT("add_uniform_grid"), TEXT("add_wrap_box"), TEXT("add_scroll_box"),
		TEXT("add_size_box"), TEXT("add_scale_box"), TEXT("add_border"),
		TEXT("add_text_block"), TEXT("add_rich_text_block"), TEXT("add_image"),
		TEXT("add_button"), TEXT("add_check_box"), TEXT("add_slider"),
		TEXT("add_progress_bar"), TEXT("add_text_input"),
		TEXT("add_combo_box"), TEXT("add_spin_box"), TEXT("add_list_view"),
		TEXT("add_tree_view"), TEXT("set_anchor"), TEXT("set_alignment"),
		TEXT("set_position"), TEXT("set_size"), TEXT("set_padding"),
		TEXT("set_z_order"), TEXT("set_render_transform"),
		TEXT("set_visibility"), TEXT("set_style"), TEXT("set_clipping"),
		TEXT("create_property_binding"), TEXT("bind_text"),
		TEXT("bind_visibility"), TEXT("bind_color"), TEXT("bind_enabled"),
		TEXT("bind_on_clicked"), TEXT("bind_on_hovered"),
		TEXT("bind_on_value_changed"), TEXT("create_widget_animation"),
		TEXT("add_animation_track"), TEXT("add_animation_keyframe"),
		TEXT("set_animation_loop"), TEXT("create_main_menu"),
		TEXT("create_pause_menu"), TEXT("create_settings_menu"),
		TEXT("create_loading_screen"), TEXT("create_hud_widget"),
		TEXT("add_health_bar"), TEXT("add_ammo_counter"),
		TEXT("add_minimap"), TEXT("add_crosshair"), TEXT("add_compass"),
		TEXT("add_interaction_prompt"), TEXT("add_objective_tracker"),
		TEXT("add_damage_indicator"), TEXT("create_inventory_ui"),
		TEXT("create_dialog_widget"), TEXT("create_radial_menu"),
		TEXT("get_widget_info"), TEXT("preview_widget")
	};
	return Actions;
}

inline TArray<FString> ManageBlueprint()
{
	TArray<FString> Actions = ManageBlueprintCore();
	AppendUniqueActions(Actions, WidgetAuthoring());
	return Actions;
}

inline const TArray<FString>& BuildEnvironmentCore()
{
	static const TArray<FString> Actions = {
		TEXT("create_landscape"), TEXT("sculpt"), TEXT("sculpt_landscape"),
		TEXT("add_foliage"), TEXT("paint_foliage"),
		TEXT("create_procedural_terrain"),
		TEXT("create_procedural_foliage"),
		TEXT("add_foliage_instances"), TEXT("get_foliage_instances"),
		TEXT("remove_foliage"), TEXT("paint_landscape"),
		TEXT("paint_landscape_layer"), TEXT("modify_heightmap"),
		TEXT("create_landscape_edit_layer"), TEXT("list_landscape_edit_layers"),
		TEXT("remove_landscape_edit_layer"),
		TEXT("verify_landscape_edit_layers"),
		TEXT("set_landscape_material"), TEXT("create_landscape_grass_type"),
		TEXT("inspect_landscape"), TEXT("delete_landscape"), TEXT("resize_landscape"),
		TEXT("generate_landscape_heightmap"), TEXT("apply_landscape_erosion"),
		TEXT("sculpt_landscape_region"), TEXT("paint_landscape_by_rule"),
		TEXT("create_landscape_material"), TEXT("configure_landscape_layer_blend"),
		TEXT("scatter_landscape_foliage"), TEXT("inspect_generated_foliage"),
		TEXT("regenerate_generated_foliage"), TEXT("clear_generated_foliage"),
		TEXT("inspect_world_building_capabilities"),
		TEXT("generate_lods"), TEXT("bake_lightmap"),
		TEXT("export_snapshot"), TEXT("import_snapshot"), TEXT("delete"),
		TEXT("create_sky_sphere"), TEXT("set_time_of_day"),
		TEXT("create_fog_volume"),
		TEXT("generate_procedural_building"), TEXT("generate_city_block"),
		TEXT("inspect_procedural_building"), TEXT("regenerate_procedural_building"),
		TEXT("save_procedural_building_blueprint"),
		TEXT("import_heightmap"), TEXT("export_heightmap"),
		TEXT("create_landscape_layer_info"),
		TEXT("configure_landscape_material"),
		TEXT("configure_landscape_splines"), TEXT("configure_landscape_lod"),
		TEXT("create_landscape_streaming_proxy"),
		TEXT("create_foliage_type"), TEXT("configure_foliage_mesh"),
		TEXT("configure_foliage_placement"), TEXT("configure_foliage_lod"),
		TEXT("configure_foliage_collision"), TEXT("configure_foliage_culling"),
		TEXT("paint_foliage_instances"), TEXT("remove_foliage_instances"),
		TEXT("configure_sky_atmosphere"), TEXT("configure_sky_light"),
		TEXT("configure_directional_light_atmosphere"),
		TEXT("configure_exponential_height_fog"),
		TEXT("configure_volumetric_cloud"), TEXT("create_weather_system"),
		TEXT("configure_rain_particles"), TEXT("configure_snow_particles"),
		TEXT("configure_wind"), TEXT("configure_lightning"),
		TEXT("create_time_of_day_system"), TEXT("configure_sun_position"),
		TEXT("configure_light_color_curve"), TEXT("configure_sky_color_curve"),
		TEXT("create_water_body_ocean"), TEXT("create_water_body_lake"),
		TEXT("create_water_body_river"), TEXT("create_water_body_custom"),
		TEXT("configure_water_waves"), TEXT("configure_water_material"),
		TEXT("configure_water_collision"), TEXT("create_buoyancy_component")
	};
	return Actions;
}

inline const TArray<FString>& Lighting()
{
	static const TArray<FString> Actions = {
		TEXT("spawn_light"), TEXT("create_light"), TEXT("spawn_sky_light"),
		TEXT("create_sky_light"), TEXT("ensure_single_sky_light"),
		TEXT("create_lightmass_volume"),
		TEXT("create_lighting_enabled_level"), TEXT("create_dynamic_light"),
		TEXT("setup_global_illumination"), TEXT("configure_shadows"),
		TEXT("set_exposure"), TEXT("set_ambient_occlusion"),
		TEXT("setup_volumetric_fog"), TEXT("build_lighting"),
		TEXT("list_light_types"),
		// Phase 29.1: Advanced ray-tracing configuration.
		TEXT("configure_ray_traced_shadows"), TEXT("configure_ray_traced_gi"),
		TEXT("configure_ray_traced_reflections"), TEXT("configure_ray_traced_ao"),
		TEXT("configure_path_tracing"), TEXT("configure_ray_traced_translucency"),
		TEXT("configure_ray_tracing_quality"),
		// Phase 29.2: Light channel assignment and readback.
		TEXT("set_light_channel"), TEXT("set_actor_light_channel"),
		TEXT("get_light_channels"),
		// Phase 29.3: Lightmass and precomputed lighting.
		TEXT("configure_lightmass_settings"), TEXT("build_lighting_quality"),
		TEXT("configure_indirect_lighting_cache"), TEXT("configure_volumetric_lightmaps"),
		TEXT("configure_lightmass_ambient_occlusion"), TEXT("inspect_lightmass_settings"),
		// Phase 29.4: Reflection captures and dynamic reflections.
		TEXT("create_sphere_reflection_capture"), TEXT("create_box_reflection_capture"),
		TEXT("configure_capture_resolution"), TEXT("configure_capture_offset"),
		TEXT("recapture_scene"), TEXT("create_planar_reflection"),
		TEXT("configure_planar_reflection"), TEXT("configure_ssr_settings"),
		TEXT("configure_lumen_reflection_settings"), TEXT("inspect_reflection_captures"),
		// Phase 29.5: Post Process Volume and FPostProcessSettings.
		TEXT("create_post_process_volume"), TEXT("configure_pp_blend"),
		TEXT("set_pp_white_balance"), TEXT("set_pp_color_grading"), TEXT("set_pp_lut"),
		TEXT("configure_tonemapper"), TEXT("set_tonemapper_type"),
		TEXT("configure_bloom"), TEXT("set_bloom_intensity"), TEXT("set_bloom_threshold"),
		TEXT("configure_lens_flare"), TEXT("configure_dof"), TEXT("set_dof_method"),
		TEXT("set_focal_distance"), TEXT("set_aperture"), TEXT("configure_bokeh"),
		TEXT("configure_motion_blur"), TEXT("set_motion_blur_amount"), TEXT("set_motion_blur_max"),
		TEXT("configure_exposure"), TEXT("set_exposure_method"), TEXT("set_exposure_compensation"),
		TEXT("set_exposure_min_max"), TEXT("configure_ssao"), TEXT("configure_gtao"),
		TEXT("configure_vignette"), TEXT("configure_chromatic_aberration"), TEXT("configure_grain"),
		TEXT("configure_screen_percentage"), TEXT("inspect_post_process_volume"),
		// Phase 29.6: Scene Capture 2D/cube and render-target workflows.
		TEXT("create_scene_capture_2d"), TEXT("create_scene_capture_cube"), TEXT("create_render_target_cube"),
		TEXT("configure_scene_capture"), TEXT("configure_scene_capture_resolution"),
		TEXT("configure_capture_source"), TEXT("assign_render_target"), TEXT("capture_scene"),
		TEXT("inspect_scene_captures")
	};
	return Actions;
}

inline const TArray<FString>& Splines()
{
    static const TArray<FString> Actions = {
        TEXT("create_spline_actor"), TEXT("add_spline_point"),
        TEXT("insert_spline_point"), TEXT("update_spline_point"),
        TEXT("remove_spline_point"), TEXT("set_spline_point_position"),
        TEXT("set_spline_point_tangents"), TEXT("set_spline_point_rotation"),
        TEXT("set_spline_point_scale"), TEXT("set_spline_point_roll"), TEXT("set_spline_type"),
        TEXT("create_spline_mesh_component"), TEXT("set_spline_mesh_asset"),
        TEXT("configure_spline_mesh_axis"),
        TEXT("set_spline_mesh_material"),
        TEXT("generate_spline_mesh_segments"), TEXT("rebuild_spline_mesh_segments"),
        TEXT("clear_generated_spline_segments"),
        TEXT("scatter_meshes_along_spline"),
		TEXT("configure_mesh_spacing"),
		TEXT("configure_mesh_randomization"), TEXT("create_road_spline"),
		TEXT("create_river_spline"), TEXT("create_fence_spline"),
		TEXT("create_wall_spline"), TEXT("create_cable_spline"),
        TEXT("create_pipe_spline"), TEXT("create_path_spline"), TEXT("conform_spline_to_landscape"),
        TEXT("find_spline_actors"),
        TEXT("find_spline_components"), TEXT("inspect_spline_points"), TEXT("get_splines_info")
	};
	return Actions;
}

inline TArray<FString> BuildEnvironment()
{
	TArray<FString> Actions = BuildEnvironmentCore();
	AppendUniqueActions(Actions, Lighting());
	AppendUniqueActions(Actions, Splines());
	return Actions;
}

inline const TArray<FString>& PCG()
{
 static const TArray<FString> Actions = {
		TEXT("create_pcg_graph"), TEXT("create_pcg_subgraph"),
		TEXT("add_pcg_node"), TEXT("connect_pcg_pins"),
		TEXT("set_pcg_node_settings"),
		TEXT("search_static_mesh_assets"), TEXT("validate_static_mesh_assets"),
		TEXT("find_static_mesh_spawner"), TEXT("configure_static_mesh_spawner"),
		TEXT("add_static_mesh_entry"), TEXT("update_static_mesh_entry"),
		TEXT("remove_static_mesh_entry"), TEXT("inspect_static_mesh_spawner"),
		TEXT("add_landscape_data_node"), TEXT("add_spline_data_node"),
		TEXT("add_volume_data_node"), TEXT("add_actor_data_node"),
		TEXT("add_texture_data_node"), TEXT("add_surface_sampler"),
		TEXT("add_mesh_sampler"), TEXT("add_spline_sampler"),
		TEXT("add_volume_sampler"), TEXT("add_bounds_modifier"),
		TEXT("add_density_filter"), TEXT("add_height_filter"),
		TEXT("add_slope_filter"), TEXT("add_distance_filter"),
		TEXT("add_bounds_filter"), TEXT("add_self_pruning"),
		TEXT("add_transform_points"), TEXT("add_project_to_surface"),
		TEXT("add_copy_points"), TEXT("add_merge_points"),
		TEXT("add_static_mesh_spawner"), TEXT("add_actor_spawner"),
		TEXT("add_spline_spawner"), TEXT("execute_pcg_graph"),
		TEXT("regenerate_pcg_component"), TEXT("read_pcg_generated_instances"),
		TEXT("clear_pcg_generated_output"),
		TEXT("set_pcg_partition_grid_size")
	};
	return Actions;
}

inline const TArray<FString>& AnimationPhysicsCore()
{
	static const TArray<FString> Actions = {
		TEXT("create_animation_blueprint"), TEXT("create_animation_bp"),
		TEXT("create_anim_blueprint"), TEXT("create_blend_space"),
		TEXT("create_blend_space_1d"), TEXT("create_blend_space_2d"),
		TEXT("create_blend_tree"), TEXT("create_procedural_anim"),
		TEXT("create_aim_offset"), TEXT("add_aim_offset_sample"),
		TEXT("create_state_machine"), TEXT("add_state_machine"),
		TEXT("add_state"), TEXT("add_transition"),
		TEXT("set_transition_rules"), TEXT("add_blend_node"),
		TEXT("add_cached_pose"), TEXT("add_slot_node"),
		TEXT("create_control_rig"), TEXT("create_ik_rig"),
		TEXT("setup_ik"), TEXT("create_pose_library"),
		TEXT("create_animation_asset"), TEXT("create_animation_sequence"),
		TEXT("set_sequence_length"), TEXT("add_bone_track"),
		TEXT("set_bone_key"), TEXT("set_curve_key"), TEXT("create_montage"),
		TEXT("add_montage_section"), TEXT("add_montage_slot"),
		TEXT("set_section_timing"), TEXT("add_montage_notify"),
		TEXT("set_blend_in"), TEXT("set_blend_out"), TEXT("link_sections"),
		TEXT("add_notify"), TEXT("play_montage"),
		TEXT("play_anim_montage"), TEXT("setup_ragdoll"),
		TEXT("activate_ragdoll"), TEXT("configure_vehicle"),
		TEXT("setup_physics_simulation"), TEXT("add_blend_sample"),
		TEXT("set_axis_settings"), TEXT("set_interpolation_settings"),
		TEXT("setup_retargeting"), TEXT("cleanup")
	};
	return Actions;
}

inline const TArray<FString>& AnimationAuthoring()
{
	static const TArray<FString> Actions = {
		TEXT("create_animation_sequence"), TEXT("set_sequence_length"),
		TEXT("add_bone_track"), TEXT("set_bone_key"), TEXT("set_curve_key"),
		TEXT("add_notify_state"), TEXT("add_sync_marker"),
		TEXT("set_root_motion_settings"), TEXT("set_additive_settings"),
		TEXT("create_montage"), TEXT("add_montage_section"),
		TEXT("add_montage_slot"), TEXT("set_section_timing"),
		TEXT("add_montage_notify"), TEXT("set_blend_in"),
		TEXT("set_blend_out"), TEXT("link_sections"),
		TEXT("create_blend_space_1d"), TEXT("create_blend_space_2d"),
		TEXT("add_blend_sample"), TEXT("force_rebuild_blend_space"),
		TEXT("set_axis_settings"), TEXT("set_interpolation_settings"),
		TEXT("create_aim_offset"), TEXT("add_aim_offset_sample"),
		TEXT("create_anim_blueprint"), TEXT("create_animation_bp"),
		TEXT("create_animation_blueprint"), TEXT("add_state_machine"),
		TEXT("add_state"), TEXT("add_transition"),
		TEXT("set_transition_rules"), TEXT("add_blend_node"),
		TEXT("add_cached_pose"), TEXT("add_slot_node"),
		TEXT("add_layered_blend_per_bone"),
		TEXT("set_anim_graph_node_value"), TEXT("create_control_rig"),
		TEXT("create_ik_rig"), TEXT("create_ik_retargeter"),
		TEXT("set_retarget_chain_mapping"), TEXT("get_animation_info"),
		TEXT("validate_animation_asset")
	};
	return Actions;
}

inline const TArray<FString>& Skeleton()
{
	static const TArray<FString> Actions = {
		TEXT("create_skeleton"), TEXT("add_bone"), TEXT("remove_bone"),
		TEXT("rename_bone"), TEXT("set_bone_transform"),
		TEXT("set_bone_parent"), TEXT("create_virtual_bone"),
		TEXT("create_socket"), TEXT("configure_socket"),
		TEXT("auto_skin_weights"), TEXT("set_vertex_weights"),
		TEXT("normalize_weights"), TEXT("prune_weights"), TEXT("copy_weights"),
		TEXT("mirror_weights"), TEXT("create_physics_asset"),
		TEXT("add_physics_body"), TEXT("configure_physics_body"),
		TEXT("add_physics_constraint"), TEXT("configure_constraint_limits"),
		TEXT("bind_cloth_to_skeletal_mesh"),
		TEXT("assign_cloth_asset_to_mesh"), TEXT("create_morph_target"),
		TEXT("set_morph_target_deltas"), TEXT("import_morph_targets"),
		TEXT("get_skeleton_info"), TEXT("list_bones"), TEXT("list_sockets"),
		TEXT("list_physics_bodies")
	};
	return Actions;
}

inline TArray<FString> AnimationPhysics()
{
	TArray<FString> Actions = AnimationPhysicsCore();
	AppendUniqueActions(Actions, AnimationAuthoring());
	AppendUniqueActions(Actions, Skeleton());
	return Actions;
}

inline const TArray<FString>& AudioAuthoring()
{
	static const TArray<FString> Actions = {
		TEXT("create_sound_cue"), TEXT("create_sound_class"),
		TEXT("create_sound_mix"),
		TEXT("add_cue_node"), TEXT("connect_cue_nodes"),
		TEXT("set_cue_attenuation"), TEXT("set_cue_concurrency"),
		TEXT("create_metasound"), TEXT("add_metasound_node"),
		TEXT("connect_metasound_nodes"), TEXT("add_metasound_input"),
		TEXT("add_metasound_output"), TEXT("set_metasound_default"),
		TEXT("set_class_properties"), TEXT("set_class_parent"),
		TEXT("add_mix_modifier"), TEXT("configure_mix_eq"),
		TEXT("create_attenuation_settings"),
		TEXT("configure_distance_attenuation"),
		TEXT("configure_spatialization"), TEXT("configure_occlusion"),
		TEXT("configure_reverb_send"), TEXT("create_dialogue_voice"),
		TEXT("create_dialogue_wave"), TEXT("set_dialogue_context"),
		TEXT("create_reverb_effect"), TEXT("create_source_effect_chain"),
		TEXT("add_source_effect"), TEXT("create_submix_effect"),
		TEXT("get_audio_info"), TEXT("validate_audio_asset")
	};
	return Actions;
}

inline const TArray<FString>& SystemControlCore()
{
	static const TArray<FString> Actions = {
		TEXT("profile"), TEXT("show_fps"), TEXT("set_quality"), TEXT("set_quality_level"), TEXT("configure_scalability_group"),
		TEXT("screenshot"), TEXT("set_resolution"), TEXT("set_fullscreen"),
      TEXT("execute_command"), TEXT("console_command"), TEXT("run_ubt"), TEXT("compile_shaders"), TEXT("create_device_profile"), TEXT("set_cvar_for_profile"), TEXT("configure_build_settings"), TEXT("configure_platform_settings"), TEXT("configure_plugin_settings"), TEXT("configure_windows_build"), TEXT("configure_linux_build"), TEXT("configure_mac_build"), TEXT("configure_ios_build"), TEXT("configure_android_build"), TEXT("generate_project_files"),
      TEXT("run_tests"), TEXT("run_uat"), TEXT("validate_release"), TEXT("release_gate"), TEXT("validate_project"), TEXT("create_game_architecture_manifest"), TEXT("add_architecture_requirement"), TEXT("validate_game_architecture"), TEXT("inspect_platform_capabilities"), TEXT("manage_project_plugin"), TEXT("list_plugins"), TEXT("enable_plugin"), TEXT("disable_plugin"), TEXT("get_plugin_status"), TEXT("create_asset_validator"), TEXT("run_data_validation"), TEXT("wait_for_job"), TEXT("get_job_status"), TEXT("list_jobs"), TEXT("cancel_job"),
      TEXT("read_project_file"), TEXT("write_project_file"), TEXT("validate_blueprints"), TEXT("start_memory_report"), TEXT("configure_stat_commands"), TEXT("configure_console_variables"), TEXT("check_for_errors"), TEXT("start_network_profiler"), TEXT("enable_visual_logger"), TEXT("add_visual_log_entry"),
      TEXT("generate_save_game_class"),
      TEXT("cook_content"), TEXT("package_project"), TEXT("configure_chunking"), TEXT("create_pak_file"), TEXT("configure_compression"), TEXT("configure_asset_encryption"), TEXT("create_test_level"), TEXT("configure_test_settings"), TEXT("configure_demo_settings"), TEXT("configure_localization_target"), TEXT("import_localization"), TEXT("export_localization"), TEXT("run_gauntlet_test"), TEXT("create_build_target"),
      TEXT("runtime_health"), TEXT("get_runtime_capabilities"), TEXT("get_runtime_world"), TEXT("get_runtime_actors"), TEXT("get_runtime_actor"),
      TEXT("list_gameplay_tags"), TEXT("get_runtime_gameplay_tag"), TEXT("add_gameplay_tag"), TEXT("remove_gameplay_tag"),
      TEXT("create_gameplay_tag"), TEXT("register_native_tag"),
      TEXT("create_tag_container"), TEXT("add_tag_to_container"), TEXT("remove_tag_from_container"), TEXT("check_tag_match"),
      TEXT("create_string_table"), TEXT("add_string_entry"), TEXT("get_localized_string"), TEXT("set_culture"), TEXT("set_language_and_locale"), TEXT("set_locale"),
      TEXT("read_config_value"), TEXT("write_config_value"), TEXT("get_section"), TEXT("create_config_section"),
      TEXT("list_config_layers"), TEXT("get_config_value"), TEXT("set_config_value"), TEXT("reload_config"), TEXT("flush_config"), TEXT("get_config_hierarchy"),
      TEXT("save_game_to_slot"), TEXT("load_game_from_slot"), TEXT("inspect_save_game_schema"), TEXT("delete_save_game_slot"),
      TEXT("check_save_game_slot"), TEXT("list_save_game_slots"),
      TEXT("subscribe"), TEXT("unsubscribe"),
		TEXT("spawn_category"), TEXT("enable_gameplay_debugger"), TEXT("start_session"), TEXT("stop_session"), TEXT("get_session_status"), TEXT("capture_insights_trace"), TEXT("check_map_errors"), TEXT("create_functional_test"), TEXT("create_automation_test"), TEXT("get_test_results"),
		TEXT("lumen_update_scene"), TEXT("play_sound"), TEXT("create_widget"),
		TEXT("show_widget"), TEXT("add_widget_child"), TEXT("set_cvar"),
		TEXT("get_project_settings"), TEXT("validate_assets"),
		TEXT("set_project_setting"), TEXT("execute_python"), TEXT("execute_python_script"), TEXT("execute_python_string"), TEXT("execute_python_file"), TEXT("configure_python_paths"), TEXT("list_python_packages"), TEXT("create_editor_utility_widget"), TEXT("create_editor_utility_blueprint"),
		TEXT("create_game_instance_subsystem"), TEXT("create_world_subsystem"),
		TEXT("create_local_player_subsystem"), TEXT("create_engine_subsystem"),
		TEXT("configure_subsystem_tick"), TEXT("get_subsystem"),
		TEXT("inspect_subsystem"), TEXT("list_subsystems"),
		TEXT("set_timer"), TEXT("clear_timer"), TEXT("pause_timer"),
		TEXT("resume_timer"), TEXT("get_timer"), TEXT("list_timers"),
		TEXT("create_latent_action"), TEXT("clear_latent_action"),
		TEXT("get_latent_action"), TEXT("list_latent_actions"),
		TEXT("create_async_action"), TEXT("cancel_async_action"),
		TEXT("get_async_action"), TEXT("wait_for_async_action"), TEXT("list_async_actions"),
		TEXT("create_gameplay_task"), TEXT("end_gameplay_task"),
		TEXT("get_gameplay_task"), TEXT("list_gameplay_tasks"),
		TEXT("configure_task_priority"),
		TEXT("create_event_dispatcher"), TEXT("bind_to_event"),
		TEXT("unbind_from_event"), TEXT("broadcast_event"),
		TEXT("create_delegate"), TEXT("bind_delegate"),
		TEXT("inspect_delegate"), TEXT("list_delegate_bindings"),
		TEXT("create_blueprint_interface"), TEXT("add_interface_function"),
		TEXT("implement_interface"), TEXT("get_interface_info"),
		TEXT("call_interface_function")
	};
	return Actions;
}

inline const TArray<FString>& Performance()
{
	static const TArray<FString> Actions = {
		TEXT("start_profiling"), TEXT("stop_profiling"),
		TEXT("run_benchmark"), TEXT("show_fps"), TEXT("show_stats"),
		TEXT("generate_memory_report"), TEXT("set_scalability"),
		TEXT("set_resolution_scale"), TEXT("set_vsync"),
		TEXT("set_frame_rate_limit"), TEXT("enable_gpu_timing"),
		TEXT("configure_texture_streaming"), TEXT("configure_lod"),
		TEXT("apply_baseline_settings"), TEXT("optimize_draw_calls"),
		TEXT("merge_actors"), TEXT("configure_occlusion_culling"),
		TEXT("optimize_shaders"), TEXT("configure_nanite"),
		TEXT("configure_world_partition")
		, TEXT("sign_release"), TEXT("run_packaged"), TEXT("deploy_package"), TEXT("run_network_soak"), TEXT("analyze_trace")
	};
	return Actions;
}

inline TArray<FString> SystemControl()
{
	TArray<FString> Actions = SystemControlCore();
	AppendUniqueActions(Actions, Performance());
	return Actions;
}

inline const TArray<FString>& ManageNetworkingCore()
{
	static const TArray<FString> Actions = {
		TEXT("set_property_replicated"), TEXT("set_replication_condition"),
		TEXT("configure_net_update_frequency"), TEXT("configure_net_priority"),
		TEXT("set_net_dormancy"), TEXT("configure_replication_graph"),
		TEXT("create_rpc_function"), TEXT("configure_rpc_validation"),
		TEXT("set_rpc_reliability"), TEXT("set_owner"),
		TEXT("set_autonomous_proxy"), TEXT("check_has_authority"),
		TEXT("check_is_locally_controlled"),
		TEXT("configure_net_cull_distance"), TEXT("set_always_relevant"),
		TEXT("set_only_relevant_to_owner"),
		TEXT("configure_net_serialization"), TEXT("set_replicated_using"),
		TEXT("configure_push_model"), TEXT("configure_client_prediction"),
		TEXT("configure_server_correction"),
		TEXT("add_network_prediction_data"),
		TEXT("configure_movement_prediction"), TEXT("configure_net_driver"),
		TEXT("set_net_role"), TEXT("configure_replicated_movement"),
		TEXT("get_networking_info")
	};
	return Actions;
}

inline const TArray<FString>& Input()
{
	static const TArray<FString> Actions = {
		TEXT("create_input_action"), TEXT("create_input_mapping_context"),
		TEXT("add_mapping"), TEXT("remove_mapping"), TEXT("map_input_action"),
		TEXT("add_legacy_action_mapping"), TEXT("remove_legacy_action_mapping"),
		TEXT("add_legacy_axis_mapping"), TEXT("remove_legacy_axis_mapping"),
		TEXT("set_input_trigger"), TEXT("set_input_modifier"),
		TEXT("enable_input_mapping"), TEXT("disable_input_action"),
		TEXT("get_input_info"), TEXT("set_input_action_type"),
		TEXT("add_input_mapping"), TEXT("remove_input_mapping"),
		TEXT("add_mapping_modifier"), TEXT("add_mapping_trigger"),
		TEXT("inspect_input_asset")
	};
	return Actions;
}

inline const TArray<FString>& GameFramework()
{
	static const TArray<FString> Actions = {
		TEXT("create_game_mode"), TEXT("create_game_state"),
		TEXT("create_player_controller"), TEXT("create_player_state"),
		TEXT("create_game_instance"), TEXT("create_hud_class"),
		TEXT("set_default_pawn_class"), TEXT("set_player_controller_class"),
		TEXT("set_level_game_mode"),
		TEXT("set_game_state_class"), TEXT("set_player_state_class"),
		TEXT("configure_game_rules"), TEXT("setup_match_states"),
		TEXT("configure_round_system"), TEXT("configure_team_system"),
		TEXT("configure_scoring_system"), TEXT("configure_spawn_system"),
		TEXT("configure_player_start"), TEXT("set_respawn_rules"),
		TEXT("configure_spectating"), TEXT("get_game_framework_info")
	};
	return Actions;
}

inline const TArray<FString>& Sessions()
{
	static const TArray<FString> Actions = {
		TEXT("configure_local_session_settings"),
		TEXT("configure_session_interface"), TEXT("configure_split_screen"),
		TEXT("set_split_screen_type"), TEXT("add_local_player"),
		TEXT("remove_local_player"), TEXT("configure_lan_play"),
		TEXT("host_lan_server"), TEXT("join_lan_server"),
		TEXT("enable_voice_chat"), TEXT("configure_voice_settings"),
		TEXT("set_voice_channel"), TEXT("mute_player"),
		TEXT("set_voice_attenuation"), TEXT("configure_push_to_talk"),
		TEXT("get_sessions_info"), TEXT("get_online_capabilities"),
		TEXT("get_online_session_status"), TEXT("get_online_identity_status"), TEXT("get_online_presence"), TEXT("set_online_presence"), TEXT("get_online_friends"), TEXT("send_online_friend_invite"), TEXT("accept_online_friend_invite"), TEXT("reject_online_friend_invite"), TEXT("delete_online_friend"), TEXT("create_online_session"), TEXT("find_online_sessions"),
		TEXT("join_online_session"), TEXT("destroy_online_session"),
		TEXT("configure_network_conditions")
	};
	return Actions;
}

inline TArray<FString> ManageNetworking()
{
	TArray<FString> Actions = ManageNetworkingCore();
	AppendUniqueActions(Actions, Input());
	AppendUniqueActions(Actions, GameFramework());
	AppendUniqueActions(Actions, Sessions());
	return Actions;
}

inline const TArray<FString>& ManageLevelStructureCore()
{
	static const TArray<FString> Actions = {
		TEXT("create_level"), TEXT("create_sublevel"),
		TEXT("configure_level_streaming"), TEXT("set_streaming_distance"),
		TEXT("configure_level_bounds"), TEXT("enable_world_partition"),
		TEXT("get_wp_cell_status"), TEXT("load_cells"), TEXT("pin_wp_cells"), TEXT("unpin_wp_cells"), TEXT("unload_cells"),
		TEXT("configure_grid_size"), TEXT("create_data_layer"),
		TEXT("assign_actor_to_data_layer"), TEXT("configure_hlod_layer"), TEXT("configure_hlod_transition"),
		TEXT("create_hlod_layer"), TEXT("list_hlod_layers"),
		TEXT("inspect_hlod_layer"), TEXT("assign_hlod_layer"),
		TEXT("remove_hlod_layer"), TEXT("report_missing_hlod_assignments"),
		TEXT("build_hlods"), TEXT("rebuild_hlods"), TEXT("delete_hlod_output"),
		TEXT("get_hlod_build_status"), TEXT("cancel_hlod_build"),
		TEXT("inspect_generated_hlods"), TEXT("validate_hlods"),
		TEXT("create_minimap_volume"), TEXT("open_level_blueprint"),
		TEXT("add_level_blueprint_node"),
		TEXT("connect_level_blueprint_nodes"), TEXT("create_level_instance"),
		TEXT("create_packed_level_actor"), TEXT("prepare_pie_capture"), TEXT("get_level_structure_info")
	};
	return Actions;
}

inline const TArray<FString>& Volumes()
{
	static const TArray<FString> Actions = {
		TEXT("create_trigger_volume"), TEXT("add_trigger_volume"),
		TEXT("create_trigger_box"), TEXT("create_trigger_sphere"),
		TEXT("create_trigger_capsule"), TEXT("create_blocking_volume"),
		TEXT("add_blocking_volume"), TEXT("create_kill_z_volume"),
		TEXT("add_kill_z_volume"), TEXT("create_pain_causing_volume"),
		TEXT("create_physics_volume"), TEXT("add_physics_volume"),
		TEXT("create_audio_volume"), TEXT("create_reverb_volume"),
		TEXT("create_cull_distance_volume"),
		TEXT("add_cull_distance_volume"),
		TEXT("create_precomputed_visibility_volume"),
		TEXT("create_lightmass_importance_volume"),
		TEXT("create_nav_mesh_bounds_volume"),
		TEXT("create_nav_modifier_volume"),
		TEXT("create_camera_blocking_volume"),
		TEXT("create_post_process_volume"),
		TEXT("add_post_process_volume"), TEXT("set_volume_extent"),
		TEXT("set_volume_bounds"), TEXT("set_volume_properties"),
		TEXT("remove_volume"), TEXT("get_volumes_info")
	};
	return Actions;
}

inline TArray<FString> ManageLevelStructure()
{
	TArray<FString> Actions = ManageLevelStructureCore();
	AppendUniqueActions(Actions, Volumes());
	return Actions;
}

inline const TArray<FString>& ManageAICore()
{
	static const TArray<FString> Actions = {
		TEXT("create_ai_controller"), TEXT("assign_behavior_tree"),
		TEXT("assign_blackboard"), TEXT("create_blackboard_asset"),
		TEXT("add_blackboard_key"), TEXT("set_key_instance_synced"),
		TEXT("create_behavior_tree"), TEXT("add_composite_node"),
		TEXT("add_task_node"), TEXT("add_decorator"), TEXT("add_service"),
		TEXT("configure_bt_node"), TEXT("create_eqs_query"),
		TEXT("add_eqs_generator"), TEXT("add_eqs_context"),
		TEXT("add_eqs_test"), TEXT("configure_test_scoring"),
		TEXT("add_ai_perception_component"), TEXT("configure_sight_config"),
		TEXT("configure_hearing_config"),
		TEXT("configure_damage_sense_config"), TEXT("set_perception_team"),
		TEXT("create_state_tree"), TEXT("add_state_tree_state"),
		TEXT("add_state_tree_transition"), TEXT("configure_state_tree_task"),
		TEXT("create_smart_object_definition"),
		TEXT("add_smart_object_slot"), TEXT("configure_slot_behavior"),
		TEXT("add_smart_object_component"),
		TEXT("create_mass_entity_config"), TEXT("configure_mass_entity"),
		TEXT("add_mass_spawner"), TEXT("inspect_ai_capabilities"), TEXT("get_ai_info"),
		TEXT("create_blackboard"), TEXT("setup_perception"),
		TEXT("create_nav_link_proxy"), TEXT("set_focus"), TEXT("clear_focus"),
		TEXT("set_blackboard_value"), TEXT("get_blackboard_value"),
		TEXT("run_behavior_tree"), TEXT("stop_behavior_tree"),
		TEXT("inspect_runtime_ai"), TEXT("query_runtime_ai"), TEXT("debug_runtime_ai"),
		TEXT("run_env_query"), TEXT("run_runtime_eqs"),
		TEXT("spawn_runtime_ai"), TEXT("spawn_ai_runtime"),
		TEXT("start_runtime_behavior_tree"), TEXT("run_behavior_tree_runtime"),
		TEXT("create"), TEXT("add_node"), TEXT("connect_nodes"),
		TEXT("remove_node"), TEXT("break_connections"),
		TEXT("set_node_properties"), TEXT("configure_nav_mesh_settings"),
		TEXT("set_nav_agent_properties"), TEXT("rebuild_navigation"),
		TEXT("create_nav_mesh_bounds"), TEXT("build_navigation"),
		TEXT("query_navigation_path"), TEXT("validate_navigation"),
		TEXT("create_nav_modifier_component"), TEXT("set_nav_area_class"),
		TEXT("configure_nav_area_cost"), TEXT("configure_nav_link"),
		TEXT("set_nav_link_type"), TEXT("create_smart_link"),
		TEXT("configure_smart_link_behavior"), TEXT("get_navigation_info")
	};
	return Actions;
}

inline const TArray<FString>& BehaviorTree()
{
	static const TArray<FString> Actions = {
		TEXT("create"), TEXT("add_node"), TEXT("connect_nodes"),
		TEXT("remove_node"), TEXT("break_connections"),
		TEXT("set_node_properties"), TEXT("add_subnode"),
		TEXT("get_tree")
	};
	return Actions;
}

inline const TArray<FString>& Navigation()
{
	static const TArray<FString> Actions = {
		TEXT("configure_nav_mesh_settings"),
		TEXT("set_nav_agent_properties"), TEXT("rebuild_navigation"),
		TEXT("create_nav_mesh_bounds"), TEXT("build_navigation"),
		TEXT("query_navigation_path"), TEXT("validate_navigation"),
		TEXT("create_nav_modifier_component"), TEXT("set_nav_area_class"),
		TEXT("configure_nav_area_cost"), TEXT("create_nav_link_proxy"),
		TEXT("configure_nav_link"), TEXT("set_nav_link_type"),
		TEXT("create_smart_link"), TEXT("configure_smart_link_behavior"),
		TEXT("get_navigation_info")
	};
	return Actions;
}

inline TArray<FString> ManageAI()
{
	TArray<FString> Actions = ManageAICore();
	AppendUniqueActions(Actions, BehaviorTree());
	AppendUniqueActions(Actions, Navigation());
	return Actions;
}

inline bool IsMaterialAuthoringAction(const FString& Action) { return ContainsAction(MaterialAuthoring(), Action); }
inline bool IsTextureAction(const FString& Action) { return ContainsAction(Texture(), Action); }
inline bool IsWidgetAuthoringAction(const FString& Action) { return ContainsAction(WidgetAuthoring(), Action); }
inline bool IsAnimationAuthoringAction(const FString& Action) { return ContainsAction(AnimationAuthoring(), Action); }
inline bool IsAudioAuthoringAction(const FString& Action) { return ContainsAction(AudioAuthoring(), Action); }
inline bool IsLightingAction(const FString& Action) { return ContainsAction(Lighting(), Action); }
inline bool IsSplineAction(const FString& Action) { return ContainsAction(Splines(), Action); }
inline bool IsSkeletonAction(const FString& Action) { return ContainsAction(Skeleton(), Action); }
inline bool IsPerformanceAction(const FString& Action) { return ContainsAction(Performance(), Action); }
inline bool IsInputAction(const FString& Action) { return ContainsAction(Input(), Action); }
inline bool IsGameFrameworkAction(const FString& Action) { return ContainsAction(GameFramework(), Action); }
inline bool IsSessionAction(const FString& Action) { return ContainsAction(Sessions(), Action); }
inline bool IsVolumeAction(const FString& Action) { return ContainsAction(Volumes(), Action); }
inline bool IsBehaviorTreeAction(const FString& Action) { return ContainsAction(BehaviorTree(), Action); }
inline bool IsNavigationAction(const FString& Action) { return ContainsAction(Navigation(), Action); }
inline bool IsPCGAction(const FString& Action) { return ContainsAction(PCG(), Action); }
}
