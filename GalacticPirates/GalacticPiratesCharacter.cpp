// Copyright Epic Games, Inc. All Rights Reserved.

#include "GalacticPiratesCharacter.h"
#include "Animation/AnimInstance.h"
#include "QuatCamera.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/MovementBaseInterface.h"
#include "WalkableShip.h"
#include "ShipMovementComponent.h"
#include "HelmComponent.h"
#include "WeaponTerminalComponent.h"
#include "MissileSalvoTerminalComponent.h"
#include "MinigunPodComponent.h"
#include "ShipPulseCannonComponent.h"
#include "ShipMissileSalvoComponent.h"
#include "Net/UnrealNetwork.h"
#include "GalacticPirates.h"
#include "ShipDebug.h"
#include "ShipPolish.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/PlayerInput.h"
#include "Kismet/GameplayStatics.h"

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

	ShipThrustAction = CreateDefaultSubobject<UInputAction>(TEXT("Default Ship Thrust Action"));
	ShipThrustAction->ValueType = EInputActionValueType::Axis2D;
	ShipVerticalAction = CreateDefaultSubobject<UInputAction>(TEXT("Default Ship Vertical Action"));
	ShipVerticalAction->ValueType = EInputActionValueType::Axis1D;
	ShipRotationAction = CreateDefaultSubobject<UInputAction>(TEXT("Default Ship Rotation Action"));
	ShipRotationAction->ValueType = EInputActionValueType::Axis2D;
	ShipRollAction = CreateDefaultSubobject<UInputAction>(TEXT("Default Ship Roll Action"));
	ShipRollAction->ValueType = EInputActionValueType::Axis1D;
	ShipInteractAction = CreateDefaultSubobject<UInputAction>(TEXT("Default Ship Interact Action"));
	ShipInteractAction->ValueType = EInputActionValueType::Boolean;
	MinigunFireAction = CreateDefaultSubobject<UInputAction>(TEXT("Default Minigun Fire Action"));
	MinigunFireAction->ValueType = EInputActionValueType::Boolean;

	bReplicates = true;
	bAlwaysRelevant = true;

	BoardedShip = nullptr;
	SpawnShip = nullptr;
	bIsPiloting = false;
	AccumulatedThrustInput = FVector::ZeroVector;
	AccumulatedRotationInput = FVector::ZeroVector;
	TimeSinceLastPilotInputSend = 0.0f;
	HelmMouseSteer = FVector2D::ZeroVector;
	bHelmMouseSteerThisFrame = false;
}

void AGalacticPiratesCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGalacticPiratesCharacter, BoardedShip);
	DOREPLIFETIME(AGalacticPiratesCharacter, bIsPiloting);
	DOREPLIFETIME(AGalacticPiratesCharacter, OccupiedMinigun);
}

void AGalacticPiratesCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && !BoardedShip)
	{
		AWalkableShip* ShipToBoard = SpawnShip;
		if (!ShipToBoard)
		{
			ShipToBoard = AWalkableShip::FindPersistentShip(GetWorld());
		}
		if (ShipToBoard)
		{
			BoardShip(ShipToBoard);
		}
	}

	GPShipDebugSnapshot(this, TEXT("BeginPlay"));
}

void AGalacticPiratesCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (IsLocallyControlled())
	{
		SetupShipAccessInputContext();
		GPShipDebugSnapshot(this, TEXT("ControllerChanged"));
		if (GPInteriorTestEnabled() && !GPIsInteriorWalkTestRunning())
		{
			GPStartInteriorWalkTest(this);
		}
		else if (GPShipPlaytestEnabled() && ShipPlaytestPhase < 0)
		{
			StartAutomatedShipPlaytest();
		}
	}
}

void AGalacticPiratesCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (BoardedShip && IsValid(BoardedShip))
	{
		if (IsLocallyControlled())
		{
			UpdateCameraUpDirection();
		}

		if (HasAuthority() || IsLocallyControlled())
		{
			MaintainUprightOrientation();
			TickBoardedWalkPhysics(DeltaTime);
		}
	}

	if (bIsPiloting && IsLocallyControlled())
	{
		if (!bHelmMouseSteerThisFrame)
		{
			HelmMouseSteer = FMath::Vector2DInterpConstantTo(HelmMouseSteer, FVector2D::ZeroVector, DeltaTime, 2.5f);
		}
		bHelmMouseSteerThisFrame = false;

		TimeSinceLastPilotInputSend += DeltaTime;
		const float SendInterval = 1.0f / PilotInputSendRate;
		if (TimeSinceLastPilotInputSend >= SendInterval)
		{
			SendAccumulatedPilotInput();
			TimeSinceLastPilotInputSend = 0.0f;
		}
	}

	GPTickInteriorWalkTest(this, DeltaTime);
	GPTickExtremeFlightTest(this, DeltaTime);
	GPTickHelmSteerTest(this, DeltaTime);
	GPTickHelmJitterTest(this, DeltaTime);
	GPTickShipJumpTest(this, DeltaTime);

	if (ShipPlaytestPhase >= 0)
	{
		TickAutomatedShipPlaytest(DeltaTime);
	}
	else if (GPShipDebugLevel() >= 2 && IsLocallyControlled())
	{
		static float TimeSinceSnapshot = 0.0f;
		TimeSinceSnapshot += DeltaTime;
		if (TimeSinceSnapshot >= 1.0f)
		{
			TimeSinceSnapshot = 0.0f;
			GPShipDebugSnapshot(this, TEXT("Tick"));
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
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AGalacticPiratesCharacter::MouseLookInput);

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
		if (MinigunFireAction)
		{
			EnhancedInputComponent->BindAction(MinigunFireAction, ETriggerEvent::Started, this, &AGalacticPiratesCharacter::MinigunFireInput);
			EnhancedInputComponent->BindAction(MinigunFireAction, ETriggerEvent::Completed, this, &AGalacticPiratesCharacter::MinigunFireInput);
		}

		SetupShipAccessInputContext();
	}
	else
	{
		UE_LOG(LogGalacticPirates, Error, TEXT("'%s' Failed to find an Enhanced Input Component!"), *GetNameSafe(this));
	}
}

void AGalacticPiratesCharacter::SetupShipAccessInputContext()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!Subsystem)
	{
		return;
	}

	if (ShipInteractAction)
	{
		if (!RuntimeShipAccessIMC)
		{
			RuntimeShipAccessIMC = NewObject<UInputMappingContext>(this, TEXT("RuntimeShipAccessIMC"));
			RuntimeShipAccessIMC->MapKey(ShipInteractAction, EKeys::F);
			RuntimeShipAccessIMC->MapKey(ShipInteractAction, EKeys::Gamepad_FaceButton_Left);
		}

		Subsystem->AddMappingContext(RuntimeShipAccessIMC, 2);
		GPShipDebugEvent(*FString::Printf(TEXT("Added RuntimeShipAccessIMC (F / Face Left) to %s"), *GetNameSafe(this)));
	}

	if (MinigunFireAction)
	{
		if (!RuntimeMinigunFireIMC)
		{
			RuntimeMinigunFireIMC = NewObject<UInputMappingContext>(this, TEXT("RuntimeMinigunFireIMC"));
			RuntimeMinigunFireIMC->MapKey(MinigunFireAction, EKeys::LeftMouseButton);
			RuntimeMinigunFireIMC->MapKey(MinigunFireAction, EKeys::Gamepad_RightTrigger);
		}
		Subsystem->AddMappingContext(RuntimeMinigunFireIMC, 2);
	}

	UInputMappingContext* DefaultIMC = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default"));
	UInputMappingContext* MouseIMC = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));
	const bool bHasWalkLook = (DefaultIMC && Subsystem->HasMappingContext(DefaultIMC))
		&& (MouseIMC && Subsystem->HasMappingContext(MouseIMC));

	if (!bHasWalkLook)
	{
		if (!RuntimeLocomotionIMC)
		{
			RuntimeLocomotionIMC = NewObject<UInputMappingContext>(this, TEXT("RuntimeLocomotionIMC"));
			MapRuntimeLocomotionKeys();
		}

		Subsystem->AddMappingContext(RuntimeLocomotionIMC, 0);
		GPShipDebugEvent(*FString::Printf(TEXT("Added RuntimeLocomotionIMC (WASD / mouse / jump) to %s"), *GetNameSafe(this)));
	}
}

void AGalacticPiratesCharacter::MapRuntimeLocomotionKeys()
{
	if (!RuntimeLocomotionIMC)
	{
		return;
	}

	auto MapNegated = [this](const UInputAction* Action, const FKey& Key)
	{
		if (!Action)
		{
			return;
		}
		FEnhancedActionKeyMapping& Mapping = RuntimeLocomotionIMC->MapKey(Action, Key);
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(RuntimeLocomotionIMC));
	};

	auto MapToYAxis = [this](const UInputAction* Action, const FKey& Key, bool bNegate)
	{
		if (!Action)
		{
			return;
		}
		FEnhancedActionKeyMapping& Mapping = RuntimeLocomotionIMC->MapKey(Action, Key);
		Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(RuntimeLocomotionIMC));
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(RuntimeLocomotionIMC));
		}
	};

	if (MoveAction)
	{
		MapToYAxis(MoveAction, EKeys::W, false);
		MapToYAxis(MoveAction, EKeys::S, true);
		MapNegated(MoveAction, EKeys::A);
		RuntimeLocomotionIMC->MapKey(MoveAction, EKeys::D);

		FEnhancedActionKeyMapping& Stick = RuntimeLocomotionIMC->MapKey(MoveAction, EKeys::Gamepad_Left2D);
		Stick.Modifiers.Add(NewObject<UInputModifierDeadZone>(RuntimeLocomotionIMC));
	}

	if (LookAction)
	{
		FEnhancedActionKeyMapping& RightStick = RuntimeLocomotionIMC->MapKey(LookAction, EKeys::Gamepad_Right2D);
		RightStick.Modifiers.Add(NewObject<UInputModifierDeadZone>(RuntimeLocomotionIMC));
		UInputModifierNegate* NegatePitch = NewObject<UInputModifierNegate>(RuntimeLocomotionIMC);
		NegatePitch->bX = false;
		NegatePitch->bY = true;
		NegatePitch->bZ = false;
		RightStick.Modifiers.Add(NegatePitch);
	}

	if (MouseLookAction)
	{
		RuntimeLocomotionIMC->MapKey(MouseLookAction, EKeys::Mouse2D);
	}

	if (JumpAction)
	{
		RuntimeLocomotionIMC->MapKey(JumpAction, EKeys::SpaceBar);
		RuntimeLocomotionIMC->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Bottom);
	}
}

void AGalacticPiratesCharacter::BoardShip(AWalkableShip* Ship)
{
	if (!HasAuthority() || !Ship || Ship->IsWrecked())
	{
		return;
	}

	if (BoardedShip)
	{
		LeaveShip();
	}

	BoardedShip = Ship;
	Ship->RegisterPlayer(this);

	const int32 Slot = FMath::Max(0, Ship->GetPlayersAboard().Num() - 1);
	FTransform SpawnTransform = Ship->GetSpawnTransformForSlot(Slot);
	SetActorLocationAndRotation(SpawnTransform.GetLocation(), SpawnTransform.GetRotation());

	OnRep_BoardedShip();
	GPShipDebugSnapshot(this, TEXT("BoardShip"));

	if (IsLocallyControlled() && GPInteriorTestEnabled() && !GPIsInteriorWalkTestRunning())
	{
		GPStartInteriorWalkTest(this);
	}
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
	OnRep_BoardedShip();
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
			GPShipDebugEvent(*FString::Printf(TEXT("%s SetPiloting=%s"), *GetNameSafe(this), bNewPiloting ? TEXT("true") : TEXT("false")));
			GPShipDebugSnapshot(this, bNewPiloting ? TEXT("EnterHelm") : TEXT("ExitHelm"));
		}
	}
}

void AGalacticPiratesCharacter::OnShipDestroyed()
{
	if (!HasAuthority())
	{
		return;
	}

	if (BoardedShip)
	{
		BoardedShip->OnShipRotationChanged.RemoveDynamic(this, &AGalacticPiratesCharacter::OnShipRotationChanged);
	}

	BoardedShip = nullptr;
	bIsPiloting = false;
	OccupiedMinigun = nullptr;
	ClearMovementBase();
	OnRep_BoardedShip();
	OnRep_IsPiloting();
	OnRep_OccupiedMinigun();
}

void AGalacticPiratesCharacter::OnRep_BoardedShip()
{
	if (BoardedShip)
	{
		BoardedShip->OnShipRotationChanged.RemoveDynamic(this, &AGalacticPiratesCharacter::OnShipRotationChanged);
		BoardedShip->OnShipRotationChanged.AddDynamic(this, &AGalacticPiratesCharacter::OnShipRotationChanged);
		SetupMovementBaseOnShip();

		if (QuatCameraComponent && IsLocallyControlled())
		{
			QuatCameraComponent->SetReferenceOrientation(BoardedShip->GetActorQuat(), true);
		}

		if (!HasAuthority())
		{
			UE_LOG(LogGalacticPirates, Warning, TEXT("[DedicatedNet] Client boarded %s loc=%s"),
				*GetNameSafe(BoardedShip),
				*GetActorLocation().ToCompactString());
		}
	}
	else
	{
		ClearMovementBase();
		if (QuatCameraComponent && IsLocallyControlled())
		{
			QuatCameraComponent->SetReferenceOrientation(FQuat::Identity, true);
		}
	}
}

void AGalacticPiratesCharacter::OnRep_IsPiloting()
{
	AccumulatedThrustInput = FVector::ZeroVector;
	AccumulatedRotationInput = FVector::ZeroVector;
	TimeSinceLastPilotInputSend = 0.0f;
	HelmMouseSteer = FVector2D::ZeroVector;
	bHelmMouseSteerThisFrame = false;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetComponentTickEnabled(!bIsPiloting && OccupiedMinigun == nullptr);
		if (bIsPiloting)
		{
			MoveComp->StopMovementImmediately();
			MoveComp->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));
		}
	}

	if (bIsPiloting)
	{
		if (QuatCameraComponent && IsLocallyControlled())
		{
			QuatCameraComponent->ResetOrientation();
		}
	}
	else if (!OccupiedMinigun)
	{
		SetupMovementBaseOnShip();
	}

	if (!HasAuthority())
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[DedicatedNet] Client piloting=%s"),
			bIsPiloting ? TEXT("true") : TEXT("false"));
	}
}

void AGalacticPiratesCharacter::SetManningMinigun(UMinigunPodComponent* Pod)
{
	if (!HasAuthority())
	{
		return;
	}

	OccupiedMinigun = Pod;
	OnRep_OccupiedMinigun();
}

void AGalacticPiratesCharacter::OnRep_OccupiedMinigun()
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetComponentTickEnabled(!bIsPiloting && OccupiedMinigun == nullptr);
		if (OccupiedMinigun)
		{
			MoveComp->StopMovementImmediately();
			MoveComp->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));
		}
	}

	if (!OccupiedMinigun && !bIsPiloting)
	{
		SetupMovementBaseOnShip();
	}
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

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	MoveComp->SetGravityDirection(-BoardedShip->GetShipUpVector());
	MoveComp->bAlwaysCheckFloor = true;
	MoveComp->bIgnoreBaseRotation = false;
	MoveComp->bCanWalkOffLedges = false;
	MoveComp->bStayBasedInAir = true;
	MoveComp->StayBasedInAirHeight = 1000.0f;

	if (BoardedShip->ShipMovement)
	{
		PrimaryActorTick.AddPrerequisite(BoardedShip->ShipMovement, BoardedShip->ShipMovement->PrimaryComponentTick);
		MoveComp->PrimaryComponentTick.AddPrerequisite(BoardedShip->ShipMovement, BoardedShip->ShipMovement->PrimaryComponentTick);
	}

	UPrimitiveComponent* InteriorMesh = BoardedShip->GetInteriorMesh();
	if (InteriorMesh)
	{
		FMovementBaseInterfaceData MovementBaseData;
		MovementBaseData.Set(InteriorMesh);
		MoveComp->SetBase(&MovementBaseData);
		MoveComp->SetMovementMode(MOVE_Walking);
		if (HasAuthority() || IsLocallyControlled())
		{
			MoveComp->Velocity = FVector::ZeroVector;
		}
		GPShipDebugEvent(*FString::Printf(TEXT("Set movement base to %s, mode=Walking gravity=%s"),
			*GetNameSafe(InteriorMesh),
			*MoveComp->GetGravityDirection().ToCompactString()));
	}
	else
	{
		GPShipDebugEvent(TEXT("SetupMovementBaseOnShip failed: InteriorMesh is null"));
	}
}

void AGalacticPiratesCharacter::ClearMovementBase()
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		if (BoardedShip && BoardedShip->ShipMovement)
		{
			PrimaryActorTick.RemovePrerequisite(BoardedShip->ShipMovement, BoardedShip->ShipMovement->PrimaryComponentTick);
			MoveComp->PrimaryComponentTick.RemovePrerequisite(BoardedShip->ShipMovement, BoardedShip->ShipMovement->PrimaryComponentTick);
		}

		MoveComp->SetGravityDirection(UCharacterMovementComponent::DefaultGravityDirection);
		MoveComp->bCanWalkOffLedges = true;
		MoveComp->bStayBasedInAir = false;
		MoveComp->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));
		MoveComp->SetMovementMode(MOVE_Falling);
	}
}

void AGalacticPiratesCharacter::RestoreWalkingOnShip()
{
	if (!BoardedShip)
	{
		return;
	}

	const FTransform ShipTM = BoardedShip->GetActorTransform();
	FVector WorldLoc = GetActorLocation();
	FQuat WorldRot = GetActorQuat();

	if (HasAuthority() && !BoardedShip->IsWalkableWorldLocation(WorldLoc))
	{
		if (bHasLastGoodShipRelative)
		{
			WorldLoc = ShipTM.TransformPosition(LastGoodShipRelative.GetLocation());
			WorldRot = ShipTM.TransformRotation(LastGoodShipRelative.GetRotation());
		}
		else
		{
			const FTransform Spawn = BoardedShip->GetSpawnTransform();
			WorldLoc = Spawn.GetLocation();
			WorldRot = Spawn.GetRotation();
		}

		SetActorLocationAndRotation(WorldLoc, WorldRot, false, nullptr, ETeleportType::TeleportPhysics);
		GPShipDebugEvent(*FString::Printf(TEXT("RestoreWalkingOnShip snapped to %s"), *WorldLoc.ToCompactString()));
	}

	SetupMovementBaseOnShip();
	TimeOffShipDeck = 0.0f;
}

void AGalacticPiratesCharacter::TickBoardedWalkPhysics(float DeltaTime)
{
	if (!BoardedShip || OccupiedMinigun)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	MoveComp->SetGravityDirection(-BoardedShip->GetShipUpVector());
	MoveComp->bAlwaysCheckFloor = true;
	MoveComp->bIgnoreBaseRotation = false;
	MoveComp->bCanWalkOffLedges = false;
	MoveComp->bStayBasedInAir = true;
	MoveComp->StayBasedInAirHeight = 1000.0f;

	const bool bOnShipFloor = MoveComp->IsMovingOnGround() && BoardedShip->IsWalkableWorldLocation(GetActorLocation());
	if (bOnShipFloor)
	{
		const FTransform ShipTM = BoardedShip->GetActorTransform();
		LastGoodShipRelative = FTransform(
			ShipTM.InverseTransformRotation(GetActorQuat()),
			ShipTM.InverseTransformPosition(GetActorLocation()));
		bHasLastGoodShipRelative = true;
		TimeOffShipDeck = 0.0f;
		return;
	}

	if (bIsPiloting || OccupiedMinigun)
	{
		return;
	}

	TimeOffShipDeck += DeltaTime;
	if (!HasAuthority())
	{
		return;
	}

	const bool bJumping = MoveComp->IsFalling();
	const float RecoverAfter = bJumping ? 1.25f : 0.12f;
	const bool bHasDeck = BoardedShip->HasDeckBelow(GetActorLocation(), bJumping ? 800.0f : 400.0f);
	if (TimeOffShipDeck < RecoverAfter || bHasDeck)
	{
		return;
	}

	GPShipDebugEvent(*FString::Printf(
		TEXT("Off-deck recovery after %.2fs mode=%d loc=%s"),
		TimeOffShipDeck,
		static_cast<int32>(MoveComp->MovementMode),
		*GetActorLocation().ToCompactString()));
	RestoreWalkingOnShip();
}

void AGalacticPiratesCharacter::UpdateCameraUpDirection()
{
	if (!QuatCameraComponent)
	{
		return;
	}

	if (BoardedShip)
	{
		QuatCameraComponent->SetReferenceOrientation(BoardedShip->GetActorQuat(), bIsPiloting || OccupiedMinigun != nullptr);
	}
	else
	{
		QuatCameraComponent->SetReferenceOrientation(FQuat::Identity);
	}
}

void AGalacticPiratesCharacter::MaintainUprightOrientation()
{
	if (!BoardedShip || bIsPiloting || OccupiedMinigun)
	{
		return;
	}

	const FVector ShipUp = BoardedShip->GetShipUpVector();
	FVector DesiredForward = FVector::ZeroVector;

	if (QuatCameraComponent)
	{
		DesiredForward = FVector::VectorPlaneProject(QuatCameraComponent->GetPlanarLookForward(), ShipUp);
	}

	if (DesiredForward.IsNearlyZero())
	{
		DesiredForward = FVector::VectorPlaneProject(GetActorForwardVector(), ShipUp);
	}
	if (DesiredForward.IsNearlyZero())
	{
		DesiredForward = FVector::VectorPlaneProject(GetActorRightVector(), ShipUp);
	}

	DesiredForward.Normalize();
	SetActorRotation(FRotationMatrix::MakeFromXZ(DesiredForward, ShipUp).ToQuat());
}

void AGalacticPiratesCharacter::MoveInput(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	DebugMoveInputCount++;
	DebugLastMoveInput = MovementVector;
	if (GPShipDebugLevel() >= 3)
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDebug][MoveInput] %s count=%d"), *MovementVector.ToString(), DebugMoveInputCount);
	}
	DoMove(MovementVector.X, MovementVector.Y);
}

void AGalacticPiratesCharacter::LookInput(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	DebugLookInputCount++;
	DebugLastLookInput = LookAxisVector;
	if (GPShipDebugLevel() >= 3)
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDebug][LookAction] %s count=%d"), *LookAxisVector.ToString(), DebugLookInputCount);
	}
	DoAim(LookAxisVector.X, LookAxisVector.Y);
}

void AGalacticPiratesCharacter::MouseLookInput(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	DebugLookInputCount++;
	DebugLastLookInput = LookAxisVector;
	if (bIsPiloting)
	{
		ApplyHelmMouseSteer(LookAxisVector);
		return;
	}

	DoAim(LookAxisVector.X, LookAxisVector.Y);
}

void AGalacticPiratesCharacter::ApplyHelmMouseSteer(const FVector2D& MouseDelta)
{
	// Map mouse deltas onto the same -1..1 stick the gamepad uses, then keep
	// that deflection until the mouse stops so steering does not pulse on/off.
	const FVector2D Target(
		FMath::Clamp(MouseDelta.X * 8.0f, -1.0f, 1.0f),
		FMath::Clamp(MouseDelta.Y * 8.0f, -1.0f, 1.0f));
	HelmMouseSteer = FMath::Lerp(HelmMouseSteer, Target, 0.35f);
	bHelmMouseSteerThisFrame = true;
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
	GPShipDebugEvent(*FString::Printf(TEXT("ShipInteractInput fired on %s piloting=%s minigun=%s"),
		*GetNameSafe(this), bIsPiloting ? TEXT("true") : TEXT("false"), OccupiedMinigun ? TEXT("true") : TEXT("false")));

	if (IsLocallyControlled() && BoardedShip && !BoardedShip->IsWrecked())
	{
		if (bIsPiloting || OccupiedMinigun)
		{
			GPPlayPolishSound2D(this, TEXT("SFX_Helm"), 0.7f);
		}
		else if (BoardedShip->WeaponTerminal && BoardedShip->WeaponTerminal->IsCharacterInRange(this))
		{
			if (BoardedShip->PulseCannon && !BoardedShip->PulseCannon->CanFire())
			{
				GPPlayPolishSound2D(this, TEXT("SFX_Denied"), 0.85f);
			}
			else
			{
				GPPlayPolishSound2D(this, TEXT("SFX_Terminal"), 0.8f);
			}
		}
		else if (BoardedShip->MissileTerminal && BoardedShip->MissileTerminal->IsCharacterInRange(this))
		{
			if (BoardedShip->MissileSalvo && !BoardedShip->MissileSalvo->CanFire())
			{
				GPPlayPolishSound2D(this, TEXT("SFX_Denied"), 0.85f);
			}
			else
			{
				GPPlayPolishSound2D(this, TEXT("SFX_Terminal"), 0.8f);
			}
		}
		else if ((BoardedShip->PortMinigun && BoardedShip->PortMinigun->IsCharacterInRange(this))
			|| (BoardedShip->StarboardMinigun && BoardedShip->StarboardMinigun->IsCharacterInRange(this)))
		{
			GPPlayPolishSound2D(this, TEXT("SFX_Helm"), 0.65f);
		}
		else if (BoardedShip->Helm
			&& FVector::Dist(GetActorLocation(), BoardedShip->Helm->GetComponentLocation()) <= BoardedShip->Helm->InteractRange)
		{
			GPPlayPolishSound2D(this, TEXT("SFX_Helm"), 0.7f);
		}
	}

	Server_RequestHelmInteraction();
}

void AGalacticPiratesCharacter::DoAim(float Yaw, float Pitch)
{
	if (bIsPiloting)
	{
		return;
	}

	if (OccupiedMinigun)
	{
		OccupiedMinigun->AddAimInput(Yaw, Pitch);
		Server_SendMinigunAim(Yaw, Pitch);
		return;
	}

	if (QuatCameraComponent)
	{
		QuatCameraComponent->AddLookInput(Yaw, Pitch);
	}
}

void AGalacticPiratesCharacter::DoMove(float Right, float Forward)
{
	if (bIsPiloting || OccupiedMinigun || !GetController())
	{
		return;
	}

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

void AGalacticPiratesCharacter::DoJumpStart()
{
	if (bIsPiloting || OccupiedMinigun)
	{
		return;
	}

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

	FVector RotationToSend = AccumulatedRotationInput;
	const bool bUsingStickOrKeys = !FMath::IsNearlyZero(RotationToSend.Y, 0.01f) || !FMath::IsNearlyZero(RotationToSend.Z, 0.01f);
	if (!bUsingStickOrKeys)
	{
		RotationToSend.Y = HelmMouseSteer.Y;
		RotationToSend.Z = HelmMouseSteer.X;
	}

	if (!AccumulatedThrustInput.IsNearlyZero() || !RotationToSend.IsNearlyZero())
	{
		Server_SendPilotInput(AccumulatedThrustInput, RotationToSend);
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

	const bool bHandled = BoardedShip->TryStationInteract(this);
	GPShipDebugEvent(*FString::Printf(TEXT("Server station interact requested handled=%s piloting=%s minigun=%s"),
		bHandled ? TEXT("true") : TEXT("false"),
		bIsPiloting ? TEXT("true") : TEXT("false"),
		OccupiedMinigun ? TEXT("true") : TEXT("false")));
}

void AGalacticPiratesCharacter::MinigunFireInput(const FInputActionValue& Value)
{
	if (!OccupiedMinigun)
	{
		return;
	}

	const bool bFire = Value.Get<bool>();
	OccupiedMinigun->SetFiring(bFire);
	Server_SetMinigunFiring(bFire);
}

bool AGalacticPiratesCharacter::Server_SendMinigunAim_Validate(float YawDelta, float PitchDelta)
{
	return !FMath::IsNaN(YawDelta) && !FMath::IsNaN(PitchDelta)
		&& FMath::Abs(YawDelta) < 90.0f && FMath::Abs(PitchDelta) < 90.0f;
}

void AGalacticPiratesCharacter::Server_SendMinigunAim_Implementation(float YawDelta, float PitchDelta)
{
	if (OccupiedMinigun && OccupiedMinigun->GetGunner() == this)
	{
		OccupiedMinigun->AddAimInput(YawDelta, PitchDelta);
	}
}

bool AGalacticPiratesCharacter::Server_SetMinigunFiring_Validate(bool bNewFiring)
{
	return true;
}

void AGalacticPiratesCharacter::Server_SetMinigunFiring_Implementation(bool bNewFiring)
{
	if (OccupiedMinigun && OccupiedMinigun->GetGunner() == this)
	{
		OccupiedMinigun->SetFiring(bNewFiring);
	}
}

void AGalacticPiratesCharacter::DumpShipDebugSnapshot(const TCHAR* Reason) const
{
	GPShipDebugSnapshot(this, Reason);
}

void AGalacticPiratesCharacter::ApplyLookForTest(float Yaw, float Pitch)
{
	DoAim(Yaw, Pitch);
}

void AGalacticPiratesCharacter::ApplyJumpForTest()
{
	if (bIsPiloting || OccupiedMinigun)
	{
		return;
	}

	Jump();
}

void AGalacticPiratesCharacter::StartAutomatedShipPlaytest()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	ShipPlaytestPhase = 0;
	ShipPlaytestTime = 0.0f;
	ShipPlaytestStartLocation = GetActorLocation();
	ShipPlaytestStartYaw = QuatCameraComponent ? QuatCameraComponent->GetCurrentYaw() : 0.0f;
	ShipPlaytestShipStartRotation = BoardedShip ? BoardedShip->GetActorRotation() : FRotator::ZeroRotator;
	ShipPlaytestMoveCountAtMark = DebugMoveInputCount;
	ShipPlaytestLookCountAtMark = DebugLookInputCount;
	bShipPlaytestLookDirectPassed = false;
	bShipPlaytestMoveDirectPassed = false;
	bShipPlaytestWasdMappedPassed = false;
	bShipPlaytestLookMappedPassed = false;
	bShipPlaytestHelmFollowPassed = false;

	GPShipDebugScreen(TEXT("SHIP PLAYTEST STARTING"), FColor::Cyan, 4.0f);
	GPShipDebugSnapshot(this, TEXT("PlaytestStart"));
}

void AGalacticPiratesCharacter::InjectPlaytestKey(const FKey& Key, EInputEvent Event, float Delta)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	PC->InputKey(FInputKeyParams(Key, Event, static_cast<double>(Delta)));
}

void AGalacticPiratesCharacter::InjectPlaytestAxis(const FKey& Key, FVector Delta)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	PC->InputKey(FInputKeyParams(Key, Delta, 0.016f, 1));
}

void AGalacticPiratesCharacter::TickAutomatedShipPlaytest(float DeltaTime)
{
	ShipPlaytestTime += DeltaTime;
	const int32 PreviousPhase = ShipPlaytestPhase;

	if (ShipPlaytestTime < 0.4f)
	{
		ShipPlaytestPhase = 0;
	}
	else if (ShipPlaytestTime < 0.7f)
	{
		ShipPlaytestPhase = 1;
	}
	else if (ShipPlaytestTime < 1.2f)
	{
		ShipPlaytestPhase = 2;
	}
	else if (ShipPlaytestTime < 1.8f)
	{
		ShipPlaytestPhase = 3;
	}
	else if (ShipPlaytestTime < 2.3f)
	{
		ShipPlaytestPhase = 4;
	}
	else if (ShipPlaytestTime < 2.7f)
	{
		ShipPlaytestPhase = 5;
	}
	else if (ShipPlaytestTime < 3.2f)
	{
		ShipPlaytestPhase = 6;
	}
	else if (ShipPlaytestTime < 3.8f)
	{
		ShipPlaytestPhase = 7;
	}
	else
	{
		ShipPlaytestPhase = 8;
	}

	const bool bPhaseEntered = ShipPlaytestPhase != PreviousPhase;

	switch (ShipPlaytestPhase)
	{
	case 0:
		if (bPhaseEntered)
		{
			GPShipDebugSnapshot(this, TEXT("Playtest/IdleSnapshot"));
		}
		break;

	case 1:
		if (bPhaseEntered)
		{
			ShipPlaytestStartYaw = QuatCameraComponent ? QuatCameraComponent->GetCurrentYaw() : 0.0f;
			if (QuatCameraComponent)
			{
				QuatCameraComponent->AddLookInput(25.0f, 0.0f);
			}
			GPShipDebugEvent(TEXT("Playtest: injected direct AddLookInput +25 yaw"));
		}
		break;

	case 2:
		if (bPhaseEntered)
		{
			const float NewYaw = QuatCameraComponent ? QuatCameraComponent->GetCurrentYaw() : ShipPlaytestStartYaw;
			bShipPlaytestLookDirectPassed = !FMath::IsNearlyEqual(NewYaw, ShipPlaytestStartYaw, 1.0f);
			GPShipDebugScreen(
				FString::Printf(TEXT("Direct look %s (yaw %.1f -> %.1f)"),
					bShipPlaytestLookDirectPassed ? TEXT("PASS") : TEXT("FAIL"),
					ShipPlaytestStartYaw, NewYaw),
				bShipPlaytestLookDirectPassed ? FColor::Green : FColor::Red);

			ShipPlaytestStartLocation = GetActorLocation();
		}
		AddMovementInput(GetActorForwardVector(), 1.0f);
		break;

	case 3:
		if (bPhaseEntered)
		{
			const float Moved = FVector::Dist(GetActorLocation(), ShipPlaytestStartLocation);
			bShipPlaytestMoveDirectPassed = Moved > 5.0f;
			GPShipDebugScreen(
				FString::Printf(TEXT("Direct AddMovementInput %s (moved %.1f cm)"),
					bShipPlaytestMoveDirectPassed ? TEXT("PASS") : TEXT("FAIL"),
					Moved),
				bShipPlaytestMoveDirectPassed ? FColor::Green : FColor::Red);

			ShipPlaytestMoveCountAtMark = DebugMoveInputCount;
			InjectPlaytestKey(EKeys::W, IE_Pressed);
		}
		InjectPlaytestKey(EKeys::W, IE_Repeat);
		break;

	case 4:
		if (bPhaseEntered)
		{
			InjectPlaytestKey(EKeys::W, IE_Released);
			const int32 MoveDelta = DebugMoveInputCount - ShipPlaytestMoveCountAtMark;
			bShipPlaytestWasdMappedPassed = MoveDelta > 0;
			GPShipDebugScreen(
				FString::Printf(TEXT("WASD mapping %s (MoveInput +%d)"),
					bShipPlaytestWasdMappedPassed ? TEXT("PASS") : TEXT("FAIL"),
					MoveDelta),
				bShipPlaytestWasdMappedPassed ? FColor::Green : FColor::Red);

			ShipPlaytestLookCountAtMark = DebugLookInputCount;
		}
		InjectPlaytestAxis(EKeys::Mouse2D, FVector(40.0f, 0.0f, 0.0f));
		InjectPlaytestAxis(EKeys::MouseX, FVector(40.0f, 0.0f, 0.0f));
		break;

	case 5:
		if (bPhaseEntered)
		{
			const int32 LookDelta = DebugLookInputCount - ShipPlaytestLookCountAtMark;
			bShipPlaytestLookMappedPassed = LookDelta > 0;
			GPShipDebugScreen(
				FString::Printf(TEXT("Mouse look mapping %s (LookInput +%d)"),
					bShipPlaytestLookMappedPassed ? TEXT("PASS") : TEXT("FAIL"),
					LookDelta),
				bShipPlaytestLookMappedPassed ? FColor::Green : FColor::Red);

			GPShipDebugEvent(TEXT("Playtest: injecting F to take helm"));
			InjectPlaytestKey(EKeys::F, IE_Pressed);
			if (BoardedShip && BoardedShip->Helm && HasAuthority())
			{
				BoardedShip->Helm->TryInteract(this);
			}
		}
		break;

	case 6:
		if (bPhaseEntered)
		{
			GPShipDebugSnapshot(this, TEXT("Playtest/HelmBeforeYaw"));
			if (BoardedShip)
			{
				ShipPlaytestShipStartRotation = BoardedShip->GetActorRotation();
				FRotator Yawed = ShipPlaytestShipStartRotation;
				Yawed.Yaw += 45.0f;
				BoardedShip->SetActorRotation(Yawed);
				GPShipDebugEvent(TEXT("Playtest: yawed ship +45 degrees"));
			}
		}
		break;

	case 7:
		if (bPhaseEntered)
		{
			float CamVsShip = -1.0f;
			if (QuatCameraComponent && BoardedShip)
			{
				CamVsShip = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
					FVector::DotProduct(QuatCameraComponent->GetForwardVector().GetSafeNormal(), BoardedShip->GetActorForwardVector().GetSafeNormal()),
					-1.0f, 1.0f)));
			}
			bShipPlaytestHelmFollowPassed = CamVsShip >= 0.0f && CamVsShip < 20.0f;
			GPShipDebugScreen(
				FString::Printf(TEXT("Helm camera follows ship yaw %s (angle %.1f deg)"),
					bShipPlaytestHelmFollowPassed ? TEXT("PASS") : TEXT("FAIL"),
					CamVsShip),
				bShipPlaytestHelmFollowPassed ? FColor::Green : FColor::Red);
			GPShipDebugSnapshot(this, TEXT("Playtest/HelmAfterYaw"));

			InjectPlaytestKey(EKeys::Gamepad_FaceButton_Left, IE_Pressed);
			GPShipDebugEvent(TEXT("Playtest: injected Gamepad Face Left (expected interact miss)"));
		}
		break;

	case 8:
		if (bPhaseEntered)
		{
			if (BoardedShip)
			{
				BoardedShip->SetActorRotation(ShipPlaytestShipStartRotation);
				if (IsPiloting() && BoardedShip->Helm)
				{
					BoardedShip->Helm->TryInteract(this);
				}
			}

			const FString Summary = FString::Printf(
				TEXT("PLAYTEST SUMMARY lookDirect=%s moveDirect=%s wasdMap=%s mouseMap=%s helmFollow=%s interact=F/A not FaceLeft"),
				bShipPlaytestLookDirectPassed ? TEXT("PASS") : TEXT("FAIL"),
				bShipPlaytestMoveDirectPassed ? TEXT("PASS") : TEXT("FAIL"),
				bShipPlaytestWasdMappedPassed ? TEXT("PASS") : TEXT("FAIL"),
				bShipPlaytestLookMappedPassed ? TEXT("PASS") : TEXT("FAIL"),
				bShipPlaytestHelmFollowPassed ? TEXT("PASS") : TEXT("FAIL"));
			GPShipDebugScreen(Summary, FColor::Yellow, 20.0f);
			UE_LOG(LogGalacticPirates, Error, TEXT("[ShipDebug] %s"), *Summary);
			GPShipDebugSnapshot(this, TEXT("PlaytestComplete"));
			ShipPlaytestPhase = -2;
		}
		break;

	default:
		break;
	}
}
