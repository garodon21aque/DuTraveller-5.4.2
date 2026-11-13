using UnrealBuildTool;

public class DuTraveller : ModuleRules
{
	public DuTraveller(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"AIModule",
			"Core",
			"CoreUObject",
			"Engine",
			"EnhancedInput",
			"InputCore",
			"GameplayAbilities",
			"GameplayTasks",
			"GameplayTags",
			"Niagara",
			"Slate",
			"SlateCore",
			"UMG"
		});

		if (Target.bBuildEditor)
		{
			// Editor-only asset-repair helpers (user-defined struct/enum utilities).
			PrivateDependencyModuleNames.AddRange(new[]
			{
				"AnimGraph",
				"AnimGraphRuntime",
				"BlueprintGraph",
				"AssetTools",
				"UMGEditor",
				"UnrealEd"
			});
		}
	}
}
