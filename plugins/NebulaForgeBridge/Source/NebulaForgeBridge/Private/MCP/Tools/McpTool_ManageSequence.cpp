// McpTool_ManageSequence.cpp — manage_sequence tool definition

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageSequence : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_sequence"); }

	FString GetDescription() const override
	{
		return TEXT("Edit Level Sequences: add tracks, bind actors, set keyframes, "
			"control playback, and record camera.");
	}

	FString GetCategory() const override { return TEXT("utility"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create"),
				TEXT("create_master_sequence"),
				TEXT("open"),
				TEXT("add_camera"),
				TEXT("create_cine_camera_actor"),
				TEXT("add_subsequence"),
				TEXT("add_shot_track"),
				TEXT("configure_shot_settings"),
				TEXT("inspect_shot_settings"),
				TEXT("add_audio_track"),
				TEXT("add_material_parameter_track"),
				TEXT("add_material_color_track"),
				TEXT("add_custom_primitive_data_track"),
				TEXT("add_niagara_system_track"),
				TEXT("create_niagara_float_parameter_track"),
				TEXT("add_camera_cut_track"),
				TEXT("add_camera_shake_track"),
				TEXT("add_fade_track"),
				TEXT("add_level_visibility_track"),
				TEXT("add_skeletal_animation_track"),
				TEXT("add_transform_track"),
				TEXT("add_event_track"),
				TEXT("add_property_track"),
				TEXT("add_actor"),
				TEXT("add_actors"),
				TEXT("remove_actors"),
				TEXT("get_bindings"),
				TEXT("play"),
				TEXT("pause"),
				TEXT("stop"),
				TEXT("set_playback_speed"),
				TEXT("add_keyframe"),
				TEXT("get_properties"),
				TEXT("set_properties"),
				TEXT("duplicate"),
				TEXT("rename"),
				TEXT("delete"),
				TEXT("list"),
				TEXT("get_metadata"),
				TEXT("set_metadata"),
				TEXT("add_spawnable_from_class"),
				TEXT("add_track"),
				TEXT("add_section"),
				TEXT("set_display_rate"),
				TEXT("set_tick_resolution"),
				TEXT("set_work_range"),
				TEXT("set_view_range"),
				TEXT("set_track_muted"),
				TEXT("set_track_solo"),
				TEXT("set_track_locked"),
				TEXT("list_tracks"),
				TEXT("remove_track"),
				TEXT("list_track_types"),
				TEXT("render_sequence_mrq"), TEXT("configure_burn_ins"),
				TEXT("get_mrq_status"),
				TEXT("cancel_mrq"),
				TEXT("render_sequence_queue")
			}, TEXT("Action"))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.Array(TEXT("actorNames"), TEXT(""))
			.String(TEXT("shakeClass"), TEXT("Camera shake class path for add_camera_shake_track."))
			.Number(TEXT("frame"), TEXT(""))
			.FreeformObject(TEXT("value"), TEXT(""))
			.String(TEXT("property"), TEXT("Name of the property."))
			.String(TEXT("destinationPath"), TEXT("Destination path for move/copy."))
			.String(TEXT("newName"), TEXT("New name for renaming."))
			.Number(TEXT("speed"), TEXT(""))
			.Number(TEXT("startTime"), TEXT(""))
			.String(TEXT("loopMode"), TEXT(""))
			.String(TEXT("className"), TEXT(""))
			.Bool(TEXT("spawnable"), TEXT(""))
			.String(TEXT("trackType"), TEXT(""))
			.String(TEXT("trackName"), TEXT(""))
			.Bool(TEXT("muted"), TEXT(""))
			.Bool(TEXT("solo"), TEXT(""))
			.Bool(TEXT("locked"), TEXT(""))
			.Number(TEXT("startFrame"), TEXT(""))
			.Number(TEXT("endFrame"), TEXT(""))
			.StringEnum(TEXT("outputFormat"), { TEXT("png"), TEXT("jpg"), TEXT("jpeg"), TEXT("bmp"), TEXT("exr") }, TEXT("MRQ image-sequence output format."))
			.StringEnum(TEXT("renderPass"), { TEXT("beauty"), TEXT("object_id") }, TEXT("Optional MRQ render pass; object_id requires Movie Render Queue Additional Render Passes and EXR output."))
			.String(TEXT("burnInClass"), TEXT("Optional UMoviePipelineBurnInWidget class path."))
			.Bool(TEXT("compositeBurnIn"), TEXT("Composite the burn-in into the final image."))
			.Integer(TEXT("spatialSampleCount"), TEXT("MRQ spatial anti-aliasing sample count."))
			.Integer(TEXT("temporalSampleCount"), TEXT("MRQ temporal anti-aliasing sample count."))
			.String(TEXT("mrqPresetPath"), TEXT("Optional /Game UMoviePipelinePrimaryConfig asset copied into the transient MRQ job."))
			.String(TEXT("frameRate"), TEXT(""))
			.String(TEXT("resolution"), TEXT(""))
			.Number(TEXT("start"), TEXT(""))
			.Number(TEXT("end"), TEXT(""))
			.Number(TEXT("lengthInFrames"), TEXT(""))
			.Number(TEXT("playbackStart"), TEXT(""))
			.Number(TEXT("playbackEnd"), TEXT(""))
			.FreeformObject(TEXT("metadata"), TEXT(""))
			.String(TEXT("subsequencePath"), TEXT("Child sequence asset path."))
			.String(TEXT("childSequencePath"), TEXT("Alias for subsequencePath."))
			.Number(TEXT("durationFrames"), TEXT("Subsequence duration in frames."))
			.Integer(TEXT("shotIndex"), TEXT("Zero-based cinematic shot section index."))
			.String(TEXT("shotDisplayName"), TEXT("Cinematic shot display name."))
			.Number(TEXT("thumbnailReferenceOffset"), TEXT("Shot thumbnail reference offset."))
			.String(TEXT("soundPath"), TEXT("USoundBase asset path for a cinematic audio section."))
			.Bool(TEXT("looping"), TEXT("Repeat the sound when the section is longer than its natural duration."))
			.Bool(TEXT("playUntilFinished"), TEXT("Play through the sound's full duration past the section end."))
			.String(TEXT("parameterName"), TEXT("Scalar material parameter name to key."))
			.Number(TEXT("colorR"), TEXT("Red channel for a material color key."))
			.Number(TEXT("colorG"), TEXT("Green channel for a material color key."))
			.Number(TEXT("colorB"), TEXT("Blue channel for a material color key."))
			.Number(TEXT("colorA"), TEXT("Alpha channel for a material color key."))
			.Integer(TEXT("customPrimitiveDataIndex"), TEXT("Custom primitive data start index."))
			.Number(TEXT("rowIndex"), TEXT("Optional subsequence track row."))
			.String(TEXT("outputPath"), TEXT("Project-relative Movie Render Queue output directory."))
			.String(TEXT("mrqJobId"), TEXT("Movie Render Queue job identifier."))
			.Array(TEXT("queue"), TEXT("Bounded ordered render jobs; each item requires path and outputPath."))
			.Bool(TEXT("waitForCompletion"), TEXT("Wait for each queued MRQ job to reach a terminal state."))
			.Number(TEXT("pollIntervalMs"), TEXT("Polling interval for queued MRQ jobs."))
			.Number(TEXT("timeoutMs"), TEXT("Overall timeout for queued MRQ jobs."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageSequence);
