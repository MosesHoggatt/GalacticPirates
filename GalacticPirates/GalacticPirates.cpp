// Copyright Epic Games, Inc. All Rights Reserved.

#include "GalacticPirates.h"
#include "Modules/ModuleManager.h"
#include "ShipDebug.h"
#include "GalacticPiratesCharacter.h"
#include "WalkableShip.h"
#include "ShipMissileSalvoComponent.h"
#include "HoloMapPoiComponent.h"
#include "HoloMapTypes.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, GalacticPirates, "GalacticPirates" );

DEFINE_LOG_CATEGORY(LogGalacticPirates);

static FAutoConsoleCommandWithWorld GDumpShipDebugCommand(
	TEXT("gp.DumpShipDebug"),
	TEXT("Dump a walkable-ship diagnostic snapshot for the local player."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World)
		{
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		AGalacticPiratesCharacter* Character = PC ? Cast<AGalacticPiratesCharacter>(PC->GetPawn()) : nullptr;
		if (!Character)
		{
			UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDebug] gp.DumpShipDebug found no player character"));
			return;
		}

		Character->DumpShipDebugSnapshot(TEXT("Console"));
	}));

static FAutoConsoleCommandWithWorld GStartShipPlaytestCommand(
	TEXT("gp.StartShipPlaytest"),
	TEXT("Start the automated walkable-ship PIE playtest on the local player."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World)
		{
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		AGalacticPiratesCharacter* Character = PC ? Cast<AGalacticPiratesCharacter>(PC->GetPawn()) : nullptr;
		if (!Character)
		{
			UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDebug] gp.StartShipPlaytest found no player character"));
			return;
		}

		Character->StartAutomatedShipPlaytest();
	}));

static FAutoConsoleCommandWithWorld GStartInteriorTestCommand(
	TEXT("gp.StartInteriorTest"),
	TEXT("Start the interior walk test: camera vs capsule facing, and slide on a moving ship."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World)
		{
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		AGalacticPiratesCharacter* Character = PC ? Cast<AGalacticPiratesCharacter>(PC->GetPawn()) : nullptr;
		if (!Character)
		{
			UE_LOG(LogGalacticPirates, Warning, TEXT("[Interior] gp.StartInteriorTest found no player character"));
			return;
		}

		GPStartInteriorWalkTest(Character);
	}));

static FAutoConsoleCommandWithWorld GStartExtremeFlightTestCommand(
	TEXT("gp.StartExtremeFlightTest"),
	TEXT("Start the extreme flight walkability test: max speed/rotation, helm exit, walk on a tumbling ship."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World)
		{
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		AGalacticPiratesCharacter* Character = PC ? Cast<AGalacticPiratesCharacter>(PC->GetPawn()) : nullptr;
		if (!Character)
		{
			UE_LOG(LogGalacticPirates, Warning, TEXT("[ExtremeFlight] gp.StartExtremeFlightTest found no player character"));
			return;
		}

		GPStartExtremeFlightTest(Character);
	}));

static FAutoConsoleCommandWithWorld GStartHelmSteerTestCommand(
	TEXT("gp.StartHelmSteerTest"),
	TEXT("Take the helm and inject mouse yaw to verify ship rotation responds."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World)
		{
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		AGalacticPiratesCharacter* Character = PC ? Cast<AGalacticPiratesCharacter>(PC->GetPawn()) : nullptr;
		if (!Character)
		{
			UE_LOG(LogGalacticPirates, Warning, TEXT("[HelmSteer] gp.StartHelmSteerTest found no player character"));
			return;
		}

		GPStartHelmSteerTest(Character);
	}));

static FAutoConsoleCommandWithWorld GStartHelmJitterTestCommand(
	TEXT("gp.StartHelmJitterTest"),
	TEXT("Take the helm, spin the ship, and measure pilot/camera jitter."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World)
		{
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		AGalacticPiratesCharacter* Character = PC ? Cast<AGalacticPiratesCharacter>(PC->GetPawn()) : nullptr;
		if (!Character)
		{
			UE_LOG(LogGalacticPirates, Warning, TEXT("[HelmJitter] gp.StartHelmJitterTest found no player character"));
			return;
		}

		GPStartHelmJitterTest(Character);
	}));

static FAutoConsoleCommandWithWorld GStartShipJumpTestCommand(
	TEXT("gp.StartShipJumpTest"),
	TEXT("Jump on a walkable ship while idle, translating, rotating, and combined."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World)
		{
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		AGalacticPiratesCharacter* Character = PC ? Cast<AGalacticPiratesCharacter>(PC->GetPawn()) : nullptr;
		if (!Character)
		{
			UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipJump] gp.StartShipJumpTest found no player character"));
			return;
		}

		GPStartShipJumpTest(Character);
	}));

static FAutoConsoleCommandWithWorld GStartDedicatedNetTestCommand(
	TEXT("gp.StartDedicatedNetTest"),
	TEXT("Start the dedicated-server persistent-world boarding and helm test."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World)
		{
			return;
		}

		GPStartDedicatedNetTest(World);
	}));

static FAutoConsoleCommandWithWorld GStartShipDuelTestCommand(
	TEXT("gp.StartShipDuelTest"),
	TEXT("Dedicated-server two-ship pulse cannon duel with explosions."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World)
		{
			return;
		}

		GPStartShipDuelTest(World);
	}));

static FAutoConsoleCommandWithWorld GTestMissileSalvoCommand(
	TEXT("gp.TestMissileSalvo"),
	TEXT("Spawn a dummy target ahead and fire a heat-seeking missile salvo (server/PIE)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGalacticPirates, Warning, TEXT("[MissileSalvo] gp.TestMissileSalvo ignored on client"));
			return;
		}

		AWalkableShip* Shooter = AWalkableShip::FindPersistentShip(World);
		if (!Shooter || !Shooter->MissileSalvo)
		{
			UE_LOG(LogGalacticPirates, Warning, TEXT("[MissileSalvo] no ship"));
			return;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector TargetLoc = Shooter->GetActorLocation() + Shooter->GetActorForwardVector() * 3200.0f;
		AWalkableShip* Target = World->SpawnActor<AWalkableShip>(TargetLoc, (-Shooter->GetActorForwardVector()).Rotation(), Params);
		if (Target && Target->HoloPoi)
		{
			Target->HoloPoi->Kind = EHoloMapPoiKind::EnemyShip;
		}

		const bool bFired = Shooter->MissileSalvo->FireIgnoringTerminalRange(nullptr);
		UE_LOG(LogGalacticPirates, Warning, TEXT("[MissileSalvo] gp.TestMissileSalvo fired=%s target=%s"),
			bFired ? TEXT("true") : TEXT("false"),
			*GetNameSafe(Target));
	}));
