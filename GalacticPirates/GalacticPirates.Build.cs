// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GalacticPirates : ModuleRules
{
	public GalacticPirates(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate",
			"SlateCore",
			"EngineCameras",
			"ProceduralMeshComponent"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"GalacticPirates",
			"GalacticPirates/Variant_Horror",
			"GalacticPirates/Variant_Horror/UI",
			"GalacticPirates/Variant_Shooter",
			"GalacticPirates/Variant_Shooter/AI",
			"GalacticPirates/Variant_Shooter/UI",
			"GalacticPirates/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
