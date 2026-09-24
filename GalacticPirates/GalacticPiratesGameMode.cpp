// Copyright Epic Games, Inc. All Rights Reserved.

#include "GalacticPiratesGameMode.h"
#include "GalacticPiratesCharacter.h"
#include "GalacticPirates.h"
#include "GalacticPiratesHUD.h"
#include "WalkableShip.h"
#include "BulldogFighter.h"
#include "HoloMapPoiComponent.h"
#include "HoloMapTypes.h"
#include "OccupancyComponent.h"
#include "CraftWreck.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/EngineTypes.h"
#include "UObject/ConstructorHelpers.h"

AGalacticPiratesGameMode::AGalacticPiratesGameMode()
{
	bUseSeamlessTravel = true;
	HUDClass = AGalacticPiratesHUD::StaticClass();

	static ConstructorHelpers::FClassFinder<AWalkableShip> ShipBP(TEXT("/Game/Ships/Blockout/BP_DebugWalkableShip"));
	if (ShipBP.Succeeded())
	{
		DefaultWalkableShipClass = ShipBP.Class;
	}
}

void AGalacticPiratesGameMode::StartPlay()
{
	Super::StartPlay();
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	if (AWalkableShip* Ship = AWalkableShip::FindPersistentShip(World))
	{
		ABulldogFighter::SpawnDockedOnShip(World, Ship);
	}
}

void AGalacticPiratesGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!NewPlayer)
	{
		Super::RestartPlayer(NewPlayer);
		return;
	}

	AWalkableShip* Ship = GetOrSpawnPersistentShip();
	UE_LOG(LogGalacticPirates, Warning, TEXT("[DedicatedNet] RestartPlayer controller=%s ship=%s"),
		*GetNameSafe(NewPlayer),
		*GetNameSafe(Ship));

	if (!Ship)
	{
		Super::RestartPlayer(NewPlayer);
		return;
	}

	const int32 Slot = Ship->GetPlayersAboard().Num();
	RestartPlayerAtTransform(NewPlayer, Ship->GetSpawnTransformForSlot(Slot));

	AGalacticPiratesCharacter* Character = Cast<AGalacticPiratesCharacter>(NewPlayer->GetPawn());
	if (!Character)
	{
		Super::RestartPlayer(NewPlayer);
		Character = Cast<AGalacticPiratesCharacter>(NewPlayer->GetPawn());
	}

	if (Character && Character->GetBoardedShip() != Ship)
	{
		Character->BoardShip(Ship);
	}
}

void AGalacticPiratesGameMode::Logout(AController* Exiting)
{
	if (APawn* Pawn = Exiting ? Exiting->GetPawn() : nullptr)
	{
		if (AGalacticPiratesCharacter* Character = Cast<AGalacticPiratesCharacter>(Pawn))
		{
			if (AActor* Vehicle = Character->GetOccupiedVehicle())
			{
				if (UOccupancyComponent* Seat = GPFindPilotOccupancy(Vehicle))
				{
					Seat->ForceRelease();
				}
			}
			if (AWalkableShip* Ship = Character->GetBoardedShip())
			{
				Ship->HandlePlayerDisconnected(Character);
			}
		}
	}

	Super::Logout(Exiting);
}

AWalkableShip* AGalacticPiratesGameMode::GetOrSpawnPersistentShip()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	if (AWalkableShip* Existing = AWalkableShip::FindPersistentShip(World))
	{
		if (Existing->HoloPoi)
		{
			Existing->HoloPoi->Kind = EHoloMapPoiKind::OwnShip;
		}
		ABulldogFighter::SpawnDockedOnShip(World, Existing);
		return Existing;
	}

	UClass* ShipClass = DefaultWalkableShipClass;
	if (!ShipClass)
	{
		ShipClass = LoadClass<AWalkableShip>(nullptr, TEXT("/Game/Ships/Blockout/BP_DebugWalkableShip.BP_DebugWalkableShip_C"));
	}
	if (!ShipClass)
	{
		UE_LOG(LogGalacticPirates, Error, TEXT("[DedicatedNet] No walkable ship class available to spawn"));
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Spawned = World->SpawnActor<AWalkableShip>(ShipClass, FTransform::Identity, Params);
	if (Spawned && Spawned->HoloPoi)
	{
		Spawned->HoloPoi->Kind = EHoloMapPoiKind::OwnShip;
	}
	UE_LOG(LogGalacticPirates, Warning, TEXT("[DedicatedNet] Spawned persistent walkable ship %s"), *GetNameSafe(Spawned));
	ABulldogFighter::SpawnDockedOnShip(World, Spawned);
	return Spawned;
}
