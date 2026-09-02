// McpTool_ManageLighting.cpp — manage_lighting tool definition (15 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageLighting : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_lighting"); }

	FString GetDescription() const override
	{
		return TEXT("Spawn lights (point, spot, rect, sky), configure GI, shadows, "
			"Lightmass, volumetric fog, and build lighting.");
	}

	FString GetCategory() const override { return TEXT("world"); }


	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("spawn_light"),
				TEXT("create_light"),
				TEXT("spawn_sky_light"),
				TEXT("create_sky_light"),
				TEXT("ensure_single_sky_light"),
				TEXT("create_lightmass_volume"),
				TEXT("create_lighting_enabled_level"),
				TEXT("create_dynamic_light"),
				TEXT("setup_global_illumination"),
				TEXT("configure_shadows"),
				TEXT("set_exposure"),
				TEXT("set_ambient_occlusion"),
				TEXT("setup_volumetric_fog"),
				TEXT("build_lighting"),
				TEXT("build_lighting_quality"),
				TEXT("configure_lightmass_settings"),
				TEXT("configure_indirect_lighting_cache"),
				TEXT("configure_volumetric_lightmaps"),
				TEXT("configure_lightmass_ambient_occlusion"),
				TEXT("inspect_lightmass_settings"),
				TEXT("create_sphere_reflection_capture"),
				TEXT("create_box_reflection_capture"),
				TEXT("configure_capture_resolution"),
				TEXT("configure_capture_offset"),
				TEXT("recapture_scene"),
				TEXT("create_planar_reflection"),
				TEXT("configure_planar_reflection"),
				TEXT("configure_ssr_settings"),
				TEXT("configure_lumen_reflection_settings"),
				TEXT("inspect_reflection_captures"),
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
				TEXT("create_scene_capture_2d"), TEXT("create_scene_capture_cube"), TEXT("create_render_target_cube"),
				TEXT("configure_scene_capture"), TEXT("configure_scene_capture_resolution"),
				TEXT("configure_capture_source"), TEXT("assign_render_target"), TEXT("capture_scene"),
				TEXT("inspect_scene_captures"),
				TEXT("list_light_types")
			}, TEXT("Action"))
			.StringEnum(TEXT("action"), McpConsolidatedActions::Lighting(), TEXT("Action"))
			.String(TEXT("name"), TEXT("Name identifier."))
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.StringEnum(TEXT("lightType"), {
				TEXT("Directional"),
				TEXT("Point"),
				TEXT("Spot"),
				TEXT("Rect"),
				TEXT("DirectionalLight"),
				TEXT("PointLight"),
				TEXT("SpotLight"),
				TEXT("RectLight"),
				TEXT("directional"),
				TEXT("point"),
				TEXT("spot"),
				TEXT("rect")
			}, TEXT("Light type. Accepts short names (Point), class names "
				"(PointLight), or lowercase (point)."))
			.String(TEXT("lightClass"),
				TEXT("Unreal light class name (e.g., PointLight, SpotLight). "
					"Alternative to lightType."))
			.Number(TEXT("intensity"), TEXT(""))
			.Array(TEXT("color"), TEXT("RGBA color as an array [r, g, b, a]."),
				TEXT("number"))
			.Bool(TEXT("castShadows"), TEXT(""))
			.Bool(TEXT("useAsAtmosphereSunLight"),
				TEXT("For Directional Lights, use as Atmosphere Sun Light."))
			.Number(TEXT("temperature"), TEXT(""))
			.Number(TEXT("radius"), TEXT(""))
			.Number(TEXT("falloffExponent"), TEXT(""))
			.Number(TEXT("innerCone"), TEXT(""))
			.Number(TEXT("outerCone"), TEXT(""))
			.Number(TEXT("width"), TEXT(""))
			.Number(TEXT("height"), TEXT(""))
			.StringEnum(TEXT("sourceType"), {
				TEXT("CapturedScene"),
				TEXT("SpecifiedCubemap")
			}, TEXT(""))
			.String(TEXT("cubemapPath"), TEXT("Texture asset path."))
			.Bool(TEXT("recapture"), TEXT(""))
			.StringEnum(TEXT("method"), {
				TEXT("Lightmass"),
				TEXT("LumenGI"),
				TEXT("ScreenSpace"),
				TEXT("None")
			}, TEXT(""))
			.String(TEXT("quality"), TEXT(""))
			.Number(TEXT("indirectLightingIntensity"), TEXT(""))
			.Number(TEXT("bounces"), TEXT(""))
			.Number(TEXT("staticLightingLevelScale"), TEXT("World scale used by Lightmass."))
			.Number(TEXT("numIndirectLightingBounces"), TEXT("Number of direct-light bounces."))
			.Number(TEXT("numSkyLightingBounces"), TEXT("Number of sky/emissive bounces."))
			.Number(TEXT("indirectLightingQuality"), TEXT("Lightmass indirect-lighting sample quality."))
			.Number(TEXT("indirectLightingSmoothness"), TEXT("Indirect-lighting smoothing."))
			.FreeformObject(TEXT("environmentColor"), TEXT("Lightmass environment color."))
			.Number(TEXT("environmentIntensity"), TEXT("Lightmass environment intensity."))
			.Number(TEXT("diffuseBoost"), TEXT("Diffuse contribution multiplier."))
			.Number(TEXT("emissiveBoost"), TEXT("Emissive contribution multiplier."))
			.StringEnum(TEXT("volumeLightingMethod"), { TEXT("VolumetricLightmap"), TEXT("SparseVolumeLightingSamples") }, TEXT("Precomputed volume lighting method."))
			.Bool(TEXT("useAmbientOcclusion"), TEXT("Enable baked ambient occlusion."))
			.Bool(TEXT("generateAmbientOcclusionMaterialMask"), TEXT("Generate the precomputed AO material mask."))
			.Bool(TEXT("visualizeMaterialDiffuse"), TEXT("Visualize Lightmass material diffuse."))
			.Bool(TEXT("visualizeAmbientOcclusion"), TEXT("Visualize Lightmass ambient occlusion."))
			.Bool(TEXT("compressLightmaps"), TEXT("Compress baked lightmaps."))
			.Number(TEXT("volumetricLightmapDetailCellSize"), TEXT("Volumetric lightmap detail cell size."))
			.Number(TEXT("volumetricLightmapMaximumBrickMemoryMb"), TEXT("Volumetric lightmap brick memory budget."))
			.Number(TEXT("volumetricLightmapLoadingCellSize"), TEXT("World Partition volumetric lightmap loading cell size."))
			.Number(TEXT("volumetricLightmapSphericalHarmonicSmoothing"), TEXT("Volumetric lightmap SH smoothing."))
			.Number(TEXT("volumeLightSamplePlacementScale"), TEXT("Volume lighting sample placement scale."))
			.Number(TEXT("directIlluminationOcclusionFraction"), TEXT("Baked AO fraction for direct light."))
			.Number(TEXT("indirectIlluminationOcclusionFraction"), TEXT("Baked AO fraction for indirect light."))
			.Number(TEXT("occlusionExponent"), TEXT("Baked AO contrast exponent."))
			.Number(TEXT("fullyOccludedSamplesFraction"), TEXT("Samples required for full occlusion."))
			.Number(TEXT("maxOcclusionDistance"), TEXT("Maximum baked AO distance."))
			.Bool(TEXT("updateEveryFrame"), TEXT("Update the indirect lighting cache every frame."))
			.Number(TEXT("lightingCacheDimension"), TEXT("Indirect lighting cache dimension."))
			.Number(TEXT("movableObjectAllocationSize"), TEXT("Indirect lighting cache allocation size."))
			.String(TEXT("captureName"), TEXT("Reflection capture or planar reflection actor name/path."))
			.StringEnum(TEXT("captureType"), { TEXT("sphere"), TEXT("box"), TEXT("planar") }, TEXT("Reflection capture shape."))
			.Number(TEXT("influenceRadius"), TEXT("Sphere capture influence radius."))
			.Number(TEXT("boxTransitionDistance"), TEXT("Box capture transition distance."))
			.FreeformObject(TEXT("captureOffset"), TEXT("World-space reflection capture offset."))
			.Number(TEXT("captureResolution"), TEXT("Reflection capture cubemap resolution; must be a power of two."))
			.Number(TEXT("sourceCubemapAngle"), TEXT("Source cubemap rotation angle."))
			.Number(TEXT("brightness"), TEXT("Reflection capture brightness multiplier."))
			.Bool(TEXT("runtimeCapture"), TEXT("Generate reflection capture content at runtime."))
			.Number(TEXT("maxViewDistance"), TEXT("Runtime capture maximum view distance."))
			.Bool(TEXT("fastRender"), TEXT("Render all capture faces in one frame when refreshing."))
			.Bool(TEXT("smoothBlend"), TEXT("Smoothly blend runtime capture refreshes."))
			.Number(TEXT("normalDistortionStrength"), TEXT("Planar reflection normal distortion."))
			.Number(TEXT("prefilterRoughness"), TEXT("Planar reflection prefilter roughness."))
			.Number(TEXT("prefilterRoughnessDistance"), TEXT("Distance at which planar prefilter roughness is reached."))
			.Number(TEXT("distanceFromPlaneFadeoutStart"), TEXT("Planar reflection distance fade start."))
			.Number(TEXT("distanceFromPlaneFadeoutEnd"), TEXT("Planar reflection distance fade end."))
			.Number(TEXT("angleFromPlaneFadeStart"), TEXT("Planar reflection angle fade start."))
			.Number(TEXT("angleFromPlaneFadeEnd"), TEXT("Planar reflection angle fade end."))
			.Number(TEXT("screenPercentage"), TEXT("Planar reflection render screen percentage."))
			.Number(TEXT("extraFOV"), TEXT("Extra field of view for planar reflections."))
			.Bool(TEXT("renderSceneTwoSided"), TEXT("Render planar reflection scene two-sided."))
			.Bool(TEXT("showPreviewPlane"), TEXT("Show the planar reflection preview plane."))
			.Bool(TEXT("ssrEnabled"), TEXT("Enable screen-space reflections."))
			.Number(TEXT("ssrIntensity"), TEXT("SSR intensity percentage."))
			.Number(TEXT("ssrQuality"), TEXT("SSR quality from 0 to 100."))
			.Number(TEXT("ssrMaxRoughness"), TEXT("Maximum roughness receiving SSR."))
			.Bool(TEXT("lumenReflectionsEnabled"), TEXT("Enable Lumen reflections."))
			.Number(TEXT("lumenReflectionQuality"), TEXT("Lumen reflection scalability quality."))
			.Number(TEXT("lumenReflectionMaxRoughness"), TEXT("Lumen maximum roughness to trace."))
			.Number(TEXT("lumenReflectionMaxBounces"), TEXT("Lumen reflection bounce count."))
			.Number(TEXT("lumenReflectionDownsampleFactor"), TEXT("Lumen reflection downsample factor."))
			.Bool(TEXT("lumenReflectionScreenTraces"), TEXT("Use screen traces for Lumen reflections."))
			.Bool(TEXT("lumenReflectionDownsampleCheckerboard"), TEXT("Use checkerboard downsampling for Lumen reflections."))
			.String(TEXT("volumeName"), TEXT("Post Process Volume name."))
			.String(TEXT("volumePath"), TEXT("Post Process Volume object path."))
			.Object(TEXT("extent"), TEXT("Post Process Volume extent."),
				[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
			.Bool(TEXT("bUnbound"), TEXT("Whether the Post Process Volume affects the entire world."))
			.Number(TEXT("blendRadius"), TEXT("Post Process Volume blend radius."))
			.Number(TEXT("blendWeight"), TEXT("Post Process Volume blend weight (0-1)."))
			.Number(TEXT("priority"), TEXT("Post Process Volume priority."))
			.Number(TEXT("bloomIntensity"), TEXT("Bloom intensity."))
			.Number(TEXT("bloomThreshold"), TEXT("Bloom threshold."))
			.Number(TEXT("bloomSizeScale"), TEXT("Bloom size scale."))
			.String(TEXT("bloomMethod"), TEXT("Bloom method enum name."))
			.Number(TEXT("lensFlareIntensity"), TEXT("Lens flare intensity."))
			.Number(TEXT("lensFlareBokehSize"), TEXT("Lens flare bokeh size."))
			.Number(TEXT("lensFlareThreshold"), TEXT("Lens flare threshold."))
			.String(TEXT("dofMethod"), TEXT("Depth of field method enum name."))
			.Number(TEXT("dofFocalDistance"), TEXT("Depth of field focal distance."))
			.Number(TEXT("dofFocalRegion"), TEXT("Depth of field focal region."))
			.Number(TEXT("dofFstop"), TEXT("Depth of field aperture F-stop."))
			.Number(TEXT("dofMinFstop"), TEXT("Depth of field minimum F-stop."))
			.Number(TEXT("dofNearBlurSize"), TEXT("Depth of field near blur size."))
			.Number(TEXT("dofFarBlurSize"), TEXT("Depth of field far blur size."))
			.Number(TEXT("dofNearTransitionRegion"), TEXT("Depth of field near transition."))
			.Number(TEXT("dofFarTransitionRegion"), TEXT("Depth of field far transition."))
			.Number(TEXT("dofScale"), TEXT("Depth of field scale."))
			.Number(TEXT("dofBladeCount"), TEXT("Depth of field bokeh blade count."))
			.Number(TEXT("motionBlurAmount"), TEXT("Motion blur amount."))
			.Number(TEXT("motionBlurMax"), TEXT("Motion blur maximum."))
			.Number(TEXT("motionBlurTargetFPS"), TEXT("Motion blur target FPS."))
			.Number(TEXT("motionBlurPerObjectSize"), TEXT("Motion blur per-object size."))
			.String(TEXT("exposureMethod"), TEXT("Auto exposure method enum name."))
			.Number(TEXT("exposureCompensation"), TEXT("Auto exposure compensation."))
			.Number(TEXT("exposureMinBrightness"), TEXT("Auto exposure minimum brightness."))
			.Number(TEXT("exposureMaxBrightness"), TEXT("Auto exposure maximum brightness."))
			.Number(TEXT("exposureSpeedUp"), TEXT("Auto exposure speed up."))
			.Number(TEXT("exposureSpeedDown"), TEXT("Auto exposure speed down."))
			.Number(TEXT("exposureLowPercent"), TEXT("Auto exposure low percentile."))
			.Number(TEXT("exposureHighPercent"), TEXT("Auto exposure high percentile."))
			.Number(TEXT("whiteBalanceTemperature"), TEXT("White balance temperature in Kelvin."))
			.Number(TEXT("whiteBalanceTint"), TEXT("White balance tint."))
			.Array(TEXT("colorSaturation"), TEXT("Global color saturation RGBA."), TEXT("number"))
			.Array(TEXT("colorContrast"), TEXT("Global color contrast RGBA."), TEXT("number"))
			.Array(TEXT("colorGamma"), TEXT("Global color gamma RGBA."), TEXT("number"))
			.Array(TEXT("colorGain"), TEXT("Global color gain RGBA."), TEXT("number"))
			.Array(TEXT("colorOffset"), TEXT("Global color offset RGBA."), TEXT("number"))
			.String(TEXT("lutPath"), TEXT("Color grading LUT texture asset path."))
			.Number(TEXT("lutIntensity"), TEXT("Color grading LUT intensity."))
			.Number(TEXT("toneCurveAmount"), TEXT("Tonemapper tone curve amount."))
			.Number(TEXT("expandGamut"), TEXT("Tonemapper expand gamut."))
			.Number(TEXT("filmBlackClip"), TEXT("Film black clip."))
			.Number(TEXT("filmWhiteClip"), TEXT("Film white clip."))
			.Number(TEXT("tonemapperType"), TEXT("Tonemapper type console value."))
			.Number(TEXT("ssaoIntensity"), TEXT("SSAO intensity."))
			.Number(TEXT("ssaoRadius"), TEXT("SSAO radius."))
			.Number(TEXT("ssaoPower"), TEXT("SSAO power."))
			.Number(TEXT("ssaoBias"), TEXT("SSAO bias."))
			.Number(TEXT("ssaoDistance"), TEXT("SSAO distance."))
			.Number(TEXT("ssaoStaticFraction"), TEXT("SSAO static fraction."))
			.Number(TEXT("ssaoFadeDistance"), TEXT("SSAO fade distance."))
			.Number(TEXT("gtaoIntensity"), TEXT("GTAO intensity."))
			.Number(TEXT("gtaoRadius"), TEXT("GTAO radius."))
			.Number(TEXT("gtaoPower"), TEXT("GTAO power."))
			.Number(TEXT("gtaoThickness"), TEXT("GTAO thickness console value."))
			.Number(TEXT("vignetteIntensity"), TEXT("Vignette intensity."))
			.Number(TEXT("chromaticAberrationIntensity"), TEXT("Chromatic aberration intensity."))
			.Number(TEXT("grainIntensity"), TEXT("Film grain intensity."))
			.String(TEXT("sceneCaptureName"), TEXT("Scene Capture actor name."))
			.String(TEXT("sceneCapturePath"), TEXT("Scene Capture actor path."))
			.String(TEXT("renderTargetPath"), TEXT("Render target asset path."))
			.String(TEXT("renderTargetName"), TEXT("Cube render target asset name."))
			.String(TEXT("captureSource"), TEXT("ESceneCaptureSource enum name."))
			.String(TEXT("projectionType"), TEXT("Perspective or Orthographic for 2D captures."))
			.Number(TEXT("fovAngle"), TEXT("2D capture field of view."))
			.Number(TEXT("orthoWidth"), TEXT("2D orthographic width."))
			.Bool(TEXT("captureEveryFrame"), TEXT("Capture every frame."))
			.Bool(TEXT("captureOnMovement"), TEXT("Capture when the component moves."))
			.Bool(TEXT("alwaysPersistRenderingState"), TEXT("Persist capture rendering state."))
			.Bool(TEXT("captureRotation"), TEXT("Capture cube faces with rotation."))
			.Bool(TEXT("captureDeferred"), TEXT("Defer one-shot capture."))
			.Number(TEXT("capturePriority"), TEXT("Scene capture sort priority."))
			.Number(TEXT("width"), TEXT("Render target width or cube resolution."))
			.Number(TEXT("height"), TEXT("Render target height."))
			.String(TEXT("format"), TEXT("Render target pixel format."))
			.Bool(TEXT("forceLinearGamma"), TEXT("Force linear gamma on the render target."))
			.Bool(TEXT("autoGenerateMips"), TEXT("Generate render target mipmaps."))
			.Bool(TEXT("supportsUAV"), TEXT("Allow unordered access on the render target."))
			.Bool(TEXT("hdr"), TEXT("Use an HDR render target format."))
			.Array(TEXT("clearColor"), TEXT("Render target clear color RGBA."), TEXT("number"))
			.Array(TEXT("hiddenActors"), TEXT("Actor names hidden from this capture."), TEXT("string"))
			.Array(TEXT("showOnlyActors"), TEXT("Actor names shown exclusively by this capture."), TEXT("string"))
			.Number(TEXT("postProcessBlendWeight"), TEXT("Scene capture post-process blend weight."))
			.String(TEXT("shadowQuality"), TEXT(""))
			.Bool(TEXT("cascadedShadows"), TEXT(""))
			.Number(TEXT("shadowDistance"), TEXT(""))
			.Bool(TEXT("contactShadows"), TEXT(""))
			.Bool(TEXT("rayTracedShadows"), TEXT(""))
			.Number(TEXT("compensationValue"), TEXT(""))
			.Number(TEXT("minBrightness"), TEXT(""))
			.Number(TEXT("maxBrightness"), TEXT(""))
			.Bool(TEXT("enabled"),
				TEXT("Whether the item/feature is enabled."))
			.Number(TEXT("density"), TEXT(""))
			.Number(TEXT("scatteringIntensity"), TEXT(""))
			.Number(TEXT("fogHeight"), TEXT(""))
			.Bool(TEXT("buildOnlySelected"), TEXT(""))
			.Bool(TEXT("buildReflectionCaptures"), TEXT(""))
			.String(TEXT("levelName"), TEXT(""))
			.Bool(TEXT("copyActors"), TEXT(""))
			.Bool(TEXT("useTemplate"), TEXT(""))
			.Object(TEXT("size"), TEXT("3D scale (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageLighting);
