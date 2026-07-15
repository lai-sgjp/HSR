using UnrealBuildTool;

public class HSR : ModuleRules
{
	public HSR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}