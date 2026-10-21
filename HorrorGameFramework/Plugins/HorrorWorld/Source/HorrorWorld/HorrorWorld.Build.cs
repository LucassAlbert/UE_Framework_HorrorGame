using UnrealBuildTool;
public class HorrorWorld : ModuleRules
{
    public HorrorWorld(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "SaveSystem" });
    }
}
