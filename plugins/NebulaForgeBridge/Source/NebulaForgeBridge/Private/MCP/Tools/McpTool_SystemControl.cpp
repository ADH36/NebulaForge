// McpTool_SystemControl.cpp — system_control tool definition

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_SystemControl : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("system_control"); }

	FString GetDescription() const override
	{
		return TEXT("Run profiling, set quality/CVars, execute console commands, "
			"execute Python scripts, manage Unreal subsystems, timers, async work, and gameplay tasks, "
			"run UBT, manage widgets, delegates, Blueprint interfaces, and take screenshots.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), McpConsolidatedActions::SystemControl(),
				TEXT("Action"))
			.StringEnum(TEXT("type"), {
				TEXT("CPU"),
				TEXT("GPU"),
				TEXT("Memory"),
				TEXT("RenderThread"),
				TEXT("GameThread"),
				TEXT("All")
			}, TEXT("Profiling or benchmark type."))
			.String(TEXT("profileType"), TEXT(""))
			.String(TEXT("category"), TEXT(""))
			.Number(TEXT("level"), TEXT(""))
			.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."))
			.String(TEXT("resolution"), TEXT("Resolution setting (e.g., 1024x1024)."))
			.String(TEXT("command"), TEXT(""))
			.String(TEXT("target"), TEXT(""))
			.String(TEXT("platform"), TEXT(""))
			.String(TEXT("configuration"), TEXT(""))
			.String(TEXT("arguments"), TEXT(""))
			.Bool(TEXT("async"), TEXT("For SaveGame and host operations, return a lifecycle identifier instead of waiting."))
			.String(TEXT("jobId"), TEXT("Managed host job identifier."))
			.String(TEXT("uatOperation"), TEXT("RunUAT BuildCookRun operation."))
			.Bool(TEXT("server"), TEXT("Build a dedicated server/no-client target."))
			.String(TEXT("serverConfiguration"), TEXT("Dedicated server configuration."))
			.String(TEXT("archiveDirectory"), TEXT("Archive output directory."))
			.String(TEXT("filePath"), TEXT("Relative path inside the Unreal project."))
			.Array(TEXT("requiredDirectories"), TEXT("Relative directories required in the project."))
			.Bool(TEXT("includeInventory"), TEXT("Include a bounded project content/config/map inventory."))
			.StringEnum(TEXT("validationMode"), { TEXT("static"), TEXT("data_validation") }, TEXT("Validation depth; data_validation runs UnrealEditor-Cmd DataValidation."))
			.Array(TEXT("validationArguments"), TEXT("Additional validated DataValidation commandlet tokens."))
			.Number(TEXT("timeoutMs"), TEXT("Maximum host validation job duration in milliseconds."))
			.StringEnum(TEXT("pluginAction"), { TEXT("list"), TEXT("validate"), TEXT("enable"), TEXT("disable") }, TEXT("Project plugin operation."))
			.String(TEXT("pluginName"), TEXT("Declared project plugin identifier."))
			.String(TEXT("enginePath"), TEXT("Optional Unreal Engine root used for capability discovery."))
			.String(TEXT("artifactPath"), TEXT("Release artifact to sign, confined to the project or archive directory."))
			.String(TEXT("certificatePath"), TEXT("Windows signing certificate path."))
			.String(TEXT("signingIdentity"), TEXT("Platform signing identity or certificate thumbprint."))
			.String(TEXT("keystorePath"), TEXT("Android signing keystore path."))
			.String(TEXT("signingAlias"), TEXT("Android signing key alias."))
			.String(TEXT("signingPasswordEnv"), TEXT("Environment variable containing the Android signing password."))
			.Bool(TEXT("dryRun"), TEXT("Validate and return the signing command without executing it."))
			.String(TEXT("content"), TEXT("Project file content."))
			.Bool(TEXT("backup"), TEXT("Create a .bak file before replacement."))
			.String(TEXT("className"), TEXT("U-prefixed SaveGame class name."))
			.String(TEXT("headerPath"), TEXT("Relative SaveGame header path."))
			.String(TEXT("sourcePath"), TEXT("Relative SaveGame source path."))
			.Array(TEXT("variables"), TEXT("SaveGame variables with name, type, and optional defaultValue."))
			.String(TEXT("tag"), TEXT("Gameplay Tag name."))
			.String(TEXT("comment"), TEXT("Gameplay Tag developer comment."))
			.String(TEXT("configName"), TEXT("Safe Unreal Config filename, for example DefaultGame.ini."))
			.String(TEXT("section"), TEXT("INI section name."))
			.String(TEXT("key"), TEXT("INI key name."))
			.String(TEXT("saveGameObject"), TEXT("Loaded USaveGame object path to save."))
			.String(TEXT("slotName"), TEXT("Safe SaveGame slot identifier."))
			.Number(TEXT("userIndex"), TEXT("SaveGame local user index."))
			.Array(TEXT("requiredFiles"), TEXT("Relative files required in the release archive."))
			.Bool(TEXT("requirePak"), TEXT("Require at least one .pak file in the release archive."))
			.String(TEXT("filter"), TEXT(""))
			.String(TEXT("channels"), TEXT(""))
			.String(TEXT("subAction"), TEXT("Insights session operation: start_session, stop_session, or get_session_status."))
			.String(TEXT("widgetPath"), TEXT("Widget blueprint path."))
			.String(TEXT("childClass"), TEXT(""))
			.String(TEXT("parentName"), TEXT(""))
			// section/key are declared above for config hierarchy actions.
			.String(TEXT("value"), TEXT(""))
			.String(TEXT("code"), TEXT("Python code to execute inline"))
			.String(TEXT("file"), TEXT("Path to .py file to execute"))
			.Number(TEXT("duration"), TEXT("Duration in seconds."))
			.String(TEXT("outputPath"), TEXT("Output file or directory path."))
			.Bool(TEXT("detailed"), TEXT("Whether to include detailed output."))
			.Number(TEXT("scale"), TEXT("Resolution scale or percentage."))
			.Number(TEXT("maxFPS"), TEXT("Frame rate limit."))
			.Number(TEXT("poolSize"), TEXT("Texture streaming pool size in MB."))
			.Bool(TEXT("boostPlayerLocation"), TEXT("Whether to boost streaming around the player location."))
			.Number(TEXT("forceLOD"), TEXT("Forced LOD index."))
			.Number(TEXT("lodBias"), TEXT("LOD bias."))
			.Bool(TEXT("enableInstancing"), TEXT("Whether to enable instancing."))
			.Bool(TEXT("enableBatching"), TEXT("Whether to enable batching."))
			.Bool(TEXT("mergeActors"), TEXT("Whether to merge source actors."))
			.Array(TEXT("actors"), TEXT("Actor names."))
			.Number(TEXT("streamingDistance"), TEXT("World Partition streaming distance."))
			.Number(TEXT("cellSize"), TEXT("World Partition cell size."))
			.String(TEXT("packageName"), TEXT("Package name or asset path."))
			.String(TEXT("assetPath"), TEXT("Asset path to validate."))
			.String(TEXT("path"), TEXT("Asset or directory path to validate."))
			.Array(TEXT("paths"), TEXT("Asset or directory paths to validate."))
			.Bool(TEXT("recursive"), TEXT("Whether directory validation counts assets recursively."))
			.Bool(TEXT("replaceSourceActors"), TEXT("Whether to replace source actors after merge."))
			.String(TEXT("filename"), TEXT("Screenshot filename."))
			.String(TEXT("mode"), TEXT("Screenshot source: editor_viewport, game_viewport, full_editor_window."))
			.Bool(TEXT("returnBase64"), TEXT("Return PNG image data as base64 when supported. Defaults to true for full_editor_window and game_viewport modes."))
			.Bool(TEXT("includeMetadata"), TEXT("Attach caller-provided metadata to the response."))
			.FreeformObject(TEXT("metadata"), TEXT("Caller-provided screenshot metadata."))
			.String(TEXT("subsystemClass"), TEXT("Subsystem class path, for example /Script/Engine.WorldPartitionSubsystem."))
			.String(TEXT("subsystemName"), TEXT("Subsystem class name alias."))
			.String(TEXT("subsystemScope"), TEXT("engine, game_instance, world, local_player, or editor."))
			.String(TEXT("worldContext"), TEXT("auto, pie, or editor when resolving world-owned subsystems."))
			.Number(TEXT("playerIndex"), TEXT("Local player index for local-player subsystems."))
			.String(TEXT("tickType"), TEXT("conditional, always, or never for tickable world subsystems."))
			.Bool(TEXT("tickEnabled"), TEXT("Enable or disable ticking for a tickable world subsystem."))
			.String(TEXT("timerId"), TEXT("Managed timer identifier."))
			.Number(TEXT("rate"), TEXT("Timer interval in seconds."))
			.Number(TEXT("firstDelay"), TEXT("Initial timer delay in seconds."))
			.Bool(TEXT("looping"), TEXT("Repeat the timer after each interval."))
			.String(TEXT("callbackObject"), TEXT("Loaded UObject path for an optional zero-argument callback."))
			.String(TEXT("callbackFunction"), TEXT("Optional zero-argument UFunction to invoke when a timer or latent action completes."))
			.String(TEXT("latentId"), TEXT("Managed latent-action identifier."))
			.Number(TEXT("uuid"), TEXT("Latent action UUID; generated when omitted."))
			.Number(TEXT("linkage"), TEXT("Latent callback linkage value."))
			.String(TEXT("asyncId"), TEXT("Managed async-action identifier."))
			.String(TEXT("execution"), TEXT("Async execution mode: task_graph, task_graph_main_thread, task_graph_main_tick, thread, thread_pool, or large_thread_pool."))
			.String(TEXT("label"), TEXT("Optional label for an async action."))
			.String(TEXT("taskId"), TEXT("Managed gameplay-task identifier."))
			.String(TEXT("ownerObject"), TEXT("Loaded UObject path implementing a gameplay-task owner."))
			.String(TEXT("instanceName"), TEXT("Gameplay task instance name."))
			.Number(TEXT("priority"), TEXT("Gameplay task priority from 0 to 255."))
			.Bool(TEXT("activate"), TEXT("Activate the gameplay task immediately; defaults to true."))
			.String(TEXT("taskType"), TEXT("Managed task type; currently generic is supported."))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path for delegate or interface authoring."))
			.String(TEXT("folder"), TEXT("Content folder for a newly created Blueprint Interface."))
			.String(TEXT("delegateObject"), TEXT("Loaded UObject path containing the reflected delegate property."))
			.String(TEXT("delegateName"), TEXT("Delegate or event-dispatcher property name."))
			.String(TEXT("delegateKind"), TEXT("single, multicast, or event_dispatcher for authored delegate variables."))
			.String(TEXT("targetObject"), TEXT("Loaded UObject path receiving a delegate callback."))
			.String(TEXT("functionName"), TEXT("Function name used for interface or delegate operations."))
			.String(TEXT("interfacePath"), TEXT("Blueprint Interface asset path or interface class path."))
			.String(TEXT("interfaceClass"), TEXT("Interface class path or name."))
			.String(TEXT("interfaceFunction"), TEXT("Interface function name."))
			.String(TEXT("interfaceFunctionName"), TEXT("Alias for interfaceFunction."))
			.FreeformObject(TEXT("parameterValues"), TEXT("JSON values for reflected function or delegate parameters."))
			.Bool(TEXT("saveAsset"), TEXT("Save modified Blueprint assets; defaults to true."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_SystemControl);
