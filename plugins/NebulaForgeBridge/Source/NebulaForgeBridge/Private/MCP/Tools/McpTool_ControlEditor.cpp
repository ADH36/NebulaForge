// McpTool_ControlEditor.cpp — control_editor tool definition

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ControlEditor : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("control_editor"); }

	FString GetDescription() const override
	{
		return TEXT("Start/stop PIE, control viewport camera, and run bounded automated "
			"play-tests with PIE-world validation, screenshots, runtime probes, and cleanup.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("play"),
				TEXT("start_pie"),
				TEXT("stop"),
				TEXT("stop_pie"),
				TEXT("pause"),
				TEXT("resume"),
				TEXT("eject"),
				TEXT("possess"),
				TEXT("set_view_target"),
				TEXT("set_game_view_target"),
				TEXT("set_game_speed"),
				TEXT("set_fixed_delta_time"),
				TEXT("set_camera"),
				TEXT("set_camera_position"),
				TEXT("set_viewport_camera"),
				TEXT("set_camera_fov"),
				TEXT("set_view_mode"),
				TEXT("set_viewport_resolution"),
				TEXT("console_command"),
				TEXT("execute_command"),
				TEXT("screenshot"),
				TEXT("take_screenshot"),
				TEXT("step_frame"),
				TEXT("single_frame_step"),
				TEXT("start_recording"), TEXT("start_demo_recording"),
				TEXT("stop_recording"), TEXT("stop_demo_recording"),
				TEXT("open_media"),
				TEXT("play_media"),
				TEXT("pause_media"),
				TEXT("seek_media"),
				TEXT("start_take_recording"),
				TEXT("configure_take_sources"),
				TEXT("configure_recorded_tracks"),
				TEXT("stop_take_recording"),
				TEXT("get_take_recording_status"),
				TEXT("play_demo"),
				TEXT("pause_demo"),
				TEXT("seek_demo"),
				TEXT("set_demo_playback_speed"),
				TEXT("configure_killcam_duration"),
				TEXT("start_killcam"),
				TEXT("create_bookmark"),
				TEXT("jump_to_bookmark"),
				TEXT("set_preferences"),
				TEXT("set_viewport_realtime"),
				TEXT("open_asset"),
				TEXT("close_asset"),
				TEXT("simulate_input"),
				TEXT("get_pie_state"),
				TEXT("query_pie_actor"),
				TEXT("get_pie_metrics"),
				TEXT("detect_pie_issues"),
				TEXT("send_input"),
				TEXT("send_enhanced_input"),
				TEXT("move"),
				TEXT("look"),
				TEXT("jump"),
				TEXT("sprint"),
				TEXT("interact"),
				TEXT("capture_pie_screenshot"),
				TEXT("read_pie_logs"),
				TEXT("run_playtest_sequence"),
				TEXT("open_level"),
				TEXT("focus_actor"),
				TEXT("show_stats"),
				TEXT("hide_stats"),
				TEXT("set_editor_mode"),
				TEXT("configure_editor_preferences"),
				TEXT("set_grid_settings"),
				TEXT("set_snap_settings"),
				TEXT("manage_editor_layouts"),
				TEXT("create_custom_editor_mode"),
				TEXT("set_immersive_mode"),
				TEXT("set_game_view"),
				TEXT("undo"),
				TEXT("redo"),
				TEXT("save_all")
			}, TEXT("Editor action. Note: editor_viewport screenshots are async "
				"and are written on the next rendered viewport frame. "
				"full_editor_window captures synchronously and can return image content."))
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.String(TEXT("viewMode"), TEXT(""))
			.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."))
			.Number(TEXT("speed"), TEXT(""))
			.String(TEXT("filename"), TEXT(""))
			.Number(TEXT("fov"), TEXT(""))
			.Number(TEXT("width"), TEXT(""))
			.Number(TEXT("height"), TEXT(""))
			.String(TEXT("command"), TEXT(""))
			.Integer(TEXT("steps"), TEXT(""))
			.Integer(TEXT("id"), TEXT("Bookmark identifier/index."))
			.String(TEXT("bookmarkName"), TEXT(""))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("levelPath"), TEXT("Level asset path."))
			.String(TEXT("path"), TEXT("Path to a directory."))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.Array(TEXT("actorNames"), TEXT("Actor names to add as Take Recorder sources."), TEXT("string"))
			.Bool(TEXT("reduceKeys"), TEXT("Enable key reduction for added actor sources."))
			.Bool(TEXT("showProgress"), TEXT("Show Take Recorder source processing progress."))
			.Bool(TEXT("recordToPossessable"), TEXT("Record actor sources to possessable bindings."))
			.Bool(TEXT("removeRedundantTracks"), TEXT("Remove redundant recorded tracks."))
			.Bool(TEXT("saveRecordedAssets"), TEXT("Save recorded animation and other assets."))
			.Bool(TEXT("recordIntoSubSequences"), TEXT("Record sources into sub-sequences."))
			.Bool(TEXT("autoLock"), TEXT("Auto-lock the recorded take."))
			.Bool(TEXT("startAtCurrentTimecode"), TEXT("Start recording at the current timecode."))
			.String(TEXT("objectPath"), TEXT("Object path alias for actorName."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.Number(TEXT("blendTime"), TEXT("Blend time in seconds for set_view_target."))
			.String(TEXT("mode"), TEXT("Editor mode or screenshot source: editor_viewport, game_viewport, full_editor_window, standalone_window."))
			.Bool(TEXT("returnBase64"), TEXT("Return PNG image data as base64 when supported. Defaults to true for full_editor_window and game_viewport modes."))
			.Bool(TEXT("includeMetadata"), TEXT("Attach caller-provided metadata to the response."))
			.FreeformObject(TEXT("metadata"), TEXT("Caller-provided screenshot metadata."))
			.Number(TEXT("demoTime"), TEXT("Demo playback time in seconds."))
			.Number(TEXT("demoSpeed"), TEXT("Demo playback speed from 0 to 16."))
			.Number(TEXT("durationSeconds"), TEXT("Killcam playback duration in seconds."))
			.Number(TEXT("startTime"), TEXT("Replay time in seconds at which the killcam starts."))
			.String(TEXT("mediaPlayerPath"), TEXT("Media player asset path."))
			.String(TEXT("mediaUrl"), TEXT("Media URL to open."))
			.Number(TEXT("mediaTime"), TEXT("Media playback time in seconds."))
			.String(TEXT("sequencePath"), TEXT("Optional level sequence to record into."))
			.Bool(TEXT("openSequencer"), TEXT("Open Sequencer when starting Take Recorder."))
			.Bool(TEXT("showErrorMessage"), TEXT("Show the editor error message when Take Recorder fails."))
			.Number(TEXT("deltaTime"), TEXT(""))
			.String(TEXT("resolution"), TEXT("Resolution setting (e.g., 1024x1024)."))
			.Bool(TEXT("realtime"), TEXT(""))
			.String(TEXT("stat"), TEXT(""))
			.String(TEXT("category"), TEXT(""))
			.FreeformObject(TEXT("preferences"), TEXT(""))
			.Bool(TEXT("gridEnabled"), TEXT("Enable translation grid snapping."))
			.Number(TEXT("gridSize"), TEXT("Translation grid size in Unreal units."))
			.Bool(TEXT("rotationGridEnabled"), TEXT("Enable rotation grid snapping."))
			.Bool(TEXT("scaleGridEnabled"), TEXT("Enable scale grid snapping."))
			.Bool(TEXT("snapToSurface"), TEXT("Snap dragged actors to surfaces."))
			.Bool(TEXT("snapRotation"), TEXT("Rotate surface-snapped actors to the surface normal."))
			.Number(TEXT("snapOffsetExtent"), TEXT("Surface snap offset extent."))
			.Number(TEXT("actorSnapDistance"), TEXT("Global actor snap distance."))
			.Number(TEXT("snapDistance"), TEXT("Viewport snap distance."))
			.Number(TEXT("actorSnapScale"), TEXT("Global actor snap scale."))
			.Bool(TEXT("usePowerOf2SnapSize"), TEXT("Use power-of-two translation grid sizes."))
			.String(TEXT("layoutAction"), TEXT("Layout operation: save, load, remove, reset, export, or import."))
			.String(TEXT("layoutName"), TEXT("Named editor layout."))
			.String(TEXT("customModeName"), TEXT("Human-readable custom editor mode name."))
			.String(TEXT("customModeId"), TEXT("Stable custom editor mode identifier."))
			.String(TEXT("modeDescription"), TEXT("Custom editor mode description."))
			.String(TEXT("key"), TEXT(""))
			.String(TEXT("type"), TEXT("Input event type for simulate_input, e.g. key_down, key_up, mouse_click, mouse_move."))
			.String(TEXT("inputType"), TEXT("Alias for type used by simulate_input."))
			.String(TEXT("inputAction"), TEXT(""))
			.Number(TEXT("x"), TEXT("Mouse X coordinate for simulate_input."))
			.Number(TEXT("y"), TEXT("Mouse Y coordinate for simulate_input."))
			.String(TEXT("button"), TEXT("Mouse button for simulate_input."))
			.Integer(TEXT("playerIndex"), TEXT("PIE local-player/controller index for input dispatch."))
			.String(TEXT("axisName"), TEXT("Axis name for analog input dispatch."))
			.Number(TEXT("axisValue"), TEXT("Analog axis value."))
			.Bool(TEXT("relative"), TEXT("Treat mouse coordinates as relative deltas."))
			.StringEnum(TEXT("pieMode"), { TEXT("viewport"), TEXT("new_window"), TEXT("standalone") }, TEXT("PIE destination. Standalone cannot be runtime-probed in-process."))
			.String(TEXT("playerStart"), TEXT("PIE PlayerStart name used when starting or possessing."))
			.String(TEXT("pawnName"), TEXT("PIE pawn name used when starting or possessing."))
			.String(TEXT("enhancedAction"), TEXT("Enhanced Input action label; key delivery still uses the PIE input stack."))
			.Number(TEXT("durationMs"), TEXT("Bounded input hold duration in milliseconds."))
			.Integer(TEXT("warmupFrames"), TEXT("Frames to render before screenshot capture."))
			.Number(TEXT("screenshotDelayMs"), TEXT("Delay after warm-up before screenshot capture."))
			.StringEnum(TEXT("captureMode"), { TEXT("game_viewport"), TEXT("editor_viewport"), TEXT("standalone_window") }, TEXT("Screenshot capture mode."))
			.String(TEXT("screenshotPath"), TEXT("For standalone_window, a sanitized path relative to Saved/ for an existing PNG written by the standalone game. The editor reads this file; it does not capture another process window."))
			.Number(TEXT("timeoutMs"), TEXT("Client-side operation deadline in milliseconds."))
			.Number(TEXT("axisX"), TEXT("Horizontal movement input."))
			.Number(TEXT("axisY"), TEXT("Vertical movement input."))
			.Number(TEXT("minMovementCm"), TEXT("Minimum expected displacement for blocked movement detection."))
			.Bool(TEXT("expectedMovement"), TEXT("Whether blocked movement detection should expect displacement."))
			.Object(TEXT("previousLocation"), TEXT("Previous PIE actor location for blocked movement detection."),
				[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z")); })
			.ArrayOfObjects(TEXT("sequence"), TEXT("Ordered run_playtest_sequence steps with action and optional assertion."))
			.Bool(TEXT("autoStop"), TEXT("Stop PIE in a finally block after run_playtest_sequence; default true."))
			.Bool(TEXT("saveRuntimeChanges"), TEXT("Explicit save opt-in. Runtime changes are not saved by this feature."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ControlEditor);
