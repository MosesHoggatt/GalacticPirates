#include "HelmComponent.h"
#include "WalkableShip.h"
#include "ShipMovementComponent.h"
#include "GalacticPiratesCharacter.h"
#include "GalacticPirates.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/EngineTypes.h"
#include "Interfaces/MovementBaseInterface.h"

UHelmComponent::UHelmComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHelmComponent::BeginPlay()
{
	Super::BeginPlay();
	OwningShip = Cast<AWalkableShip>(GetOwner());
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	if (OwningShip && OwningShip->ShipMovement)
	{
		PrimaryComponentTick.AddPrerequisite(OwningShip->ShipMovement, OwningShip->ShipMovement->PrimaryComponentTick);
	}
}

void UHelmComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!bIsOccupied || !CurrentPilotRef)
	{
		return;
	}

	USceneComponent* PilotRoot = CurrentPilotRef->GetRootComponent();
	USceneComponent* CurrentAttach = PilotRoot ? PilotRoot->GetAttachParent() : nullptr;
	USceneComponent* Interior = OwningShip ? OwningShip->GetInteriorMesh() : nullptr;
	if (CurrentAttach != this && CurrentAttach != Interior)
	{
		LockPilotToHelm(CurrentPilotRef);
	}

	if (bLockPilotPosition && PilotRoot && PilotRoot->GetAttachParent() == this)
	{
		CurrentPilotRef->SetActorRelativeLocation(PilotRelativeLocation);
		CurrentPilotRef->SetActorRelativeRotation(PilotRelativeRotation);
	}
}

bool UHelmComponent::TryInteract(AGalacticPiratesCharacter* Character)
{
	if (!OwningShip || !Character)
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDebug] Helm TryInteract failed: missing ship or character"));
		return false;
	}

	if (bIsOccupied)
	{
		if (OwningShip->IsPilot(Character))
		{
			UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDebug] Helm releasing pilot %s"), *GetNameSafe(Character));
			OwningShip->ReleasePilot(Character);
			return true;
		}
		UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDebug] Helm occupied by someone else"));
		return false;
	}

	UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDebug] Helm requesting pilot assignment for %s"), *GetNameSafe(Character));
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
	const bool bAuthority = GetOwner() && GetOwner()->HasAuthority();

	if (OldPilot)
	{
		RemoveInputContextFromPilot(OldPilot);
		if (bAuthority)
		{
			UnlockPilotFromHelm(OldPilot);
		}
	}

	CurrentPilotRef = NewPilot;
	bIsOccupied = (NewPilot != nullptr);

	if (NewPilot)
	{
		AddInputContextToPilot(NewPilot);
		if (bAuthority)
		{
			LockPilotToHelm(NewPilot);
		}
	}

	SetComponentTickEnabled(bIsOccupied && bAuthority);

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

UInputMappingContext* UHelmComponent::GetOrCreateShipControlContext(AGalacticPiratesCharacter* Pilot)
{
	if (RuntimeShipControlIMC)
	{
		return RuntimeShipControlIMC;
	}

	if (!Pilot || !Pilot->GetShipThrustAction() || !Pilot->GetShipVerticalAction()
		|| !Pilot->GetShipRotationAction() || !Pilot->GetShipRollAction())
	{
		return nullptr;
	}

	RuntimeShipControlIMC = NewObject<UInputMappingContext>(this, TEXT("RuntimeShipControlIMC"));

	auto MapNegated = [this](const UInputAction* Action, const FKey& Key)
	{
		FEnhancedActionKeyMapping& Mapping = RuntimeShipControlIMC->MapKey(Action, Key);
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(RuntimeShipControlIMC));
	};

	auto MapToYAxis = [this](const UInputAction* Action, const FKey& Key, bool bNegate)
	{
		FEnhancedActionKeyMapping& Mapping = RuntimeShipControlIMC->MapKey(Action, Key);
		Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(RuntimeShipControlIMC));
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(RuntimeShipControlIMC));
		}
	};

	auto MapStick = [this](const UInputAction* Action, const FKey& Key)
	{
		FEnhancedActionKeyMapping& Mapping = RuntimeShipControlIMC->MapKey(Action, Key);
		Mapping.Modifiers.Add(NewObject<UInputModifierDeadZone>(RuntimeShipControlIMC));
	};

	const UInputAction* ThrustAction = Pilot->GetShipThrustAction();
	MapToYAxis(ThrustAction, EKeys::W, false);
	MapToYAxis(ThrustAction, EKeys::S, true);
	MapNegated(ThrustAction, EKeys::A);
	RuntimeShipControlIMC->MapKey(ThrustAction, EKeys::D);
	MapStick(ThrustAction, EKeys::Gamepad_Left2D);

	const UInputAction* RotationAction = Pilot->GetShipRotationAction();
	MapToYAxis(RotationAction, EKeys::Up, false);
	MapToYAxis(RotationAction, EKeys::Down, true);
	MapNegated(RotationAction, EKeys::Left);
	RuntimeShipControlIMC->MapKey(RotationAction, EKeys::Right);
	MapStick(RotationAction, EKeys::Gamepad_Right2D);

	const UInputAction* VerticalAction = Pilot->GetShipVerticalAction();
	RuntimeShipControlIMC->MapKey(VerticalAction, EKeys::SpaceBar);
	MapNegated(VerticalAction, EKeys::LeftControl);
	RuntimeShipControlIMC->MapKey(VerticalAction, EKeys::Gamepad_RightTriggerAxis);
	MapNegated(VerticalAction, EKeys::Gamepad_LeftTriggerAxis);

	const UInputAction* RollAction = Pilot->GetShipRollAction();
	RuntimeShipControlIMC->MapKey(RollAction, EKeys::E);
	MapNegated(RollAction, EKeys::Q);
	RuntimeShipControlIMC->MapKey(RollAction, EKeys::Gamepad_RightShoulder);
	MapNegated(RollAction, EKeys::Gamepad_LeftShoulder);

	return RuntimeShipControlIMC;
}

void UHelmComponent::AddInputContextToPilot(AGalacticPiratesCharacter* Pilot)
{
	if (!Pilot)
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
		if (UInputMappingContext* ControlContext = GetOrCreateShipControlContext(Pilot))
		{
			Subsystem->AddMappingContext(ControlContext, IMCPriority);
			UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDebug] Added ship control IMC to %s priority=%d"), *GetNameSafe(Pilot), IMCPriority);
		}
	}
}

void UHelmComponent::RemoveInputContextFromPilot(AGalacticPiratesCharacter* Pilot)
{
	if (!Pilot)
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
		if (ShipControlIMC)
		{
			Subsystem->RemoveMappingContext(ShipControlIMC);
		}
		if (RuntimeShipControlIMC)
		{
			Subsystem->RemoveMappingContext(RuntimeShipControlIMC);
		}
	}
}

void UHelmComponent::LockPilotToHelm(AGalacticPiratesCharacter* Pilot)
{
	if (!Pilot)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = Pilot->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->Velocity = FVector::ZeroVector;
		Movement->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));
		Movement->SetComponentTickEnabled(false);
	}

	USceneComponent* HelmAttachTarget = this;
	if (!bLockPilotPosition && OwningShip && OwningShip->GetInteriorMesh())
	{
		HelmAttachTarget = OwningShip->GetInteriorMesh();
	}

	if (bLockPilotPosition)
	{
		const FVector TargetLocation = GetComponentLocation() + GetComponentQuat().RotateVector(PilotRelativeLocation);
		const FQuat TargetRotation = GetComponentQuat() * PilotRelativeRotation.Quaternion();
		Pilot->SetActorLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
		Pilot->AttachToComponent(HelmAttachTarget, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Pilot->SetActorRelativeLocation(PilotRelativeLocation);
		Pilot->SetActorRelativeRotation(PilotRelativeRotation);
	}
	else
	{
		Pilot->AttachToComponent(HelmAttachTarget, FAttachmentTransformRules::KeepWorldTransform);
	}
}

void UHelmComponent::UnlockPilotFromHelm(AGalacticPiratesCharacter* Pilot)
{
	if (!Pilot)
	{
		return;
	}

	Pilot->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	if (UCharacterMovementComponent* Movement = Pilot->GetCharacterMovement())
	{
		Movement->SetComponentTickEnabled(true);
	}

	Pilot->RestoreWalkingOnShip();
}
