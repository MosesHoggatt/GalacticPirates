#include "WalkableShip.h"
#include "ShipMovementComponent.h"
#include "HelmComponent.h"
#include "GalacticPiratesCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"

AWalkableShip::AWalkableShip()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	ShipRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ShipRoot"));
	RootComponent = ShipRoot;

	HullMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HullMesh"));
	HullMesh->SetupAttachment(ShipRoot);
	HullMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HullMesh->SetVisibility(true);

	InteriorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InteriorMesh"));
	InteriorMesh->SetupAttachment(ShipRoot);
	InteriorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	InteriorMesh->SetCollisionProfileName(TEXT("BlockAll"));

	SpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnPoint"));
	SpawnPoint->SetupAttachment(ShipRoot);
	SpawnPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));

	ShipMovement = CreateDefaultSubobject<UShipMovementComponent>(TEXT("ShipMovement"));

	Helm = CreateDefaultSubobject<UHelmComponent>(TEXT("Helm"));
	Helm->SetupAttachment(ShipRoot);

	CurrentPilot = nullptr;
	LastReplicatedRotation = FQuat::Identity;
}

void AWalkableShip::BeginPlay()
{
	Super::BeginPlay();
	LastReplicatedRotation = GetActorQuat();
}

void AWalkableShip::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	BroadcastRotationChange();
}

void AWalkableShip::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupAllPlayers();
	Super::EndPlay(EndPlayReason);
}

void AWalkableShip::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWalkableShip, CurrentPilot);
	DOREPLIFETIME(AWalkableShip, PlayersAboard);
}

void AWalkableShip::RegisterPlayer(AGalacticPiratesCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}

	if (!PlayersAboard.Contains(Character))
	{
		PlayersAboard.Add(Character);
	}
}

void AWalkableShip::UnregisterPlayer(AGalacticPiratesCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}

	if (CurrentPilot == Character)
	{
		ReleasePilot(Character);
	}

	PlayersAboard.Remove(Character);
}

bool AWalkableShip::RequestPilotAssignment(AGalacticPiratesCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return false;
	}

	if (CurrentPilot != nullptr)
	{
		return false;
	}

	if (!PlayersAboard.Contains(Character))
	{
		return false;
	}

	AGalacticPiratesCharacter* OldPilot = CurrentPilot;
	CurrentPilot = Character;
	OnRep_CurrentPilot(OldPilot);

	return true;
}

void AWalkableShip::ReleasePilot(AGalacticPiratesCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}

	if (CurrentPilot != Character)
	{
		return;
	}

	AGalacticPiratesCharacter* OldPilot = CurrentPilot;
	CurrentPilot = nullptr;
	OnRep_CurrentPilot(OldPilot);
}

void AWalkableShip::OnRep_CurrentPilot(AGalacticPiratesCharacter* OldPilot)
{
	if (OldPilot && OldPilot != CurrentPilot)
	{
		OldPilot->SetPiloting(false);
	}

	if (CurrentPilot)
	{
		CurrentPilot->SetPiloting(true);
	}

	OnPilotChanged.Broadcast(CurrentPilot, OldPilot);

	if (Helm)
	{
		Helm->OnPilotChanged(CurrentPilot, OldPilot);
	}
}

void AWalkableShip::ApplyPilotInput(AGalacticPiratesCharacter* Pilot, const FVector& ThrustInput, const FVector& RotationInput)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Pilot != CurrentPilot)
	{
		return;
	}

	if (ShipMovement)
	{
		ShipMovement->SetThrustInput(ThrustInput);
		ShipMovement->SetRotationInput(RotationInput);
	}
}

void AWalkableShip::HandlePlayerDisconnected(AGalacticPiratesCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}

	UnregisterPlayer(Character);
}

FTransform AWalkableShip::GetSpawnTransform() const
{
	if (SpawnPoint)
	{
		return SpawnPoint->GetComponentTransform();
	}
	return GetActorTransform();
}

void AWalkableShip::BroadcastRotationChange()
{
	FQuat CurrentRotation = GetActorQuat();
	
	if (!CurrentRotation.Equals(LastReplicatedRotation, 0.001f))
	{
		LastReplicatedRotation = CurrentRotation;
		OnShipRotationChanged.Broadcast(CurrentRotation);
	}
}

void AWalkableShip::CleanupAllPlayers()
{
	if (!HasAuthority())
	{
		return;
	}

	if (CurrentPilot)
	{
		ReleasePilot(CurrentPilot);
	}

	for (AGalacticPiratesCharacter* Character : PlayersAboard)
	{
		if (Character)
		{
			Character->OnShipDestroyed();
		}
	}

	PlayersAboard.Empty();
}
