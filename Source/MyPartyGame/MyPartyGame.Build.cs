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
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"MyPartyGame",
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
			"MyPartyGame/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
