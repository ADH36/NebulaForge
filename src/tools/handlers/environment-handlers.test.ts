import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock, exportEnvironmentSnapshotMock, importEnvironmentSnapshotMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async () => ({ success: true, result: {} })),
  exportEnvironmentSnapshotMock: vi.fn(async () => ({ success: true })),
  importEnvironmentSnapshotMock: vi.fn(async () => ({ success: true }))
}));

vi.mock('./common-handlers.js', async () => {
  const actual = await vi.importActual<typeof import('./common-handlers.js')>('./common-handlers.js');
  return {
    ...actual,
    executeAutomationRequest: executeAutomationRequestMock
  };
});

vi.mock('../../utils/environment-snapshot.js', () => ({
  exportEnvironmentSnapshot: exportEnvironmentSnapshotMock,
  importEnvironmentSnapshot: importEnvironmentSnapshotMock
}));

import { handleEnvironmentTools } from './environment-handlers.js';
import { handleLightingTools } from './lighting-handlers.js';
import { consolidatedToolDefinitions } from '../consolidated-tool-definitions.js';

const PHASE_28_ENVIRONMENT_ACTIONS = [
  'create_landscape', 'import_heightmap', 'export_heightmap', 'sculpt_landscape',
  'paint_landscape_layer', 'create_landscape_layer_info', 'configure_landscape_material',
  'create_landscape_grass_type', 'configure_landscape_splines', 'configure_landscape_lod',
  'create_landscape_streaming_proxy', 'create_foliage_type', 'configure_foliage_mesh',
  'configure_foliage_placement', 'configure_foliage_lod', 'configure_foliage_collision',
  'configure_foliage_culling', 'paint_foliage_instances', 'remove_foliage_instances',
  'configure_sky_atmosphere', 'configure_sky_light', 'configure_directional_light_atmosphere',
  'configure_exponential_height_fog', 'configure_volumetric_cloud', 'create_sky_sphere',
  'create_weather_system', 'configure_rain_particles', 'configure_snow_particles',
  'configure_wind', 'configure_lightning', 'create_time_of_day_system', 'configure_sun_position',
  'configure_light_color_curve', 'configure_sky_color_curve', 'create_water_body_ocean',
  'create_water_body_lake', 'create_water_body_river', 'create_water_body_custom',
  'configure_water_waves', 'configure_water_material', 'configure_water_collision',
  'create_buoyancy_component'
] as const;

const UE_581_LANDSCAPE_FOLIAGE_ACTIONS = [
  'inspect_landscape', 'delete_landscape', 'resize_landscape',
  'generate_landscape_heightmap', 'apply_landscape_erosion',
  'sculpt_landscape_region', 'paint_landscape_by_rule',
  'create_landscape_material', 'configure_landscape_layer_blend',
  'scatter_landscape_foliage', 'inspect_generated_foliage',
  'regenerate_generated_foliage', 'clear_generated_foliage'
] as const;

const WORLD_RECIPE_ACTIONS = [
  'generate_world', 'apply_biome', 'create_biome_preset',
  'inspect_biome_preset', 'list_biome_presets'
] as const;

const PHASE_29_1_RAY_TRACING_ACTIONS = [
  'configure_ray_traced_shadows', 'configure_ray_traced_gi',
  'configure_ray_traced_reflections', 'configure_ray_traced_ao',
  'configure_path_tracing', 'configure_ray_traced_translucency',
  'configure_ray_tracing_quality'
] as const;

const PHASE_29_2_LIGHT_CHANNEL_ACTIONS = [
  'set_light_channel', 'set_actor_light_channel', 'get_light_channels'
] as const;

const PHASE_29_3_LIGHTMASS_ACTIONS = [
  'configure_lightmass_settings', 'build_lighting_quality',
  'configure_indirect_lighting_cache', 'configure_volumetric_lightmaps',
  'configure_lightmass_ambient_occlusion', 'inspect_lightmass_settings'
] as const;

const PHASE_29_4_REFLECTION_ACTIONS = [
  'create_sphere_reflection_capture', 'create_box_reflection_capture',
  'configure_capture_resolution', 'configure_capture_offset', 'recapture_scene',
  'create_planar_reflection', 'configure_planar_reflection',
  'configure_ssr_settings', 'configure_lumen_reflection_settings',
  'inspect_reflection_captures'
] as const;

const PHASE_29_5_POST_PROCESS_ACTIONS = [
  'create_post_process_volume', 'configure_pp_blend',
  'set_pp_white_balance', 'set_pp_color_grading', 'set_pp_lut',
  'configure_tonemapper', 'set_tonemapper_type',
  'configure_bloom', 'set_bloom_intensity', 'set_bloom_threshold',
  'configure_lens_flare', 'configure_dof', 'set_dof_method',
  'set_focal_distance', 'set_aperture', 'configure_bokeh',
  'configure_motion_blur', 'set_motion_blur_amount', 'set_motion_blur_max',
  'configure_exposure', 'set_exposure_method', 'set_exposure_compensation',
  'set_exposure_min_max', 'configure_ssao', 'configure_gtao',
  'configure_vignette', 'configure_chromatic_aberration', 'configure_grain',
  'configure_screen_percentage', 'inspect_post_process_volume'
] as const;

const PHASE_29_6_SCENE_CAPTURE_ACTIONS = [
  'create_scene_capture_2d', 'create_scene_capture_cube', 'create_render_target_cube',
  'configure_scene_capture', 'configure_scene_capture_resolution', 'configure_capture_source',
  'assign_render_target', 'capture_scene', 'inspect_scene_captures'
] as const;

function getBuildEnvironmentActionEnum(): readonly string[] {
  const tool = consolidatedToolDefinitions.find(def => def.name === 'build_environment');
  const inputSchema = tool?.inputSchema as { properties?: { action?: { enum?: string[] } } } | undefined;
  return inputSchema?.properties?.action?.enum ?? [];
}

function getBuildEnvironmentProperties(): Record<string, unknown> {
  const tool = consolidatedToolDefinitions.find(def => def.name === 'build_environment');
  const inputSchema = tool?.inputSchema as { properties?: Record<string, unknown> } | undefined;
  return inputSchema?.properties ?? {};
}

describe('handleEnvironmentTools path normalization', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockClear();
    exportEnvironmentSnapshotMock.mockClear();
    importEnvironmentSnapshotMock.mockClear();
  });

  it('normalizes landscape material path aliases before dispatch', async () => {
    await handleEnvironmentTools('create_landscape', {
      action: 'create_landscape',
      name: 'TestLandscape',
      materialPath: 'Content/MCPTest/Materials/M_Landscape'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'create_landscape',
      expect.objectContaining({
        materialPath: '/Game/MCPTest/Materials/M_Landscape'
      })
    );
  });

  it('normalizes foliage asset path aliases before dispatch', async () => {
    await handleEnvironmentTools('add_foliage', {
      action: 'add_foliage',
      name: 'TestFoliage',
      meshPath: 'Engine/BasicShapes/Sphere'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'add_foliage_type',
      expect.objectContaining({
        meshPath: '/Engine/BasicShapes/Sphere'
      })
    );
  });

  it('normalizes existing foliage type aliases before dispatch', async () => {
    await handleEnvironmentTools('paint_foliage', {
      action: 'paint_foliage',
      foliageType: 'Game/Foliage/TestFoliage',
      locations: [{ x: 0, y: 0, z: 100 }]
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'paint_foliage',
      expect.objectContaining({
        foliageType: '/Game/Foliage/TestFoliage'
      })
    );
  });

  it('normalizes procedural foliage nested mesh paths before dispatch', async () => {
    await handleEnvironmentTools('create_procedural_foliage', {
      action: 'create_procedural_foliage',
      volumeName: 'TestProceduralFoliage',
      foliageTypes: [{ meshPath: 'Content/Foliage/SM_Bush', density: 1 }]
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'create_procedural_foliage',
      expect.objectContaining({
        foliageTypes: [expect.objectContaining({ meshPath: '/Game/Foliage/SM_Bush' })],
        types: [expect.objectContaining({ meshPath: '/Game/Foliage/SM_Bush' })]
      })
    );
  });

  it('exposes every Phase 28 roadmap action on the build_environment schema', () => {
    expect(getBuildEnvironmentActionEnum()).toEqual(expect.arrayContaining([...PHASE_28_ENVIRONMENT_ACTIONS]));
  });

  it('exposes every Phase 29.1 ray-tracing action on the build_environment schema', () => {
    expect(getBuildEnvironmentActionEnum()).toEqual(expect.arrayContaining([...PHASE_29_1_RAY_TRACING_ACTIONS]));
    expect(getBuildEnvironmentProperties()).toEqual(expect.objectContaining({
      samplesPerPixel: expect.any(Object),
      maxBounces: expect.any(Object),
      denoiser: expect.any(Object),
      aoRadius: expect.any(Object),
      aoIntensity: expect.any(Object)
    }));
  });

  it('exposes every Phase 29.2 light-channel action and property on the schema', () => {
    expect(getBuildEnvironmentActionEnum()).toEqual(expect.arrayContaining([...PHASE_29_2_LIGHT_CHANNEL_ACTIONS]));
    expect(getBuildEnvironmentProperties()).toEqual(expect.objectContaining({
      lightName: expect.any(Object),
      lightPath: expect.any(Object),
      channel: expect.any(Object),
      channels: expect.any(Object),
      componentName: expect.any(Object),
      applyToAllComponents: expect.any(Object)
    }));
  });

  it('exposes every Phase 29.3 Lightmass action and property on the schema', () => {
    expect(getBuildEnvironmentActionEnum()).toEqual(expect.arrayContaining([...PHASE_29_3_LIGHTMASS_ACTIONS]));
    expect(getBuildEnvironmentProperties()).toEqual(expect.objectContaining({
      staticLightingLevelScale: expect.any(Object),
      numIndirectLightingBounces: expect.any(Object),
      volumeLightingMethod: expect.any(Object),
      volumetricLightmapDetailCellSize: expect.any(Object),
      updateEveryFrame: expect.any(Object),
      lightingCacheDimension: expect.any(Object)
    }));
  });

  it.each([...PHASE_29_1_RAY_TRACING_ACTIONS])('forwards %s with normalized ray-tracing settings', async action => {
    await handleLightingTools(action, {
      action,
      enabled: true,
      samplesPerPixel: 4,
      maxBounces: 6,
      denoiser: true,
      aoRadius: 150,
      aoIntensity: 1.25
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      action,
      expect.objectContaining({
        enabled: true,
        samplesPerPixel: 4,
        maxBounces: 6,
        denoiser: true,
        radius: 150,
        intensity: 1.25
      }),
      `Automation bridge not available for ${action}`
    );
  });

  it.each([...PHASE_29_2_LIGHT_CHANNEL_ACTIONS])('forwards %s with target and channel settings', async action => {
    await handleLightingTools(action, {
      action,
      lightName: 'Phase29Light',
      actorName: 'Phase29Actor',
      channel: 1,
      enabled: true
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      action,
      expect.objectContaining({
        lightName: 'Phase29Light',
        actorName: 'Phase29Actor',
        channel: 1,
        enabled: true
      }),
      `Automation bridge not available for ${action}`
    );
  });

  it('normalizes and forwards Lightmass world settings', async () => {
    await handleLightingTools('configure_lightmass_settings', {
      action: 'configure_lightmass_settings',
      staticLightingLevelScale: 2,
      numIndirectLightingBounces: 4,
      environmentColor: [0.25, 0.5, 0.75],
      volumeLightingMethod: 'VolumetricLightmap',
      volumetricLightmapDetailCellSize: 200,
      useAmbientOcclusion: true
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'configure_lightmass_settings',
      expect.objectContaining({
        staticLightingLevelScale: 2,
        numIndirectLightingBounces: 4,
        environmentColor: { r: 0.25, g: 0.5, b: 0.75, a: 1 },
        volumeLightingMethod: 'VolumetricLightmap',
        useAmbientOcclusion: true
      }),
      'Automation bridge not available for Lightmass settings'
    );
  });

  it('exposes every Phase 29.4 reflection action and property on the schema', () => {
    expect(getBuildEnvironmentActionEnum()).toEqual(expect.arrayContaining([...PHASE_29_4_REFLECTION_ACTIONS]));
    expect(getBuildEnvironmentProperties()).toEqual(expect.objectContaining({
      captureName: expect.any(Object),
      influenceRadius: expect.any(Object),
      captureResolution: expect.any(Object),
      captureOffset: expect.any(Object),
      screenPercentage: expect.any(Object),
      ssrQuality: expect.any(Object),
      lumenReflectionMaxBounces: expect.any(Object)
    }));
  });

  it('normalizes and forwards reflection capture settings', async () => {
    await handleLightingTools('create_sphere_reflection_capture', {
      action: 'create_sphere_reflection_capture',
      name: 'Phase29Reflection',
      location: { x: 10, y: 20, z: 30 },
      captureOffset: { x: 1, y: 2, z: 3 },
      influenceRadius: 1000,
      captureResolution: 256,
      brightness: 1.25,
      runtimeCapture: true
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'create_sphere_reflection_capture',
      expect.objectContaining({
        name: 'Phase29Reflection',
        location: { x: 10, y: 20, z: 30 },
        captureOffset: { x: 1, y: 2, z: 3 },
        influenceRadius: 1000,
        captureResolution: 256,
        brightness: 1.25,
        runtimeCapture: true
      }),
      'Automation bridge not available for create_sphere_reflection_capture'
    );
  });

  it('exposes every Phase 29.5 post-processing action and property on the schema', () => {
    expect(getBuildEnvironmentActionEnum()).toEqual(expect.arrayContaining([...PHASE_29_5_POST_PROCESS_ACTIONS]));
    expect(getBuildEnvironmentProperties()).toEqual(expect.objectContaining({
      volumeName: expect.any(Object),
      bUnbound: expect.any(Object),
      blendWeight: expect.any(Object),
      bloomIntensity: expect.any(Object),
      dofFocalDistance: expect.any(Object),
      motionBlurAmount: expect.any(Object),
      exposureMethod: expect.any(Object),
      colorGamma: expect.any(Object),
      lutPath: expect.any(Object),
      vignetteIntensity: expect.any(Object),
      grainIntensity: expect.any(Object)
    }));
  });

  it('normalizes and forwards post-processing settings', async () => {
    await handleLightingTools('configure_exposure', {
      action: 'configure_exposure',
      volumeName: 'Phase29PostProcess',
      exposureMethod: 'Manual',
      exposureCompensation: 0.5,
      exposureMinBrightness: 0,
      exposureMaxBrightness: 10,
      colorSaturation: [1, 0.9, 0.8, 1],
      bloomIntensity: 0.8,
      motionBlurAmount: 0.2,
      vignetteIntensity: 0.1
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'configure_exposure',
      expect.objectContaining({
        volumeName: 'Phase29PostProcess',
        target: 'Phase29PostProcess',
        exposureMethod: 'Manual',
        exposureCompensation: 0.5,
        colorSaturation: [1, 0.9, 0.8, 1],
        bloomIntensity: 0.8
      }),
      'Automation bridge not available for configure_exposure'
    );
  });

  it('exposes and forwards Phase 29.6 scene capture controls', async () => {
    expect(getBuildEnvironmentActionEnum()).toEqual(expect.arrayContaining([...PHASE_29_6_SCENE_CAPTURE_ACTIONS]));
    expect(getBuildEnvironmentProperties()).toEqual(expect.objectContaining({
      sceneCaptureName: expect.any(Object),
      renderTargetPath: expect.any(Object),
      captureSource: expect.any(Object),
      projectionType: expect.any(Object),
      captureEveryFrame: expect.any(Object),
      captureDeferred: expect.any(Object),
      clearColor: expect.any(Object),
      showOnlyActors: expect.any(Object)
    }));

    await handleLightingTools('configure_scene_capture', {
      action: 'configure_scene_capture',
      sceneCaptureName: 'Phase29SceneCapture',
      captureSource: 'SCS_FinalColorLDR',
      projectionType: 'Orthographic',
      fovAngle: 90,
      orthoWidth: 1024,
      captureEveryFrame: false,
      captureOnMovement: true,
      alwaysPersistRenderingState: true,
      captureRotation: true,
      capturePriority: 2,
      captureDeferred: true,
      width: 512,
      height: 256,
      format: 'RGBA8',
      forceLinearGamma: true,
      autoGenerateMips: false,
      supportsUAV: true,
      hdr: false,
      clearColor: [0, 0, 0, 1],
      hiddenActors: ['HiddenActor'],
      showOnlyActors: ['VisibleActor'],
      postProcessBlendWeight: 0.5
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {}, 'configure_scene_capture', expect.objectContaining({
        target: 'Phase29SceneCapture',
        captureSource: 'SCS_FinalColorLDR',
        projectionType: 'Orthographic',
        clearColor: [0, 0, 0, 1],
        hiddenActors: ['HiddenActor'],
        postProcessBlendWeight: 0.5
      }), 'Automation bridge not available for configure_scene_capture'
    );
  });

  it.each([...PHASE_29_3_LIGHTMASS_ACTIONS])('routes %s through the lighting bridge', async action => {
    await handleLightingTools(action, {
      action,
      quality: 'High',
      enabled: true,
      updateEveryFrame: true,
      lightingCacheDimension: 64
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalled();
  });

  it('routes the Phase 28 create_foliage_type alias to foliage type creation', async () => {
    await handleEnvironmentTools('create_foliage_type', {
      action: 'create_foliage_type',
      name: 'Phase28FoliageType',
      foliageTypePath: 'Content/Foliage/Phase28FoliageType',
      meshPath: 'Engine/BasicShapes/Cone',
      path: 'Content/Foliage',
      density: 12
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'add_foliage_type',
      expect.objectContaining({
        name: 'Phase28FoliageType',
        foliageTypePath: '/Game/Foliage/Phase28FoliageType',
        meshPath: '/Engine/BasicShapes/Cone',
        path: '/Game/Foliage',
        density: 12
      })
    );
  });

  it('exposes water steepness and sky cubemap path on the build_environment schema', () => {
    expect(getBuildEnvironmentProperties()).toHaveProperty('cubemapPath');
    expect(getBuildEnvironmentProperties()).toHaveProperty('steepness');
  });

  it('exposes and forwards the UE 5.8 procedural building contract unchanged', async () => {
    const buildingActions = [
      'generate_procedural_building', 'generate_city_block',
      'inspect_procedural_building', 'regenerate_procedural_building',
      'save_procedural_building_blueprint'
    ];
    expect(getBuildEnvironmentActionEnum()).toEqual(expect.arrayContaining(buildingActions));
    expect(getBuildEnvironmentProperties()).toEqual(expect.objectContaining({
      footprintPoints: expect.any(Object), floors: expect.any(Object), floorHeight: expect.any(Object),
      wallMaterial: expect.any(Object), windowMaterial: expect.any(Object), roofMaterial: expect.any(Object),
      interiorMaterial: expect.any(Object), roadSplineActor: expect.any(Object), blueprintPath: expect.any(Object)
    }));

    await handleEnvironmentTools('generate_procedural_building', {
      action: 'generate_procedural_building', buildingType: 'shop', buildingName: 'MCP_Shop',
      footprintPoints: [{ x: 0, y: 0, z: 0 }, { x: 900, y: 0, z: 0 }, { x: 900, y: 600, z: 0 }, { x: 0, y: 600, z: 0 }],
      floors: 2, floorHeight: 340, wallThickness: 24, roofType: 'flat', seed: 482,
      wallMaterial: 'Content/MCPTest/M_Wall', windowMaterial: 'Content/MCPTest/M_Window'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {}, 'build_environment', expect.objectContaining({
        action: 'generate_procedural_building', buildingType: 'shop', seed: 482,
        wallMaterial: 'Content/MCPTest/M_Wall', windowMaterial: 'Content/MCPTest/M_Window'
      }), 'Automation bridge not available for environment building operations'
    );
  });

  it('preserves foliageTypePath for targeted remove_foliage_instances', async () => {
    await handleEnvironmentTools('remove_foliage_instances', {
      action: 'remove_foliage_instances',
      foliageTypePath: 'Game/Foliage/Phase28FoliageType'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'build_environment',
      expect.objectContaining({
        action: 'remove_foliage_instances',
        foliageTypePath: '/Game/Foliage/Phase28FoliageType'
      }),
      'Automation bridge not available for environment building operations'
    );
  });

  it('exposes removeAll for explicit all-foliage removal', () => {
    expect(getBuildEnvironmentProperties()).toHaveProperty('removeAll');
  });

  it.each([
    ['import_heightmap', 'landscapeActorPath', 'Content/MCPTest/Landscape.Landscape', '/Game/MCPTest/Landscape.Landscape'],
    ['export_heightmap', 'landscapeActorPath', 'Content/MCPTest/Landscape.Landscape', '/Game/MCPTest/Landscape.Landscape'],
    ['configure_landscape_material', 'landscapeActorPath', 'Content/MCPTest/Landscape.Landscape', '/Game/MCPTest/Landscape.Landscape'],
    ['configure_landscape_splines', 'landscapeActorPath', 'Content/MCPTest/Landscape.Landscape', '/Game/MCPTest/Landscape.Landscape'],
    ['configure_sky_light', 'cubemapPath', 'Content/HDRI/T_SkyCubemap', '/Game/HDRI/T_SkyCubemap']
  ])('normalizes Phase 28 alias path field %s.%s before dispatch', async (action, fieldName, rawPath, normalizedPath) => {
    await handleEnvironmentTools(action, {
      action,
      [fieldName]: rawPath
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'build_environment',
      expect.objectContaining({
        action,
        [fieldName]: normalizedPath
      }),
      'Automation bridge not available for environment building operations'
    );
  });

  it.each([
    ['create_weather_system', 'particleSystemPath', 'Content/Weather/P_Weather', '/Game/Weather/P_Weather'],
    ['configure_rain_particles', 'particleSystemPath', 'Content/Weather/P_Rain', '/Game/Weather/P_Rain'],
    ['configure_snow_particles', 'particleSystemPath', 'Content/Weather/P_Snow', '/Game/Weather/P_Snow'],
    ['configure_lightning', 'particleSystemPath', 'Content/Weather/P_Lightning', '/Game/Weather/P_Lightning'],
    ['configure_light_color_curve', 'curvePath', 'Content/Environment/Curves/C_Light', '/Game/Environment/Curves/C_Light'],
    ['configure_sky_color_curve', 'curvePath', 'Content/Environment/Curves/C_Sky', '/Game/Environment/Curves/C_Sky']
  ])('normalizes Phase 28 asset path field %s.%s before dispatch', async (action, fieldName, rawPath, normalizedPath) => {
    await handleEnvironmentTools(action, {
      action,
      [fieldName]: rawPath
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'build_environment',
      expect.objectContaining({
        action,
        [fieldName]: normalizedPath
      }),
      'Automation bridge not available for environment building operations'
    );
  });

  it.each([
    'create_water_body_ocean',
    'create_water_body_lake',
    'create_water_body_river',
    'create_water_body_custom'
  ])('normalizes water body create material paths before dispatch for %s', async action => {
    await handleEnvironmentTools(action, {
      action,
      materialPath: 'Engine/BasicShapes/BasicShapeMaterial'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'build_environment',
      expect.objectContaining({
        action,
        materialPath: '/Engine/BasicShapes/BasicShapeMaterial'
      }),
      'Automation bridge not available for environment building operations'
    );
  });

  it('normalizes generate_lods asset path arrays before dispatch', async () => {
    await handleEnvironmentTools('generate_lods', {
      action: 'generate_lods',
      assetPaths: ['Engine/BasicShapes/Sphere', 'Content/MCPTest/SM_Rock'],
      numLODs: 2
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'build_environment',
      expect.objectContaining({
        action: 'generate_lods',
        assetPaths: ['/Engine/BasicShapes/Sphere', '/Game/MCPTest/SM_Rock']
      }),
      'Automation bridge not available for environment building operations'
    );
  });

  it('preserves filesystem snapshot paths', async () => {
    await handleEnvironmentTools('export_snapshot', {
      action: 'export_snapshot',
      path: './tmp/unreal-mcp/build-environment',
      filename: 'snapshot.json'
    }, {} as never);

    expect(exportEnvironmentSnapshotMock).toHaveBeenCalledWith({
      path: './tmp/unreal-mcp/build-environment',
      filename: 'snapshot.json'
    });
  });
});

describe('UE 5.8.1 landscape and foliage authoring contract', () => {
  beforeEach(() => executeAutomationRequestMock.mockClear());

  it('exposes the complete landscape and tool-generated foliage action family', () => {
    const actions = getBuildEnvironmentActionEnum();
    const properties = getBuildEnvironmentProperties();
    for (const action of UE_581_LANDSCAPE_FOLIAGE_ACTIONS) expect(actions).toContain(action);
    for (const property of ['resolutionX', 'resolutionY', 'terrainFeature', 'placementMode', 'exclusionZones', 'minSlope', 'maxSlope', 'minHeight', 'maxHeight', 'surfaceOffset', 'cancel']) {
      expect(properties).toHaveProperty(property);
    }
  });

  it('normalizes requested foliage mesh paths and preserves deterministic scatter constraints', async () => {
    await handleEnvironmentTools('scatter_landscape_foliage', {
      action: 'scatter_landscape_foliage',
      landscapeName: 'Landscape_A',
      seed: 8128,
      foliageTypes: [{ meshPath: 'Game/Meshes/SM_Tree', count: 12, minScale: 0.8, maxScale: 1.2 }],
      exclusionZones: [{ min: { x: 0, y: 0, z: -100 }, max: { x: 200, y: 200, z: 1000 } }]
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {}, 'build_environment', expect.objectContaining({
        action: 'scatter_landscape_foliage', seed: 8128,
        foliageTypes: [{ meshPath: '/Game/Meshes/SM_Tree', count: 12, minScale: 0.8, maxScale: 1.2 }]
      }), 'Automation bridge not available for landscape and foliage authoring operations'
    );
  });
});

describe('world recipe orchestration contract', () => {
  beforeEach(() => executeAutomationRequestMock.mockClear());

  it('exposes the world recipe action family and preset properties', () => {
    const actions = getBuildEnvironmentActionEnum();
    const properties = getBuildEnvironmentProperties();
    for (const action of WORLD_RECIPE_ACTIONS) expect(actions).toContain(action);
    for (const property of [
      'biomePresetPath', 'reuseExistingLandscape', 'generateMaterial',
      'skipLandscape', 'skipTerrain', 'skipPaint', 'skipFoliage',
      'componentsX', 'componentsY', 'quadsPerComponent',
      'fadeDistance', 'fadeSlope'
    ]) {
      expect(properties).toHaveProperty(property);
    }
  });

  it('forwards generate_world with normalized preset and material paths', async () => {
    await handleEnvironmentTools('generate_world', {
      action: 'generate_world',
      name: 'Landscape_Alpine',
      seed: 4711,
      terrainFeature: 'mountains',
      componentsX: 6,
      componentsY: 6,
      quadsPerComponent: 63,
      biomePresetPath: 'Game/MCPWorldBuilder/Presets/BIOME_Alpine',
      materialPath: 'Content/MCPTest/Materials/M_Landscape',
      reuseExistingLandscape: true
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {}, 'build_environment', expect.objectContaining({
        action: 'generate_world',
        seed: 4711,
        terrainFeature: 'mountains',
        biomePresetPath: '/Game/MCPWorldBuilder/Presets/BIOME_Alpine',
        materialPath: '/Game/MCPTest/Materials/M_Landscape',
        reuseExistingLandscape: true
      }), 'Automation bridge not available for landscape and foliage authoring operations'
    );
  });

  it('normalizes foliage mesh paths inside generate_world recipes', async () => {
    await handleEnvironmentTools('apply_biome', {
      action: 'apply_biome',
      biomePresetPath: '/Game/MCPWorldBuilder/Presets/BIOME_Tundra',
      seed: 99,
      foliageTypes: [{ meshPath: 'Game/Meshes/SM_Pine', count: 24, minScale: 0.7, maxScale: 1.4 }]
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {}, 'build_environment', expect.objectContaining({
        action: 'apply_biome',
        biomePresetPath: '/Game/MCPWorldBuilder/Presets/BIOME_Tundra',
        foliageTypes: [{ meshPath: '/Game/Meshes/SM_Pine', count: 24, minScale: 0.7, maxScale: 1.4 }]
      }), 'Automation bridge not available for landscape and foliage authoring operations'
    );
  });

  it('normalizes create_biome_preset output folder before dispatch', async () => {
    await handleEnvironmentTools('create_biome_preset', {
      action: 'create_biome_preset',
      name: 'BIOME_Desert',
      path: 'Content/MCPWorldBuilder/Presets',
      seed: 7,
      layers: [{ layerName: 'Sand', maskType: 'constant', strength: 1 }]
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {}, 'build_environment', expect.objectContaining({
        action: 'create_biome_preset',
        name: 'BIOME_Desert',
        path: '/Game/MCPWorldBuilder/Presets',
        seed: 7
      }), 'Automation bridge not available for landscape and foliage authoring operations'
    );
  });

  it('forwards inspect_biome_preset with a normalized preset path', async () => {
    await handleEnvironmentTools('inspect_biome_preset', {
      action: 'inspect_biome_preset',
      biomePresetPath: 'Game/MCPWorldBuilder/Presets/BIOME_Alpine'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {}, 'build_environment', expect.objectContaining({
        action: 'inspect_biome_preset',
        biomePresetPath: '/Game/MCPWorldBuilder/Presets/BIOME_Alpine'
      }), 'Automation bridge not available for landscape and foliage authoring operations'
    );
  });
});
