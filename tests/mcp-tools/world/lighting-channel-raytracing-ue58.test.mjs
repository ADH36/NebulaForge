#!/usr/bin/env node
/**
 * Advanced lighting channel + ray-tracing configuration integration tests.
 * Covers the UE 5.8 phase-29.1/29.2/29.3/29.4/29.5 build_environment actions: per-light ray-traced
 * feature toggles, path tracing, global ray-tracing quality knobs, and light
 * channel assignment/inspection across one or all components, plus Lightmass
 * settings, volumetric lightmaps, and indirect lighting cache controls.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const DEMO_LIGHT_NAME = `MCP_DirectionalLight_${ts}`;
const POST_PROCESS_VOLUME_NAME = `MCP_PostProcessVolume_${ts}`;

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

  // === POST PROCESSING / POST PROCESS VOLUMES ===
  { scenario: 'POST: create and configure full post process volume', toolName: 'build_environment', arguments: { action: 'create_post_process_volume', volumeName: POST_PROCESS_VOLUME_NAME, volumePath: `/Game/Maps/${POST_PROCESS_VOLUME_NAME}`, location: { x: 0, y: 0, z: 100 }, rotation: { pitch: 0, yaw: 0, roll: 0 }, extent: { x: 1000, y: 1000, z: 500 }, bUnbound: true, enabled: true, blendRadius: 150, blendWeight: 1, priority: 10, bloomIntensity: 0.8, bloomThreshold: 1, bloomSizeScale: 1, bloomMethod: 'Standard', lensFlareIntensity: 0.5, lensFlareBokehSize: 5, lensFlareThreshold: 1, dofMethod: 'BokehDOF', dofFocalDistance: 500, dofFocalRegion: 100, dofFstop: 2.8, dofMinFstop: 1.2, dofNearBlurSize: 2, dofFarBlurSize: 4, dofNearTransitionRegion: 100, dofFarTransitionRegion: 100, dofScale: 1, dofBladeCount: 6, motionBlurAmount: 0.2, motionBlurMax: 5, motionBlurTargetFPS: 60, motionBlurPerObjectSize: 0.1, exposureMethod: 'Manual', exposureCompensation: 0, exposureMinBrightness: 0, exposureMaxBrightness: 10, exposureSpeedUp: 3, exposureSpeedDown: 1, exposureLowPercent: 80, exposureHighPercent: 98, whiteBalanceTemperature: 6500, whiteBalanceTint: 0, colorSaturation: [1, 1, 1, 1], colorContrast: [1, 1, 1, 1], colorGamma: [1, 1, 1, 1], colorGain: [1, 1, 1, 1], colorOffset: [0, 0, 0, 0], lutIntensity: 0.5, toneCurveAmount: 1, expandGamut: 0, filmBlackClip: 0, filmWhiteClip: 0, tonemapperType: 0, ssaoIntensity: 1, ssaoRadius: 100, ssaoPower: 1, ssaoBias: 0.1, ssaoDistance: 200, ssaoStaticFraction: 0, ssaoFadeDistance: 1000, gtaoIntensity: 1, gtaoRadius: 100, gtaoPower: 1, gtaoThickness: 1, vignetteIntensity: 0.1, chromaticAberrationIntensity: 0.05, grainIntensity: 0.02, screenPercentage: 100 }, expected: 'success' },
  { scenario: 'POST: configure post process blend', toolName: 'build_environment', arguments: { action: 'configure_pp_blend', volumeName: POST_PROCESS_VOLUME_NAME, bUnbound: false, blendRadius: 250, blendWeight: 0.8, priority: 20, enabled: true }, expected: 'success' },
  { scenario: 'POST: set white balance', toolName: 'build_environment', arguments: { action: 'set_pp_white_balance', volumeName: POST_PROCESS_VOLUME_NAME, whiteBalanceTemperature: 5600, whiteBalanceTint: 0.1 }, expected: 'success' },
  { scenario: 'POST: set color grading', toolName: 'build_environment', arguments: { action: 'set_pp_color_grading', volumeName: POST_PROCESS_VOLUME_NAME, colorSaturation: [1.1, 1, 0.95, 1], colorContrast: [1.05, 1.05, 1.05, 1], colorGamma: [1, 1, 1, 1], colorGain: [1.05, 1, 0.95, 1], colorOffset: [0, 0, 0, 0] }, expected: 'success' },
  { scenario: 'POST: set color grading LUT', toolName: 'build_environment', arguments: { action: 'set_pp_lut', volumeName: POST_PROCESS_VOLUME_NAME, lutPath: '/Game/Textures/DefaultLUT', lutIntensity: 0.5 }, expected: 'success|not found|invalid' },
  { scenario: 'POST: configure tonemapper', toolName: 'build_environment', arguments: { action: 'configure_tonemapper', volumeName: POST_PROCESS_VOLUME_NAME, toneCurveAmount: 0.9, expandGamut: 0.1, filmBlackClip: 0, filmWhiteClip: 0 }, expected: 'success' },
  { scenario: 'POST: set tonemapper type', toolName: 'build_environment', arguments: { action: 'set_tonemapper_type', tonemapperType: 0 }, expected: 'success' },
  { scenario: 'POST: configure bloom', toolName: 'build_environment', arguments: { action: 'configure_bloom', volumeName: POST_PROCESS_VOLUME_NAME, bloomIntensity: 1, bloomThreshold: 1, bloomSizeScale: 1, bloomMethod: 'Standard' }, expected: 'success' },
  { scenario: 'POST: set bloom intensity', toolName: 'build_environment', arguments: { action: 'set_bloom_intensity', volumeName: POST_PROCESS_VOLUME_NAME, bloomIntensity: 0.75 }, expected: 'success' },
  { scenario: 'POST: set bloom threshold', toolName: 'build_environment', arguments: { action: 'set_bloom_threshold', volumeName: POST_PROCESS_VOLUME_NAME, bloomThreshold: 1 }, expected: 'success' },
  { scenario: 'POST: configure lens flare', toolName: 'build_environment', arguments: { action: 'configure_lens_flare', volumeName: POST_PROCESS_VOLUME_NAME, lensFlareIntensity: 0.5, lensFlareBokehSize: 5, lensFlareThreshold: 1 }, expected: 'success' },
  { scenario: 'POST: configure depth of field', toolName: 'build_environment', arguments: { action: 'configure_dof', volumeName: POST_PROCESS_VOLUME_NAME, dofMethod: 'BokehDOF', dofFocalDistance: 500, dofFocalRegion: 100, dofFstop: 2.8, dofMinFstop: 1.2, dofNearBlurSize: 2, dofFarBlurSize: 4, dofNearTransitionRegion: 100, dofFarTransitionRegion: 100, dofScale: 1, dofBladeCount: 6 }, expected: 'success' },
  { scenario: 'POST: set depth of field method', toolName: 'build_environment', arguments: { action: 'set_dof_method', volumeName: POST_PROCESS_VOLUME_NAME, dofMethod: 'BokehDOF' }, expected: 'success' },
  { scenario: 'POST: set focal distance', toolName: 'build_environment', arguments: { action: 'set_focal_distance', volumeName: POST_PROCESS_VOLUME_NAME, dofFocalDistance: 750 }, expected: 'success' },
  { scenario: 'POST: set aperture', toolName: 'build_environment', arguments: { action: 'set_aperture', volumeName: POST_PROCESS_VOLUME_NAME, dofFstop: 4 }, expected: 'success' },
  { scenario: 'POST: configure bokeh', toolName: 'build_environment', arguments: { action: 'configure_bokeh', volumeName: POST_PROCESS_VOLUME_NAME, dofBladeCount: 8, dofMinFstop: 1.4 }, expected: 'success' },
  { scenario: 'POST: configure motion blur', toolName: 'build_environment', arguments: { action: 'configure_motion_blur', volumeName: POST_PROCESS_VOLUME_NAME, motionBlurAmount: 0.15, motionBlurMax: 4, motionBlurTargetFPS: 60, motionBlurPerObjectSize: 0.1 }, expected: 'success' },
  { scenario: 'POST: set motion blur amount', toolName: 'build_environment', arguments: { action: 'set_motion_blur_amount', volumeName: POST_PROCESS_VOLUME_NAME, motionBlurAmount: 0.2 }, expected: 'success' },
  { scenario: 'POST: set motion blur max', toolName: 'build_environment', arguments: { action: 'set_motion_blur_max', volumeName: POST_PROCESS_VOLUME_NAME, motionBlurMax: 5 }, expected: 'success' },
  { scenario: 'POST: configure exposure', toolName: 'build_environment', arguments: { action: 'configure_exposure', volumeName: POST_PROCESS_VOLUME_NAME, exposureMethod: 'Histogram', exposureCompensation: 0.5, exposureMinBrightness: 0, exposureMaxBrightness: 10, exposureSpeedUp: 3, exposureSpeedDown: 1, exposureLowPercent: 80, exposureHighPercent: 98 }, expected: 'success' },
  { scenario: 'POST: set exposure method', toolName: 'build_environment', arguments: { action: 'set_exposure_method', volumeName: POST_PROCESS_VOLUME_NAME, exposureMethod: 'Manual' }, expected: 'success' },
  { scenario: 'POST: set exposure compensation', toolName: 'build_environment', arguments: { action: 'set_exposure_compensation', volumeName: POST_PROCESS_VOLUME_NAME, exposureCompensation: 1 }, expected: 'success' },
  { scenario: 'POST: set exposure min max', toolName: 'build_environment', arguments: { action: 'set_exposure_min_max', volumeName: POST_PROCESS_VOLUME_NAME, exposureMinBrightness: 0, exposureMaxBrightness: 8 }, expected: 'success' },
  { scenario: 'POST: configure SSAO', toolName: 'build_environment', arguments: { action: 'configure_ssao', volumeName: POST_PROCESS_VOLUME_NAME, ssaoIntensity: 1, ssaoRadius: 100, ssaoPower: 1, ssaoBias: 0.1, ssaoDistance: 200, ssaoStaticFraction: 0, ssaoFadeDistance: 1000 }, expected: 'success' },
  { scenario: 'POST: configure GTAO', toolName: 'build_environment', arguments: { action: 'configure_gtao', volumeName: POST_PROCESS_VOLUME_NAME, gtaoIntensity: 1, gtaoRadius: 100, gtaoPower: 1, gtaoThickness: 1 }, expected: 'success' },
  { scenario: 'POST: configure vignette', toolName: 'build_environment', arguments: { action: 'configure_vignette', volumeName: POST_PROCESS_VOLUME_NAME, vignetteIntensity: 0.1 }, expected: 'success' },
  { scenario: 'POST: configure chromatic aberration', toolName: 'build_environment', arguments: { action: 'configure_chromatic_aberration', volumeName: POST_PROCESS_VOLUME_NAME, chromaticAberrationIntensity: 0.05 }, expected: 'success' },
  { scenario: 'POST: configure film grain', toolName: 'build_environment', arguments: { action: 'configure_grain', volumeName: POST_PROCESS_VOLUME_NAME, grainIntensity: 0.02 }, expected: 'success' },
  { scenario: 'POST: configure screen percentage', toolName: 'build_environment', arguments: { action: 'configure_screen_percentage', screenPercentage: 100 }, expected: 'success' },
  { scenario: 'POST: inspect post process volumes', toolName: 'build_environment', arguments: { action: 'inspect_post_process_volume', volumeName: POST_PROCESS_VOLUME_NAME }, expected: 'success' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete demo light actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: DEMO_LIGHT_NAME }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete sphere reflection capture', toolName: 'control_actor', arguments: { action: 'delete', actorName: `${DEMO_LIGHT_NAME}_SphereCapture` }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete box reflection capture', toolName: 'control_actor', arguments: { action: 'delete', actorName: `${DEMO_LIGHT_NAME}_BoxCapture` }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete planar reflection', toolName: 'control_actor', arguments: { action: 'delete', actorName: `${DEMO_LIGHT_NAME}_PlanarReflection` }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete post process volume', toolName: 'control_actor', arguments: { action: 'delete', actorName: POST_PROCESS_VOLUME_NAME }, expected: 'success|not found' },
];

runToolTests('lighting-channel-raytracing-ue58', testCases);
