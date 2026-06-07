#include "HelmComponent.h"
#include "WalkableShip.h"
#include "GalacticPiratesCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"

UHelmComponent::UHelmComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHelmComponent::BeginPlay()
{
	Super::BeginPlay();
	OwningShip = Cast<AWalkableShip>(GetOwner());
}

void UHelmComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsOccupied && bLockPilotPosition && CurrentPilotRef && OwningShip)
	{
		FVector TargetLocation = GetComponentLocation() + GetComponentQuat().RotateVector(PilotRelativeLocation);
		FQuat TargetRotation = GetComponentQuat() * PilotRelativeRotation.Quaternion();
		
		CurrentPilotRef->SetActorLocationAndRotation(TargetLocation, TargetRotation);
	}
}

bool UHelmComponent::TryInteract(AGalacticPiratesCharacter* Character)
{
	if (!OwningShip || !Character)
	{
		return false;
	}

	if (bIsOccupied)
	{
		if (OwningShip->IsPilot(Character))
		{
			OwningShip->ReleasePilot(Character);
			return true;
		}
		return false;
	}

	return OwningShip->RequestPilotAssignment(Character);
}

void UHelmComponent::ForceRelease()
{
	if (!OwningShip || !bIsOccupied)
	{
		return;
	}

	AGalacticPiratesCharacter* CurrentPilot = OwningShip->GetCurrentPilot();
	if (CurrentPilot)
	{
		OwningShip->ReleasePilot(CurrentPilot);
	}
}

AGalacticPiratesCharacter* UHelmComponent::GetCurrentPilot() const
{
	if (OwningShip)
	{
		return OwningShip->GetCurrentPilot();
	}
	return nullptr;
}

void UHelmComponent::OnPilotChanged(AGalacticPiratesCharacter* NewPilot, AGalacticPiratesCharacter* OldPilot)
{
	if (OldPilot)
	{
		RemoveInputContextFromPilot(OldPilot);
		UnlockPilotFromHelm(OldPilot);
	}

	CurrentPilotRef = NewPilot;
	bIsOccupied = (NewPilot != nullptr);

	if (NewPilot)
	{
		AddInputContextToPilot(NewPilot);
		LockPilotToHelm(NewPilot);
	}

	SetComponentTickEnabled(bIsOccupied && bLockPilotPosition);

	OnHelmOccupancyChanged.Broadcast(bIsOccupied);
}

void UHelmComponent::HandlePilotDisconnected(AGalacticPiratesCharacter* DisconnectedPilot)
{
	if (CurrentPilotRef == DisconnectedPilot)
	{
		CurrentPilotRef = nullptr;
		bIsOccupied = false;
		SetComponentTickEnabled(false);
		OnHelmOccupancyChanged.Broadcast(false);
	}
}

void UHelmComponent::AddInputContextToPilot(AGalacticPiratesCharacter* Pilot)
{
	if (!Pilot || !ShipControlIMC)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pilot->GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (Subsystem)
	{
		Subsystem->AddMappingContext(ShipControlIMC, IMCPriority);
	}
}

void UHelmComponent::RemoveInputContextFromPilot(AGalacticPiratesCharacter* Pilot)
{
	if (!Pilot || !ShipControlIMC)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pilot->GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (Subsystem)
	{
		Subsystem->RemoveMappingContext(ShipControlIMC);
	}
}

void UHelmComponent::LockPilotToHelm(AGalacticPiratesCharacter* Pilot)
{
	if (!Pilot || !bLockPilotPosition)
	{
		return;
	}

	UCharacterMovementComponent* Movement = Pilot->GetCharacterMovement();
	if (Movement)
	{
		Movement->DisableMovement();
	}

	FVector TargetLocation = GetComponentLocation() + GetComponentQuat().RotateVector(PilotRelativeLocation);
	FQuat TargetRotation = GetComponentQuat() * PilotRelativeRotation.Quaternion();
	Pilot->SetActorLocationAndRotation(TargetLocation, TargetRotation);
}

void UHelmComponent::UnlockPilotFromHelm(AGalacticPiratesCharacter* Pilot)
{
	if (!Pilot || !bLockPilotPosition)
	{
		return;
	}

	UCharacterMovementComponent* Movement = Pilot->GetCharacterMovement();
	if (Movement)
	{
		Movement->SetMovementMode(MOVE_Walking);
	}
}
