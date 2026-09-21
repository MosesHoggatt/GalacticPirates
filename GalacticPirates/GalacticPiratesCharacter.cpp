// Copyright Epic Games, Inc. All Rights Reserved.

#include "GalacticPiratesCharacter.h"
#include "Animation/AnimInstance.h"
#include "QuatCamera.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputLibrary.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/MovementBaseInterface.h"
#include "ShipWreckDebris.h"
#include "WalkableShip.h"
#include "BulldogFighter.h"
#include "OccupancyComponent.h"
#include "CraftWreck.h"
#include "SpaceCraft.h"
#include "ShipMovementComponent.h"
#include "HelmComponent.h"
#include "WeaponTerminalComponent.h"
#include "MissileSalvoTerminalComponent.h"
#include "MinigunPodComponent.h"
#include "ShipPulseCannonComponent.h"
#include "ShipMissileSalvoComponent.h"
#include "CraftReplication.h"
#include "Net/UnrealNetwork.h"
#include "GalacticPirates.h"
#include "ShipDebug.h"
#include "ShipPolish.h"
#include "GalacticPiratesPlayerController.h"
#include "GalacticPiratesHUD.h"
#include "Blueprint/UserWidget.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
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

	GPCraftNet::Apply(this, GPCraftNet::Crew());

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
	DOREPLIFETIME(AGalacticPiratesCharacter, OccupiedVehicle);
	DOREPLIFETIME(AGalacticPiratesCharacter, bIsAiCrew);
	DOREPLIFETIME(AGalacticPiratesCharacter, bDead);
}

void AGalacticPiratesCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && !BoardedShip && !bDead)
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

	if (IsLocallyControlled() || HasAuthority())
	{
		EnsureNativeShipInputActions();
		BindNativeVehicleInput();
	}

	if (IsLocallyControlled())
	{
		SetupShipAccessInputContext();
		if (bIsPiloting)
		{
			ApplyVehicleControlMapping(true);
		}
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

	if (bDead)
	{
		if (IsLocallyControlled())
		{
			UpdateDeathCamera(DeltaTime);
		}
		return;
	}

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

	if (!bIsPiloting && (HasAuthority() || IsLocallyControlled()))
	{
		PollHeldWalkKeys();
	}

	if (bIsPiloting && (HasAuthority() || IsLocallyControlled()))
	{
		PollHeldVehicleKeys();
		if (!bHelmMouseSteerThisFrame && IsLocallyControlled())
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
	if (HasAuthority())
	{
		if (BoardedShip)
		{
			BoardedShip->HandlePlayerDisconnected(this);
		}
		if (UOccupancyComponent* Seat = GPFindPilotOccupancy(OccupiedVehicle))
		{
			Seat->ForceRelease();
		}
		if (OccupiedMinigun)
		{
			OccupiedMinigun->ForceRelease();
		}
	}
	Super::Destroyed();
}

bool AGalacticPiratesCharacter::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	if (const AGalacticPiratesCharacter* Other = Cast<AGalacticPiratesCharacter>(ViewTarget))
	{
		if (BoardedShip && BoardedShip == Other->GetBoardedShip())
		{
			return true;
		}
		if (OccupiedVehicle && OccupiedVehicle == Other->GetOccupiedVehicle())
		{
			return true;
		}
	}
	if (ViewTarget && (ViewTarget == BoardedShip || ViewTarget == OccupiedVehicle))
	{
		return true;
	}

	const AActor* Craft = BoardedShip ? static_cast<const AActor*>(BoardedShip) : OccupiedVehicle.Get();
	if (Craft)
	{
		const float CullSq = FMath::Max(GetNetCullDistanceSquared(), Craft->GetNetCullDistanceSquared());
		if (FVector::DistSquared(SrcLocation, Craft->GetActorLocation()) <= CullSq)
		{
			return true;
		}
	}
	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

void AGalacticPiratesCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	EnsureNativeShipInputActions();
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AGalacticPiratesCharacter::DoJumpStart);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AGalacticPiratesCharacter::DoJumpEnd);
		}
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGalacticPiratesCharacter::MoveInput);
		}
		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGalacticPiratesCharacter::LookInput);
		}
		if (MouseLookAction)
		{
			EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AGalacticPiratesCharacter::MouseLookInput);
		}

		BindNativeVehicleInput();
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

void AGalacticPiratesCharacter::ApplyVehicleControlMapping(bool bEnable)
{
	EnsureNativeShipInputActions();
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

	if (!bEnable)
	{
		if (RuntimeVehicleControlIMC)
		{
			Subsystem->RemoveMappingContext(RuntimeVehicleControlIMC);
			UE_LOG(LogGalacticPirates, Warning, TEXT("[FighterPilot] Removed vehicle IMC from %s"), *GetNameSafe(this));
		}
		return;
	}

	if (!ShipThrustAction || !ShipVerticalAction || !ShipRotationAction || !ShipRollAction)
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[FighterPilot] ship input actions missing on %s — C++ fallback keys unavailable"), *GetNameSafe(this));
		return;
	}

	if (!RuntimeVehicleControlIMC)
	{
		RuntimeVehicleControlIMC = NewObject<UInputMappingContext>(this, TEXT("RuntimeVehicleControlIMC"));
		auto MapNegated = [this](const UInputAction* Action, const FKey& Key)
		{
			FEnhancedActionKeyMapping& Mapping = RuntimeVehicleControlIMC->MapKey(Action, Key);
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(RuntimeVehicleControlIMC));
		};
		auto MapToYAxis = [this](const UInputAction* Action, const FKey& Key, bool bNegate)
		{
			FEnhancedActionKeyMapping& Mapping = RuntimeVehicleControlIMC->MapKey(Action, Key);
			Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(RuntimeVehicleControlIMC));
			if (bNegate)
			{
				Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(RuntimeVehicleControlIMC));
			}
		};

		MapToYAxis(ShipThrustAction, EKeys::W, false);
		MapToYAxis(ShipThrustAction, EKeys::S, true);
		MapNegated(ShipThrustAction, EKeys::A);
		RuntimeVehicleControlIMC->MapKey(ShipThrustAction, EKeys::D);

		MapToYAxis(ShipRotationAction, EKeys::Up, false);
		MapToYAxis(ShipRotationAction, EKeys::Down, true);
		MapNegated(ShipRotationAction, EKeys::Left);
		RuntimeVehicleControlIMC->MapKey(ShipRotationAction, EKeys::Right);

		RuntimeVehicleControlIMC->MapKey(ShipVerticalAction, EKeys::SpaceBar);
		MapNegated(ShipVerticalAction, EKeys::LeftControl);

		RuntimeVehicleControlIMC->MapKey(ShipRollAction, EKeys::E);
		MapNegated(ShipRollAction, EKeys::Q);
	}

	Subsystem->AddMappingContext(RuntimeVehicleControlIMC, 3);
	UE_LOG(LogGalacticPirates, Warning, TEXT("[FighterPilot] Added vehicle IMC to %s (WASD/arrows) for %s"),
		*GetNameSafe(this),
		*GetNameSafe(OccupiedVehicle ? OccupiedVehicle.Get() : BoardedShip));
}

void AGalacticPiratesCharacter::EnsureNativeShipInputActions()
{
	auto EnsureAction = [this](UInputAction*& Action, const TCHAR* Name, EInputActionValueType Type)
	{
		if (!Action)
		{
			Action = NewObject<UInputAction>(this, Name);
			Action->ValueType = Type;
			UE_LOG(LogGalacticPirates, Warning, TEXT("[FighterPilot] created C++ input action %s on %s"), Name, *GetNameSafe(this));
		}
	};

	EnsureAction(ShipThrustAction, TEXT("NativeShipThrust"), EInputActionValueType::Axis2D);
	EnsureAction(ShipVerticalAction, TEXT("NativeShipVertical"), EInputActionValueType::Axis1D);
	EnsureAction(ShipRotationAction, TEXT("NativeShipRotation"), EInputActionValueType::Axis2D);
	EnsureAction(ShipRollAction, TEXT("NativeShipRoll"), EInputActionValueType::Axis1D);
	EnsureAction(ShipInteractAction, TEXT("NativeShipInteract"), EInputActionValueType::Boolean);
	EnsureAction(MinigunFireAction, TEXT("NativeMinigunFire"), EInputActionValueType::Boolean);
	EnsureAction(MoveAction, TEXT("NativeMove"), EInputActionValueType::Axis2D);
	EnsureAction(LookAction, TEXT("NativeLook"), EInputActionValueType::Axis2D);
	EnsureAction(MouseLookAction, TEXT("NativeMouseLook"), EInputActionValueType::Axis2D);
	EnsureAction(JumpAction, TEXT("NativeJump"), EInputActionValueType::Boolean);
}

void AGalacticPiratesCharacter::BindNativeVehicleInput()
{
	EnsureNativeShipInputActions();
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInputComponent || bNativeVehicleInputBound)
	{
		return;
	}

	EnhancedInputComponent->BindAction(ShipThrustAction, ETriggerEvent::Triggered, this, &AGalacticPiratesCharacter::ShipThrustInput);
	EnhancedInputComponent->BindAction(ShipVerticalAction, ETriggerEvent::Triggered, this, &AGalacticPiratesCharacter::ShipVerticalInput);
	EnhancedInputComponent->BindAction(ShipRotationAction, ETriggerEvent::Triggered, this, &AGalacticPiratesCharacter::ShipRotationInput);
	EnhancedInputComponent->BindAction(ShipRollAction, ETriggerEvent::Triggered, this, &AGalacticPiratesCharacter::ShipRollInput);
	EnhancedInputComponent->BindAction(ShipInteractAction, ETriggerEvent::Started, this, &AGalacticPiratesCharacter::ShipInteractInput);
	EnhancedInputComponent->BindAction(MinigunFireAction, ETriggerEvent::Started, this, &AGalacticPiratesCharacter::MinigunFireInput);
	EnhancedInputComponent->BindAction(MinigunFireAction, ETriggerEvent::Completed, this, &AGalacticPiratesCharacter::MinigunFireInput);
	bNativeVehicleInputBound = true;
	UE_LOG(LogGalacticPirates, Warning, TEXT("[FighterPilot] bound C++ vehicle actions on %s"), *GetNameSafe(this));
}

bool AGalacticPiratesCharacter::IsControlKeyDown(const FKey& Key) const
{
	if (SimulatedHeldKeys.Contains(Key))
	{
		return true;
	}
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		return PC->IsInputKeyDown(Key);
	}
	return false;
}

void AGalacticPiratesCharacter::SimulateControlKey(const FKey& Key, bool bPressed)
{
	if (bPressed)
	{
		SimulatedHeldKeys.Add(Key);
	}
	else
	{
		SimulatedHeldKeys.Remove(Key);
	}

	InjectPlaytestKey(Key, bPressed ? IE_Pressed : IE_Released, 1.0f);

	if (Key == EKeys::F && bPressed)
	{
		ShipInteractInput(FInputActionValue(true));
	}

	if (bIsPiloting)
	{
		PollHeldVehicleKeys();
		PollAndSendVehicleInput(true);
	}
	else
	{
		PollHeldWalkKeys();
	}
}

void AGalacticPiratesCharacter::ClearSimulatedControlKeys()
{
	SimulatedHeldKeys.Reset();
	AccumulatedThrustInput = FVector::ZeroVector;
	AccumulatedRotationInput = FVector::ZeroVector;
	HelmMouseSteer = FVector2D::ZeroVector;
}

void AGalacticPiratesCharacter::PollHeldWalkKeys()
{
	if (bIsPiloting || OccupiedMinigun || SimulatedHeldKeys.Num() == 0)
	{
		return;
	}

	float Forward = 0.0f;
	float Right = 0.0f;
	if (IsControlKeyDown(EKeys::W)) { Forward += 1.0f; }
	if (IsControlKeyDown(EKeys::S)) { Forward -= 1.0f; }
	if (IsControlKeyDown(EKeys::D)) { Right += 1.0f; }
	if (IsControlKeyDown(EKeys::A)) { Right -= 1.0f; }
	if (!FMath::IsNearlyZero(Forward) || !FMath::IsNearlyZero(Right))
	{
		DoMove(Right, Forward);
	}
}

void AGalacticPiratesCharacter::PollHeldVehicleKeys()
{
	if (!bIsPiloting)
	{
		return;
	}

	FVector Thrust = FVector::ZeroVector;
	FVector Rotation = FVector::ZeroVector;

	if (IsControlKeyDown(EKeys::W)) { Thrust.X += 1.0f; }
	if (IsControlKeyDown(EKeys::S)) { Thrust.X -= 1.0f; }
	if (IsControlKeyDown(EKeys::D)) { Thrust.Y += 1.0f; }
	if (IsControlKeyDown(EKeys::A)) { Thrust.Y -= 1.0f; }
	if (IsControlKeyDown(EKeys::SpaceBar) || IsControlKeyDown(EKeys::Gamepad_FaceButton_Bottom)) { Thrust.Z += 1.0f; }
	if (IsControlKeyDown(EKeys::LeftControl) || IsControlKeyDown(EKeys::LeftAlt)) { Thrust.Z -= 1.0f; }

	if (IsControlKeyDown(EKeys::Up)) { Rotation.Y += 1.0f; }
	if (IsControlKeyDown(EKeys::Down)) { Rotation.Y -= 1.0f; }
	if (IsControlKeyDown(EKeys::Right)) { Rotation.Z += 1.0f; }
	if (IsControlKeyDown(EKeys::Left)) { Rotation.Z -= 1.0f; }
	if (IsControlKeyDown(EKeys::E) || IsControlKeyDown(EKeys::Gamepad_RightShoulder)) { Rotation.X += 1.0f; }
	if (IsControlKeyDown(EKeys::Q) || IsControlKeyDown(EKeys::Gamepad_LeftShoulder)) { Rotation.X -= 1.0f; }

	Thrust.X = FMath::Clamp(Thrust.X, -1.0f, 1.0f);
	Thrust.Y = FMath::Clamp(Thrust.Y, -1.0f, 1.0f);
	Thrust.Z = FMath::Clamp(Thrust.Z, -1.0f, 1.0f);
	Rotation.X = FMath::Clamp(Rotation.X, -1.0f, 1.0f);
	Rotation.Y = FMath::Clamp(Rotation.Y, -1.0f, 1.0f);
	Rotation.Z = FMath::Clamp(Rotation.Z, -1.0f, 1.0f);

	AccumulatedThrustInput = Thrust;
	AccumulatedRotationInput = Rotation;
}

void AGalacticPiratesCharacter::PollAndSendVehicleInput(bool bForceSend)
{
	PollHeldVehicleKeys();
	SendAccumulatedPilotInput(bForceSend);
}

void AGalacticPiratesCharacter::SimulateMouseSteer(const FVector2D& Delta)
{
	ApplyHelmMouseSteer(Delta);
	SendAccumulatedPilotInput(true);
}

void AGalacticPiratesCharacter::BoardShip(AWalkableShip* Ship)
{
	if (!HasAuthority() || !Ship || Ship->IsWrecked() || bDead)
	{
		return;
	}

	if (BoardedShip)
	{
		LeaveShip();
	}

	BoardedShip = Ship;
	Ship->RegisterPlayer(this);

	const bool bAlreadyOnDeck = Ship->IsWalkableWorldLocation(GetActorLocation()) || Ship->HasDeckBelow(GetActorLocation());
	if (!bAlreadyOnDeck)
	{
		const int32 Slot = FMath::Max(0, Ship->GetPlayersAboard().Num() - 1);
		const FTransform SpawnTransform = Ship->GetSpawnTransformForSlot(Slot);
		SetActorLocationAndRotation(SpawnTransform.GetLocation(), SpawnTransform.GetRotation());
	}

	OnRep_BoardedShip();
	LastShipWorldTM = Ship->GetActorTransform();
	bHasLastShipWorldTM = true;
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

void AGalacticPiratesCharacter::SetAiCrew(bool bNewAiCrew)
{
	if (!HasAuthority())
	{
		return;
	}
	bIsAiCrew = bNewAiCrew;
}

void AGalacticPiratesCharacter::SetOccupiedVehicle(AActor* Vehicle)
{
	if (!HasAuthority())
	{
		return;
	}
	OccupiedVehicle = Vehicle;
	OnRep_OccupiedVehicle();
}

void AGalacticPiratesCharacter::OnRep_OccupiedVehicle()
{
	ApplyVehicleControlMapping(OccupiedVehicle != nullptr && bIsPiloting);
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

void AGalacticPiratesCharacter::FlushHeldShipInputsOnTakeHelm()
{
	if (!IsLocallyControlled() || !bIsPiloting || !(BoardedShip || OccupiedVehicle))
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());

	auto Axis2D = [this](const UInputAction* Action) -> FVector2D
	{
		if (!Action)
		{
			return FVector2D::ZeroVector;
		}
		return UEnhancedInputLibrary::GetBoundActionValue(this, Action).Get<FVector2D>();
	};
	auto Axis1D = [this](const UInputAction* Action) -> float
	{
		if (!Action)
		{
			return 0.0f;
		}
		return UEnhancedInputLibrary::GetBoundActionValue(this, Action).Get<float>();
	};

	FVector Thrust = FVector::ZeroVector;
	FVector Rotation = FVector::ZeroVector;

	const FVector2D Move = Axis2D(MoveAction);
	Thrust.X = Move.Y;
	Thrust.Y = Move.X;

	const FVector2D ShipThrust = Axis2D(ShipThrustAction);
	if (!ShipThrust.IsNearlyZero())
	{
		Thrust.X = ShipThrust.Y;
		Thrust.Y = ShipThrust.X;
	}
	else if (PC && Move.IsNearlyZero())
	{
		if (PC->IsInputKeyDown(EKeys::W)) { Thrust.X += 1.0f; }
		if (PC->IsInputKeyDown(EKeys::S)) { Thrust.X -= 1.0f; }
		if (PC->IsInputKeyDown(EKeys::D)) { Thrust.Y += 1.0f; }
		if (PC->IsInputKeyDown(EKeys::A)) { Thrust.Y -= 1.0f; }
		Thrust.X = FMath::Clamp(Thrust.X, -1.0f, 1.0f);
		Thrust.Y = FMath::Clamp(Thrust.Y, -1.0f, 1.0f);
	}

	const float Vertical = Axis1D(ShipVerticalAction);
	if (!FMath::IsNearlyZero(Vertical))
	{
		Thrust.Z = Vertical;
	}
	else if (PC)
	{
		if (PC->IsInputKeyDown(EKeys::SpaceBar) || PC->GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis) > 0.1f)
		{
			Thrust.Z = 1.0f;
		}
		else if (PC->IsInputKeyDown(EKeys::LeftControl) || PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis) > 0.1f)
		{
			Thrust.Z = -1.0f;
		}
	}

	const FVector2D ShipRot = Axis2D(ShipRotationAction);
	if (!ShipRot.IsNearlyZero())
	{
		Rotation.Y = ShipRot.Y;
		Rotation.Z = ShipRot.X;
	}

	const float Roll = Axis1D(ShipRollAction);
	if (!FMath::IsNearlyZero(Roll))
	{
		Rotation.X = Roll;
	}
	else if (PC)
	{
		if (PC->IsInputKeyDown(EKeys::E) || PC->IsInputKeyDown(EKeys::Gamepad_RightShoulder))
		{
			Rotation.X = 1.0f;
		}
		else if (PC->IsInputKeyDown(EKeys::Q) || PC->IsInputKeyDown(EKeys::Gamepad_LeftShoulder))
		{
			Rotation.X = -1.0f;
		}
	}

	AccumulatedThrustInput = Thrust;
	AccumulatedRotationInput = Rotation;
	HelmMouseSteer = FVector2D::ZeroVector;
	TimeSinceLastPilotInputSend = 0.0f;
	SendAccumulatedPilotInput(true);
}

void AGalacticPiratesCharacter::OnShipDestroyed()
{
	if (!HasAuthority() || bDead)
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

void AGalacticPiratesCharacter::DieInWreck(const FVector& Epicenter)
{
	if (bDead)
	{
		return;
	}

	bDead = true;

	if (HasAuthority())
	{
		if (OccupiedMinigun)
		{
			OccupiedMinigun->ForceRelease();
		}

		AWalkableShip* Ship = BoardedShip;
		if (UOccupancyComponent* Seat = GPFindPilotOccupancy(OccupiedVehicle))
		{
			Seat->ForceRelease();
		}
		OccupiedVehicle = nullptr;
		if (Ship)
		{
			Ship->OnShipRotationChanged.RemoveDynamic(this, &AGalacticPiratesCharacter::OnShipRotationChanged);
			if (Ship->IsPilot(this))
			{
				Ship->ReleasePilot(this);
			}
			Ship->UnregisterPlayer(this);
		}

		BoardedShip = nullptr;
		bIsPiloting = false;
		OccupiedMinigun = nullptr;
		ClearMovementBase();
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		SetLifeSpan(24.0f);

		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			DisableInput(PC);
			PC->SetIgnoreMoveInput(true);
			PC->SetIgnoreLookInput(true);
		}

		Multicast_WreckRagdoll(Epicenter);
	}
}

void AGalacticPiratesCharacter::OnRep_Dead()
{
	if (bDead && GetMesh() && !GetMesh()->IsSimulatingPhysics())
	{
		ApplyWreckRagdoll(GetActorLocation());
	}
}

void AGalacticPiratesCharacter::Multicast_WreckRagdoll_Implementation(FVector Epicenter)
{
	bDead = true;
	ApplyWreckRagdoll(Epicenter);
}

void AGalacticPiratesCharacter::ApplyWreckRagdoll(const FVector& Epicenter)
{
	bDead = true;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetReplicateMovement(false);

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
		MoveComp->SetComponentTickEnabled(false);
		MoveComp->GravityScale = 0.0f;
		MoveComp->SetMovementMode(MOVE_None);
		MoveComp->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Capsule->SetEnableGravity(false);
	}

	if (QuatCameraComponent)
	{
		QuatCameraComponent->SetHiddenInGame(true);
#if WITH_EDITORONLY_DATA
		QuatCameraComponent->bCameraMeshHiddenInGame = true;
#endif
		QuatCameraComponent->bEnableFirstPersonFieldOfView = false;
		QuatCameraComponent->bEnableFirstPersonScale = false;
		QuatCameraComponent->SetComponentTickEnabled(true);
		QuatCameraComponent->SetGunSightLock(false);
		QuatCameraComponent->SetDeathFollow(true);
	}

	if (FirstPersonMesh)
	{
		FirstPersonMesh->SetVisibility(false, false);
		FirstPersonMesh->SetHiddenInGame(true);
		FirstPersonMesh->SetSimulatePhysics(false);
		FirstPersonMesh->SetOwnerNoSee(true);
		FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::None;
	}

	USkeletalMeshComponent* Body = GetMesh();
	if (!Body)
	{
		return;
	}

	Body->SetOnlyOwnerSee(false);
	Body->SetOwnerNoSee(false);
	Body->SetHiddenInGame(false);
	Body->SetVisibility(true, false);
	Body->bCastHiddenShadow = false;
	Body->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::None;
	Body->SetRenderInMainPass(true);
	Body->SetRenderInDepthPass(true);
	Body->SetComponentTickEnabled(true);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Body->UnHideBoneByName(TEXT("head"));
	Body->UnHideBoneByName(TEXT("Head"));
	Body->UnHideBoneByName(TEXT("neck_01"));
	Body->UnHideBoneByName(TEXT("neck_02"));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetCollisionObjectType(ECC_PhysicsBody);
	Body->SetCollisionResponseToAllChannels(ECR_Block);
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Body->SetEnableGravity(true);
	Body->bBlendPhysics = true;
	Body->SetAllBodiesSimulatePhysics(true);
	if (Body->GetBoneName(0) != NAME_None)
	{
		Body->SetAllBodiesBelowSimulatePhysics(Body->GetBoneName(0), true, true);
	}
	Body->SetAllBodiesPhysicsBlendWeight(1.0f);
	Body->bPauseAnims = true;
	AShipWreckDebris::ApplyWreckKick(Body, Epicenter);

	UpdateDeathCamera(0.0f);
	BeginLocalDeathPresentation();
}

void AGalacticPiratesCharacter::BeginLocalDeathPresentation()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalPlayerController() || PC->GetPawn() != this)
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[DeathFX] BeginLocalDeathPresentation skip crew=%s pc=%s pawn=%s"),
			*GetName(),
			*GetNameSafe(PC),
			*GetNameSafe(PC ? PC->GetPawn() : nullptr));
		return;
	}

	UE_LOG(LogGalacticPirates, Warning, TEXT("[DeathFX] BeginLocalDeathPresentation crew=%s pc=%s"),
		*GetName(),
		*GetNameSafe(PC));

	if (AGalacticPiratesPlayerController* GPPC = Cast<AGalacticPiratesPlayerController>(PC))
	{
		GPPC->BeginCrewDeathPresentation();
		return;
	}

	PC->ClientSetHUD(AGalacticPiratesHUD::StaticClass());
	if (AGalacticPiratesHUD* DeathHud = Cast<AGalacticPiratesHUD>(PC->GetHUD()))
	{
		DeathHud->BeginDeathPresentation();
	}
}

void AGalacticPiratesCharacter::UpdateDeathCamera(float DeltaTime)
{
	if (!QuatCameraComponent)
	{
		return;
	}

	USkeletalMeshComponent* Body = GetMesh();
	if (!Body)
	{
		return;
	}

	if (FirstPersonMesh)
	{
		FirstPersonMesh->SetVisibility(false, false);
		FirstPersonMesh->SetHiddenInGame(true);
		FirstPersonMesh->SetOwnerNoSee(true);
		FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::None;
	}

	TInlineComponentArray<UPrimitiveComponent*> Prims;
	GetComponents(Prims);
	for (UPrimitiveComponent* Prim : Prims)
	{
		if (!Prim || Prim == FirstPersonMesh)
		{
			continue;
		}
		if (Prim->IsA(UCameraProxyMeshComponent::StaticClass()) || Prim->IsAttachedTo(QuatCameraComponent))
		{
			Prim->SetHiddenInGame(true);
			Prim->SetVisibility(false);
			continue;
		}
		Prim->SetOwnerNoSee(false);
		Prim->SetHiddenInGame(false);
		Prim->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::None;
	}
	Body->SetVisibility(true, false);
	Body->SetOwnerNoSee(false);
	Body->SetHiddenInGame(false);
	Body->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::None;

	auto FindBone = [Body](const FName* Names, int32 Count) -> FName
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Body->GetBoneIndex(Names[Index]) != INDEX_NONE)
			{
				return Names[Index];
			}
		}
		return NAME_None;
	};

	static const FName HeadNames[] = { TEXT("head"), TEXT("Head"), TEXT("CC_Base_Head") };
	const FName HeadBone = FindBone(HeadNames, UE_ARRAY_COUNT(HeadNames));
	const FQuat RelRot = FRotator(0.0f, 90.0f, -90.0f).Quaternion() * FRotator(-20.0f, 0.0f, 0.0f).Quaternion();
	const FVector RelLoc(-2.8f, 5.89f, 0.0f);

	QuatCameraComponent->SetUsingAbsoluteLocation(false);
	QuatCameraComponent->SetUsingAbsoluteRotation(false);
	QuatCameraComponent->SetHiddenInGame(true);
#if WITH_EDITORONLY_DATA
	QuatCameraComponent->bCameraMeshHiddenInGame = true;
#endif
	QuatCameraComponent->bEnableFirstPersonFieldOfView = false;
	QuatCameraComponent->bEnableFirstPersonScale = false;
	QuatCameraComponent->FirstPersonScale = 1.0f;
	QuatCameraComponent->FieldOfView = 90.0f;

	if (QuatCameraComponent->GetAttachParent() != Body || QuatCameraComponent->GetAttachSocketName() != HeadBone)
	{
		QuatCameraComponent->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HeadBone);
	}
	QuatCameraComponent->SetRelativeLocationAndRotation(RelLoc, RelRot);
}

void AGalacticPiratesCharacter::OnRep_BoardedShip()
{
	if (bDead)
	{
		return;
	}
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
		FlushHeldShipInputsOnTakeHelm();
		ApplyVehicleControlMapping(true);
	}
	else if (!OccupiedMinigun)
	{
		ApplyVehicleControlMapping(false);
		SetupMovementBaseOnShip();
	}

	if (!HasAuthority())
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[DedicatedNet] Client piloting=%s vehicle=%s"),
			bIsPiloting ? TEXT("true") : TEXT("false"),
			*GetNameSafe(OccupiedVehicle));
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

	if (OccupiedMinigun)
	{
		OccupiedMinigun->ApplyGunnerCamera();
	}
	else
	{
		RestoreWalkCamera();
		if (!bIsPiloting)
		{
			SetupMovementBaseOnShip();
		}
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

	if (BoardedShip)
	{
		LastShipWorldTM = BoardedShip->GetActorTransform();
		bHasLastShipWorldTM = true;
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
	if (BoardedShip)
	{
		LastShipWorldTM = BoardedShip->GetActorTransform();
		bHasLastShipWorldTM = true;
	}
}

void AGalacticPiratesCharacter::RestoreWalkCamera()
{
	if (!QuatCameraComponent)
	{
		return;
	}

	QuatCameraComponent->SetGunSightLock(false);
	if (FirstPersonMesh)
	{
		FirstPersonMesh->SetVisibility(true, true);
		QuatCameraComponent->AttachToComponent(FirstPersonMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("head"));
		QuatCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	}
}

void AGalacticPiratesCharacter::TickBoardedWalkPhysics(float DeltaTime)
{
	if (!BoardedShip)
	{
		return;
	}

	if (OccupiedMinigun || OccupiedVehicle || bIsPiloting)
	{
		bHasLastShipWorldTM = false;
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

	const FTransform ShipTM = BoardedShip->GetActorTransform();
	if (bHasLastShipWorldTM)
	{
		const FVector RelLoc = LastShipWorldTM.InverseTransformPosition(GetActorLocation());
		const FQuat RelRot = LastShipWorldTM.InverseTransformRotation(GetActorQuat());
		const FVector NewLoc = ShipTM.TransformPosition(RelLoc);
		const FQuat NewRot = ShipTM.TransformRotation(RelRot);
		const float Carry = FVector::Dist(GetActorLocation(), NewLoc);
		if (Carry > 0.05f && (HasAuthority() || IsLocallyControlled()))
		{
			SetActorLocationAndRotation(NewLoc, NewRot, false, nullptr, ETeleportType::None);
		}
		if ((GFrameCounter % 15) == 0)
		{
			UE_LOG(LogGalacticPirates, Warning,
				TEXT("[ShipWalk] %s auth=%d net=%d carry=%.1f rel=%s ship=%s"),
				*GetName(),
				HasAuthority() ? 1 : 0,
				static_cast<int32>(GetNetMode()),
				Carry,
				*RelLoc.ToCompactString(),
				*ShipTM.GetLocation().ToCompactString());
		}
	}
	LastShipWorldTM = ShipTM;
	bHasLastShipWorldTM = true;

	const bool bOnShipFloor = BoardedShip->IsWalkableWorldLocation(GetActorLocation()) || BoardedShip->HasDeckBelow(GetActorLocation(), 400.0f);
	if (bOnShipFloor)
	{
		LastGoodShipRelative = FTransform(
			ShipTM.InverseTransformRotation(GetActorQuat()),
			ShipTM.InverseTransformPosition(GetActorLocation()));
		bHasLastGoodShipRelative = true;
		TimeOffShipDeck = 0.0f;
		return;
	}

	TimeOffShipDeck += DeltaTime;
	if ((GFrameCounter % 15) == 0)
	{
		const FVector Rel = ShipTM.InverseTransformPosition(GetActorLocation());
		UE_LOG(LogGalacticPirates, Warning,
			TEXT("[OffDeck] auth=%d local=%d net=%d mode=%d t=%.2f rel=%s world=%s ship=%s"),
			HasAuthority() ? 1 : 0,
			IsLocallyControlled() ? 1 : 0,
			static_cast<int32>(GetNetMode()),
			static_cast<int32>(MoveComp->MovementMode),
			TimeOffShipDeck,
			*Rel.ToCompactString(),
			*GetActorLocation().ToCompactString(),
			*BoardedShip->GetActorLocation().ToCompactString());
	}
	if (!HasAuthority())
	{
		return;
	}

	const bool bJumping = MoveComp->IsFalling();
	const float RecoverAfter = bJumping ? 1.25f : 0.35f;
	if (TimeOffShipDeck < RecoverAfter)
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
	FVector2D LookAxisVector = Value.Get<FVector2D>() * GamepadLookSensitivity;
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
		Server_SendMinigunAim(Yaw, Pitch, NextMinigunAimSeq++);
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

void AGalacticPiratesCharacter::SendAccumulatedPilotInput(bool bForceSend)
{
	if (!bIsPiloting || (!BoardedShip && !OccupiedVehicle))
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

	if (bForceSend || !AccumulatedThrustInput.IsNearlyZero() || !RotationToSend.IsNearlyZero())
	{
		if (GPShipMoveLogLevel() > 0)
		{
			UE_LOG(LogGalacticPirates, Warning,
				TEXT("[ShipMove] SendPilot %s piloting=%d ship=%s thrust=%s rot=%s"),
				*GetName(),
				bIsPiloting ? 1 : 0,
				*GetNameSafe(OccupiedVehicle ? OccupiedVehicle.Get() : BoardedShip),
				*AccumulatedThrustInput.ToCompactString(),
				*RotationToSend.ToCompactString());
		}
		Server_SendPilotInput(AccumulatedThrustInput, RotationToSend, NextPilotInputSeq++);
	}

	AccumulatedThrustInput = FVector::ZeroVector;
	AccumulatedRotationInput = FVector::ZeroVector;
}

bool AGalacticPiratesCharacter::Server_SendPilotInput_Validate(FVector ThrustInput, FVector RotationInput, uint32 InputSeq)
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

void AGalacticPiratesCharacter::Server_SendPilotInput_Implementation(FVector ThrustInput, FVector RotationInput, uint32 InputSeq)
{
	AuthorityReceiveSequencedPilotInput(ThrustInput, RotationInput, InputSeq);
}

void AGalacticPiratesCharacter::AuthorityReceiveSequencedPilotInput(FVector ThrustInput, FVector RotationInput, uint32 InputSeq)
{
	if (!HasAuthority() || !bIsPiloting)
	{
		return;
	}

	if (InputSeq != 0 && InputSeq <= LastAcceptedPilotInputSeq)
	{
		return;
	}
	LastAcceptedPilotInputSeq = InputSeq;

	ThrustInput.X = FMath::Clamp(ThrustInput.X, -1.0f, 1.0f);
	ThrustInput.Y = FMath::Clamp(ThrustInput.Y, -1.0f, 1.0f);
	ThrustInput.Z = FMath::Clamp(ThrustInput.Z, -1.0f, 1.0f);
	RotationInput.X = FMath::Clamp(RotationInput.X, -1.0f, 1.0f);
	RotationInput.Y = FMath::Clamp(RotationInput.Y, -1.0f, 1.0f);
	RotationInput.Z = FMath::Clamp(RotationInput.Z, -1.0f, 1.0f);

	if (ABulldogFighter* Fighter = Cast<ABulldogFighter>(OccupiedVehicle))
	{
		Fighter->ApplyPilotInput(this, ThrustInput, RotationInput);
		return;
	}

	if (BoardedShip && BoardedShip->IsPilot(this))
	{
		BoardedShip->ApplyPilotInput(this, ThrustInput, RotationInput);
	}
}

bool AGalacticPiratesCharacter::Server_RequestHelmInteraction_Validate()
{
	return true;
}

void AGalacticPiratesCharacter::Server_RequestHelmInteraction_Implementation()
{
	bool bHandled = false;
	if (OccupiedVehicle)
	{
		if (ABulldogFighter* Fighter = Cast<ABulldogFighter>(OccupiedVehicle))
		{
			bHandled = Fighter->TryCockpitInteract(this);
		}
	}
	else if (BoardedShip)
	{
		bHandled = BoardedShip->TryStationInteract(this);
	}
	else if (AActor* Craft = GPFindOccupiableCraftInRange(this))
	{
		if (ABulldogFighter* Fighter = Cast<ABulldogFighter>(Craft))
		{
			bHandled = Fighter->TryCockpitInteract(this);
		}
		else if (UOccupancyComponent* Seat = GPFindPilotOccupancy(Craft))
		{
			bHandled = Seat->TryOccupy(this);
		}
	}
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

bool AGalacticPiratesCharacter::Server_SendMinigunAim_Validate(float YawDelta, float PitchDelta, uint32 InputSeq)
{
	return !FMath::IsNaN(YawDelta) && !FMath::IsNaN(PitchDelta)
		&& FMath::Abs(YawDelta) < 90.0f && FMath::Abs(PitchDelta) < 90.0f;
}

void AGalacticPiratesCharacter::Server_SendMinigunAim_Implementation(float YawDelta, float PitchDelta, uint32 InputSeq)
{
	AuthorityReceiveSequencedMinigunAim(YawDelta, PitchDelta, InputSeq);
}

void AGalacticPiratesCharacter::AuthorityReceiveSequencedMinigunAim(float YawDelta, float PitchDelta, uint32 InputSeq)
{
	if (!HasAuthority() || !OccupiedMinigun || OccupiedMinigun->GetGunner() != this)
	{
		return;
	}

	if (InputSeq != 0 && InputSeq <= LastAcceptedMinigunAimSeq)
	{
		return;
	}
	LastAcceptedMinigunAimSeq = InputSeq;
	OccupiedMinigun->AddAimInput(YawDelta, PitchDelta);
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
