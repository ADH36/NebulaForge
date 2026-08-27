/**
 * Centralized action name constants for executeAutomationRequest calls.
 *
 * This file eliminates string literal duplication across handlers,
 * making refactoring safer and providing single source of truth.
 *
 * Categories:
 * - TOOL_ACTIONS: Primary tool/domain names (2nd param to executeAutomationRequest)
 * - ACTOR_ACTIONS: Inner actions for control_actor tool
 * - INPUT_ACTIONS: Inner actions for manage_input tool
 */

// ============================================================================
// PRIMARY TOOL ACTIONS (2nd parameter to executeAutomationRequest)
// ============================================================================

/** Primary tool/domain action names */
export const TOOL_ACTIONS = {
  // ==================== CORE TOOLS ====================
  MANAGE_ASSET: 'manage_asset',
  CONTROL_ACTOR: 'control_actor',
  CONTROL_EDITOR: 'control_editor',
  MANAGE_LEVEL: 'manage_level',
  SYSTEM_CONTROL: 'system_control',
  INSPECT: 'inspect',
  MANAGE_TOOLS: 'manage_tools',

  // ==================== WORLD TOOLS ====================
  BUILD_ENVIRONMENT: 'build_environment',
  MANAGE_LIGHTING: 'manage_lighting',
  MANAGE_VOLUMES: 'manage_volumes',
  MANAGE_NAVIGATION: 'manage_navigation',
  MANAGE_SPLINES: 'manage_splines',
  MANAGE_LEVEL_STRUCTURE: 'manage_level_structure',

  // ==================== AUTHORING TOOLS ====================
  MANAGE_BLUEPRINT: 'manage_blueprint',
  MANAGE_MATERIAL_AUTHORING: 'manage_material_authoring',
  MANAGE_TEXTURE: 'manage_texture',
  MANAGE_GEOMETRY: 'manage_geometry',
  MANAGE_SKELETON: 'manage_skeleton',

  // ==================== GAMEPLAY TOOLS ====================
  ANIMATION_PHYSICS: 'animation_physics',
  MANAGE_EFFECT: 'manage_effect',
  MANAGE_AUDIO: 'manage_audio',
  MANAGE_INPUT: 'manage_input',
  MANAGE_SEQUENCE: 'manage_sequence',
  MANAGE_BEHAVIOR_TREE: 'manage_behavior_tree',
  MANAGE_GAS: 'manage_gas',
  MANAGE_CHARACTER: 'manage_character',
  MANAGE_COMBAT: 'manage_combat',
  MANAGE_AI: 'manage_ai',
  MANAGE_INVENTORY: 'manage_inventory',
  MANAGE_INTERACTION: 'manage_interaction',
  MANAGE_WIDGET_AUTHORING: 'manage_widget_authoring',
  MANAGE_NETWORKING: 'manage_networking',
  MANAGE_GAME_FRAMEWORK: 'manage_game_framework',
  MANAGE_SESSIONS: 'manage_sessions',

  // ==================== UTILITY TOOLS ====================
  MANAGE_PERFORMANCE: 'manage_performance',

  // ==================== INTERNAL ROUTING TOOLS ====================
  // These are not in schema but used for internal dispatch
  MANAGE_RENDER: 'manage_render',
  MANAGE_WORLD_PARTITION: 'manage_world_partition',

  // ==================== CONSOLE/SYSTEM ====================
  CONSOLE_COMMAND: 'console_command',

  // ==================== AUDIO ACTIONS ====================
  CREATE_SOUND_CUE: 'create_sound_cue',
  PLAY_SOUND_AT_LOCATION: 'play_sound_at_location',
  PLAY_SOUND_2D: 'play_sound_2d',
  CREATE_AUDIO_COMPONENT: 'create_audio_component',
  SET_SOUND_ATTENUATION: 'set_sound_attenuation',
  CREATE_SOUND_CLASS: 'create_sound_class',
  CREATE_SOUND_MIX: 'create_sound_mix',
  PUSH_SOUND_MIX: 'push_sound_mix',
  POP_SOUND_MIX: 'pop_sound_mix',
  CREATE_AMBIENT_SOUND: 'create_ambient_sound',
  CREATE_REVERB_ZONE: 'create_reverb_zone',
  ENABLE_AUDIO_ANALYSIS: 'enable_audio_analysis',
  FADE_SOUND: 'fade_sound',
  SET_DOPPLER_EFFECT: 'set_doppler_effect',
  SET_AUDIO_OCCLUSION: 'set_audio_occlusion',
  SPAWN_SOUND_AT_LOCATION: 'spawn_sound_at_location',
  PLAY_SOUND_ATTACHED: 'play_sound_attached',
  SET_SOUND_MIX_CLASS_OVERRIDE: 'set_sound_mix_class_override',
  CLEAR_SOUND_MIX_CLASS_OVERRIDE: 'clear_sound_mix_class_override',
  SET_BASE_SOUND_MIX: 'set_base_sound_mix',
  PRIME_SOUND: 'prime_sound',

  // ==================== LIGHTING ACTIONS ====================
  SPAWN_LIGHT: 'spawn_light',
  SPAWN_SKY_LIGHT: 'spawn_sky_light',
  ENSURE_SINGLE_SKY_LIGHT: 'ensure_single_sky_light',
  SETUP_GLOBAL_ILLUMINATION: 'setup_global_illumination',
  CONFIGURE_SHADOWS: 'configure_shadows',
  BAKE_LIGHTMAP: 'bake_lightmap',
  CREATE_LIGHTING_ENABLED_LEVEL: 'create_lighting_enabled_level',
  CREATE_LIGHTMASS_VOLUME: 'create_lightmass_volume',
  SET_EXPOSURE: 'set_exposure',
  SET_AMBIENT_OCCLUSION: 'set_ambient_occlusion',
  SETUP_VOLUMETRIC_FOG: 'setup_volumetric_fog',
  LIST_LIGHT_TYPES: 'list_light_types',
  CONFIGURE_RAY_TRACED_SHADOWS: 'configure_ray_traced_shadows',
  CONFIGURE_RAY_TRACED_GI: 'configure_ray_traced_gi',
  CONFIGURE_RAY_TRACED_REFLECTIONS: 'configure_ray_traced_reflections',
  CONFIGURE_RAY_TRACED_AO: 'configure_ray_traced_ao',
  CONFIGURE_PATH_TRACING: 'configure_path_tracing',
  CONFIGURE_RAY_TRACED_TRANSLUCENCY: 'configure_ray_traced_translucency',
  CONFIGURE_RAY_TRACING_QUALITY: 'configure_ray_tracing_quality',
  SET_LIGHT_CHANNEL: 'set_light_channel',
  SET_ACTOR_LIGHT_CHANNEL: 'set_actor_light_channel',
  GET_LIGHT_CHANNELS: 'get_light_channels',
  CONFIGURE_LIGHTMASS_SETTINGS: 'configure_lightmass_settings',
  BUILD_LIGHTING_QUALITY: 'build_lighting_quality',
  CONFIGURE_INDIRECT_LIGHTING_CACHE: 'configure_indirect_lighting_cache',
  CONFIGURE_VOLUMETRIC_LIGHTMAPS: 'configure_volumetric_lightmaps',
  CONFIGURE_LIGHTMASS_AMBIENT_OCCLUSION: 'configure_lightmass_ambient_occlusion',
  INSPECT_LIGHTMASS_SETTINGS: 'inspect_lightmass_settings',
  CREATE_SPHERE_REFLECTION_CAPTURE: 'create_sphere_reflection_capture',
  CREATE_BOX_REFLECTION_CAPTURE: 'create_box_reflection_capture',
  CONFIGURE_CAPTURE_RESOLUTION: 'configure_capture_resolution',
  CONFIGURE_CAPTURE_OFFSET: 'configure_capture_offset',
  RECAPTURE_SCENE: 'recapture_scene',
  CREATE_PLANAR_REFLECTION: 'create_planar_reflection',
  CONFIGURE_PLANAR_REFLECTION: 'configure_planar_reflection',
  CONFIGURE_SSR_SETTINGS: 'configure_ssr_settings',
  CONFIGURE_LUMEN_REFLECTION_SETTINGS: 'configure_lumen_reflection_settings',
  INSPECT_REFLECTION_CAPTURES: 'inspect_reflection_captures',
  CREATE_POST_PROCESS_VOLUME: 'create_post_process_volume',
  CONFIGURE_PP_BLEND: 'configure_pp_blend',
  SET_PP_WHITE_BALANCE: 'set_pp_white_balance',
  SET_PP_COLOR_GRADING: 'set_pp_color_grading',
  SET_PP_LUT: 'set_pp_lut',
  CONFIGURE_TONEMAPPER: 'configure_tonemapper',
  SET_TONEMAPPER_TYPE: 'set_tonemapper_type',
  CONFIGURE_BLOOM: 'configure_bloom',
  SET_BLOOM_INTENSITY: 'set_bloom_intensity',
  SET_BLOOM_THRESHOLD: 'set_bloom_threshold',
  CONFIGURE_LENS_FLARE: 'configure_lens_flare',
  CONFIGURE_DOF: 'configure_dof',
  SET_DOF_METHOD: 'set_dof_method',
  SET_FOCAL_DISTANCE: 'set_focal_distance',
  SET_APERTURE: 'set_aperture',
  CONFIGURE_BOKEH: 'configure_bokeh',
  CONFIGURE_MOTION_BLUR: 'configure_motion_blur',
  SET_MOTION_BLUR_AMOUNT: 'set_motion_blur_amount',
  SET_MOTION_BLUR_MAX: 'set_motion_blur_max',
  CONFIGURE_EXPOSURE: 'configure_exposure',
  SET_EXPOSURE_METHOD: 'set_exposure_method',
  SET_EXPOSURE_COMPENSATION: 'set_exposure_compensation',
  SET_EXPOSURE_MIN_MAX: 'set_exposure_min_max',
  CONFIGURE_SSAO: 'configure_ssao',
  CONFIGURE_GTAO: 'configure_gtao',
  CONFIGURE_VIGNETTE: 'configure_vignette',
  CONFIGURE_CHROMATIC_ABERRATION: 'configure_chromatic_aberration',
  CONFIGURE_GRAIN: 'configure_grain',
  CONFIGURE_SCREEN_PERCENTAGE: 'configure_screen_percentage',
  INSPECT_POST_PROCESS_VOLUME: 'inspect_post_process_volume',
  CREATE_SCENE_CAPTURE_2D: 'create_scene_capture_2d',
  CREATE_SCENE_CAPTURE_CUBE: 'create_scene_capture_cube',
  CREATE_RENDER_TARGET_CUBE: 'create_render_target_cube',
  CONFIGURE_SCENE_CAPTURE: 'configure_scene_capture',
  CONFIGURE_SCENE_CAPTURE_RESOLUTION: 'configure_scene_capture_resolution',
  CONFIGURE_CAPTURE_SOURCE: 'configure_capture_source',
  ASSIGN_RENDER_TARGET: 'assign_render_target',
  CAPTURE_SCENE: 'capture_scene',
  INSPECT_SCENE_CAPTURES: 'inspect_scene_captures',

  // ==================== PERFORMANCE ACTIONS ====================
  START_PROFILING: 'start_profiling',
  STOP_PROFILING: 'stop_profiling',
  SHOW_FPS: 'show_fps',
  SHOW_STATS: 'show_stats',
  SET_SCALABILITY: 'set_scalability',
  SET_RESOLUTION_SCALE: 'set_resolution_scale',
  SET_VSYNC: 'set_vsync',
  SET_FRAME_RATE_LIMIT: 'set_frame_rate_limit',
  GENERATE_MEMORY_REPORT: 'generate_memory_report',
  CONFIGURE_TEXTURE_STREAMING: 'configure_texture_streaming',
  CONFIGURE_LOD: 'configure_lod',
  MERGE_ACTORS: 'merge_actors',
  CONFIGURE_NANITE: 'configure_nanite',
} as const;

// ============================================================================
// ACTOR INNER ACTIONS (payload.action for control_actor tool)
// ============================================================================

/** Inner actions for control_actor tool */
export const ACTOR_ACTIONS = {
  SPAWN: 'spawn',
  DELETE: 'delete',
  APPLY_FORCE: 'apply_force',
  GET_COMPONENTS: 'get_components',
  SET_COMPONENT_PROPERTIES: 'set_component_properties',
  SET_TRANSFORM: 'set_transform',
  GET_TRANSFORM: 'get_transform',
  DUPLICATE: 'duplicate',
  ATTACH: 'attach',
  DETACH: 'detach',
  ADD_TAG: 'add_tag',
  REMOVE_TAG: 'remove_tag',
  FIND_BY_TAG: 'find_by_tag',
  DELETE_BY_TAG: 'delete_by_tag',
  SPAWN_BLUEPRINT: 'spawn_blueprint',
  LIST: 'list',
  FIND_BY_NAME: 'find_by_name',
  REMOVE_COMPONENT: 'remove_component',
  GET_COMPONENT_PROPERTY: 'get_component_property',
  SET_COLLISION: 'set_collision',
  CALL_FUNCTION: 'call_function',
  FIND_BY_CLASS: 'find_by_class',
  GET_BOUNDING_BOX: 'get_bounding_box',
} as const;

// ============================================================================
// INPUT INNER ACTIONS (payload.action for manage_input tool)
// ============================================================================

/** Inner actions for manage_input tool */
export const INPUT_ACTIONS = {
  CREATE_INPUT_ACTION: 'create_input_action',
  CREATE_INPUT_MAPPING_CONTEXT: 'create_input_mapping_context',
  ADD_MAPPING: 'add_mapping',
  REMOVE_MAPPING: 'remove_mapping',
  ADD_LEGACY_ACTION_MAPPING: 'add_legacy_action_mapping',
  REMOVE_LEGACY_ACTION_MAPPING: 'remove_legacy_action_mapping',
  ADD_LEGACY_AXIS_MAPPING: 'add_legacy_axis_mapping',
  REMOVE_LEGACY_AXIS_MAPPING: 'remove_legacy_axis_mapping',
  MAP_INPUT_ACTION: 'map_input_action',
  SET_INPUT_TRIGGER: 'set_input_trigger',
  SET_INPUT_MODIFIER: 'set_input_modifier',
  ENABLE_INPUT_MAPPING: 'enable_input_mapping',
  DISABLE_INPUT_ACTION: 'disable_input_action',
  GET_INPUT_INFO: 'get_input_info',
  SET_INPUT_ACTION_TYPE: 'set_input_action_type',
  ADD_INPUT_MAPPING: 'add_input_mapping',
  REMOVE_INPUT_MAPPING: 'remove_input_mapping',
  ADD_MAPPING_MODIFIER: 'add_mapping_modifier',
  ADD_MAPPING_TRIGGER: 'add_mapping_trigger',
  INSPECT_INPUT_ASSET: 'inspect_input_asset',
} as const;

// ============================================================================
// TYPE EXPORTS
// ============================================================================

export type ToolAction = typeof TOOL_ACTIONS[keyof typeof TOOL_ACTIONS];
export type ActorAction = typeof ACTOR_ACTIONS[keyof typeof ACTOR_ACTIONS];
export type InputAction = typeof INPUT_ACTIONS[keyof typeof INPUT_ACTIONS];
