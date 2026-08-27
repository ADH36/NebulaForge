#!/usr/bin/env node
/**
 * Advanced lighting channel + ray-tracing configuration integration tests.
 * Covers the UE 5.8 phase-29.1/29.2/29.3 build_environment actions: per-light ray-traced
 * feature toggles, path tracing, global ray-tracing quality knobs, and light
 * channel assignment/inspection across one or all components, plus Lightmass
 * settings, volumetric lightmaps, and indirect lighting cache controls.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const DEMO_LIGHT_NAME = `MCP_DirectionalLight_${ts}`;

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: spawn directional demo light for channel tests', toolName: 'build_environment', arguments: { action: 'create_dynamic_light', name: DEMO_LIGHT_NAME, lightType: 'Directional', intensity: 5 }, expected: 'success|already exists' },

  // === RAY-TRACED FEATURE CONFIGURATION ===
  { scenario: 'RT: configure_ray_traced_shadows toggles shadows and culling radius', toolName: 'build_environment', arguments: { action: 'configure_ray_traced_shadows', lightName: DEMO_LIGHT_NAME, rayTracedShadows: true, cullingRadius: 10000 }, expected: 'success' },
  { scenario: 'RT: configure_ray_traced_gi with bounce budget', toolName: 'build_environment', arguments: { action: 'configure_ray_traced_gi', rayTracedGI: true, maxBounces: 2 }, expected: 'success' },
  { scenario: 'RT: configure_ray_traced_reflections roughness cutoff', toolName: 'build_environment', arguments: { action: 'configure_ray_traced_reflections', rayTracedReflections: true, maxRoughness: 0.6 }, expected: 'success' },
  { scenario: 'RT: configure_ray_traced_ao with ambient occlusion tuning', toolName: 'build_environment', arguments: { action: 'configure_ray_traced_ao', rayTracedAO: true, aoRadius: 50, aoIntensity: 1 }, expected: 'success' },
  { scenario: 'RT: configure_path_tracing sample budget and denoiser', toolName: 'build_environment', arguments: { action: 'configure_path_tracing', pathTracing: true, samplesPerPixel: 1024, denoiser: true }, expected: 'success' },
  { scenario: 'RT: configure_ray_traced_translucency refraction and denoiser channels', toolName: 'build_environment', arguments: { action: 'configure_ray_traced_translucency', rayTracedTranslucency: true, includeTranslucentObjects: true, refraction: true, refractionRays: 2, spatialDenoiserType: 1, maxBounces: 3 }, expected: 'success' },

  // === GLOBAL QUALITY / RESIDENCY ===
  { scenario: 'RT: configure_ray_tracing_quality residency and feedback knobs', toolName: 'build_environment', arguments: { action: 'configure_ray_tracing_quality', geometry: { mode: 'dynamic' }, useReferenceBasedResidency: true, residentGeometryMemoryPoolSizeInMB: 2048, compactInstances: true, priorityBasedUpdate: true, useTracingFeedback: true, reflectionCaptures: true, includeTranslucentObjects: true, maxUpdatePrimitivesPerFrame: 64, cullingMode: 1, cullingAngle: 30, cullingRadius: 25000 }, expected: 'success' },

  // === LIGHT CHANNELS ===
  { scenario: 'CHANNEL: set_light_channel assigns a numeric channel to a named light', toolName: 'build_environment', arguments: { action: 'set_light_channel', lightName: DEMO_LIGHT_NAME, channel: 1 }, expected: 'success' },
  { scenario: 'CHANNEL: set_actor_light_channel applies the channel map to actor components', toolName: 'build_environment', arguments: { action: 'set_actor_light_channel', actorName: DEMO_LIGHT_NAME, channels: { 0: true, 1: false, 2: false }, applyToAllComponents: true }, expected: 'success' },
  { scenario: 'CHANNEL: get_light_channels reports per-actor channel state via lightPath', toolName: 'build_environment', arguments: { action: 'get_light_channels', lightPath: `${DEMO_LIGHT_NAME}` }, expected: 'success|not found' },

  // === LIGHTMASS / PRECOMPUTED LIGHTING ===
  { scenario: 'LIGHTMASS: configure world bounces and volumetric lightmaps', toolName: 'build_environment', arguments: { action: 'configure_lightmass_settings', staticLightingLevelScale: 1, numIndirectLightingBounces: 3, indirectLightingQuality: 1, volumeLightingMethod: 'VolumetricLightmap', volumetricLightmapDetailCellSize: 200 }, expected: 'success' },
  { scenario: 'LIGHTMASS: configure full volumetric-lightmap quality and memory settings', toolName: 'build_environment', arguments: { action: 'configure_volumetric_lightmaps', volumeLightingMethod: 'VolumetricLightmap', environmentColor: [1, 1, 1], environmentIntensity: 1, diffuseBoost: 1, emissiveBoost: 1, indirectLightingSmoothness: 1, numSkyLightingBounces: 1, compressLightmaps: true, volumetricLightmapMaximumBrickMemoryMb: 30, volumetricLightmapLoadingCellSize: 1600, volumetricLightmapSphericalHarmonicSmoothing: 0.02, volumeLightSamplePlacementScale: 1, generateAmbientOcclusionMaterialMask: false, visualizeMaterialDiffuse: false, visualizeAmbientOcclusion: false, fullyOccludedSamplesFraction: 1, maxOcclusionDistance: 200 }, expected: 'success' },
  { scenario: 'LIGHTMASS: configure baked ambient occlusion', toolName: 'build_environment', arguments: { action: 'configure_lightmass_ambient_occlusion', useAmbientOcclusion: true, directIlluminationOcclusionFraction: 0.5, indirectIlluminationOcclusionFraction: 1, occlusionExponent: 1 }, expected: 'success' },
  { scenario: 'LIGHTMASS: configure indirect lighting cache', toolName: 'build_environment', arguments: { action: 'configure_indirect_lighting_cache', enabled: true, updateEveryFrame: false, lightingCacheDimension: 64, movableObjectAllocationSize: 5 }, expected: 'success' },
  { scenario: 'LIGHTMASS: inspect current settings', toolName: 'build_environment', arguments: { action: 'inspect_lightmass_settings' }, expected: 'success' },
  { scenario: 'LIGHTMASS: start a preview quality build', toolName: 'build_environment', arguments: { action: 'build_lighting_quality', quality: 'Preview' }, expected: 'success|already exists' },

  // === REFLECTION CAPTURES / DYNAMIC REFLECTIONS ===
  { scenario: 'REFLECTION: configure capture cubemap resolution', toolName: 'build_environment', arguments: { action: 'configure_capture_resolution', captureResolution: 256 }, expected: 'success' },
  { scenario: 'REFLECTION: create sphere capture', toolName: 'build_environment', arguments: { action: 'create_sphere_reflection_capture', name: `${DEMO_LIGHT_NAME}_SphereCapture`, location: { x: 0, y: 0, z: 200 }, captureOffset: { x: 10, y: 20, z: 30 }, influenceRadius: 1500, brightness: 1, sourceCubemapAngle: 0, runtimeCapture: false, maxViewDistance: 0 }, expected: 'success' },
  { scenario: 'REFLECTION: create box capture with transition', toolName: 'build_environment', arguments: { action: 'create_box_reflection_capture', name: `${DEMO_LIGHT_NAME}_BoxCapture`, location: { x: 500, y: 0, z: 200 }, size: { x: 1000, y: 1000, z: 500 }, boxTransitionDistance: 100, captureOffset: { x: 0, y: 0, z: 0 }, influenceRadius: 1000, brightness: 1, sourceCubemapAngle: 45, runtimeCapture: false, maxViewDistance: 0 }, expected: 'success' },
  { scenario: 'REFLECTION: configure capture offset', toolName: 'build_environment', arguments: { action: 'configure_capture_offset', captureName: `${DEMO_LIGHT_NAME}_SphereCapture`, captureOffset: { x: 0, y: 0, z: 50 } }, expected: 'success' },
  { scenario: 'REFLECTION: create planar reflection with full tuning', toolName: 'build_environment', arguments: { action: 'create_planar_reflection', name: `${DEMO_LIGHT_NAME}_PlanarReflection`, location: { x: 0, y: 0, z: 0 }, normalDistortionStrength: 0, prefilterRoughness: 0.04, prefilterRoughnessDistance: 1000, distanceFromPlaneFadeoutStart: 100, distanceFromPlaneFadeoutEnd: 2000, angleFromPlaneFadeStart: 0, angleFromPlaneFadeEnd: 90, screenPercentage: 75, extraFOV: 0, renderSceneTwoSided: false, showPreviewPlane: true }, expected: 'success' },
  { scenario: 'REFLECTION: configure planar reflection', toolName: 'build_environment', arguments: { action: 'configure_planar_reflection', captureName: `${DEMO_LIGHT_NAME}_PlanarReflection`, normalDistortionStrength: 0.1, prefilterRoughness: 0.02, screenPercentage: 80, renderSceneTwoSided: true, showPreviewPlane: false }, expected: 'success' },
  { scenario: 'REFLECTION: configure SSR settings', toolName: 'build_environment', arguments: { action: 'configure_ssr_settings', ssrEnabled: true, ssrIntensity: 100, ssrQuality: 50, ssrMaxRoughness: 0.8 }, expected: 'success' },
  { scenario: 'REFLECTION: configure Lumen reflection settings', toolName: 'build_environment', arguments: { action: 'configure_lumen_reflection_settings', lumenReflectionsEnabled: true, lumenReflectionQuality: 2, lumenReflectionMaxRoughness: 0.4, lumenReflectionMaxBounces: 2, lumenReflectionDownsampleFactor: 1, lumenReflectionScreenTraces: true, lumenReflectionDownsampleCheckerboard: false }, expected: 'success' },
  { scenario: 'REFLECTION: inspect reflection captures', toolName: 'build_environment', arguments: { action: 'inspect_reflection_captures' }, expected: 'success' },
  { scenario: 'REFLECTION: exercise capture type selector', toolName: 'build_environment', arguments: { action: 'inspect_reflection_captures', captureType: 'sphere' }, expected: 'success' },
  { scenario: 'REFLECTION: recapture reflection scene', toolName: 'build_environment', arguments: { action: 'recapture_scene', captureName: `${DEMO_LIGHT_NAME}_SphereCapture`, fastRender: true, smoothBlend: false }, expected: 'success' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete demo light actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: DEMO_LIGHT_NAME }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete sphere reflection capture', toolName: 'control_actor', arguments: { action: 'delete', actorName: `${DEMO_LIGHT_NAME}_SphereCapture` }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete box reflection capture', toolName: 'control_actor', arguments: { action: 'delete', actorName: `${DEMO_LIGHT_NAME}_BoxCapture` }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete planar reflection', toolName: 'control_actor', arguments: { action: 'delete', actorName: `${DEMO_LIGHT_NAME}_PlanarReflection` }, expected: 'success|not found' },
];

runToolTests('lighting-channel-raytracing-ue58', testCases);
