// Copyright Epic Games, Inc. All Rights Reserved.

#include "GalacticPiratesGameMode.h"
#include "GalacticPiratesCharacter.h"
#include "GalacticPirates.h"
#include "WalkableShip.h"
#include "HoloMapPoiComponent.h"
#include "HoloMapTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/EngineTypes.h"
#include "UObject/ConstructorHelpers.h"

AGalacticPiratesGameMode::AGalacticPiratesGameMode()
{
	bUseSeamlessTravel = true;

	static ConstructorHelpers::FClassFinder<AWalkableShip> ShipBP(TEXT("/Game/Ships/Debug/BP_DebugWalkableShip"));
	if (ShipBP.Succeeded())
	{
		DefaultWalkableShipClass = ShipBP.Class;
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
		for (TActorIterator<AGalacticPiratesCharacter> It(GetWorld()); It; ++It)
		{
			if (*It && (*It)->GetController() == nullptr)
			{
				NewPlayer->Possess(*It);
				Character = *It;
				UE_LOG(LogGalacticPirates, Warning, TEXT("[DedicatedNet] Possessed existing character %s"), *GetNameSafe(Character));
				break;
			}
		}
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
		return Existing;
	}

	UClass* ShipClass = DefaultWalkableShipClass;
	if (!ShipClass)
	{
		ShipClass = LoadClass<AWalkableShip>(nullptr, TEXT("/Game/Ships/Debug/BP_DebugWalkableShip.BP_DebugWalkableShip_C"));
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
	return Spawned;
}
