#include "WeaponTerminalComponent.h"
#include "WalkableShip.h"
#include "ShipPulseCannonComponent.h"
#include "GalacticPiratesCharacter.h"
#include "GalacticPirates.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

UWeaponTerminalComponent::UWeaponTerminalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	TerminalMesh = nullptr;
}

void UWeaponTerminalComponent::BeginPlay()
{
	Super::BeginPlay();
	OwningShip = Cast<AWalkableShip>(GetOwner());

	if (!TerminalMesh && GetOwner())
	{
		TerminalMesh = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("WeaponTerminalMesh"));
		TerminalMesh->SetupAttachment(this);
		TerminalMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		TerminalMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		TerminalMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		TerminalMesh->SetRelativeScale3D(FVector(0.6f, 0.8f, 1.1f));
		if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			TerminalMesh->SetStaticMesh(CubeMesh);
		}
		TerminalMesh->RegisterComponent();
	}
}

bool UWeaponTerminalComponent::IsCharacterInRange(const AGalacticPiratesCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}

	return FVector::Dist(Character->GetActorLocation(), GetComponentLocation()) <= InteractRange;
}

bool UWeaponTerminalComponent::TryInteract(AGalacticPiratesCharacter* Character)
{
	OwningShip = Cast<AWalkableShip>(GetOwner());
	if (!OwningShip || !Character || !OwningShip->HasAuthority())
	{
		return false;
	}

	if (OwningShip->IsWrecked())
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[WeaponTerminal] Ship is wrecked"));
		return false;
	}

	if (Character->GetBoardedShip() != OwningShip)
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[WeaponTerminal] %s is not aboard"), *GetNameSafe(Character));
		return false;
	}

	if (!IsCharacterInRange(Character))
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[WeaponTerminal] %s out of range dist=%.1f"),
			*GetNameSafe(Character),
			FVector::Dist(Character->GetActorLocation(), GetComponentLocation()));
		return false;
	}

	if (!OwningShip->PulseCannon)
	{
		return false;
	}

	const bool bFired = OwningShip->PulseCannon->Fire(Character);
	UE_LOG(LogGalacticPirates, Warning, TEXT("[WeaponTerminal] Interact by %s fired=%s"),
		*GetNameSafe(Character),
		bFired ? TEXT("true") : TEXT("false"));
	return bFired;
}
