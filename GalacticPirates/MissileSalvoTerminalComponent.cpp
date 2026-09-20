#include "MissileSalvoTerminalComponent.h"
#include "WalkableShip.h"
#include "ShipMissileSalvoComponent.h"
#include "GalacticPiratesCharacter.h"
#include "GalacticPirates.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

UMissileSalvoTerminalComponent::UMissileSalvoTerminalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	TerminalMesh = nullptr;
}

void UMissileSalvoTerminalComponent::BeginPlay()
{
	Super::BeginPlay();
	OwningShip = Cast<AWalkableShip>(GetOwner());

	if (!TerminalMesh && GetOwner())
	{
		TerminalMesh = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("MissileSalvoTerminalMesh"));
		TerminalMesh->SetupAttachment(this);
		TerminalMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		TerminalMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		TerminalMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		TerminalMesh->SetRelativeScale3D(FVector(0.55f, 0.7f, 1.25f));
		if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			TerminalMesh->SetStaticMesh(CubeMesh);
		}
		TerminalMesh->RegisterComponent();
	}
}

bool UMissileSalvoTerminalComponent::IsCharacterInRange(const AGalacticPiratesCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	return FVector::Dist(Character->GetActorLocation(), GetComponentLocation()) <= InteractRange;
}

bool UMissileSalvoTerminalComponent::TryInteract(AGalacticPiratesCharacter* Character)
{
	OwningShip = Cast<AWalkableShip>(GetOwner());
	if (!OwningShip || !Character || !OwningShip->HasAuthority())
	{
		return false;
	}

	if (OwningShip->IsWrecked())
	{
		return false;
	}

	if (Character->GetBoardedShip() != OwningShip)
	{
		return false;
	}

	if (!IsCharacterInRange(Character))
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[MissileTerminal] %s out of range"), *GetNameSafe(Character));
		return false;
	}

	if (!OwningShip->MissileSalvo)
	{
		return false;
	}

	const bool bFired = OwningShip->MissileSalvo->Fire(Character);
	UE_LOG(LogGalacticPirates, Warning, TEXT("[MissileTerminal] Interact by %s fired=%s"),
		*GetNameSafe(Character),
		bFired ? TEXT("true") : TEXT("false"));
	return bFired;
}
