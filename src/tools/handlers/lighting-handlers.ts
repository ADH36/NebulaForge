/**
 * Lighting tool handlers - migrated from class-based LightingTools
 * All operations route through executeAutomationRequest to the C++ bridge
 */
import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { LightingArgs } from '../../types/handler-types.js';
import { executeAutomationRequest, normalizeLocation, executeBatchConsoleCommands } from './common-handlers.js';
import { toNumber, toBoolean, toString, toColor3, toLocationObj, toRotationObj, normalizeName } from '../../utils/type-coercion.js';
import { ResponseFactory } from '../../utils/response-factory.js';
import { TOOL_ACTIONS } from '../../utils/action-constants.js';


// Valid light types supported by UE - accepts multiple formats
const VALID_LIGHT_TYPES = [
  'point', 'directional', 'spot', 'rect', 'sky',           // lowercase short names
  'pointlight', 'directionallight', 'spotlight', 'rectlight', 'skylight'  // lowercase class names
];

// Alias for lighting-specific name normalization
const normalizeLightName = (value: unknown, defaultName?: string): string => normalizeName(value, defaultName, 'Light');

/**
 * Spawn a light via the automation bridge
 */
async function spawnLight(
  tools: ITools,
  lightClass: string,
  params: {
    name: string;
    location?: unknown;
    rotation?: unknown;
    properties?: Record<string, unknown>;
  }
): Promise<Record<string, unknown>> {
  const payload: Record<string, unknown> = {
    lightClass,
    name: params.name,
  };

  if (params.location) {
    const locObj = toLocationObj(params.location);
    if (locObj) payload.location = locObj;
  }

  if (params.rotation) {
    const rotObj = toRotationObj(params.rotation);
    if (rotObj) payload.rotation = rotObj;
  }

  if (params.properties) {
    payload.properties = params.properties;
  }

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.SPAWN_LIGHT, payload, 'Automation bridge not available for light spawning')) as Record<string, unknown>;
}

/**
 * Create directional light
 */
async function createDirectionalLight(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const name = normalizeLightName(args.name);
  const intensity = toNumber(args.intensity);
  const color = toColor3(args.color);
  const castShadows = toBoolean(args.castShadows);
  const temperature = toNumber(args.temperature);
  const useAsAtmosphereSunLight = toBoolean(args.useAsAtmosphereSunLight);

  // Validate numeric parameters
  if (intensity !== undefined && intensity < 0) {
    return { success: false, isError: true, error: 'Invalid intensity: must be non-negative' };
  }

  // Build properties for the light
  const properties: Record<string, unknown> = {};
  if (intensity !== undefined) properties.intensity = intensity;
  if (color) properties.color = { r: color[0], g: color[1], b: color[2], a: 1.0 };
  if (castShadows !== undefined) properties.castShadows = castShadows;
  if (temperature !== undefined) properties.temperature = temperature;
  if (useAsAtmosphereSunLight !== undefined) properties.useAsAtmosphereSunLight = useAsAtmosphereSunLight;

  const result = await spawnLight(tools, 'DirectionalLight', {
    name,
    location: [0, 0, 500],
    rotation: args.rotation || [0, 0, 0],
    properties
  });

  return cleanObject(result);
}

/**
 * Create point light
 */
async function createPointLight(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const name = normalizeLightName(args.name);
  const location = normalizeLocation(args.location) || [0, 0, 0];
  const intensity = toNumber(args.intensity);
  const radius = toNumber(args.radius);
  const color = toColor3(args.color);
  const castShadows = toBoolean(args.castShadows);
  const falloffExponent = toNumber(args.falloffExponent);

  // Validate numeric parameters
  if (intensity !== undefined && intensity < 0) {
    return { success: false, isError: true, error: 'Invalid intensity: must be non-negative' };
  }
  if (radius !== undefined && radius < 0) {
    return { success: false, isError: true, error: 'Invalid radius: must be non-negative' };
  }

  // Build properties for the light
  const properties: Record<string, unknown> = {};
  if (intensity !== undefined) properties.intensity = intensity;
  if (radius !== undefined) properties.attenuationRadius = radius;
  if (color) properties.color = { r: color[0], g: color[1], b: color[2], a: 1.0 };
  if (castShadows !== undefined) properties.castShadows = castShadows;
  if (falloffExponent !== undefined) properties.lightFalloffExponent = falloffExponent;

  const result = await spawnLight(tools, 'PointLight', {
    name,
    location,
    rotation: args.rotation,
    properties
  });

  return cleanObject(result);
}

/**
 * Create spot light
 */
async function createSpotLight(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const name = normalizeLightName(args.name);
  const location = normalizeLocation(args.location);
  const intensity = toNumber(args.intensity);
  const innerCone = toNumber(args.innerCone);
  const outerCone = toNumber(args.outerCone);
  const radius = toNumber(args.radius);
  const color = toColor3(args.color);
  const castShadows = toBoolean(args.castShadows);

  // Validate required location
  if (!location) {
    return { success: false, isError: true, error: 'Location is required for spot light' };
  }

  // Validate numeric parameters
  if (intensity !== undefined && intensity < 0) {
    return { success: false, isError: true, error: 'Invalid intensity: must be non-negative' };
  }
  if (innerCone !== undefined && (innerCone < 0 || innerCone > 180)) {
    return { success: false, isError: true, error: 'Invalid innerCone: must be between 0 and 180 degrees' };
  }
  if (outerCone !== undefined && (outerCone < 0 || outerCone > 180)) {
    return { success: false, isError: true, error: 'Invalid outerCone: must be between 0 and 180 degrees' };
  }
  if (radius !== undefined && radius < 0) {
    return { success: false, isError: true, error: 'Invalid radius: must be non-negative' };
  }

  // Build properties for the light
  const properties: Record<string, unknown> = {};
  if (intensity !== undefined) properties.intensity = intensity;
  if (innerCone !== undefined) properties.innerConeAngle = innerCone;
  if (outerCone !== undefined) properties.outerConeAngle = outerCone;
  if (radius !== undefined) properties.attenuationRadius = radius;
  if (color) properties.color = { r: color[0], g: color[1], b: color[2], a: 1.0 };
  if (castShadows !== undefined) properties.castShadows = castShadows;

  const result = await spawnLight(tools, 'SpotLight', {
    name,
    location,
    rotation: args.rotation || [0, 0, 0],
    properties
  });

  return cleanObject(result);
}

/**
 * Create rect light
 */
async function createRectLight(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const name = normalizeLightName(args.name);
  const location = normalizeLocation(args.location);
  const intensity = toNumber(args.intensity);
  const width = toNumber(args.width);
  const height = toNumber(args.height);
  const color = toColor3(args.color);
  // Used in properties below

  // Validate required location
  if (!location) {
    return { success: false, isError: true, error: 'Location is required for rect light' };
  }

  // Validate numeric parameters
  if (intensity !== undefined && intensity < 0) {
    return { success: false, isError: true, error: 'Invalid intensity: must be non-negative' };
  }
  if (width !== undefined && width <= 0) {
    return { success: false, isError: true, error: 'Invalid width: must be positive' };
  }
  if (height !== undefined && height <= 0) {
    return { success: false, isError: true, error: 'Invalid height: must be positive' };
  }

  // Build properties for the light
  const properties: Record<string, unknown> = {};
  if (intensity !== undefined) properties.intensity = intensity;
  if (color) properties.color = { r: color[0], g: color[1], b: color[2], a: 1.0 };
  if (width !== undefined) properties.sourceWidth = width;
  if (height !== undefined) properties.sourceHeight = height;

  const result = await spawnLight(tools, 'RectLight', {
    name,
    location,
    rotation: args.rotation || [0, 0, 0],
    properties
  });

  return cleanObject(result);
}

/**
 * Create dynamic light - routes to specific light type handlers
 */
async function createDynamicLight(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const name = normalizeLightName(args.name);
  const lightTypeRaw = toString(args.lightType) || 'Point';
  const intensity = toNumber(args.intensity);
  const color = toColor3(args.color);
  const typeNorm = lightTypeRaw.toLowerCase();

  switch (typeNorm) {
    case 'directional':
    case 'directionallight':
      return createDirectionalLight(tools, { ...args, name, intensity, color });
    case 'spot':
    case 'spotlight':
      return createSpotLight(tools, { ...args, name, intensity, color });
    case 'rect':
    case 'rectlight':
      return createRectLight(tools, { ...args, name, intensity, color });
    case 'point':
    case 'pointlight':
    default:
      return createPointLight(tools, { ...args, name, intensity, color });
  }
}

/**
 * Create sky light
 */
async function createSkyLight(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const name = normalizeLightName(args.name);
  const sourceType = toString(args.sourceType) || 'CapturedScene';
  const cubemapPath = toString(args.cubemapPath);
  const intensity = toNumber(args.intensity);
  const recapture = toBoolean(args.recapture);
  const realTimeCapture = toBoolean(args.realTimeCapture);
  const castShadows = toBoolean(args.castShadows);
  const color = toColor3(args.color);

  // Validate cubemap requirement
  if (sourceType === 'SpecifiedCubemap' && !cubemapPath) {
    return { success: false, isError: true, error: 'cubemapPath is required when sourceType is SpecifiedCubemap' };
  }

  const payload: Record<string, unknown> = {
    name,
    sourceType,
    location: args.location,
    rotation: args.rotation
  };

  if (cubemapPath) payload.cubemapPath = cubemapPath;
  if (intensity !== undefined) payload.intensity = intensity;
  if (recapture !== undefined) payload.recapture = recapture;

  // Build properties
  const properties: Record<string, unknown> = {};
  if (intensity !== undefined) properties.Intensity = intensity;
  if (castShadows !== undefined) properties.CastShadows = castShadows;
  if (realTimeCapture !== undefined) properties.RealTimeCapture = realTimeCapture;
  if (color) properties.LightColor = { r: color[0], g: color[1], b: color[2], a: 1.0 };

  if (Object.keys(properties).length > 0) payload.properties = properties;

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.SPAWN_SKY_LIGHT, payload, 'Automation bridge not available for sky light creation')) as Record<string, unknown>;
}

/**
 * Ensure single sky light
 */
async function ensureSingleSkyLight(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const defaultName = 'MCP_Test_Sky';
  const name = normalizeLightName(args.name, defaultName);
  const recapture = args.recapture !== false;

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.ENSURE_SINGLE_SKY_LIGHT, { name, recapture }, 'Automation bridge not available for sky light management')) as Record<string, unknown>;
}

/**
 * Setup global illumination
 */
async function setupGlobalIllumination(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  if (!args.method) {
    return {
      success: false,
      isError: true,
      error: 'MISSING_REQUIRED_PARAM',
      message: "'method' parameter is required for setup_global_illumination. Must be one of: LumenGI, ScreenSpace, None, RayTraced, Lightmass"
    };
  }

  // Normalize method
  let normalizedMethod: string | undefined;
  const methodLower = String(args.method).toLowerCase();

  if (methodLower === 'lumen' || methodLower === 'lumengi') {
    normalizedMethod = 'LumenGI';
  } else if (methodLower === 'screenspace' || methodLower === 'ssgi') {
    normalizedMethod = 'ScreenSpace';
  } else if (methodLower === 'none') {
    normalizedMethod = 'None';
  } else if (methodLower === 'raytraced') {
    normalizedMethod = 'RayTraced';
  } else if (methodLower === 'lightmass') {
    normalizedMethod = 'Lightmass';
  } else {
    return {
      success: false,
      isError: true,
      error: 'INVALID_GI_METHOD',
      message: `Invalid GI method: '${args.method}'. Must be one of: LumenGI, ScreenSpace, None, RayTraced, Lightmass`
    };
  }

  const payload = {
    method: normalizedMethod,
    quality: toString(args.quality),
    indirectLightingIntensity: toNumber(args.indirectLightingIntensity),
    bounces: toNumber(args.bounces)
  };

  const result = await executeAutomationRequest(tools, TOOL_ACTIONS.SETUP_GLOBAL_ILLUMINATION, payload);

  // If bridge fails with connection error, fall back to console commands
  const resultObj = result as Record<string, unknown>;
  if (resultObj.success === false && typeof resultObj.error === 'string' &&
      (resultObj.error.includes('not available') || resultObj.error.includes('Connection'))) {
    // Console command fallback
    const commands: string[] = [];

    switch (normalizedMethod) {
      case 'Lightmass': commands.push('r.DynamicGlobalIlluminationMethod 0'); break;
      case 'LumenGI': commands.push('r.DynamicGlobalIlluminationMethod 1'); break;
      case 'ScreenSpace': commands.push('r.DynamicGlobalIlluminationMethod 2'); break;
      case 'None': commands.push('r.DynamicGlobalIlluminationMethod 3'); break;
    }

    if (args.quality) {
      const qualityMap: Record<string, number> = { 'Low': 0, 'Medium': 1, 'High': 2, 'Epic': 3 };
      commands.push(`r.Lumen.Quality ${qualityMap[args.quality] ?? 1}`);
    }

    if (args.indirectLightingIntensity !== undefined) {
      commands.push(`r.IndirectLightingIntensity ${args.indirectLightingIntensity}`);
    }

    if (args.bounces !== undefined) {
      commands.push(`r.Lumen.MaxReflectionBounces ${args.bounces}`);
    }

    // Use batch execution for all console commands - significantly faster than sequential
    if (commands.length > 0) {
      await executeBatchConsoleCommands(tools, commands);
    }

    return { success: true, message: 'Global illumination configured (console)' };
  }

  return cleanObject(result) as Record<string, unknown>;
}

/**
 * Configure shadows
 */
async function configureShadows(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const payload = {
    shadowQuality: toString(args.shadowQuality),
    cascadedShadows: toBoolean(args.cascadedShadows),
    shadowDistance: toNumber(args.shadowDistance),
    contactShadows: toBoolean(args.contactShadows),
    rayTracedShadows: toBoolean(args.rayTracedShadows),
    virtualShadowMaps: toBoolean(args.rayTracedShadows)
  };

  const result = await executeAutomationRequest(tools, TOOL_ACTIONS.CONFIGURE_SHADOWS, payload);

  // Fallback to console commands if bridge fails
  const resultObj = result as Record<string, unknown>;
  if (resultObj.success === false && typeof resultObj.error === 'string' &&
      (resultObj.error.includes('not available') || resultObj.error.includes('Connection'))) {
    const commands: string[] = [];

    if (args.shadowQuality) {
      const qualityMap: Record<string, number> = { 'Low': 0, 'Medium': 1, 'High': 2, 'Epic': 3 };
      commands.push(`r.ShadowQuality ${qualityMap[args.shadowQuality] ?? 1}`);
    }

    if (args.cascadedShadows !== undefined) {
      commands.push(`r.Shadow.CSM.MaxCascades ${args.cascadedShadows ? 4 : 1}`);
    }

    if (args.shadowDistance !== undefined) {
      commands.push(`r.Shadow.DistanceScale ${args.shadowDistance}`);
    }

    if (args.contactShadows !== undefined) {
      commands.push(`r.ContactShadows ${args.contactShadows ? 1 : 0}`);
    }

    if (args.rayTracedShadows !== undefined) {
      commands.push(`r.RayTracing.Shadows ${args.rayTracedShadows ? 1 : 0}`);
    }

    // Use batch execution for all console commands - significantly faster than sequential
    if (commands.length > 0) {
      await executeBatchConsoleCommands(tools, commands);
    }

    return { success: true, message: 'Shadow settings configured (console)' };
  }

  return cleanObject(result) as Record<string, unknown>;
}

/**
 * Build lighting
 */
async function buildLighting(
  tools: ITools,
  args: LightingArgs,
  automationAction: string = TOOL_ACTIONS.BAKE_LIGHTMAP
): Promise<Record<string, unknown>> {
  const payload = {
    quality: toString(args.quality) || 'High',
    buildOnlySelected: toBoolean(args.buildOnlySelected) || false,
    buildReflectionCaptures: toBoolean(args.buildReflectionCaptures) !== false,
    levelPath: toString(args.levelPath)
  };

  return (await executeAutomationRequest(tools, automationAction, payload, 'Automation bridge not available for lighting build')) as Record<string, unknown>;
}

/**
 * Configure world-level CPU Lightmass and volumetric-lightmap settings.
 * The native handler applies only fields supplied by the caller and returns a
 * complete readback, which makes this safe to use for incremental tuning.
 */
async function configureLightmassSettings(
  tools: ITools,
  args: LightingArgs,
  action: string = TOOL_ACTIONS.CONFIGURE_LIGHTMASS_SETTINGS
): Promise<Record<string, unknown>> {
  const environmentColor = toColor3(args.environmentColor);
  const payload: Record<string, unknown> = {
    staticLightingLevelScale: toNumber(args.staticLightingLevelScale),
    numIndirectLightingBounces: toNumber(args.numIndirectLightingBounces),
    numSkyLightingBounces: toNumber(args.numSkyLightingBounces),
    indirectLightingQuality: toNumber(args.indirectLightingQuality),
    indirectLightingSmoothness: toNumber(args.indirectLightingSmoothness),
    environmentColor: environmentColor
      ? { r: environmentColor[0], g: environmentColor[1], b: environmentColor[2], a: 1 }
      : undefined,
    environmentIntensity: toNumber(args.environmentIntensity),
    diffuseBoost: toNumber(args.diffuseBoost),
    emissiveBoost: toNumber(args.emissiveBoost),
    volumeLightingMethod: toString(args.volumeLightingMethod),
    useAmbientOcclusion: toBoolean(args.useAmbientOcclusion),
    generateAmbientOcclusionMaterialMask: toBoolean(args.generateAmbientOcclusionMaterialMask),
    visualizeMaterialDiffuse: toBoolean(args.visualizeMaterialDiffuse),
    visualizeAmbientOcclusion: toBoolean(args.visualizeAmbientOcclusion),
    compressLightmaps: toBoolean(args.compressLightmaps),
    volumetricLightmapDetailCellSize: toNumber(args.volumetricLightmapDetailCellSize),
    volumetricLightmapMaximumBrickMemoryMb: toNumber(args.volumetricLightmapMaximumBrickMemoryMb),
    volumetricLightmapLoadingCellSize: toNumber(args.volumetricLightmapLoadingCellSize),
    volumetricLightmapSphericalHarmonicSmoothing: toNumber(args.volumetricLightmapSphericalHarmonicSmoothing),
    volumeLightSamplePlacementScale: toNumber(args.volumeLightSamplePlacementScale),
    directIlluminationOcclusionFraction: toNumber(args.directIlluminationOcclusionFraction),
    indirectIlluminationOcclusionFraction: toNumber(args.indirectIlluminationOcclusionFraction),
    occlusionExponent: toNumber(args.occlusionExponent),
    fullyOccludedSamplesFraction: toNumber(args.fullyOccludedSamplesFraction),
    maxOcclusionDistance: toNumber(args.maxOcclusionDistance)
  };

  return cleanObject(await executeAutomationRequest(tools, action, payload, 'Automation bridge not available for Lightmass settings')) as Record<string, unknown>;
}

/** Configure the legacy indirect-lighting cache used by movable primitives. */
async function configureIndirectLightingCache(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const payload = {
    enabled: toBoolean(args.enabled),
    updateEveryFrame: toBoolean(args.updateEveryFrame),
    lightingCacheDimension: toNumber(args.lightingCacheDimension),
    movableObjectAllocationSize: toNumber(args.movableObjectAllocationSize)
  };

  return cleanObject(await executeAutomationRequest(
    tools,
    TOOL_ACTIONS.CONFIGURE_INDIRECT_LIGHTING_CACHE,
    payload,
    'Automation bridge not available for indirect lighting cache configuration'
  )) as Record<string, unknown>;
}

/** Reflection capture, planar reflection, SSR, and Lumen reflection controls. */
async function configureReflectionAction(
  tools: ITools,
  action: string,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const location = toLocationObj(args.location);
  const rotation = toRotationObj(args.rotation);
  const captureOffset = toLocationObj(args.captureOffset);
  const target = args.captureName || args.lightName || args.actorName || args.actorPath || args.name;
  const payload: Record<string, unknown> = {
    name: args.name,
    captureName: args.captureName,
    target,
    location,
    rotation,
    captureOffset,
    influenceRadius: toNumber(args.influenceRadius),
    boxTransitionDistance: toNumber(args.boxTransitionDistance),
    captureResolution: toNumber(args.captureResolution),
    sourceCubemapAngle: toNumber(args.sourceCubemapAngle),
    brightness: toNumber(args.brightness),
    runtimeCapture: toBoolean(args.runtimeCapture),
    maxViewDistance: toNumber(args.maxViewDistance),
    fastRender: toBoolean(args.fastRender),
    smoothBlend: toBoolean(args.smoothBlend),
    normalDistortionStrength: toNumber(args.normalDistortionStrength),
    prefilterRoughness: toNumber(args.prefilterRoughness),
    prefilterRoughnessDistance: toNumber(args.prefilterRoughnessDistance),
    distanceFromPlaneFadeoutStart: toNumber(args.distanceFromPlaneFadeoutStart),
    distanceFromPlaneFadeoutEnd: toNumber(args.distanceFromPlaneFadeoutEnd),
    angleFromPlaneFadeStart: toNumber(args.angleFromPlaneFadeStart),
    angleFromPlaneFadeEnd: toNumber(args.angleFromPlaneFadeEnd),
    screenPercentage: toNumber(args.screenPercentage),
    extraFOV: toNumber(args.extraFOV),
    renderSceneTwoSided: toBoolean(args.renderSceneTwoSided),
    showPreviewPlane: toBoolean(args.showPreviewPlane),
    ssrEnabled: args.ssrEnabled === undefined ? toBoolean(args.enabled) : toBoolean(args.ssrEnabled),
    ssrIntensity: toNumber(args.ssrIntensity),
    ssrQuality: toNumber(args.ssrQuality),
    ssrMaxRoughness: toNumber(args.ssrMaxRoughness),
    lumenReflectionsEnabled: args.lumenReflectionsEnabled === undefined ? toBoolean(args.enabled) : toBoolean(args.lumenReflectionsEnabled),
    lumenReflectionQuality: toNumber(args.lumenReflectionQuality),
    lumenReflectionMaxRoughness: toNumber(args.lumenReflectionMaxRoughness),
    lumenReflectionMaxBounces: toNumber(args.lumenReflectionMaxBounces),
    lumenReflectionDownsampleFactor: toNumber(args.lumenReflectionDownsampleFactor),
    lumenReflectionScreenTraces: toBoolean(args.lumenReflectionScreenTraces),
    lumenReflectionDownsampleCheckerboard: toBoolean(args.lumenReflectionDownsampleCheckerboard),
    captureType: toString(args.captureType)
  };

  return cleanObject(await executeAutomationRequest(
    tools,
    action,
    payload,
    `Automation bridge not available for ${action}`
  )) as Record<string, unknown>;
}

/** Post Process Volume and FPostProcessSettings controls (Phase 29.5). */
async function configurePostProcessAction(
  tools: ITools,
  action: string,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const toNumberArray = (value: unknown): number[] | undefined => {
    if (!Array.isArray(value)) return undefined;
    return value.filter((item): item is number => typeof item === 'number' && Number.isFinite(item));
  };
  const target = args.volumeName || args.volumePath || args.actorName || args.actorPath || args.name;
  const payload: Record<string, unknown> = {
    name: args.name,
    target,
    volumeName: args.volumeName,
    volumePath: args.volumePath,
    location: toLocationObj(args.location),
    rotation: toRotationObj(args.rotation),
    extent: toLocationObj(args.extent),
    bUnbound: toBoolean(args.bUnbound),
    blendRadius: toNumber(args.blendRadius),
    blendWeight: toNumber(args.blendWeight),
    priority: toNumber(args.priority),
    enabled: toBoolean(args.enabled),
    bloomIntensity: toNumber(args.bloomIntensity),
    bloomThreshold: toNumber(args.bloomThreshold),
    bloomSizeScale: toNumber(args.bloomSizeScale),
    bloomMethod: toString(args.bloomMethod),
    lensFlareIntensity: toNumber(args.lensFlareIntensity),
    lensFlareBokehSize: toNumber(args.lensFlareBokehSize),
    lensFlareThreshold: toNumber(args.lensFlareThreshold),
    dofMethod: toString(args.dofMethod),
    dofFocalDistance: toNumber(args.dofFocalDistance),
    dofFocalRegion: toNumber(args.dofFocalRegion),
    dofFstop: toNumber(args.dofFstop),
    dofMinFstop: toNumber(args.dofMinFstop),
    dofNearBlurSize: toNumber(args.dofNearBlurSize),
    dofFarBlurSize: toNumber(args.dofFarBlurSize),
    dofNearTransitionRegion: toNumber(args.dofNearTransitionRegion),
    dofFarTransitionRegion: toNumber(args.dofFarTransitionRegion),
    dofScale: toNumber(args.dofScale),
    dofBladeCount: toNumber(args.dofBladeCount),
    motionBlurAmount: toNumber(args.motionBlurAmount),
    motionBlurMax: toNumber(args.motionBlurMax),
    motionBlurTargetFPS: toNumber(args.motionBlurTargetFPS),
    motionBlurPerObjectSize: toNumber(args.motionBlurPerObjectSize),
    exposureMethod: toString(args.exposureMethod),
    exposureCompensation: toNumber(args.exposureCompensation),
    exposureMinBrightness: toNumber(args.exposureMinBrightness),
    exposureMaxBrightness: toNumber(args.exposureMaxBrightness),
    exposureSpeedUp: toNumber(args.exposureSpeedUp),
    exposureSpeedDown: toNumber(args.exposureSpeedDown),
    exposureLowPercent: toNumber(args.exposureLowPercent),
    exposureHighPercent: toNumber(args.exposureHighPercent),
    whiteBalanceTemperature: toNumber(args.whiteBalanceTemperature),
    whiteBalanceTint: toNumber(args.whiteBalanceTint),
    colorSaturation: toNumberArray(args.colorSaturation),
    colorContrast: toNumberArray(args.colorContrast),
    colorGamma: toNumberArray(args.colorGamma),
    colorGain: toNumberArray(args.colorGain),
    colorOffset: toNumberArray(args.colorOffset),
    lutPath: toString(args.lutPath),
    lutIntensity: toNumber(args.lutIntensity),
    toneCurveAmount: toNumber(args.toneCurveAmount),
    expandGamut: toNumber(args.expandGamut),
    filmBlackClip: toNumber(args.filmBlackClip),
    filmWhiteClip: toNumber(args.filmWhiteClip),
    tonemapperType: toNumber(args.tonemapperType),
    ssaoIntensity: toNumber(args.ssaoIntensity),
    ssaoRadius: toNumber(args.ssaoRadius),
    ssaoPower: toNumber(args.ssaoPower),
    ssaoBias: toNumber(args.ssaoBias),
    ssaoDistance: toNumber(args.ssaoDistance),
    ssaoStaticFraction: toNumber(args.ssaoStaticFraction),
    ssaoFadeDistance: toNumber(args.ssaoFadeDistance),
    gtaoIntensity: toNumber(args.gtaoIntensity),
    gtaoRadius: toNumber(args.gtaoRadius),
    gtaoPower: toNumber(args.gtaoPower),
    gtaoThickness: toNumber(args.gtaoThickness),
    vignetteIntensity: toNumber(args.vignetteIntensity),
    chromaticAberrationIntensity: toNumber(args.chromaticAberrationIntensity),
    grainIntensity: toNumber(args.grainIntensity),
    screenPercentage: toNumber(args.screenPercentage)
  };

  return cleanObject(await executeAutomationRequest(
    tools,
    action,
    payload,
    `Automation bridge not available for ${action}`
  )) as Record<string, unknown>;
}

/** Scene Capture 2D/cube and render-target controls (Phase 29.6). */
async function configureSceneCaptureAction(
  tools: ITools,
  action: string,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const toStringArray = (value: unknown): string[] | undefined => {
    if (!Array.isArray(value)) return undefined;
    return value.filter((item): item is string => typeof item === 'string' && item.length > 0);
  };
  const toNumberArray = (value: unknown): number[] | undefined => {
    if (!Array.isArray(value)) return undefined;
    return value.filter((item): item is number => typeof item === 'number' && Number.isFinite(item));
  };
  const target = args.sceneCaptureName || args.sceneCapturePath || args.captureName || args.actorName || args.actorPath || args.name;
  const payload: Record<string, unknown> = {
    name: args.name,
    target,
    sceneCaptureName: args.sceneCaptureName,
    sceneCapturePath: args.sceneCapturePath,
    renderTargetPath: toString(args.renderTargetPath),
    renderTargetName: toString(args.renderTargetName),
    location: toLocationObj(args.location),
    rotation: toRotationObj(args.rotation),
    captureSource: toString(args.captureSource),
    projectionType: toString(args.projectionType),
    fovAngle: toNumber(args.fovAngle),
    orthoWidth: toNumber(args.orthoWidth),
    captureEveryFrame: toBoolean(args.captureEveryFrame),
    captureOnMovement: toBoolean(args.captureOnMovement),
    alwaysPersistRenderingState: toBoolean(args.alwaysPersistRenderingState),
    captureRotation: toBoolean(args.captureRotation),
    captureDeferred: toBoolean(args.captureDeferred),
    capturePriority: toNumber(args.capturePriority),
    width: toNumber(args.width),
    height: toNumber(args.height),
    captureResolution: toNumber(args.captureResolution),
    format: toString(args.format),
    forceLinearGamma: toBoolean(args.forceLinearGamma),
    autoGenerateMips: toBoolean(args.autoGenerateMips),
    supportsUAV: toBoolean(args.supportsUAV),
    hdr: toBoolean(args.hdr),
    clearColor: toNumberArray(args.clearColor),
    hiddenActors: toStringArray(args.hiddenActors),
    showOnlyActors: toStringArray(args.showOnlyActors),
    postProcessBlendWeight: toNumber(args.postProcessBlendWeight)
  };

  return cleanObject(await executeAutomationRequest(
    tools,
    action,
    payload,
    `Automation bridge not available for ${action}`
  )) as Record<string, unknown>;
}

/**
 * Create lighting enabled level
 */
async function createLightingEnabledLevel(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const levelName = toString(args.levelName) || 'LightingEnabledLevel';
  let path = toString(args.path);
  if (!path) path = `/Game/Maps/${levelName}`;

  const payload = {
    path,
    levelName,
    copyActors: toBoolean(args.copyActors) === true,
    useTemplate: toBoolean(args.useTemplate) === true
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.CREATE_LIGHTING_ENABLED_LEVEL, payload, 'Automation bridge not available for level creation')) as Record<string, unknown>;
}

/**
 * Create lightmass volume
 */
async function createLightmassVolume(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const name = normalizeLightName(args.name);
  const location = toLocationObj(args.location) || { x: 0, y: 0, z: 0 };
  const size = toLocationObj(args.size) || { x: 1000, y: 1000, z: 1000 };

  const payload = { name, location, size };
  return (await executeAutomationRequest(tools, TOOL_ACTIONS.CREATE_LIGHTMASS_VOLUME, payload, 'Automation bridge not available for lightmass volume creation')) as Record<string, unknown>;
}

/**
 * Set exposure
 */
async function setExposure(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const payload = {
    method: toString(args.method),
    compensationValue: toNumber(args.compensationValue),
    minBrightness: toNumber(args.minBrightness),
    maxBrightness: toNumber(args.maxBrightness)
  };

  const result = await executeAutomationRequest(tools, TOOL_ACTIONS.SET_EXPOSURE, payload);

  // Fallback to console commands
  const resultObj = result as Record<string, unknown>;
  if (resultObj.success === false && typeof resultObj.error === 'string' &&
      (resultObj.error.includes('not available') || resultObj.error.includes('Connection'))) {
    const commands: string[] = [];

    commands.push(`r.EyeAdaptation.ExposureMethod ${args.method === 'Manual' ? 0 : 1}`);

    if (args.compensationValue !== undefined) {
      commands.push(`r.EyeAdaptation.ExposureCompensation ${args.compensationValue}`);
    }
    if (args.minBrightness !== undefined) {
      commands.push(`r.EyeAdaptation.MinBrightness ${args.minBrightness}`);
    }
    if (args.maxBrightness !== undefined) {
      commands.push(`r.EyeAdaptation.MaxBrightness ${args.maxBrightness}`);
    }

    // Use batch execution for all console commands - significantly faster than sequential
    if (commands.length > 0) {
      await executeBatchConsoleCommands(tools, commands);
    }

    return { success: true, message: 'Exposure settings updated (console)' };
  }

  return cleanObject(result) as Record<string, unknown>;
}

/**
 * Set ambient occlusion
 */
async function setAmbientOcclusion(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const enabled = args.enabled !== false;
  const payload = {
    enabled,
    intensity: toNumber(args.intensity),
    radius: toNumber(args.radius),
    quality: toString(args.quality)
  };

  const result = await executeAutomationRequest(tools, TOOL_ACTIONS.SET_AMBIENT_OCCLUSION, payload);

  // Fallback to console commands
  const resultObj = result as Record<string, unknown>;
  if (resultObj.success === false && typeof resultObj.error === 'string' &&
      (resultObj.error.includes('not available') || resultObj.error.includes('Connection'))) {
    const commands: string[] = [];

    commands.push(`r.AmbientOcclusion.Enabled ${enabled ? 1 : 0}`);

    if (args.intensity !== undefined) {
      commands.push(`r.AmbientOcclusion.Intensity ${args.intensity}`);
    }
    if (args.radius !== undefined) {
      commands.push(`r.AmbientOcclusion.Radius ${args.radius}`);
    }
    if (args.quality) {
      const qualityMap: Record<string, number> = { 'Low': 0, 'Medium': 1, 'High': 2 };
      commands.push(`r.AmbientOcclusion.Quality ${qualityMap[args.quality] ?? 1}`);
    }

    // Use batch execution for all console commands - significantly faster than sequential
    if (commands.length > 0) {
      await executeBatchConsoleCommands(tools, commands);
    }

    return { success: true, message: 'Ambient occlusion configured (console)' };
  }

  return cleanObject(result) as Record<string, unknown>;
}

/**
 * Configure one of the engine's hardware ray-tracing pipelines.
 *
 * The native bridge owns the CVar application so the same action works from
 * both the consolidated build_environment tool and direct bridge requests.
 */
async function configureRayTracing(
  tools: ITools,
  action: string,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const enabled = toBoolean(
    action === 'configure_ray_traced_shadows' ? args.rayTracedShadows :
        action === 'configure_ray_traced_gi' ? args.rayTracedGI :
          action === 'configure_ray_traced_reflections' ? args.rayTracedReflections :
            action === 'configure_ray_traced_ao' ? args.rayTracedAO :
              action === 'configure_ray_traced_translucency' ? args.rayTracedTranslucency : args.pathTracing
  );
  const samplesPerPixel = toNumber(args.samplesPerPixel);
  const maxBounces = toNumber(args.maxBounces);
  const denoiser = toBoolean(args.denoiser);
  const radius = toNumber(args.aoRadius ?? args.radius);
  const intensity = toNumber(args.aoIntensity);
  const refraction = toBoolean(args.refraction);
  const refractionRays = toNumber(args.refractionRays);
  const maxRoughness = toNumber(args.maxRoughness);
  const spatialDenoiserType = toNumber(args.spatialDenoiserType);
  const cullingMode = toNumber(args.cullingMode);
  const cullingRadius = toNumber(args.cullingRadius);
  const cullingAngle = toNumber(args.cullingAngle);
  const maxUpdatePrimitivesPerFrame = toNumber(args.maxUpdatePrimitivesPerFrame);
  const residentGeometryMemoryPoolSizeInMB = toNumber(args.residentGeometryMemoryPoolSizeInMB);

  for (const [field, value] of Object.entries({ samplesPerPixel, maxBounces, radius, intensity, refractionRays, spatialDenoiserType, maxUpdatePrimitivesPerFrame, residentGeometryMemoryPoolSizeInMB })) {
    if (value !== undefined && (!Number.isFinite(value) || value < 0)) {
      return {
        success: false,
        isError: true,
        error: 'INVALID_ARGUMENT',
        message: `${field} must be a finite, non-negative number`
      };
    }
  }
  if (maxRoughness !== undefined && (maxRoughness < 0 || maxRoughness > 1)) {
    return { success: false, isError: true, error: 'INVALID_ARGUMENT', message: 'maxRoughness must be between 0 and 1' };
  }
  if (cullingMode !== undefined && (!Number.isInteger(cullingMode) || cullingMode < 0 || cullingMode > 3)) {
    return { success: false, isError: true, error: 'INVALID_ARGUMENT', message: 'cullingMode must be an integer from 0 to 3' };
  }
  if (cullingRadius !== undefined && cullingRadius < -1) {
    return { success: false, isError: true, error: 'INVALID_ARGUMENT', message: 'cullingRadius must be -1 or non-negative' };
  }
  if (cullingAngle !== undefined && cullingAngle < 0) {
    return { success: false, isError: true, error: 'INVALID_ARGUMENT', message: 'cullingAngle must be non-negative' };
  }
  if (spatialDenoiserType !== undefined && (!Number.isInteger(spatialDenoiserType) || spatialDenoiserType < 0 || spatialDenoiserType > 1)) {
    return { success: false, isError: true, error: 'INVALID_ARGUMENT', message: 'spatialDenoiserType must be 0 (spatial) or 1 (temporal)' };
  }

  const payload: Record<string, unknown> = {
    enabled: enabled ?? true,
    samplesPerPixel,
    maxBounces,
    denoiser,
    radius,
    intensity,
    refraction,
    refractionRays,
    maxRoughness,
    includeTranslucentObjects: toBoolean(args.includeTranslucentObjects),
    spatialDenoiserType,
    cullingMode,
    cullingRadius,
    cullingAngle,
    geometry: args.geometry,
    maxUpdatePrimitivesPerFrame,
    priorityBasedUpdate: toBoolean(args.priorityBasedUpdate),
    useTracingFeedback: toBoolean(args.useTracingFeedback),
    useReferenceBasedResidency: toBoolean(args.useReferenceBasedResidency),
    residentGeometryMemoryPoolSizeInMB,
    compactInstances: toBoolean(args.compactInstances),
    reflectionCaptures: toBoolean(args.reflectionCaptures)
  };

  return cleanObject(await executeAutomationRequest(
    tools,
    action,
    payload,
    `Automation bridge not available for ${action}`
  )) as Record<string, unknown>;
}

/** Configure or inspect the three Unreal lighting channels on lights/actors. */
async function configureLightChannels(
  tools: ITools,
  action: string,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const channel = toNumber(args.channel);
  if (action !== 'get_light_channels' && channel === undefined && !args.channels) {
    return { success: false, isError: true, error: 'INVALID_ARGUMENT', message: 'Provide channel plus enabled, or a channels object with channel0/channel1/channel2.' };
  }
  if (channel !== undefined && (!Number.isInteger(channel) || channel < 0 || channel > 2)) {
    return { success: false, isError: true, error: 'INVALID_ARGUMENT', message: 'channel must be an integer from 0 to 2' };
  }

  const payload: Record<string, unknown> = {
    lightName: args.lightName,
    lightPath: args.lightPath,
    actorName: args.actorName,
    actorPath: args.actorPath,
    name: args.name,
    channel,
    enabled: toBoolean(args.enabled),
    channels: args.channels,
    componentName: args.componentName,
    applyToAllComponents: toBoolean(args.applyToAllComponents)
  };
  return cleanObject(await executeAutomationRequest(
    tools,
    action,
    payload,
    `Automation bridge not available for ${action}`
  )) as Record<string, unknown>;
}

/**
 * Setup volumetric fog
 */
async function setupVolumetricFog(
  tools: ITools,
  args: LightingArgs
): Promise<Record<string, unknown>> {
  const enabled = args.enabled !== false;

  // Enable/disable global volumetric fog via CVar
  await executeAutomationRequest(tools, TOOL_ACTIONS.CONSOLE_COMMAND, { command: `r.VolumetricFog ${enabled ? 1 : 0}` });

  const payload = {
    enabled,
    density: toNumber(args.density),
    scatteringIntensity: toNumber(args.scatteringIntensity),
    fogHeight: toNumber(args.fogHeight)
  };

  return (await executeAutomationRequest(tools, TOOL_ACTIONS.SETUP_VOLUMETRIC_FOG, payload, 'Volumetric fog console setting applied (plugin required for fog actor adjustment)')) as Record<string, unknown>;
}

/**
 * List light types
 */
async function listLightTypes(tools: ITools): Promise<Record<string, unknown>> {
  return (await executeAutomationRequest(tools, TOOL_ACTIONS.LIST_LIGHT_TYPES, {}, 'Automation bridge not available for listing light types')) as Record<string, unknown>;
}

/**
 * Main handler for lighting tools
 */
export async function handleLightingTools(action: string, args: LightingArgs, tools: ITools): Promise<Record<string, unknown>> {
  switch (action) {
    case 'spawn_light':
    case 'create_light': {
      // Map generic create_light to specific types
      let lightType = args.lightType ? String(args.lightType).toLowerCase() : 'point';

      // Normalize class names to short names
      if (lightType.endsWith('light') && lightType !== 'light') {
        lightType = lightType.replace(/light$/, '');
      }

      // Validate light type
      if (!VALID_LIGHT_TYPES.includes(lightType) && !VALID_LIGHT_TYPES.includes(lightType + 'light')) {
        return {
          success: false,
          isError: true,
          error: 'INVALID_LIGHT_TYPE',
          message: `Invalid lightType: '${args.lightType}'. Must be one of: point, directional, spot, rect, sky`
        };
      }

      // Route to specific handler
      if (lightType === 'directional') {
        return createDirectionalLight(tools, args);
      } else if (lightType === 'spot') {
        return createSpotLight(tools, args);
      } else if (lightType === 'rect') {
        return createRectLight(tools, args);
      } else if (lightType === 'sky') {
        return createSkyLight(tools, args);
      } else {
        // Default to Point
        return createPointLight(tools, args);
      }
    }

    case 'create_dynamic_light':
      return createDynamicLight(tools, args);

    case 'spawn_sky_light':
    case 'create_sky_light':
      return cleanObject(await createSkyLight(tools, args));

    case 'ensure_single_sky_light':
      return cleanObject(await ensureSingleSkyLight(tools, args));

    case 'create_lightmass_volume':
      return cleanObject(await createLightmassVolume(tools, args));

    case 'setup_volumetric_fog':
      return cleanObject(await setupVolumetricFog(tools, args));

    case 'setup_global_illumination':
      return cleanObject(await setupGlobalIllumination(tools, args));

    case 'configure_shadows':
      return cleanObject(await configureShadows(tools, args));

    case 'set_exposure':
      return cleanObject(await setExposure(tools, args));

    case 'set_ambient_occlusion':
      return cleanObject(await setAmbientOcclusion(tools, args));

    case 'configure_ray_traced_shadows':
    case 'configure_ray_traced_gi':
    case 'configure_ray_traced_reflections':
    case 'configure_ray_traced_ao':
    case 'configure_path_tracing':
    case 'configure_ray_traced_translucency':
    case 'configure_ray_tracing_quality':
      return configureRayTracing(tools, action, args);

    case 'set_light_channel':
    case 'set_actor_light_channel':
    case 'get_light_channels':
      return configureLightChannels(tools, action, args);

    case 'configure_lightmass_settings':
      return configureLightmassSettings(tools, args);

    case 'configure_volumetric_lightmaps':
      return configureLightmassSettings(tools, args, TOOL_ACTIONS.CONFIGURE_VOLUMETRIC_LIGHTMAPS);

    case 'configure_lightmass_ambient_occlusion':
      return configureLightmassSettings(tools, args, TOOL_ACTIONS.CONFIGURE_LIGHTMASS_AMBIENT_OCCLUSION);

    case 'inspect_lightmass_settings':
      return cleanObject(await executeAutomationRequest(tools, TOOL_ACTIONS.INSPECT_LIGHTMASS_SETTINGS, {})) as Record<string, unknown>;

    case 'configure_indirect_lighting_cache':
      return configureIndirectLightingCache(tools, args);

    case 'create_sphere_reflection_capture':
    case 'create_box_reflection_capture':
    case 'configure_capture_resolution':
    case 'configure_capture_offset':
    case 'recapture_scene':
    case 'create_planar_reflection':
    case 'configure_planar_reflection':
    case 'configure_ssr_settings':
    case 'configure_lumen_reflection_settings':
    case 'inspect_reflection_captures':
      return configureReflectionAction(tools, action, args);

    case 'create_post_process_volume':
    case 'configure_pp_blend':
    case 'set_pp_white_balance':
    case 'set_pp_color_grading':
    case 'set_pp_lut':
    case 'configure_tonemapper':
    case 'set_tonemapper_type':
    case 'configure_bloom':
    case 'set_bloom_intensity':
    case 'set_bloom_threshold':
    case 'configure_lens_flare':
    case 'configure_dof':
    case 'set_dof_method':
    case 'set_focal_distance':
    case 'set_aperture':
    case 'configure_bokeh':
    case 'configure_motion_blur':
    case 'set_motion_blur_amount':
    case 'set_motion_blur_max':
    case 'configure_exposure':
    case 'set_exposure_method':
    case 'set_exposure_compensation':
    case 'set_exposure_min_max':
    case 'configure_ssao':
    case 'configure_gtao':
    case 'configure_vignette':
    case 'configure_chromatic_aberration':
    case 'configure_grain':
    case 'configure_screen_percentage':
    case 'inspect_post_process_volume':
      return configurePostProcessAction(tools, action, args);

    case 'create_scene_capture_2d':
    case 'create_scene_capture_cube':
    case 'create_render_target_cube':
    case 'configure_scene_capture':
    case 'configure_scene_capture_resolution':
    case 'configure_capture_source':
    case 'assign_render_target':
    case 'capture_scene':
    case 'inspect_scene_captures':
      return configureSceneCaptureAction(tools, action, args);

    case 'build_lighting':
      return cleanObject(await buildLighting(tools, args));

    case 'build_lighting_quality':
      return cleanObject(await buildLighting(tools, args, TOOL_ACTIONS.BUILD_LIGHTING_QUALITY));

    case 'create_lighting_enabled_level':
      return cleanObject(await createLightingEnabledLevel(tools, args));

    case 'list_light_types':
      return cleanObject(await listLightTypes(tools));

    default:
      return ResponseFactory.error(`Unknown lighting action: ${action}`);
  }
}
