// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MyPartyGame : ModuleRules
{
	public MyPartyGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ProceduralMeshComponent",
			"RHI",
			"RenderCore",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			// Online Subsystem — Steam (cargado dinámicamente vía plugin/config).
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"CoreOnline",
			// Fase 5 — FPlatformApplicationMisc::ClipboardCopy (compartir código de sesión privada)
			"ApplicationCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		// Steamworks SDK directo — para FPTSteamWorldwideSearch/FPTSteamDirectJoin que llaman
		// a RequestLobbyList con k_ELobbyDistanceFilterWorldwide, bypasseando el filtro regional
		// hardcodeado en OnlineSessionAsyncLobbySteam.cpp del engine.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDefinitions.Add("PT_WITH_STEAM=1");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
		}
		else
		{
			PublicDefinitions.Add("PT_WITH_STEAM=0");
		}

		PublicIncludePaths.AddRange(new string[] {
			"MyPartyGame",
			"MyPartyGame/Multiplayer",
			"MyPartyGame/UI",
			"MyPartyGame/Lobby",
			"MyPartyGame/Variant_Platforming",
			"MyPartyGame/Variant_Platforming/Animation",
			"MyPartyGame/Variant_Combat",
			"MyPartyGame/Variant_Combat/AI",
			"MyPartyGame/Variant_Combat/Animation",
			"MyPartyGame/Variant_Combat/Gameplay",
			"MyPartyGame/Variant_Combat/Interfaces",
			"MyPartyGame/Variant_Combat/UI",
			"MyPartyGame/Variant_SideScrolling",
			"MyPartyGame/Variant_SideScrolling/AI",
			"MyPartyGame/Variant_SideScrolling/Gameplay",
			"MyPartyGame/Variant_SideScrolling/Interfaces",
			"MyPartyGame/Variant_SideScrolling/UI",
			"MyPartyGame/Sculpt"
		});
	}
}
