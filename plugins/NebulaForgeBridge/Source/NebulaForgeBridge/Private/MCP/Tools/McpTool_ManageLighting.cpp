// McpTool_ManageLighting.cpp — manage_lighting tool definition (15 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

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
				TEXT("list_light_types")
			}, TEXT("Action"))
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
