// McpTool_ControlActor.cpp — control_actor tool definition (68 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ControlActor : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("control_actor"); }

	FString GetDescription() const override
	{
		return TEXT("Spawn and control actors, manage components/tags, collision profiles/channels, attach actors, "
			"and automate editor selection and grouping.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("spawn"),
				TEXT("spawn_actor"),
				TEXT("spawn_paper_sprite_actor"),
				TEXT("spawn_paper_flipbook_actor"),
				TEXT("spawn_paper_tile_map_actor"),
				TEXT("spawn_blueprint"),
				TEXT("delete"),
				TEXT("destroy_actor"),
				TEXT("delete_by_tag"),
				TEXT("duplicate"),
				TEXT("apply_force"),
				TEXT("set_transform"),
				TEXT("teleport_actor"),
				TEXT("set_actor_location"),
				TEXT("set_actor_rotation"),
				TEXT("set_actor_scale"),
				TEXT("set_actor_transform"),
				TEXT("get_transform"),
				TEXT("get_actor_transform"),
				TEXT("set_visibility"),
				TEXT("set_actor_visible"),
				TEXT("add_component"),
				TEXT("create_instanced_static_mesh_component"),
				TEXT("create_hierarchical_instanced_static_mesh"),
				TEXT("create_procedural_mesh_component"),
				TEXT("add_instance"),
				TEXT("remove_instance"),
				TEXT("update_instance_transform"),
				TEXT("configure_instance_culling"),
				TEXT("configure_instance_lod"),
				TEXT("create_media_sound_component"),
				TEXT("set_paper_sprite_color"),
				TEXT("set_paper_tile_map_color"),
				TEXT("set_paper_tile_map_layer_color"),
				TEXT("get_paper_tile_map_info"),
				TEXT("configure_paper_flipbook"),
				TEXT("configure_flipbook_loop"),
				TEXT("configure_paper_character"),
				TEXT("configure_camera_settings"),
				TEXT("configure_camera_rig_rail"),
				TEXT("configure_camera_rig_crane"),
				TEXT("remove_component"),
				TEXT("set_component_properties"),
				TEXT("set_component_property"),
				TEXT("set_material"),
				TEXT("set_actor_material"),
				TEXT("apply_material"),
				TEXT("get_component_property"),
				TEXT("get_components"),
				TEXT("get_actor_components"),
				TEXT("get_actor_bounds"),
				TEXT("add_tag"),
				TEXT("remove_tag"),
				TEXT("get_gameplay_tags"),
				TEXT("add_gameplay_tag"),
				TEXT("remove_gameplay_tag"),
				TEXT("find_by_tag"),
				TEXT("find_actors_by_tag"),
				TEXT("find_by_name"),
				TEXT("find_actors_by_name"),
				TEXT("find_by_class"),
				TEXT("find_actors_by_class"),
				TEXT("select_actor"),
				TEXT("select_actors_by_class"),
				TEXT("select_actors_by_tag"),
				TEXT("select_actors_in_volume"),
				TEXT("deselect_all"),
				TEXT("get_selected_actors"),
				TEXT("group_actors"),
				TEXT("ungroup_actors"),
				TEXT("select_all"),
				TEXT("invert_selection"),
				TEXT("select_children"),
				TEXT("remove_selected_from_group"),
				TEXT("lock_selected_groups"),
				TEXT("unlock_selected_groups"),
				TEXT("create_collision_channel"),
				TEXT("create_collision_profile"),
				TEXT("configure_channel_responses"),
				TEXT("configure_object_type"),
				TEXT("configure_trace_channel"),
				TEXT("set_actor_collision_profile"),
				TEXT("set_component_collision_profile"),
				TEXT("get_actor_collision"),
				TEXT("get_component_collision"),
				TEXT("validate_collision_profile"),
				TEXT("list"),
				TEXT("set_blueprint_variables"),
				TEXT("create_snapshot"),
				TEXT("attach"),
				TEXT("attach_actor"),
				TEXT("detach"),
				TEXT("detach_actor"),
				TEXT("set_actor_collision"),
				TEXT("call_actor_function")
			}, TEXT("Action"))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.Array(TEXT("actorNames"), TEXT("Actor names for bulk actor operations."))
			.String(TEXT("childActor"),
				TEXT("Name of the child actor (for attach/detach operations)."))
			.String(TEXT("parentActor"),
				TEXT("Name of the parent actor (for attach operations)."))
			.String(TEXT("classPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("actorClass"), TEXT("Actor class alias or path."))
			.String(TEXT("className"), TEXT("Actor class name."))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.String(TEXT("paperAssetPath"), TEXT("Paper2D sprite or flipbook asset path."))
			.Object(TEXT("color"), TEXT("Paper sprite color (r, g, b, a)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("r")).Number(TEXT("g")).Number(TEXT("b")).Number(TEXT("a"));
				})
			.Number(TEXT("playRate"), TEXT("Flipbook playback rate."))
			.Bool(TEXT("looping"), TEXT("Whether the flipbook loops."))
			.Number(TEXT("playbackPosition"), TEXT("Flipbook playback position in seconds."))
			.StringEnum(TEXT("playbackAction"), {
				TEXT("play"), TEXT("play_from_start"), TEXT("reverse"), TEXT("reverse_from_end"), TEXT("stop")
			}, TEXT("Optional flipbook playback command."))
			.Number(TEXT("maxWalkSpeed"), TEXT("Paper character maximum ground speed."))
			.Number(TEXT("gravityScale"), TEXT("Paper character gravity scale."))
			.Integer(TEXT("layer"), TEXT("Paper tile-map layer index."))
			.String(TEXT("materialPath"), TEXT("Material asset path."))
			.Integer(TEXT("materialSlot"), TEXT("Material slot index."))
			.Integer(TEXT("materialIndex"), TEXT("Material slot index alias."))
			.Bool(TEXT("allComponents"), TEXT("Apply material to every primitive component on the actor."))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Object(TEXT("scale"), TEXT("3D scale (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("offset"), TEXT("3D offset (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("force"), TEXT("3D vector."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("componentType"), TEXT(""))
			.String(TEXT("componentName"), TEXT("Name of the component."))
			.Integer(TEXT("instanceIndex"), TEXT("Instance index."))
			.Bool(TEXT("worldSpace"), TEXT("Interpret the instance transform in world space."))
			.Bool(TEXT("markRenderStateDirty"), TEXT("Update rendering immediately."))
			.Bool(TEXT("teleport"), TEXT("Teleport the instance when updating its transform."))
			.Integer(TEXT("startCullDistance"), TEXT("Distance at which instances begin fading."))
			.Integer(TEXT("endCullDistance"), TEXT("Distance at which instances finish fading."))
			.Integer(TEXT("minDrawDistance"), TEXT("Minimum distance at which instances draw."))
			.Number(TEXT("lodDistanceScale"), TEXT("Scale applied to instance LOD distances."))
			.Bool(TEXT("useGpuLodSelection"), TEXT("Use GPU LOD selection for instances."))
			.String(TEXT("mediaPlayerPath"), TEXT("UMediaPlayer asset path for media audio output."))
			.Number(TEXT("focalLength"), TEXT("Cine camera focal length in millimeters."))
			.Number(TEXT("aperture"), TEXT("Cine camera f-stop."))
			.Number(TEXT("focusDistance"), TEXT("Cine camera manual focus distance in centimeters."))
			.Number(TEXT("filmbackSensorWidth"), TEXT("Cine camera filmback sensor width in millimeters."))
			.Number(TEXT("filmbackSensorHeight"), TEXT("Cine camera filmback sensor height in millimeters."))
			.Number(TEXT("currentPositionOnRail"), TEXT("Normalized camera rig rail position from 0 to 1."))
			.Bool(TEXT("lockOrientationToRail"), TEXT("Orient the rail mount along the rail."))
			.Number(TEXT("craneArmLength"), TEXT("Camera rig crane arm length in centimeters."))
			.Number(TEXT("cranePitch"), TEXT("Camera rig crane arm pitch in degrees."))
			.Number(TEXT("craneYaw"), TEXT("Camera rig crane arm yaw in degrees."))
			.Bool(TEXT("lockMountPitch"), TEXT("Lock the crane camera mount pitch."))
			.Bool(TEXT("lockMountYaw"), TEXT("Lock the crane camera mount yaw."))
			.FreeformObject(TEXT("properties"), TEXT(""))
			.String(TEXT("propertyName"), TEXT("Name of the property."))
			.FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
			.Bool(TEXT("visible"), TEXT("Whether the item/actor is visible."))
			.String(TEXT("newName"), TEXT("New name for renaming."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("tag"), TEXT("Name of the tag."))
			.String(TEXT("volumeActorName"), TEXT("Volume actor used for spatial selection."))
			.Bool(TEXT("replaceSelection"), TEXT("Clear the current selection before selecting matches."))
			.Bool(TEXT("selectEvenIfHidden"), TEXT("Allow selection of hidden actors."))
			.Bool(TEXT("includeDerivedClasses"), TEXT("Include subclasses for class selection."))
			.Bool(TEXT("recurseChildren"), TEXT("Include all descendants when selecting children."))
			.Bool(TEXT("warnIfLevelLocked"), TEXT("Allow the editor to warn about locked levels."))
			.FreeformObject(TEXT("variables"), TEXT(""))
			.String(TEXT("snapshotName"), TEXT(""))
			.Integer(TEXT("limit"), TEXT("Maximum number of actors to return."))
			.String(TEXT("filter"), TEXT("Optional actor label/name filter for list."))
			.Bool(TEXT("collisionEnabled"), TEXT("Whether actor collision is enabled."))
			.String(TEXT("collisionMode"), TEXT("Collision profile mode: NoCollision, QueryOnly, PhysicsOnly, or QueryAndPhysics."))
			.String(TEXT("profileName"), TEXT("Collision profile name."))
			.String(TEXT("channelName"), TEXT("Collision channel display name."))
			.String(TEXT("channelType"), TEXT("Custom channel type: object or trace."))
			.String(TEXT("objectType"), TEXT("Collision object channel name."))
			.String(TEXT("traceChannel"), TEXT("Trace channel name."))
			.String(TEXT("response"), TEXT("Collision response: block, overlap, or ignore."))
			.String(TEXT("defaultResponse"), TEXT("Default response for a new channel."))
			.FreeformObject(TEXT("responses"), TEXT("Map of collision channel names to block, overlap, or ignore."))
			.Bool(TEXT("saveConfig"), TEXT("Persist collision settings to DefaultEngine.ini."))
			.Bool(TEXT("staticObject"), TEXT("Whether a custom channel is for static objects."))
			.Bool(TEXT("traceType"), TEXT("Whether a custom channel is a trace channel."))
			.String(TEXT("helpMessage"), TEXT("Optional collision profile help text."))
			.String(TEXT("functionName"), TEXT("Name of the function."))
			.Array(TEXT("arguments"), TEXT("Arguments to pass to an actor function."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ControlActor);
