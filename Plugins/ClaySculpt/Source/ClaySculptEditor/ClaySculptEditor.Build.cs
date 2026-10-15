using UnrealBuildTool;

public class ClaySculptEditor : ModuleRules
{
	public ClaySculptEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"ClaySculpt",
			"ProceduralMeshComponent",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"UnrealEd",
			"EditorFramework",
			"EditorInteractiveToolsFramework",
			"InteractiveToolsFramework",
			"ToolMenus",
			"MeshDescription",
			"StaticMeshDescription",
			"AssetTools",
		});
	}
}
