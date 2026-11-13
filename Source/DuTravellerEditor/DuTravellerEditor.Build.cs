using UnrealBuildTool;

public class DuTravellerEditor : ModuleRules
{
	public DuTravellerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"UMG",
			"DuTraveller"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"UnrealEd",
			"Blutility",
			"UMGEditor",
			"EditorFramework",
			"ToolMenus",
			"AssetRegistry",
			"AssetTools",
			"PythonScriptPlugin",
			"Json",
			"JsonUtilities",
			"InputCore"
		});
	}
}
