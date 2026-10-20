using UnrealBuildTool;

public class HorrorAI : ModuleRules
{
    public HorrorAI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "AIModule" });
        PrivateDependencyModuleNames.AddRange(new[] { "NavigationSystem" });
    }
}
