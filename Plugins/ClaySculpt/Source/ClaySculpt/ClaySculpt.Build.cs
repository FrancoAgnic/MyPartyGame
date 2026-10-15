using UnrealBuildTool;

public class ClaySculpt : ModuleRules
{
	public ClaySculpt(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProceduralMeshComponent",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
			"RHI",
		});

		// Bake a Static Mesh: SOLO en builds de editor (nunca en el juego empaquetado).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",
				"MeshDescription",
				"StaticMeshDescription",
				"AssetRegistry",
			});
		}
	}
}
