// Copyright Epic Games, Inc. All Rights Reserved.

#include "GalacticPiratesCharacter.h"
#include "Animation/AnimInstance.h"
#include "QuatCamera.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "WalkableShip.h"
#include "HelmComponent.h"
#include "Net/UnrealNetwork.h"
#include "GalacticPirates.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

AGalacticPiratesCharacter::AGalacticPiratesCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	QuatCameraComponent = CreateDefaultSubobject<UQuatCamera>(TEXT("Quaternion Camera"));
	QuatCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	QuatCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	QuatCameraComponent->bEnableFirstPersonFieldOfView = true;
	QuatCameraComponent->bEnableFirstPersonScale = true;
	QuatCameraComponent->FirstPersonFieldOfView = 70.0f;
	QuatCameraComponent->FirstPersonScale = 0.6f;

	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	bReplicates = true;
	
	BoardedShip = nullptr;
	SpawnShip = nullptr;
	bIsPiloting = false;
	AccumulatedThrustInput = FVector::ZeroVector;
	AccumulatedRotationInput = FVector::ZeroVector;
	TimeSinceLastPilotInputSend = 0.0f;
}

void AGalacticPiratesCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGalacticPiratesCharacter, BoardedShip);
	DOREPLIFETIME(AGalacticPiratesCharacter, bIsPiloting);
}

void AGalacticPiratesCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && SpawnShip)
	{
		BoardShip(SpawnShip);
	}
}

void AGalacticPiratesCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (BoardedShip)
	{
		UpdateCameraUpDirection();
		MaintainUprightOrientation();
	}

	if (bIsPiloting && IsLocallyControlled())
	{
		TimeSinceLastPilotInputSend += DeltaTime;
		float SendInterval = 1.0f / PilotInputSendRate;
		
		if (TimeSinceLastPilotInputSend >= SendInterval)
		{
			SendAccumulatedPilotInput();
			TimeSinceLastPilotInputSend = 0.0f;
		}
	}
}

void AGalacticPiratesCharacter::Destroyed()
{
	if (HasAuthority() && BoardedShip)
	{
		BoardedShip->HandlePlayerDisconnected(this);
	}
	Super::Destroyed();
}

void AGalacticPiratesCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AGalacticPiratesCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AGalacticPiratesCharacter::DoJumpEnd);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGalacticPiratesCharacter::MoveInput);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGalacticPiratesCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AGalacticPiratesCharacter::LookInput);

		if (ShipThrustAction)
		{
			EnhancedInputComponent->BindAction(ShipThrustAction, ETriggerEvent::Triggered, this, &AGalacticPiratesCharacter::ShipThrustInput);
		}
		if (ShipVerticalAction)
		{
			EnhancedInputComponent->BindAction(ShipVerticalAction, ETriggerEvent::Triggered, this, &AGalacticPiratesCharacter::ShipVerticalInput);
		}
		if (ShipRotationAction)
		{
			EnhancedInputComponent->BindAction(ShipRotationAction, ETriggerEvent::Triggered, this, &AGalacticPiratesCharacter::ShipRotationInput);
		}
		if (ShipRollAction)
		{
			EnhancedInputComponent->BindAction(ShipRollAction, ETriggerEvent::Triggered, this, &AGalacticPiratesCharacter::ShipRollInput);
		}
		if (ShipInteractAction)
		{
			EnhancedInputComponent->BindAction(ShipInteractAction, ETriggerEvent::Started, this, &AGalacticPiratesCharacter::ShipInteractInput);
		}
	}
	else
	{
		UE_LOG(LogGalacticPirates, Error, TEXT("'%s' Failed to find an Enhanced Input Component!"), *GetNameSafe(this));
	}
}

void AGalacticPiratesCharacter::BoardShip(AWalkableShip* Ship)
{
	if (!HasAuthority() || !Ship)
	{
		return;
	}

	if (BoardedShip)
	{
		LeaveShip();
	}

	BoardedShip = Ship;
	Ship->RegisterPlayer(this);

	FTransform SpawnTransform = Ship->GetSpawnTransform();
	SetActorLocationAndRotation(SpawnTransform.GetLocation(), SpawnTransform.GetRotation());

	OnRep_BoardedShip();
}

void AGalacticPiratesCharacter::LeaveShip()
{
	if (!HasAuthority() || !BoardedShip)
	{
		return;
	}

	BoardedShip->OnShipRotationChanged.RemoveDynamic(this, &AGalacticPiratesCharacter::OnShipRotationChanged);
	BoardedShip->UnregisterPlayer(this);
	ClearMovementBase();
	BoardedShip = nullptr;
}

void AGalacticPiratesCharacter::SetPiloting(bool bNewPiloting)
{
	if (HasAuthority())
	{
		bool bOldPiloting = bIsPiloting;
		bIsPiloting = bNewPiloting;
		if (bOldPiloting != bNewPiloting)
		{
			OnRep_IsPiloting();
		}
	}
}

void AGalacticPiratesCharacter::OnShipDestroyed()
{
	if (BoardedShip)
	{
		BoardedShip->OnShipRotationChanged.RemoveDynamic(this, &AGalacticPiratesCharacter::OnShipRotationChanged);
	}
	
	BoardedShip = nullptr;
	bIsPiloting = false;
	ClearMovementBase();
}

void AGalacticPiratesCharacter::OnRep_BoardedShip()
{
	if (BoardedShip)
	{
		BoardedShip->OnShipRotationChanged.AddDynamic(this, &AGalacticPiratesCharacter::OnShipRotationChanged);
		SetupMovementBaseOnShip();
		
		if (QuatCameraComponent)
		{
			QuatCameraComponent->SetReferenceUpDirection(BoardedShip->GetShipUpVector(), true);
		}
	}
	else
	{
		ClearMovementBase();
		if (QuatCameraComponent)
		{
			QuatCameraComponent->SetReferenceUpDirection(FVector::UpVector, true);
		}
	}
}

void AGalacticPiratesCharacter::OnRep_IsPiloting()
{
	AccumulatedThrustInput = FVector::ZeroVector;
	AccumulatedRotationInput = FVector::ZeroVector;
	TimeSinceLastPilotInputSend = 0.0f;
}

void AGalacticPiratesCharacter::OnShipRotationChanged(const FQuat& NewRotation)
{
	UpdateCameraUpDirection();
}

void AGalacticPiratesCharacter::SetupMovementBaseOnShip()
{
	if (!BoardedShip)
	{
		return;
	}

	UPrimitiveComponent* InteriorMesh = BoardedShip->GetInteriorMesh();
	if (InteriorMesh)
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
}

void AGalacticPiratesCharacter::ClearMovementBase()
{
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
}

void AGalacticPiratesCharacter::UpdateCameraUpDirection()
{
	if (!BoardedShip || !QuatCameraComponent)
	{
		return;
	}

	FVector ShipUp = BoardedShip->GetShipUpVector();
	QuatCameraComponent->SetReferenceUpDirection(ShipUp);
}

void AGalacticPiratesCharacter::MaintainUprightOrientation()
{
	if (!BoardedShip)
	{
		return;
	}

	FVector ShipUp = BoardedShip->GetShipUpVector();
	FVector CurrentForward = GetActorForwardVector();
	
	FVector ProjectedForward = FVector::VectorPlaneProject(CurrentForward, ShipUp);
	if (ProjectedForward.IsNearlyZero())
	{
		ProjectedForward = FVector::VectorPlaneProject(GetActorRightVector(), ShipUp);
	}
	ProjectedForward.Normalize();

	FQuat TargetRotation = FRotationMatrix::MakeFromXZ(ProjectedForward, ShipUp).ToQuat();
	SetActorRotation(TargetRotation);
}

void AGalacticPiratesCharacter::MoveInput(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void AGalacticPiratesCharacter::LookInput(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoAim(LookAxisVector.X, LookAxisVector.Y);
}

void AGalacticPiratesCharacter::ShipThrustInput(const FInputActionValue& Value)
{
	if (!bIsPiloting)
	{
		return;
	}

	FVector2D ThrustVector = Value.Get<FVector2D>();
	AccumulatedThrustInput.X = ThrustVector.Y;
	AccumulatedThrustInput.Y = ThrustVector.X;
}

void AGalacticPiratesCharacter::ShipVerticalInput(const FInputActionValue& Value)
{
	if (!bIsPiloting)
	{
		return;
	}

	float VerticalValue = Value.Get<float>();
	AccumulatedThrustInput.Z = VerticalValue;
}

void AGalacticPiratesCharacter::ShipRotationInput(const FInputActionValue& Value)
{
	if (!bIsPiloting)
	{
		return;
	}

	FVector2D RotationVector = Value.Get<FVector2D>();
	AccumulatedRotationInput.Y = RotationVector.Y;
	AccumulatedRotationInput.Z = RotationVector.X;
}

void AGalacticPiratesCharacter::ShipRollInput(const FInputActionValue& Value)
{
	if (!bIsPiloting)
	{
		return;
	}

	float RollValue = Value.Get<float>();
	AccumulatedRotationInput.X = RollValue;
}

void AGalacticPiratesCharacter::ShipInteractInput(const FInputActionValue& Value)
{
	Server_RequestHelmInteraction();
}

void AGalacticPiratesCharacter::DoAim(float Yaw, float Pitch)
{
	if (QuatCameraComponent)
	{
		QuatCameraComponent->AddLookInput(Yaw, -Pitch);
	}
}

void AGalacticPiratesCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		FVector MoveDirection = FVector::ZeroVector;
		
		if (QuatCameraComponent)
		{
			FQuat CameraRotation = QuatCameraComponent->GetComponentQuat();
			FVector CameraForward = CameraRotation.GetForwardVector();
			FVector CameraRight = CameraRotation.GetRightVector();

			if (BoardedShip)
			{
				FVector ShipUp = BoardedShip->GetShipUpVector();
				CameraForward = FVector::VectorPlaneProject(CameraForward, ShipUp).GetSafeNormal();
				CameraRight = FVector::VectorPlaneProject(CameraRight, ShipUp).GetSafeNormal();
			}

			MoveDirection = CameraForward * Forward + CameraRight * Right;
		}
		else
		{
			MoveDirection = GetActorForwardVector() * Forward + GetActorRightVector() * Right;
		}

		AddMovementInput(MoveDirection, 1.0f);
	}
}

void AGalacticPiratesCharacter::DoJumpStart()
{
	Jump();
}

void AGalacticPiratesCharacter::DoJumpEnd()
{
	StopJumping();
}

void AGalacticPiratesCharacter::SendAccumulatedPilotInput()
{
	if (!bIsPiloting || !BoardedShip)
	{
		return;
	}

	if (!AccumulatedThrustInput.IsNearlyZero() || !AccumulatedRotationInput.IsNearlyZero())
	{
		Server_SendPilotInput(AccumulatedThrustInput, AccumulatedRotationInput);
	}

	AccumulatedThrustInput = FVector::ZeroVector;
	AccumulatedRotationInput = FVector::ZeroVector;
}

bool AGalacticPiratesCharacter::Server_SendPilotInput_Validate(FVector ThrustInput, FVector RotationInput)
{
	if (ThrustInput.ContainsNaN() || RotationInput.ContainsNaN())
	{
		return false;
	}

	if (ThrustInput.SizeSquared() > 10.0f || RotationInput.SizeSquared() > 10.0f)
	{
		return false;
	}

	return true;
}

void AGalacticPiratesCharacter::Server_SendPilotInput_Implementation(FVector ThrustInput, FVector RotationInput)
{
	if (!bIsPiloting || !BoardedShip)
	{
		return;
	}

	if (!BoardedShip->IsPilot(this))
	{
		return;
	}

	ThrustInput.X = FMath::Clamp(ThrustInput.X, -1.0f, 1.0f);
	ThrustInput.Y = FMath::Clamp(ThrustInput.Y, -1.0f, 1.0f);
	ThrustInput.Z = FMath::Clamp(ThrustInput.Z, -1.0f, 1.0f);
	RotationInput.X = FMath::Clamp(RotationInput.X, -1.0f, 1.0f);
	RotationInput.Y = FMath::Clamp(RotationInput.Y, -1.0f, 1.0f);
	RotationInput.Z = FMath::Clamp(RotationInput.Z, -1.0f, 1.0f);

	BoardedShip->ApplyPilotInput(this, ThrustInput, RotationInput);
}

bool AGalacticPiratesCharacter::Server_RequestHelmInteraction_Validate()
{
	return BoardedShip != nullptr;
}

void AGalacticPiratesCharacter::Server_RequestHelmInteraction_Implementation()
{
	if (!BoardedShip)
	{
		return;
	}

	UHelmComponent* Helm = BoardedShip->Helm;
	if (!Helm)
	{
		return;
	}

	Helm->TryInteract(this);
}
