using UnrealBuildTool;

public class NebulaForgeBridgeRuntime : ModuleRules
{
    public NebulaForgeBridgeRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Json",
            "Networking",
            "Sockets"
        });
    }
}
