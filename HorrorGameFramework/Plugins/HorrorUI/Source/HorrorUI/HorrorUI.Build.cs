using UnrealBuildTool;

public class HorrorUI : ModuleRules
{
    public HorrorUI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "Slate", "SlateCore", "UMG", "DeveloperSettings"
        });
        PrivateDependencyModuleNames.Add("InputCore");
    }
}
