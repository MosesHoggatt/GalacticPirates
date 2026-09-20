// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class SpaceBattleSimulator : ModuleRules
{
	public SpaceBattleSimulator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
            "CoreUObject",
            "Engine", // Ensure this is present
            "InputCore",
            "AIModule", // Already needed for Behavior Trees
            "GameplayTasks", // Already needed for Behavior Trees
            "Kismet"
        });

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
