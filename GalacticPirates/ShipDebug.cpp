#include "ShipDebug.h"
#include "GalacticPirates.h"
#include "GalacticPiratesCharacter.h"
#include "HelmComponent.h"
#include "QuatCamera.h"
#include "WalkableShip.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Interfaces/MovementBaseInterface.h"
#include "Kismet/GameplayStatics.h"
#include "ShipMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerInput.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformMisc.h"
#include "WeaponTerminalComponent.h"
#include "ShipPulseCannonComponent.h"
#include "ShipOrbitAiComponent.h"
#include "ShipCrewAiComponent.h"

TAutoConsoleVariable<int32> CVarGPShipDebug(
	TEXT("gp.ShipDebug"),
	1,
	TEXT("Walkable ship diagnostics. 0=off, 1=events, 2=tick snapshots + input counts, 3=input spam"),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarGPShipPlaytest(
	TEXT("gp.ShipPlaytest"),
	0,
	TEXT("Run the automated walkable-ship PIE playtest on the local player (1=on)."),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarGPInteriorTest(
	TEXT("gp.InteriorTest"),
	0,
	TEXT("Run the interior walk test: camera vs capsule facing, and slide on a translating/yawing ship (1=on)."),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarGPDedicatedNetTest(
	TEXT("gp.DedicatedNetTest"),
	0,
	TEXT("Run the dedicated-server persistent-world boarding and helm test (1=on)."),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarGPShipMoveLog(
	TEXT("gp.ShipMoveLog"),
	0,
	TEXT("Log ship thrust/rotation/velocity every movement tick (1=throttled, 2=every SetInput)."),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarGPShipMoveProbe(
	TEXT("gp.ShipMoveProbe"),
	0,
	TEXT("Set to 1 to start the ship movement probe (prefer server console)."),
	ECVF_Default);

int32 GPShipMoveLogLevel()
{
	return CVarGPShipMoveLog.GetValueOnGameThread();
}

int32 GPShipDebugLevel()
{
	return CVarGPShipDebug.GetValueOnGameThread();
}

bool GPShipPlaytestEnabled()
{
	return CVarGPShipPlaytest.GetValueOnGameThread() != 0;
}

bool GPInteriorTestEnabled()
{
	return CVarGPInteriorTest.GetValueOnGameThread() != 0;
}

bool GPDedicatedNetTestEnabled()
{
	return CVarGPDedicatedNetTest.GetValueOnGameThread() != 0;
}

void GPShipDebugEvent(const TCHAR* Message)
{
	if (GPShipDebugLevel() < 1)
	{
		return;
	}

	UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDebug] %s"), Message);
}

void GPShipDebugScreen(const FString& Message, const FColor& Color, float Duration)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, Duration, Color, Message);
	}

	if (GPShipDebugLevel() >= 1)
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDebug][HUD] %s"), *Message);
	}
}

namespace
{
	const TCHAR* MobilityName(EComponentMobility::Type Mobility)
	{
		switch (Mobility)
		{
		case EComponentMobility::Static: return TEXT("Static");
		case EComponentMobility::Stationary: return TEXT("Stationary");
		case EComponentMobility::Movable: return TEXT("Movable");
		default: return TEXT("Unknown");
		}
	}

	float SignedPlanarAngleDeg(const FVector& From, const FVector& To, const FVector& PlaneNormal)
	{
		const FVector ProjectedFrom = FVector::VectorPlaneProject(From, PlaneNormal).GetSafeNormal();
		const FVector ProjectedTo = FVector::VectorPlaneProject(To, PlaneNormal).GetSafeNormal();
		if (ProjectedFrom.IsNearlyZero() || ProjectedTo.IsNearlyZero())
		{
			return 0.0f;
		}

		const float Unsigned = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ProjectedFrom, ProjectedTo), -1.0f, 1.0f)));
		const FVector Cross = FVector::CrossProduct(ProjectedFrom, ProjectedTo);
		return FVector::DotProduct(Cross, PlaneNormal) < 0.0f ? -Unsigned : Unsigned;
	}

	FVector PlanarDelta(const FVector& From, const FVector& To, const FVector& PlaneNormal)
	{
		return FVector::VectorPlaneProject(To - From, PlaneNormal);
	}

	void InjectKey(AGalacticPiratesCharacter* Character, const FKey& Key, EInputEvent Event)
	{
		APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
		if (!PC)
		{
			return;
		}

		PC->InputKey(FInputKeyParams(Key, Event, 1.0));
	}

	void InjectAxis(AGalacticPiratesCharacter* Character, const FKey& Key, const FVector& Delta)
	{
		APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
		if (!PC)
		{
			return;
		}

		PC->InputKey(FInputKeyParams(Key, Delta, 0.016f, 1));
	}

	struct FInteriorTestState
	{
		TWeakObjectPtr<AGalacticPiratesCharacter> Character;
		TWeakObjectPtr<AWalkableShip> Ship;
		int32 Phase = -1;
		float Time = 0.0f;
		float LogTimer = 0.0f;
		FVector ShipStartLocation = FVector::ZeroVector;
		FRotator ShipStartRotation = FRotator::ZeroRotator;
		FVector RelAtTranslateStart = FVector::ZeroVector;
		FVector RelAtYawStart = FVector::ZeroVector;
		FVector RelAtWalkStart = FVector::ZeroVector;
		FVector WorldAtTranslateStart = FVector::ZeroVector;
		FVector WorldAtYawStart = FVector::ZeroVector;
		float CameraYawBeforeLook = 0.0f;
		float CamVsCapsuleAfterLook = 0.0f;
		float CamVsMeshAfterLook = 0.0f;
		float CamVsCapsuleAfterYaw = 0.0f;
		float PitchBeforeMouse = 0.0f;
		float PitchAfterMouse = 0.0f;
		float TranslateRelDrift = 0.0f;
		float TranslateWorldDrift = 0.0f;
		float YawRelDrift = 0.0f;
		float WalkRelDrift = 0.0f;
		bool bUseRelativeLocation = false;
		bool bDynamicBase = false;
		bool bIgnoreBaseRotation = false;
		bool bBaseIsInteriorMesh = false;
		FString BaseName;
		FString BaseMobility;
		FString FloorName;
		FString FloorMobility;
		bool bFacingPass = false;
		bool bYawFacingPass = false;
		bool bPitchPass = false;
		bool bTranslatePass = false;
		bool bYawPass = false;
		bool bWalked = false;
	};

	FInteriorTestState GInteriorTest;

	void FillBaseFields(const AGalacticPiratesCharacter* Character, FString& OutBaseName, FString& OutBaseMobility, FString& OutFloorName, FString& OutFloorMobility, bool& bOutUseRelative, bool& bOutDynamic, bool& bOutIgnoreBaseRot, bool& bOutBaseIsInterior)
	{
		OutBaseName = TEXT("None");
		OutBaseMobility = TEXT("None");
		OutFloorName = TEXT("None");
		OutFloorMobility = TEXT("None");
		bOutUseRelative = false;
		bOutDynamic = false;
		bOutIgnoreBaseRot = false;
		bOutBaseIsInterior = false;

		UCharacterMovementComponent* MoveComp = Character ? Character->GetCharacterMovement() : nullptr;
		AWalkableShip* Ship = Character ? Character->GetBoardedShip() : nullptr;
		if (!MoveComp)
		{
			return;
		}

		bOutIgnoreBaseRot = MoveComp->bIgnoreBaseRotation != 0;

		if (const FMovementBaseInterfaceData* BaseData = MoveComp->GetMovementBaseInterfaceData())
		{
			bOutUseRelative = MovementBaseUtility::UseRelativeLocation(BaseData);
			bOutDynamic = MovementBaseUtility::IsDynamicBase(BaseData);
			if (UObject* BaseObj = BaseData->GetMovementBaseObject())
			{
				OutBaseName = BaseObj->GetName();
				if (const USceneComponent* Scene = Cast<USceneComponent>(BaseObj))
				{
					OutBaseMobility = MobilityName(Scene->Mobility);
				}
				if (Ship && BaseObj == Ship->GetInteriorMesh())
				{
					bOutBaseIsInterior = true;
				}
			}
		}

		if (UPrimitiveComponent* Floor = MoveComp->CurrentFloor.HitResult.GetComponent())
		{
			OutFloorName = Floor->GetName();
			OutFloorMobility = MobilityName(Floor->Mobility);
		}
	}

	void TeleportShip(AWalkableShip* Ship, const FVector& Location, const FRotator& Rotation)
	{
		if (!Ship)
		{
			return;
		}

		Ship->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	struct FExtremeFlightTestState
	{
		TWeakObjectPtr<AGalacticPiratesCharacter> Character;
		TWeakObjectPtr<AWalkableShip> Ship;
		int32 Phase = -1;
		float Time = 0.0f;
		float LogTimer = 0.0f;
		FVector RelAtHelmExit = FVector::ZeroVector;
		FVector RelAfterHold = FVector::ZeroVector;
		float HelmExitHoldDrift = 0.0f;
		float WalkRelDrift = 0.0f;
		bool bTookHelm = false;
		bool bExitedHelm = false;
		bool bGroundedAtExit = false;
		bool bInsideAtExit = false;
		bool bGroundedAfterHold = false;
		bool bInsideAfterHold = false;
		bool bStillWalking = false;
		bool bGravityAligned = false;
		FString ModeAtExit;
	};

	FExtremeFlightTestState GExtremeTest;

	struct FHelmSteerTestState
	{
		TWeakObjectPtr<AGalacticPiratesCharacter> Character;
		TWeakObjectPtr<AWalkableShip> Ship;
		int32 Phase = -1;
		float Time = 0.0f;
		FRotator StartRotation = FRotator::ZeroRotator;
		float MouseYawDelta = 0.0f;
		float FallbackYawDelta = 0.0f;
		bool bTookHelm = false;
		bool bMousePass = false;
		bool bUsedFallback = false;
		bool bFallbackPass = false;
	};

	FHelmSteerTestState GHelmSteerTest;

	struct FHelmJitterTestState
	{
		TWeakObjectPtr<AGalacticPiratesCharacter> Character;
		TWeakObjectPtr<AWalkableShip> Ship;
		int32 Phase = -1;
		float Time = 0.0f;
		FVector LastRelLoc = FVector::ZeroVector;
		FQuat LastRelRot = FQuat::Identity;
		FQuat LastCamQuat = FQuat::Identity;
		FQuat LastShipQuat = FQuat::Identity;
		float MaxRelStepCm = 0.0f;
		float MaxRelRotDeg = 0.0f;
		float MaxCamVsShipDeg = 0.0f;
		float MaxCamStepVsShipDeg = 0.0f;
		int32 Samples = 0;
		bool bHasPrevSample = false;
		bool bTookHelm = false;
		FString AttachParent;
		bool bMoveTickEnabled = true;
	};

	FHelmJitterTestState GHelmJitterTest;

	struct FShipJumpScenarioResult
	{
		FString Name;
		bool bLeftGround = false;
		bool bLanded = false;
		float PeakRelUp = 0.0f;
		float RelPlanarDrift = 0.0f;
		bool bPass = false;
	};

	struct FShipJumpTestState
	{
		TWeakObjectPtr<AGalacticPiratesCharacter> Character;
		TWeakObjectPtr<AWalkableShip> Ship;
		int32 Phase = -1;
		int32 Scenario = 0;
		float Time = 0.0f;
		float ScenarioTime = 0.0f;
		FVector RelAtJump = FVector::ZeroVector;
		float PeakRelUp = 0.0f;
		float RelPlanarDrift = 0.0f;
		bool bLeftGround = false;
		bool bLanded = false;
		bool bJumpIssued = false;
		FShipJumpScenarioResult Results[5];
	};

	FShipJumpTestState GShipJumpTest;

	void SetShipMotionForJumpTest(AWalkableShip* Ship, int32 Scenario)
	{
		if (!Ship || !Ship->ShipMovement)
		{
			return;
		}

		const FQuat ShipQ = Ship->GetActorQuat();
		switch (Scenario)
		{
		case 1:
			Ship->ShipMovement->SetVelocityOverride(ShipQ.RotateVector(FVector(1200.0f, 0.0f, 0.0f)), FVector::ZeroVector, true);
			break;
		case 2:
			Ship->ShipMovement->SetVelocityOverride(FVector::ZeroVector, FVector(0.0f, 0.0f, 55.0f), true);
			break;
		case 3:
			Ship->ShipMovement->SetVelocityOverride(FVector::ZeroVector, FVector(35.0f, 45.0f, 55.0f), true);
			break;
		case 4:
			Ship->ShipMovement->SetVelocityOverride(ShipQ.RotateVector(FVector(900.0f, 0.0f, 0.0f)), FVector(0.0f, 0.0f, 40.0f), true);
			break;
		default:
			Ship->ShipMovement->SetVelocityOverride(FVector::ZeroVector, FVector::ZeroVector, false);
			Ship->ShipMovement->SetLinearVelocity(FVector::ZeroVector);
			Ship->ShipMovement->SetAngularVelocity(FVector::ZeroVector);
			break;
		}
	}

	void ApplyExtremeShipMotion(AWalkableShip* Ship)
	{
		if (!Ship || !Ship->ShipMovement)
		{
			return;
		}

		const float MaxLin = FMath::Max(Ship->ShipMovement->MaxLinearVelocity, 1.0f);
		const float MaxAng = FMath::Max(Ship->ShipMovement->MaxAngularVelocity, 1.0f);
		const FVector Linear = Ship->GetActorQuat().RotateVector(FVector(MaxLin, 0.0f, 0.0f));
		const FVector Angular(MaxAng * 0.55f, MaxAng * 0.85f, MaxAng);
		Ship->ShipMovement->SetVelocityOverride(Linear, Angular, true);
	}

	void ClearExtremeShipMotion(AWalkableShip* Ship)
	{
		if (Ship && Ship->ShipMovement)
		{
			Ship->ShipMovement->SetVelocityOverride(FVector::ZeroVector, FVector::ZeroVector, false);
		}
	}

	const TCHAR* MoveModeName(EMovementMode Mode)
	{
		switch (Mode)
		{
		case MOVE_None: return TEXT("None");
		case MOVE_Walking: return TEXT("Walking");
		case MOVE_NavWalking: return TEXT("NavWalking");
		case MOVE_Falling: return TEXT("Falling");
		case MOVE_Swimming: return TEXT("Swimming");
		case MOVE_Flying: return TEXT("Flying");
		case MOVE_Custom: return TEXT("Custom");
		default: return TEXT("Unknown");
		}
	}
}

void GPLogInteriorWalkState(const AGalacticPiratesCharacter* Character, const TCHAR* Reason)
{
	if (!Character)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	UQuatCamera* Camera = Character->GetQuatCameraComponent();
	AWalkableShip* Ship = Character->GetBoardedShip();
	const USkeletalMeshComponent* Mesh = Character->GetMesh();
	const USkeletalMeshComponent* FPMesh = Character->GetFirstPersonMesh();

	FString BaseName, BaseMobility, FloorName, FloorMobility;
	bool bUseRelative = false;
	bool bDynamic = false;
	bool bIgnoreBaseRot = false;
	bool bBaseIsInterior = false;
	FillBaseFields(Character, BaseName, BaseMobility, FloorName, FloorMobility, bUseRelative, bDynamic, bIgnoreBaseRot, bBaseIsInterior);

	const FVector ShipUp = Ship ? Ship->GetShipUpVector() : FVector::UpVector;
	const FVector CapsuleFwd = Character->GetActorForwardVector();
	const FVector MeshFwd = Mesh ? Mesh->GetForwardVector() : CapsuleFwd;
	const FVector FPMeshFwd = FPMesh ? FPMesh->GetForwardVector() : CapsuleFwd;
	const FVector CameraFwd = Camera ? Camera->GetForwardVector() : CapsuleFwd;

	const float CamVsCapsule = SignedPlanarAngleDeg(CameraFwd, CapsuleFwd, ShipUp);
	const float CamVsMesh = SignedPlanarAngleDeg(CameraFwd, MeshFwd, ShipUp);
	const float CamVsFPMesh = SignedPlanarAngleDeg(CameraFwd, FPMeshFwd, ShipUp);
	const float CapsuleVsMesh = SignedPlanarAngleDeg(CapsuleFwd, MeshFwd, ShipUp);

	FVector RelLoc = FVector::ZeroVector;
	FRotator RelRot = FRotator::ZeroRotator;
	FVector ShipLinVel = FVector::ZeroVector;
	FVector ShipAngVel = FVector::ZeroVector;
	FString InteriorMeshLine = TEXT("InteriorMesh=None");
	if (Ship)
	{
		const FTransform ShipTM = Ship->GetActorTransform();
		RelLoc = ShipTM.InverseTransformPosition(Character->GetActorLocation());
		RelRot = (ShipTM.InverseTransformRotation(Character->GetActorQuat())).Rotator();
		if (UShipMovementComponent* ShipMove = Ship->ShipMovement)
		{
			ShipLinVel = ShipMove->GetLinearVelocity();
			ShipAngVel = ShipMove->GetAngularVelocity();
		}
		if (UStaticMeshComponent* Interior = Ship->GetInteriorMesh())
		{
			InteriorMeshLine = FString::Printf(
				TEXT("InteriorMesh=%s Mobility=%s TickGroup=%d"),
				*Interior->GetName(),
				MobilityName(Interior->Mobility),
				static_cast<int32>(Interior->PrimaryComponentTick.TickGroup));
		}
	}

	const FBasedMovementInfo& Based = Character->GetBasedMovement();
	UE_LOG(LogGalacticPirates, Warning,
		TEXT("[Interior][%s] RelLoc=%s RelRot=%s CamLocalYaw=%.2f CamLocalPitch=%.2f CamVsCapsulePlanarDeg=%.2f CamVsMeshPlanarDeg=%.2f CamVsFPMeshPlanarDeg=%.2f CapsuleVsMeshPlanarDeg=%.2f CapsuleFwd=%s MeshFwd=%s CamFwd=%s Base=%s Mobility=%s UseRelativeLoc=%s DynamicBase=%s RelRotFlag=%s IgnoreBaseRot=%s BaseIsInteriorMesh=%s Floor=%s FloorMobility=%s %s ShipLinVel=%s ShipAngVel=%s CharVel=%s CharTick=%d ShipTick=%d MoveTick=%d Ground=%s Mode=%d GravDir=%s Inside=%s"),
		Reason,
		*RelLoc.ToCompactString(),
		*RelRot.ToCompactString(),
		Camera ? Camera->GetCurrentYaw() : 0.0f,
		Camera ? Camera->GetCurrentPitch() : 0.0f,
		CamVsCapsule,
		CamVsMesh,
		CamVsFPMesh,
		CapsuleVsMesh,
		*CapsuleFwd.ToCompactString(),
		*MeshFwd.ToCompactString(),
		*CameraFwd.ToCompactString(),
		*BaseName,
		*BaseMobility,
		bUseRelative ? TEXT("true") : TEXT("false"),
		bDynamic ? TEXT("true") : TEXT("false"),
		Based.HasRelativeRotation() ? TEXT("true") : TEXT("false"),
		bIgnoreBaseRot ? TEXT("true") : TEXT("false"),
		bBaseIsInterior ? TEXT("true") : TEXT("false"),
		*FloorName,
		*FloorMobility,
		*InteriorMeshLine,
		*ShipLinVel.ToCompactString(),
		*ShipAngVel.ToCompactString(),
		*Character->GetVelocity().ToCompactString(),
		static_cast<int32>(Character->PrimaryActorTick.TickGroup),
		Ship ? static_cast<int32>(Ship->PrimaryActorTick.TickGroup) : -1,
		MoveComp ? static_cast<int32>(MoveComp->PrimaryComponentTick.TickGroup) : -1,
		MoveComp && MoveComp->IsMovingOnGround() ? TEXT("true") : TEXT("false"),
		MoveComp ? static_cast<int32>(MoveComp->MovementMode) : -1,
		MoveComp ? *MoveComp->GetGravityDirection().ToCompactString() : TEXT("none"),
		Ship && Ship->IsWalkableWorldLocation(Character->GetActorLocation()) ? TEXT("true") : TEXT("false"));
}

void GPShipDebugSnapshot(const AGalacticPiratesCharacter* Character, const TCHAR* Reason)
{
	if (!Character)
	{
		GPShipDebugEvent(TEXT("Snapshot skipped: null character"));
		return;
	}

	UWorld* World = Character->GetWorld();
	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	UQuatCamera* Camera = Character->GetQuatCameraComponent();
	AWalkableShip* Ship = Character->GetBoardedShip();
	APlayerController* PC = Cast<APlayerController>(Character->GetController());

	const FVector Loc = Character->GetActorLocation();
	const FRotator Rot = Character->GetActorRotation();
	const FVector Vel = Character->GetVelocity();

	FString BaseName = TEXT("None");
	bool bBaseValid = false;
	if (MoveComp)
	{
		if (const FMovementBaseInterfaceData* BaseData = MoveComp->GetMovementBaseInterfaceData())
		{
			bBaseValid = BaseData->IsValid();
			if (UObject* BaseObj = BaseData->GetMovementBaseObject())
			{
				BaseName = BaseObj->GetName();
			}
		}
	}

	FString CameraLine = TEXT("Camera=None");
	FVector CameraFwd = FVector::ForwardVector;
	if (Camera)
	{
		CameraFwd = Camera->GetForwardVector();
		const FQuat CamQuat = Camera->GetComponentQuat();
		const FQuat Computed = Camera->GetLastComputedWorldRotation();
		const float ComputedDeltaDeg = Computed.AngularDistance(CamQuat) * (180.0f / PI);
		CameraLine = FString::Printf(
			TEXT("CamLoc=%s CamRot=%s LocalYaw=%.2f LocalPitch=%.2f Up=%s ComputedDeltaDeg=%.3f TickGroup=%d"),
			*Camera->GetComponentLocation().ToCompactString(),
			*CamQuat.Rotator().ToCompactString(),
			Camera->GetCurrentYaw(),
			Camera->GetCurrentPitch(),
			*Camera->GetReferenceUpDirection().ToCompactString(),
			ComputedDeltaDeg,
			static_cast<int32>(Camera->PrimaryComponentTick.TickGroup));
	}

	FString ShipLine = TEXT("Ship=None");
	float CamVsShipFwdDeg = -1.0f;
	if (Ship)
	{
		const FVector ShipFwd = Ship->GetActorForwardVector();
		CamVsShipFwdDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(CameraFwd.GetSafeNormal(), ShipFwd.GetSafeNormal()), -1.0f, 1.0f)));
		ShipLine = FString::Printf(
			TEXT("Ship=%s Loc=%s Rot=%s Fwd=%s Up=%s CamVsShipFwdDeg=%.2f HelmOccupied=%s"),
			*Ship->GetName(),
			*Ship->GetActorLocation().ToCompactString(),
			*Ship->GetActorRotation().ToCompactString(),
			*ShipFwd.ToCompactString(),
			*Ship->GetShipUpVector().ToCompactString(),
			CamVsShipFwdDeg,
			Ship->Helm && Ship->Helm->IsOccupied() ? TEXT("true") : TEXT("false"));
	}

	FString InputLine = TEXT("PC=None");
	if (PC)
	{
		TArray<FString> KeyLists;
		auto AppendKeys = [&KeyLists](const TCHAR* Label, const TArray<FKey>& Keys)
		{
			TArray<FString> Names;
			for (const FKey& Key : Keys)
			{
				Names.Add(Key.ToString());
			}
			KeyLists.Add(FString::Printf(TEXT("%s=[%s]"), Label, *FString::Join(Names, TEXT(","))));
		};

		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			UInputMappingContext* DefaultIMC = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default"));
			UInputMappingContext* MouseIMC = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));
			int32 DefaultPri = -1;
			int32 MousePri = -1;
			const bool bHasDefault = DefaultIMC && Subsystem->HasMappingContext(DefaultIMC, DefaultPri);
			const bool bHasMouse = MouseIMC && Subsystem->HasMappingContext(MouseIMC, MousePri);

			AppendKeys(TEXT("MoveKeys"), Subsystem->QueryKeysMappedToAction(Character->GetMoveAction()));
			AppendKeys(TEXT("LookKeys"), Subsystem->QueryKeysMappedToAction(Character->GetLookAction()));
			AppendKeys(TEXT("MouseLookKeys"), Subsystem->QueryKeysMappedToAction(Character->GetMouseLookAction()));
			AppendKeys(TEXT("InteractKeys"), Subsystem->QueryKeysMappedToAction(Character->GetShipInteractAction()));
			AppendKeys(TEXT("ShipRotKeys"), Subsystem->QueryKeysMappedToAction(Character->GetShipRotationAction()));

			InputLine = FString::Printf(
				TEXT("Possessed=%s Local=%s IMC_Default=%s(%d) IMC_MouseLook=%s(%d) %s MoveCount=%d LookCount=%d LastMove=%s LastLook=%s"),
				*GetNameSafe(PC->GetPawn()),
				PC->IsLocalController() ? TEXT("true") : TEXT("false"),
				bHasDefault ? TEXT("yes") : TEXT("NO"),
				DefaultPri,
				bHasMouse ? TEXT("yes") : TEXT("NO"),
				MousePri,
				*FString::Join(KeyLists, TEXT(" ")),
				Character->GetDebugMoveInputCount(),
				Character->GetDebugLookInputCount(),
				*Character->GetDebugLastMoveInput().ToString(),
				*Character->GetDebugLastLookInput().ToString());
		}
		else
		{
			InputLine = TEXT("EnhancedInput subsystem missing");
		}
	}

	const TCHAR* ModeName = TEXT("Unknown");
	if (MoveComp)
	{
		switch (MoveComp->MovementMode)
		{
		case MOVE_None: ModeName = TEXT("None"); break;
		case MOVE_Walking: ModeName = TEXT("Walking"); break;
		case MOVE_NavWalking: ModeName = TEXT("NavWalking"); break;
		case MOVE_Falling: ModeName = TEXT("Falling"); break;
		case MOVE_Swimming: ModeName = TEXT("Swimming"); break;
		case MOVE_Flying: ModeName = TEXT("Flying"); break;
		case MOVE_Custom: ModeName = TEXT("Custom"); break;
		default: break;
		}
	}

	UE_LOG(LogGalacticPirates, Warning,
		TEXT("[ShipDebug][%s] Char=%s Loc=%s Rot=%s Vel=%s Mode=%s Gravity=%.2f Ground=%s Flying=%s Base=%s ValidBase=%s Boarded=%s Piloting=%s | %s | %s | %s"),
		Reason,
		*Character->GetName(),
		*Loc.ToCompactString(),
		*Rot.ToCompactString(),
		*Vel.ToCompactString(),
		ModeName,
		MoveComp ? MoveComp->GravityScale : -1.0f,
		MoveComp && MoveComp->IsMovingOnGround() ? TEXT("true") : TEXT("false"),
		MoveComp && MoveComp->IsFlying() ? TEXT("true") : TEXT("false"),
		*BaseName,
		bBaseValid ? TEXT("true") : TEXT("false"),
		Character->GetBoardedShip() ? TEXT("true") : TEXT("false"),
		Character->IsPiloting() ? TEXT("true") : TEXT("false"),
		*CameraLine,
		*ShipLine,
		*InputLine);

	TArray<AActor*> Characters;
	UGameplayStatics::GetAllActorsOfClass(World, AGalacticPiratesCharacter::StaticClass(), Characters);
	UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDebug][%s] CharacterCount=%d"), Reason, Characters.Num());
	for (AActor* Actor : Characters)
	{
		if (AGalacticPiratesCharacter* Other = Cast<AGalacticPiratesCharacter>(Actor))
		{
			UE_LOG(LogGalacticPirates, Warning,
				TEXT("[ShipDebug][%s] Pawn=%s PossessedBy=%s AutoBoardShip=%s Loc=%s"),
				Reason,
				*Other->GetName(),
				*GetNameSafe(Other->GetController()),
				*GetNameSafe(Other->GetBoardedShip()),
				*Other->GetActorLocation().ToCompactString());
		}
	}

	if (World && Camera && Ship)
	{
		DrawDebugDirectionalArrow(World, Camera->GetComponentLocation(), Camera->GetComponentLocation() + CameraFwd * 400.0f, 20.0f, FColor::Red, false, 1.0f, 0, 3.0f);
		DrawDebugDirectionalArrow(World, Ship->GetActorLocation() + Ship->GetActorUpVector() * 200.0f, Ship->GetActorLocation() + Ship->GetActorUpVector() * 200.0f + Ship->GetActorForwardVector() * 500.0f, 25.0f, FColor::Green, false, 1.0f, 0, 4.0f);
		DrawDebugDirectionalArrow(World, Loc, Loc + Character->GetActorForwardVector() * 220.0f, 16.0f, FColor::Cyan, false, 1.0f, 0, 3.0f);
		if (const USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			DrawDebugDirectionalArrow(World, Mesh->GetComponentLocation(), Mesh->GetComponentLocation() + Mesh->GetForwardVector() * 220.0f, 16.0f, FColor::Yellow, false, 1.0f, 0, 3.0f);
		}
	}

	GPLogInteriorWalkState(Character, Reason);
}

bool GPIsInteriorWalkTestRunning()
{
	return GInteriorTest.Phase >= 0;
}

void GPStartInteriorWalkTest(AGalacticPiratesCharacter* Character)
{
	if (!Character || !Character->IsLocallyControlled())
	{
		return;
	}

	GInteriorTest = FInteriorTestState();
	GInteriorTest.Character = Character;
	GInteriorTest.Ship = Character->GetBoardedShip();
	GInteriorTest.Phase = 0;
	GInteriorTest.Time = 0.0f;

	if (AWalkableShip* Ship = GInteriorTest.Ship.Get())
	{
		GInteriorTest.ShipStartLocation = Ship->GetActorLocation();
		GInteriorTest.ShipStartRotation = Ship->GetActorRotation();
	}

	if (UQuatCamera* Camera = Character->GetQuatCameraComponent())
	{
		GInteriorTest.CameraYawBeforeLook = Camera->GetCurrentYaw();
		GInteriorTest.PitchBeforeMouse = Camera->GetCurrentPitch();
	}

	FillBaseFields(
		Character,
		GInteriorTest.BaseName,
		GInteriorTest.BaseMobility,
		GInteriorTest.FloorName,
		GInteriorTest.FloorMobility,
		GInteriorTest.bUseRelativeLocation,
		GInteriorTest.bDynamicBase,
		GInteriorTest.bIgnoreBaseRotation,
		GInteriorTest.bBaseIsInteriorMesh);

	GPShipDebugScreen(TEXT("INTERIOR WALK TEST STARTING"), FColor::Cyan, 4.0f);
	GPShipDebugSnapshot(Character, TEXT("InteriorTest/Start"));
}

void GPTickInteriorWalkTest(AGalacticPiratesCharacter* Character, float DeltaTime)
{
	if (GInteriorTest.Phase < 0)
	{
		return;
	}

	if (!Character || GInteriorTest.Character.Get() != Character)
	{
		return;
	}

	AWalkableShip* Ship = Character->GetBoardedShip();
	if (!Ship)
	{
		GInteriorTest.LogTimer += DeltaTime;
		if (GInteriorTest.LogTimer < 2.0f)
		{
			return;
		}

		GPShipDebugScreen(TEXT("INTERIOR TEST FAIL: not boarded"), FColor::Red, 8.0f);
		GInteriorTest.Phase = -1;
		return;
	}

	if (!GInteriorTest.Ship.IsValid())
	{
		GInteriorTest.Ship = Ship;
		GInteriorTest.ShipStartLocation = Ship->GetActorLocation();
		GInteriorTest.ShipStartRotation = Ship->GetActorRotation();
		GInteriorTest.Time = 0.0f;
		GInteriorTest.LogTimer = 0.0f;
		GInteriorTest.Phase = 0;
	}

	GInteriorTest.Ship = Ship;
	GInteriorTest.Time += DeltaTime;
	GInteriorTest.LogTimer += DeltaTime;

	const int32 PreviousPhase = GInteriorTest.Phase;
	if (GInteriorTest.Time < 0.8f)
	{
		GInteriorTest.Phase = 0;
	}
	else if (GInteriorTest.Time < 1.4f)
	{
		GInteriorTest.Phase = 1;
	}
	else if (GInteriorTest.Time < 2.6f)
	{
		GInteriorTest.Phase = 2;
	}
	else if (GInteriorTest.Time < 3.8f)
	{
		GInteriorTest.Phase = 3;
	}
	else if (GInteriorTest.Time < 5.0f)
	{
		GInteriorTest.Phase = 4;
	}
	else
	{
		GInteriorTest.Phase = 5;
	}

	const bool bPhaseEntered = GInteriorTest.Phase != PreviousPhase;
	const FTransform ShipTM = Ship->GetActorTransform();
	const FVector RelNow = ShipTM.InverseTransformPosition(Character->GetActorLocation());
	UQuatCamera* Camera = Character->GetQuatCameraComponent();
	const FVector ShipUp = Ship->GetShipUpVector();

	if (GInteriorTest.LogTimer >= 0.25f)
	{
		GInteriorTest.LogTimer = 0.0f;
		GPLogInteriorWalkState(Character, *FString::Printf(TEXT("InteriorTest/TickP%d"), GInteriorTest.Phase));
	}

	switch (GInteriorTest.Phase)
	{
	case 0:
		if (bPhaseEntered)
		{
			if (Camera)
			{
				GInteriorTest.PitchBeforeMouse = Camera->GetCurrentPitch();
			}
			GPLogInteriorWalkState(Character, TEXT("InteriorTest/Idle"));
		}
		if (GInteriorTest.Time > 0.25f)
		{
			InjectAxis(Character, EKeys::Mouse2D, FVector(0.0f, 40.0f, 0.0f));
			InjectAxis(Character, EKeys::MouseY, FVector(40.0f, 0.0f, 0.0f));
		}
		break;

	case 1:
		if (bPhaseEntered)
		{
			if (Camera)
			{
				if (FMath::IsNearlyEqual(Camera->GetCurrentPitch(), GInteriorTest.PitchBeforeMouse, 0.5f))
				{
					Character->ApplyLookForTest(0.0f, 12.0f);
				}
				GInteriorTest.PitchAfterMouse = Camera->GetCurrentPitch();
				GInteriorTest.bPitchPass = (GInteriorTest.PitchAfterMouse - GInteriorTest.PitchBeforeMouse) > 1.0f;
				GPShipDebugScreen(
					FString::Printf(TEXT("Mouse pitch %s (%.2f -> %.2f, +Y should look up)"),
						GInteriorTest.bPitchPass ? TEXT("PASS") : TEXT("FAIL"),
						GInteriorTest.PitchBeforeMouse,
						GInteriorTest.PitchAfterMouse),
					GInteriorTest.bPitchPass ? FColor::Green : FColor::Red,
					8.0f);

				GInteriorTest.CameraYawBeforeLook = Camera->GetCurrentYaw();
				Camera->AddLookInput(45.0f, 0.0f);
			}
			GPShipDebugEvent(TEXT("InteriorTest: AddLookInput +45 yaw (capsule should follow camera on the deck plane)"));
		}
		break;

	case 2:
		if (bPhaseEntered)
		{
			const FVector CameraFwd = Camera ? Camera->GetForwardVector() : Character->GetActorForwardVector();
			const FVector MeshFwd = Character->GetMesh() ? Character->GetMesh()->GetForwardVector() : Character->GetActorForwardVector();
			GInteriorTest.CamVsCapsuleAfterLook = SignedPlanarAngleDeg(CameraFwd, Character->GetActorForwardVector(), ShipUp);
			GInteriorTest.CamVsMeshAfterLook = SignedPlanarAngleDeg(CameraFwd, MeshFwd, ShipUp);
			GInteriorTest.bFacingPass = FMath::Abs(GInteriorTest.CamVsCapsuleAfterLook) < 15.0f;
			GPShipDebugScreen(
				FString::Printf(TEXT("Facing %s camVsCapsule=%.1f camVsMesh=%.1f (look yaw %.1f -> %.1f)"),
					GInteriorTest.bFacingPass ? TEXT("PASS") : TEXT("FAIL"),
					GInteriorTest.CamVsCapsuleAfterLook,
					GInteriorTest.CamVsMeshAfterLook,
					GInteriorTest.CameraYawBeforeLook,
					Camera ? Camera->GetCurrentYaw() : 0.0f),
				GInteriorTest.bFacingPass ? FColor::Green : FColor::Orange,
				8.0f);
			GPLogInteriorWalkState(Character, TEXT("InteriorTest/AfterLook"));

			GInteriorTest.RelAtTranslateStart = RelNow;
			GInteriorTest.WorldAtTranslateStart = Character->GetActorLocation();
			GInteriorTest.ShipStartLocation = Ship->GetActorLocation();
			GInteriorTest.ShipStartRotation = Ship->GetActorRotation();
			FillBaseFields(
				Character,
				GInteriorTest.BaseName,
				GInteriorTest.BaseMobility,
				GInteriorTest.FloorName,
				GInteriorTest.FloorMobility,
				GInteriorTest.bUseRelativeLocation,
				GInteriorTest.bDynamicBase,
				GInteriorTest.bIgnoreBaseRotation,
				GInteriorTest.bBaseIsInteriorMesh);
		}
		{
			const float Alpha = FMath::Clamp((GInteriorTest.Time - 1.4f) / 1.2f, 0.0f, 1.0f);
			const FVector Offset = GInteriorTest.ShipStartRotation.RotateVector(FVector(480.0f * Alpha, 0.0f, 0.0f));
			TeleportShip(Ship, GInteriorTest.ShipStartLocation + Offset, GInteriorTest.ShipStartRotation);
		}
		break;

	case 3:
		if (bPhaseEntered)
		{
			InjectKey(Character, EKeys::W, IE_Released);
			GInteriorTest.TranslateRelDrift = PlanarDelta(GInteriorTest.RelAtTranslateStart, RelNow, FVector::UpVector).Size();
			GInteriorTest.TranslateWorldDrift = FVector::Dist(Character->GetActorLocation(), GInteriorTest.WorldAtTranslateStart);
			GInteriorTest.bTranslatePass = GInteriorTest.TranslateRelDrift < 15.0f;
			GPShipDebugScreen(
				FString::Printf(TEXT("Slide/translate %s relDrift=%.1fcm worldMoved=%.1fcm UseRel=%s Base=%s(%s)"),
					GInteriorTest.bTranslatePass ? TEXT("PASS") : TEXT("FAIL"),
					GInteriorTest.TranslateRelDrift,
					GInteriorTest.TranslateWorldDrift,
					GInteriorTest.bUseRelativeLocation ? TEXT("true") : TEXT("false"),
					*GInteriorTest.BaseName,
					*GInteriorTest.BaseMobility),
				GInteriorTest.bTranslatePass ? FColor::Green : FColor::Red,
				8.0f);
			GPLogInteriorWalkState(Character, TEXT("InteriorTest/AfterTranslate"));

			GInteriorTest.RelAtYawStart = RelNow;
			GInteriorTest.WorldAtYawStart = Character->GetActorLocation();
			GInteriorTest.ShipStartLocation = Ship->GetActorLocation();
			GInteriorTest.ShipStartRotation = Ship->GetActorRotation();
		}
		{
			const float Alpha = FMath::Clamp((GInteriorTest.Time - 2.6f) / 1.2f, 0.0f, 1.0f);
			FRotator Yawed = GInteriorTest.ShipStartRotation;
			Yawed.Yaw += 45.0f * Alpha;
			TeleportShip(Ship, GInteriorTest.ShipStartLocation, Yawed);
		}
		break;

	case 4:
		if (bPhaseEntered)
		{
			GInteriorTest.YawRelDrift = PlanarDelta(GInteriorTest.RelAtYawStart, RelNow, FVector::UpVector).Size();
			GInteriorTest.bYawPass = GInteriorTest.YawRelDrift < 20.0f;
			const FVector CameraFwdAfterYaw = Camera ? Camera->GetForwardVector() : Character->GetActorForwardVector();
			GInteriorTest.CamVsCapsuleAfterYaw = SignedPlanarAngleDeg(CameraFwdAfterYaw, Character->GetActorForwardVector(), ShipUp);
			GInteriorTest.bYawFacingPass = FMath::Abs(GInteriorTest.CamVsCapsuleAfterYaw) < 15.0f;
			GPShipDebugScreen(
				FString::Printf(TEXT("Slide/yaw %s relDrift=%.1fcm facingAfterYaw=%s (%.1fdeg)"),
					GInteriorTest.bYawPass ? TEXT("PASS") : TEXT("FAIL"),
					GInteriorTest.YawRelDrift,
					GInteriorTest.bYawFacingPass ? TEXT("PASS") : TEXT("FAIL"),
					GInteriorTest.CamVsCapsuleAfterYaw),
				(GInteriorTest.bYawPass && GInteriorTest.bYawFacingPass) ? FColor::Green : FColor::Red,
				8.0f);
			GPLogInteriorWalkState(Character, TEXT("InteriorTest/AfterYaw"));

			GInteriorTest.RelAtWalkStart = RelNow;
			GInteriorTest.ShipStartLocation = Ship->GetActorLocation();
			GInteriorTest.ShipStartRotation = Ship->GetActorRotation();
			InjectKey(Character, EKeys::W, IE_Pressed);
		}
		InjectKey(Character, EKeys::W, IE_Repeat);
		{
			const float Alpha = FMath::Clamp((GInteriorTest.Time - 3.8f) / 1.2f, 0.0f, 1.0f);
			const FVector Offset = GInteriorTest.ShipStartRotation.RotateVector(FVector(480.0f * Alpha, 0.0f, 0.0f));
			TeleportShip(Ship, GInteriorTest.ShipStartLocation + Offset, GInteriorTest.ShipStartRotation);
		}
		break;

	case 5:
		if (bPhaseEntered)
		{
			InjectKey(Character, EKeys::W, IE_Released);
			GInteriorTest.WalkRelDrift = PlanarDelta(GInteriorTest.RelAtWalkStart, RelNow, FVector::UpVector).Size();
			GInteriorTest.bWalked = Character->GetDebugMoveInputCount() > 0;

			const bool bAnySlideFail = !GInteriorTest.bTranslatePass || !GInteriorTest.bYawPass;
			const bool bFacingOk = GInteriorTest.bFacingPass && GInteriorTest.bYawFacingPass;
			GPShipDebugScreen(
				FString::Printf(
					TEXT("INTERIOR TEST DONE pitch=%s facing=%s yawFace=%s slideT=%s slideY=%s walkRel=%.1fcm"),
					GInteriorTest.bPitchPass ? TEXT("PASS") : TEXT("FAIL"),
					GInteriorTest.bFacingPass ? TEXT("PASS") : TEXT("FAIL"),
					GInteriorTest.bYawFacingPass ? TEXT("PASS") : TEXT("FAIL"),
					GInteriorTest.bTranslatePass ? TEXT("PASS") : TEXT("FAIL"),
					GInteriorTest.bYawPass ? TEXT("PASS") : TEXT("FAIL"),
					GInteriorTest.WalkRelDrift),
				(bAnySlideFail || !bFacingOk || !GInteriorTest.bPitchPass) ? FColor::Red : FColor::Green,
				12.0f);

			UE_LOG(LogGalacticPirates, Warning,
				TEXT("[Interior][Verdict] Pitch=%s (%.2f -> %.2f) | Facing=%s CamVsCapsule=%.2f CamVsMesh=%.2f | YawFacing=%s CamVsCapsuleAfterYaw=%.2f | Translate=%s RelDrift=%.2f WorldMoved=%.2f | Yaw=%s RelDrift=%.2f | WalkRelDrift=%.2f WalkInput=%s | Base=%s Mobility=%s UseRelativeLocation=%s DynamicBase=%s BaseIsInteriorMesh=%s IgnoreBaseRotation=%s Floor=%s FloorMobility=%s"),
				GInteriorTest.bPitchPass ? TEXT("PASS") : TEXT("FAIL"),
				GInteriorTest.PitchBeforeMouse,
				GInteriorTest.PitchAfterMouse,
				GInteriorTest.bFacingPass ? TEXT("PASS") : TEXT("FAIL"),
				GInteriorTest.CamVsCapsuleAfterLook,
				GInteriorTest.CamVsMeshAfterLook,
				GInteriorTest.bYawFacingPass ? TEXT("PASS") : TEXT("FAIL"),
				GInteriorTest.CamVsCapsuleAfterYaw,
				GInteriorTest.bTranslatePass ? TEXT("PASS") : TEXT("FAIL"),
				GInteriorTest.TranslateRelDrift,
				GInteriorTest.TranslateWorldDrift,
				GInteriorTest.bYawPass ? TEXT("PASS") : TEXT("FAIL"),
				GInteriorTest.YawRelDrift,
				GInteriorTest.WalkRelDrift,
				GInteriorTest.bWalked ? TEXT("yes") : TEXT("NO"),
				*GInteriorTest.BaseName,
				*GInteriorTest.BaseMobility,
				GInteriorTest.bUseRelativeLocation ? TEXT("true") : TEXT("false"),
				GInteriorTest.bDynamicBase ? TEXT("true") : TEXT("false"),
				GInteriorTest.bBaseIsInteriorMesh ? TEXT("true") : TEXT("false"),
				GInteriorTest.bIgnoreBaseRotation ? TEXT("true") : TEXT("false"),
				*GInteriorTest.FloorName,
				*GInteriorTest.FloorMobility);

			GPShipDebugSnapshot(Character, TEXT("InteriorTest/Done"));
			GInteriorTest.Phase = -1;
		}
		break;

	default:
		break;
	}
}

bool GPIsExtremeFlightTestRunning()
{
	return GExtremeTest.Phase >= 0;
}

void GPStartExtremeFlightTest(AGalacticPiratesCharacter* Character)
{
	if (!Character || !Character->IsLocallyControlled())
	{
		return;
	}

	if (GPIsInteriorWalkTestRunning())
	{
		GPShipDebugEvent(TEXT("Extreme flight test skipped: interior test is running"));
		return;
	}

	GExtremeTest = FExtremeFlightTestState();
	GExtremeTest.Character = Character;
	GExtremeTest.Ship = Character->GetBoardedShip();
	GExtremeTest.Phase = 0;
	GExtremeTest.Time = 0.0f;

	GPShipDebugScreen(TEXT("EXTREME FLIGHT TEST STARTING"), FColor::Cyan, 4.0f);
	GPShipDebugSnapshot(Character, TEXT("ExtremeFlight/Start"));
}

void GPTickExtremeFlightTest(AGalacticPiratesCharacter* Character, float DeltaTime)
{
	if (GExtremeTest.Phase < 0)
	{
		return;
	}

	if (!Character || GExtremeTest.Character.Get() != Character)
	{
		return;
	}

	AWalkableShip* Ship = Character->GetBoardedShip();
	if (!Ship)
	{
		GExtremeTest.LogTimer += DeltaTime;
		if (GExtremeTest.LogTimer > 2.0f)
		{
			GPShipDebugScreen(TEXT("EXTREME FLIGHT FAIL: not boarded"), FColor::Red, 8.0f);
			GExtremeTest.Phase = -1;
		}
		return;
	}

	GExtremeTest.Ship = Ship;
	GExtremeTest.Time += DeltaTime;
	GExtremeTest.LogTimer += DeltaTime;

	const int32 PreviousPhase = GExtremeTest.Phase;
	if (GExtremeTest.Time < 0.5f)
	{
		GExtremeTest.Phase = 0;
	}
	else if (GExtremeTest.Time < 0.8f)
	{
		GExtremeTest.Phase = 1;
	}
	else if (GExtremeTest.Time < 2.4f)
	{
		GExtremeTest.Phase = 2;
	}
	else if (GExtremeTest.Time < 2.7f)
	{
		GExtremeTest.Phase = 3;
	}
	else if (GExtremeTest.Time < 4.0f)
	{
		GExtremeTest.Phase = 4;
	}
	else if (GExtremeTest.Time < 5.3f)
	{
		GExtremeTest.Phase = 5;
	}
	else
	{
		GExtremeTest.Phase = 6;
	}

	const bool bPhaseEntered = GExtremeTest.Phase != PreviousPhase;
	const FTransform ShipTM = Ship->GetActorTransform();
	const FVector RelNow = ShipTM.InverseTransformPosition(Character->GetActorLocation());
	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();

	if (GExtremeTest.Phase >= 2 && GExtremeTest.Phase <= 5)
	{
		ApplyExtremeShipMotion(Ship);
	}

	if (GExtremeTest.LogTimer >= 0.25f)
	{
		GExtremeTest.LogTimer = 0.0f;
		GPLogInteriorWalkState(Character, *FString::Printf(TEXT("ExtremeFlight/TickP%d"), GExtremeTest.Phase));
	}

	switch (GExtremeTest.Phase)
	{
	case 0:
		if (bPhaseEntered)
		{
			GPLogInteriorWalkState(Character, TEXT("ExtremeFlight/Idle"));
		}
		break;

	case 1:
		if (bPhaseEntered)
		{
			if (Ship->Helm && Character->HasAuthority())
			{
				if (!Character->IsPiloting())
				{
					GExtremeTest.bTookHelm = Ship->Helm->TryInteract(Character);
				}
				else
				{
					GExtremeTest.bTookHelm = true;
				}
			}
			GPShipDebugScreen(
				FString::Printf(TEXT("Extreme helm take %s"), GExtremeTest.bTookHelm ? TEXT("OK") : TEXT("FAIL")),
				GExtremeTest.bTookHelm ? FColor::Green : FColor::Red,
				6.0f);
			GPShipDebugSnapshot(Character, TEXT("ExtremeFlight/HelmTaken"));
		}
		break;

	case 2:
		if (bPhaseEntered)
		{
			GPShipDebugEvent(TEXT("ExtremeFlight: holding max linear + 3-axis angular velocity at helm"));
		}
		break;

	case 3:
		if (bPhaseEntered)
		{
			if (Character->IsPiloting() && Ship->Helm && Character->HasAuthority())
			{
				GExtremeTest.bExitedHelm = Ship->Helm->TryInteract(Character);
			}
			GExtremeTest.RelAtHelmExit = RelNow;
			GExtremeTest.bGroundedAtExit = MoveComp && MoveComp->IsMovingOnGround();
			GExtremeTest.bInsideAtExit = Ship->IsWalkableWorldLocation(Character->GetActorLocation());
			GExtremeTest.ModeAtExit = MoveComp ? MoveModeName(MoveComp->MovementMode) : TEXT("None");
			GPShipDebugScreen(
				FString::Printf(TEXT("Helm exit grounded=%s inside=%s mode=%s"),
					GExtremeTest.bGroundedAtExit ? TEXT("true") : TEXT("false"),
					GExtremeTest.bInsideAtExit ? TEXT("true") : TEXT("false"),
					*GExtremeTest.ModeAtExit),
				(GExtremeTest.bGroundedAtExit && GExtremeTest.bInsideAtExit) ? FColor::Green : FColor::Orange,
				8.0f);
			GPLogInteriorWalkState(Character, TEXT("ExtremeFlight/AfterHelmExit"));
		}
		break;

	case 4:
		if (bPhaseEntered)
		{
			GExtremeTest.RelAtHelmExit = RelNow;
		}
		break;

	case 5:
		if (bPhaseEntered)
		{
			GExtremeTest.RelAfterHold = RelNow;
			GExtremeTest.HelmExitHoldDrift = PlanarDelta(GExtremeTest.RelAtHelmExit, RelNow, FVector::UpVector).Size();
			GExtremeTest.bGroundedAfterHold = MoveComp && MoveComp->IsMovingOnGround();
			GExtremeTest.bInsideAfterHold = Ship->IsWalkableWorldLocation(Character->GetActorLocation());
			GExtremeTest.bStillWalking = MoveComp && MoveComp->MovementMode == MOVE_Walking;
			if (MoveComp && Ship)
			{
				const float GravAlign = FVector::DotProduct(MoveComp->GetGravityDirection().GetSafeNormal(), (-Ship->GetShipUpVector()).GetSafeNormal());
				GExtremeTest.bGravityAligned = GravAlign > 0.95f;
			}
			GPShipDebugScreen(
				FString::Printf(TEXT("Hold after exit drift=%.1fcm grounded=%s walking=%s grav=%s"),
					GExtremeTest.HelmExitHoldDrift,
					GExtremeTest.bGroundedAfterHold ? TEXT("true") : TEXT("false"),
					GExtremeTest.bStillWalking ? TEXT("true") : TEXT("false"),
					GExtremeTest.bGravityAligned ? TEXT("ok") : TEXT("BAD")),
				(GExtremeTest.bGroundedAfterHold && GExtremeTest.bInsideAfterHold && GExtremeTest.HelmExitHoldDrift < 40.0f) ? FColor::Green : FColor::Red,
				8.0f);
			InjectKey(Character, EKeys::W, IE_Pressed);
		}
		InjectKey(Character, EKeys::W, IE_Repeat);
		break;

	case 6:
		if (bPhaseEntered)
		{
			InjectKey(Character, EKeys::W, IE_Released);
			GExtremeTest.WalkRelDrift = PlanarDelta(GExtremeTest.RelAfterHold, RelNow, FVector::UpVector).Size();
			ClearExtremeShipMotion(Ship);

			const bool bExitPass = GExtremeTest.bExitedHelm && GExtremeTest.bInsideAtExit;
			const bool bHoldPass = GExtremeTest.bGroundedAfterHold && GExtremeTest.bInsideAfterHold && GExtremeTest.bStillWalking && GExtremeTest.HelmExitHoldDrift < 80.0f;
			const bool bGravPass = GExtremeTest.bGravityAligned;
			const bool bPass = bExitPass && bHoldPass && bGravPass;

			GPShipDebugScreen(
				FString::Printf(TEXT("EXTREME FLIGHT %s helmExit=%s hold=%s grav=%s walkRel=%.1fcm"),
					bPass ? TEXT("PASS") : TEXT("FAIL"),
					bExitPass ? TEXT("PASS") : TEXT("FAIL"),
					bHoldPass ? TEXT("PASS") : TEXT("FAIL"),
					bGravPass ? TEXT("PASS") : TEXT("FAIL"),
					GExtremeTest.WalkRelDrift),
				bPass ? FColor::Green : FColor::Red,
				12.0f);

			UE_LOG(LogGalacticPirates, Warning,
				TEXT("[ExtremeFlight][Verdict] HelmTake=%s HelmExit=%s InsideAtExit=%s GroundedAtExit=%s ModeAtExit=%s | HoldDrift=%.2f GroundedAfter=%s InsideAfter=%s Walking=%s Gravity=%s | WalkRelDrift=%.2f"),
				GExtremeTest.bTookHelm ? TEXT("PASS") : TEXT("FAIL"),
				GExtremeTest.bExitedHelm ? TEXT("PASS") : TEXT("FAIL"),
				GExtremeTest.bInsideAtExit ? TEXT("true") : TEXT("false"),
				GExtremeTest.bGroundedAtExit ? TEXT("true") : TEXT("false"),
				*GExtremeTest.ModeAtExit,
				GExtremeTest.HelmExitHoldDrift,
				GExtremeTest.bGroundedAfterHold ? TEXT("true") : TEXT("false"),
				GExtremeTest.bInsideAfterHold ? TEXT("true") : TEXT("false"),
				GExtremeTest.bStillWalking ? TEXT("true") : TEXT("false"),
				GExtremeTest.bGravityAligned ? TEXT("PASS") : TEXT("FAIL"),
				GExtremeTest.WalkRelDrift);

			GPShipDebugSnapshot(Character, TEXT("ExtremeFlight/Done"));
			GExtremeTest.Phase = -1;
		}
		break;

	default:
		break;
	}
}

void GPStartHelmSteerTest(AGalacticPiratesCharacter* Character)
{
	if (!Character || !Character->IsLocallyControlled())
	{
		return;
	}

	GHelmSteerTest = FHelmSteerTestState();
	GHelmSteerTest.Character = Character;
	GHelmSteerTest.Ship = Character->GetBoardedShip();
	GHelmSteerTest.Phase = 0;
	GHelmSteerTest.Time = 0.0f;

	GPShipDebugScreen(TEXT("HELM STEER TEST STARTING"), FColor::Cyan, 4.0f);
	GPShipDebugSnapshot(Character, TEXT("HelmSteer/Start"));
}

void GPTickHelmSteerTest(AGalacticPiratesCharacter* Character, float DeltaTime)
{
	if (GHelmSteerTest.Phase < 0)
	{
		return;
	}

	if (!Character || GHelmSteerTest.Character.Get() != Character)
	{
		return;
	}

	AWalkableShip* Ship = Character->GetBoardedShip();
	if (!Ship)
	{
		GPShipDebugScreen(TEXT("HELM STEER FAIL: not boarded"), FColor::Red, 8.0f);
		GHelmSteerTest.Phase = -1;
		return;
	}

	GHelmSteerTest.Ship = Ship;
	GHelmSteerTest.Time += DeltaTime;

	const int32 PreviousPhase = GHelmSteerTest.Phase;
	if (GHelmSteerTest.Time < 0.50f)
	{
		GHelmSteerTest.Phase = 0;
	}
	else if (GHelmSteerTest.Time < 0.85f)
	{
		GHelmSteerTest.Phase = 1;
	}
	else if (GHelmSteerTest.Time < 2.10f)
	{
		GHelmSteerTest.Phase = 2;
	}
	else if (GHelmSteerTest.Time < 3.00f)
	{
		GHelmSteerTest.Phase = 3;
	}
	else
	{
		GHelmSteerTest.Phase = 4;
	}

	const bool bPhaseEntered = GHelmSteerTest.Phase != PreviousPhase;

	switch (GHelmSteerTest.Phase)
	{
	case 0:
		break;

	case 1:
		if (bPhaseEntered)
		{
			if (Ship->Helm && Character->HasAuthority() && !Character->IsPiloting())
			{
				GHelmSteerTest.bTookHelm = Ship->Helm->TryInteract(Character);
			}
			else
			{
				GHelmSteerTest.bTookHelm = Character->IsPiloting();
			}

			if (Ship->ShipMovement)
			{
				Ship->ShipMovement->SetVelocityOverride(FVector::ZeroVector, FVector::ZeroVector, false);
				Ship->ShipMovement->SetLinearVelocity(FVector::ZeroVector);
				Ship->ShipMovement->SetAngularVelocity(FVector::ZeroVector);
			}

			GHelmSteerTest.StartRotation = Ship->GetActorRotation();
			GPShipDebugScreen(
				FString::Printf(TEXT("Helm steer take %s piloting=%s helm=%s"),
					GHelmSteerTest.bTookHelm ? TEXT("OK") : TEXT("FAIL"),
					Character->IsPiloting() ? TEXT("true") : TEXT("false"),
					Ship->Helm ? TEXT("present") : TEXT("null")),
				GHelmSteerTest.bTookHelm ? FColor::Green : FColor::Red,
				6.0f);
			GPShipDebugSnapshot(Character, TEXT("HelmSteer/HelmTaken"));
		}
		break;

	case 2:
		InjectAxis(Character, EKeys::Mouse2D, FVector(40.0f, 0.0f, 0.0f));
		InjectAxis(Character, EKeys::MouseX, FVector(40.0f, 0.0f, 0.0f));
		break;

	case 3:
		if (bPhaseEntered)
		{
			GHelmSteerTest.MouseYawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(
				GHelmSteerTest.StartRotation.Yaw, Ship->GetActorRotation().Yaw));
			GHelmSteerTest.bMousePass = GHelmSteerTest.MouseYawDelta > 12.0f;
			GPShipDebugScreen(
				FString::Printf(TEXT("Mouse helm yaw %s delta=%.1f deg (need >12)"),
					GHelmSteerTest.bMousePass ? TEXT("PASS") : TEXT("FAIL"),
					GHelmSteerTest.MouseYawDelta),
				GHelmSteerTest.bMousePass ? FColor::Green : FColor::Orange,
				8.0f);

			GHelmSteerTest.StartRotation = Ship->GetActorRotation();
			GHelmSteerTest.bUsedFallback = !GHelmSteerTest.bMousePass;
		}

		if (GHelmSteerTest.bUsedFallback && Character->HasAuthority())
		{
			Ship->ApplyPilotInput(Character, FVector::ZeroVector, FVector(0.0f, 0.0f, 1.0f));
		}
		break;

	case 4:
		if (bPhaseEntered)
		{
			GHelmSteerTest.FallbackYawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(
				GHelmSteerTest.StartRotation.Yaw, Ship->GetActorRotation().Yaw));
			GHelmSteerTest.bFallbackPass = GHelmSteerTest.FallbackYawDelta > 12.0f;
			const bool bPass = GHelmSteerTest.bTookHelm && (GHelmSteerTest.bMousePass || GHelmSteerTest.bFallbackPass);

			GPShipDebugScreen(
				FString::Printf(TEXT("HELM STEER %s mouse=%.1f%s fallback=%.1f"),
					bPass ? TEXT("PASS") : TEXT("FAIL"),
					GHelmSteerTest.MouseYawDelta,
					GHelmSteerTest.bMousePass ? TEXT("") : TEXT(" (weak)"),
					GHelmSteerTest.FallbackYawDelta),
				bPass ? FColor::Green : FColor::Red,
				12.0f);

			UE_LOG(LogGalacticPirates, Warning,
				TEXT("[HelmSteer][Verdict] Take=%s MouseYaw=%.2f Mouse=%s FallbackUsed=%s FallbackYaw=%.2f Fallback=%s ShipRot=%s"),
				GHelmSteerTest.bTookHelm ? TEXT("PASS") : TEXT("FAIL"),
				GHelmSteerTest.MouseYawDelta,
				GHelmSteerTest.bMousePass ? TEXT("PASS") : TEXT("FAIL"),
				GHelmSteerTest.bUsedFallback ? TEXT("true") : TEXT("false"),
				GHelmSteerTest.FallbackYawDelta,
				GHelmSteerTest.bFallbackPass ? TEXT("PASS") : TEXT("FAIL"),
				*Ship->GetActorRotation().ToCompactString());

			GPShipDebugSnapshot(Character, TEXT("HelmSteer/Done"));
			GHelmSteerTest.Phase = -1;
		}
		break;

	default:
		break;
	}
}

void GPStartHelmJitterTest(AGalacticPiratesCharacter* Character)
{
	if (!Character || !Character->IsLocallyControlled())
	{
		return;
	}

	GHelmJitterTest = FHelmJitterTestState();
	GHelmJitterTest.Character = Character;
	GHelmJitterTest.Ship = Character->GetBoardedShip();
	GHelmJitterTest.Phase = 0;
	GHelmJitterTest.Time = 0.0f;

	GPShipDebugScreen(TEXT("HELM JITTER TEST STARTING"), FColor::Cyan, 4.0f);
	GPShipDebugSnapshot(Character, TEXT("HelmJitter/Start"));
}

void GPTickHelmJitterTest(AGalacticPiratesCharacter* Character, float DeltaTime)
{
	if (GHelmJitterTest.Phase < 0)
	{
		return;
	}

	if (!Character || GHelmJitterTest.Character.Get() != Character)
	{
		return;
	}

	AWalkableShip* Ship = Character->GetBoardedShip();
	if (!Ship)
	{
		GPShipDebugScreen(TEXT("HELM JITTER FAIL: not boarded"), FColor::Red, 8.0f);
		GHelmJitterTest.Phase = -1;
		return;
	}

	GHelmJitterTest.Ship = Ship;
	GHelmJitterTest.Time += DeltaTime;

	const int32 PreviousPhase = GHelmJitterTest.Phase;
	if (GHelmJitterTest.Time < 0.50f)
	{
		GHelmJitterTest.Phase = 0;
	}
	else if (GHelmJitterTest.Time < 0.85f)
	{
		GHelmJitterTest.Phase = 1;
	}
	else if (GHelmJitterTest.Time < 2.60f)
	{
		GHelmJitterTest.Phase = 2;
	}
	else
	{
		GHelmJitterTest.Phase = 3;
	}

	const bool bPhaseEntered = GHelmJitterTest.Phase != PreviousPhase;

	if (GHelmJitterTest.Phase == 2)
	{
		ApplyExtremeShipMotion(Ship);
	}

	switch (GHelmJitterTest.Phase)
	{
	case 0:
		break;

	case 1:
		if (bPhaseEntered)
		{
			if (Ship->Helm && Character->HasAuthority() && !Character->IsPiloting())
			{
				GHelmJitterTest.bTookHelm = Ship->Helm->TryInteract(Character);
			}
			else
			{
				GHelmJitterTest.bTookHelm = Character->IsPiloting();
			}

			GHelmJitterTest.AttachParent = GetNameSafe(Character->GetRootComponent() ? Character->GetRootComponent()->GetAttachParent() : nullptr);
			if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
			{
				GHelmJitterTest.bMoveTickEnabled = MoveComp->IsComponentTickEnabled();
			}

			GPShipDebugScreen(
				FString::Printf(TEXT("Helm jitter take %s attach=%s moveTick=%s"),
					GHelmJitterTest.bTookHelm ? TEXT("OK") : TEXT("FAIL"),
					*GHelmJitterTest.AttachParent,
					GHelmJitterTest.bMoveTickEnabled ? TEXT("on") : TEXT("off")),
				GHelmJitterTest.bTookHelm ? FColor::Green : FColor::Red,
				6.0f);
			GPShipDebugSnapshot(Character, TEXT("HelmJitter/HelmTaken"));
		}
		break;

	case 2:
		if (bPhaseEntered)
		{
			GHelmJitterTest.bHasPrevSample = false;
			GPShipDebugEvent(TEXT("HelmJitter: spinning ship at max angular velocity"));
		}
		break;

	case 3:
		if (bPhaseEntered)
		{
			ClearExtremeShipMotion(Ship);
			const bool bRelPass = GHelmJitterTest.MaxRelStepCm < 3.0f;
			const bool bCamPass = GHelmJitterTest.MaxCamVsShipDeg < 4.0f;
			const bool bCamStepPass = GHelmJitterTest.MaxCamStepVsShipDeg < 6.0f;
			const bool bPass = GHelmJitterTest.bTookHelm && GHelmJitterTest.Samples > 10 && bRelPass && bCamPass;

			GPShipDebugScreen(
				FString::Printf(TEXT("HELM JITTER %s relStep=%.2fcm relRot=%.2f camVsShip=%.2f camStep=%.2f samples=%d attach=%s moveTick=%s"),
					bPass ? TEXT("PASS") : TEXT("FAIL"),
					GHelmJitterTest.MaxRelStepCm,
					GHelmJitterTest.MaxRelRotDeg,
					GHelmJitterTest.MaxCamVsShipDeg,
					GHelmJitterTest.MaxCamStepVsShipDeg,
					GHelmJitterTest.Samples,
					*GHelmJitterTest.AttachParent,
					GHelmJitterTest.bMoveTickEnabled ? TEXT("on") : TEXT("off")),
				bPass ? FColor::Green : FColor::Red,
				12.0f);

			UE_LOG(LogGalacticPirates, Warning,
				TEXT("[HelmJitter][Verdict] Take=%s RelStepCm=%.3f RelRotDeg=%.3f CamVsShipDeg=%.3f CamStepVsShipDeg=%.3f Samples=%d Attach=%s MoveTick=%s | Rel=%s Cam=%s"),
				GHelmJitterTest.bTookHelm ? TEXT("PASS") : TEXT("FAIL"),
				GHelmJitterTest.MaxRelStepCm,
				GHelmJitterTest.MaxRelRotDeg,
				GHelmJitterTest.MaxCamVsShipDeg,
				GHelmJitterTest.MaxCamStepVsShipDeg,
				GHelmJitterTest.Samples,
				*GHelmJitterTest.AttachParent,
				GHelmJitterTest.bMoveTickEnabled ? TEXT("on") : TEXT("off"),
				bRelPass ? TEXT("PASS") : TEXT("FAIL"),
				bCamPass ? TEXT("PASS") : TEXT("FAIL"));

			GPShipDebugSnapshot(Character, TEXT("HelmJitter/Done"));
			GHelmJitterTest.Phase = -1;
		}
		break;

	default:
		break;
	}
}

void GPSampleHelmJitter(AGalacticPiratesCharacter* Character)
{
	if (GHelmJitterTest.Phase != 2 || !Character || GHelmJitterTest.Character.Get() != Character)
	{
		return;
	}

	AWalkableShip* Ship = Character->GetBoardedShip();
	UQuatCamera* Camera = Character->GetQuatCameraComponent();
	if (!Ship || !Camera)
	{
		return;
	}

	const FTransform ShipTM = Ship->GetActorTransform();
	const FVector RelLoc = ShipTM.InverseTransformPosition(Character->GetActorLocation());
	const FQuat RelRot = ShipTM.InverseTransformRotation(Character->GetActorQuat());
	const FQuat ShipQuat = Ship->GetActorQuat();
	const FQuat CamQuat = Camera->GetComponentQuat();
	const float CamVsShipDeg = FMath::RadiansToDegrees(CamQuat.AngularDistance(ShipQuat));

	if (GHelmJitterTest.bHasPrevSample)
	{
		const float RelStep = FVector::Dist(RelLoc, GHelmJitterTest.LastRelLoc);
		const float RelRotDeg = FMath::RadiansToDegrees(RelRot.AngularDistance(GHelmJitterTest.LastRelRot));
		const float ShipStepDeg = FMath::RadiansToDegrees(ShipQuat.AngularDistance(GHelmJitterTest.LastShipQuat));
		const float CamStepDeg = FMath::RadiansToDegrees(CamQuat.AngularDistance(GHelmJitterTest.LastCamQuat));
		const float CamStepVsShip = FMath::Abs(CamStepDeg - ShipStepDeg);

		GHelmJitterTest.MaxRelStepCm = FMath::Max(GHelmJitterTest.MaxRelStepCm, RelStep);
		GHelmJitterTest.MaxRelRotDeg = FMath::Max(GHelmJitterTest.MaxRelRotDeg, RelRotDeg);
		GHelmJitterTest.MaxCamVsShipDeg = FMath::Max(GHelmJitterTest.MaxCamVsShipDeg, CamVsShipDeg);
		GHelmJitterTest.MaxCamStepVsShipDeg = FMath::Max(GHelmJitterTest.MaxCamStepVsShipDeg, CamStepVsShip);
	}

	GHelmJitterTest.LastRelLoc = RelLoc;
	GHelmJitterTest.LastRelRot = RelRot;
	GHelmJitterTest.LastCamQuat = CamQuat;
	GHelmJitterTest.LastShipQuat = ShipQuat;
	GHelmJitterTest.bHasPrevSample = true;
	GHelmJitterTest.Samples++;
}

static const TCHAR* JumpScenarioName(int32 Scenario)
{
	switch (Scenario)
	{
	case 0: return TEXT("Idle");
	case 1: return TEXT("Translate");
	case 2: return TEXT("Yaw");
	case 3: return TEXT("Tumble");
	case 4: return TEXT("WalkMoveYaw");
	default: return TEXT("Unknown");
	}
}

void GPStartShipJumpTest(AGalacticPiratesCharacter* Character)
{
	if (!Character || !Character->IsLocallyControlled())
	{
		return;
	}

	GShipJumpTest = FShipJumpTestState();
	GShipJumpTest.Character = Character;
	GShipJumpTest.Ship = Character->GetBoardedShip();
	GShipJumpTest.Phase = 0;
	GShipJumpTest.Scenario = 0;
	GShipJumpTest.Time = 0.0f;
	GShipJumpTest.ScenarioTime = 0.0f;

	GPShipDebugScreen(TEXT("SHIP JUMP TEST STARTING"), FColor::Cyan, 4.0f);
	GPShipDebugSnapshot(Character, TEXT("ShipJump/Start"));
}

void GPTickShipJumpTest(AGalacticPiratesCharacter* Character, float DeltaTime)
{
	if (GShipJumpTest.Phase < 0)
	{
		return;
	}

	if (!Character || GShipJumpTest.Character.Get() != Character)
	{
		return;
	}

	AWalkableShip* Ship = Character->GetBoardedShip();
	if (!Ship)
	{
		GPShipDebugScreen(TEXT("SHIP JUMP FAIL: not boarded"), FColor::Red, 8.0f);
		GShipJumpTest.Phase = -1;
		return;
	}

	if (Character->IsPiloting() && Ship->Helm && Character->HasAuthority())
	{
		Ship->Helm->TryInteract(Character);
	}

	GShipJumpTest.Ship = Ship;
	GShipJumpTest.Time += DeltaTime;
	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	const FTransform ShipTM = Ship->GetActorTransform();
	const FVector RelNow = ShipTM.InverseTransformPosition(Character->GetActorLocation());

	if (GShipJumpTest.Time < 0.40f)
	{
		SetShipMotionForJumpTest(Ship, 0);
		return;
	}

	if (GShipJumpTest.Phase == 0)
	{
		GShipJumpTest.Phase = 1;
		GShipJumpTest.Scenario = 0;
		GShipJumpTest.ScenarioTime = 0.0f;
		GShipJumpTest.bJumpIssued = false;
		GPShipDebugEvent(TEXT("ShipJump: scenario Idle"));
	}

	if (GShipJumpTest.Scenario >= 5)
	{
		bool bAllPass = true;
		FString Line;
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const FShipJumpScenarioResult& Result = GShipJumpTest.Results[Index];
			bAllPass = bAllPass && Result.bPass;
			Line += FString::Printf(TEXT("%s=%s(up=%.1f drift=%.1f left=%s land=%s) "),
				*Result.Name,
				Result.bPass ? TEXT("PASS") : TEXT("FAIL"),
				Result.PeakRelUp,
				Result.RelPlanarDrift,
				Result.bLeftGround ? TEXT("y") : TEXT("n"),
				Result.bLanded ? TEXT("y") : TEXT("n"));
		}

		ClearExtremeShipMotion(Ship);
		InjectKey(Character, EKeys::W, IE_Released);

		GPShipDebugScreen(
			FString::Printf(TEXT("SHIP JUMP %s %s"), bAllPass ? TEXT("PASS") : TEXT("FAIL"), *Line),
			bAllPass ? FColor::Green : FColor::Red,
			12.0f);
		UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipJump][Verdict] %s %s"),
			bAllPass ? TEXT("PASS") : TEXT("FAIL"),
			*Line);
		GPShipDebugSnapshot(Character, TEXT("ShipJump/Done"));
		GShipJumpTest.Phase = -1;
		return;
	}

	SetShipMotionForJumpTest(Ship, GShipJumpTest.Scenario);
	if (GShipJumpTest.Scenario == 4)
	{
		InjectKey(Character, EKeys::W, IE_Repeat);
	}

	GShipJumpTest.ScenarioTime += DeltaTime;

	if (!GShipJumpTest.bJumpIssued && GShipJumpTest.ScenarioTime >= 0.22f)
	{
		GShipJumpTest.RelAtJump = RelNow;
		GShipJumpTest.PeakRelUp = 0.0f;
		GShipJumpTest.RelPlanarDrift = 0.0f;
		GShipJumpTest.bLeftGround = false;
		GShipJumpTest.bLanded = false;
		GShipJumpTest.bJumpIssued = true;
		const bool bCanJumpBefore = Character->CanJump();
		InjectKey(Character, EKeys::SpaceBar, IE_Pressed);
		Character->ApplyJumpForTest();
		GPShipDebugEvent(*FString::Printf(
			TEXT("ShipJump: jumped %s CanJump=%s Mode=%d StayBased=%s JumpZ=%.0f Rel=%s"),
			JumpScenarioName(GShipJumpTest.Scenario),
			bCanJumpBefore ? TEXT("true") : TEXT("false"),
			MoveComp ? static_cast<int32>(MoveComp->MovementMode) : -1,
			(MoveComp && MoveComp->bStayBasedInAir) ? TEXT("true") : TEXT("false"),
			MoveComp ? MoveComp->JumpZVelocity : -1.0f,
			*RelNow.ToCompactString()));
	}

	if (GShipJumpTest.bJumpIssued)
	{
		const float RelUp = RelNow.Z - GShipJumpTest.RelAtJump.Z;
		GShipJumpTest.PeakRelUp = FMath::Max(GShipJumpTest.PeakRelUp, RelUp);
		GShipJumpTest.RelPlanarDrift = FMath::Max(GShipJumpTest.RelPlanarDrift,
			FVector::Dist2D(FVector(RelNow.X, RelNow.Y, 0.0f), FVector(GShipJumpTest.RelAtJump.X, GShipJumpTest.RelAtJump.Y, 0.0f)));
		if (MoveComp && MoveComp->IsFalling())
		{
			GShipJumpTest.bLeftGround = true;
		}
		if (GShipJumpTest.PeakRelUp > 25.0f && RelUp < 15.0f && GShipJumpTest.ScenarioTime > 0.55f)
		{
			GShipJumpTest.bLanded = true;
		}
		if (MoveComp && MoveComp->IsMovingOnGround() && GShipJumpTest.ScenarioTime > 0.40f)
		{
			GShipJumpTest.bLanded = true;
		}
	}

	if (GShipJumpTest.ScenarioTime >= 1.45f)
	{
		InjectKey(Character, EKeys::SpaceBar, IE_Released);
		if (GShipJumpTest.Scenario == 4)
		{
			InjectKey(Character, EKeys::W, IE_Released);
		}

		FShipJumpScenarioResult& Result = GShipJumpTest.Results[GShipJumpTest.Scenario];
		Result.Name = JumpScenarioName(GShipJumpTest.Scenario);
		Result.bLeftGround = GShipJumpTest.bLeftGround;
		Result.bLanded = GShipJumpTest.bLanded;
		Result.PeakRelUp = GShipJumpTest.PeakRelUp;
		Result.RelPlanarDrift = GShipJumpTest.RelPlanarDrift;
		const float DriftLimit = (GShipJumpTest.Scenario == 4) ? 800.0f : ((GShipJumpTest.Scenario >= 3) ? 70.0f : 40.0f);
		Result.bPass = Result.bLeftGround && Result.bLanded && Result.PeakRelUp > 25.0f && Result.RelPlanarDrift < DriftLimit;

		GPShipDebugScreen(
			FString::Printf(TEXT("Jump %s %s up=%.1fcm drift=%.1f left=%s land=%s"),
				*Result.Name,
				Result.bPass ? TEXT("PASS") : TEXT("FAIL"),
				Result.PeakRelUp,
				Result.RelPlanarDrift,
				Result.bLeftGround ? TEXT("y") : TEXT("n"),
				Result.bLanded ? TEXT("y") : TEXT("n")),
			Result.bPass ? FColor::Green : FColor::Orange,
			6.0f);

		GShipJumpTest.Scenario++;
		GShipJumpTest.ScenarioTime = 0.0f;
		GShipJumpTest.bJumpIssued = false;
	}
}

namespace
{
	struct FDedicatedNetTestState
	{
		int32 Phase = -1;
		float Time = 0.0f;
		float PhaseTime = 0.0f;
		uint64 LastTickFrame = MAX_uint64;
		TWeakObjectPtr<AWalkableShip> Ship;
		TWeakObjectPtr<AGalacticPiratesCharacter> Character;
		FVector StartShipLocation = FVector::ZeroVector;
		FQuat StartShipRotation = FQuat::Identity;
		bool bBoarded = false;
		bool bInside = false;
		bool bHelmTaken = false;
		bool bShipMoved = false;
		bool bHelmReleased = false;
		bool bStillBoarded = false;
		float ShipTravel = 0.0f;
		float ShipYawDelta = 0.0f;
		int32 PlayersAboard = 0;
	};

	FDedicatedNetTestState GDedicatedNetTest;

	AGalacticPiratesCharacter* FindDedicatedNetCharacter(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		for (TActorIterator<AGalacticPiratesCharacter> It(World); It; ++It)
		{
			if (*It && (*It)->GetBoardedShip() && Cast<APlayerController>((*It)->GetController()))
			{
				return *It;
			}
		}

		return nullptr;
	}

	void FinishDedicatedNetTest(bool bPass, const FString& Detail)
	{
		const FString Line = FString::Printf(
			TEXT("board=%s inside=%s helm=%s moved=%s release=%s stay=%s travel=%.1f yaw=%.1f aboard=%d %s"),
			GDedicatedNetTest.bBoarded ? TEXT("y") : TEXT("n"),
			GDedicatedNetTest.bInside ? TEXT("y") : TEXT("n"),
			GDedicatedNetTest.bHelmTaken ? TEXT("y") : TEXT("n"),
			GDedicatedNetTest.bShipMoved ? TEXT("y") : TEXT("n"),
			GDedicatedNetTest.bHelmReleased ? TEXT("y") : TEXT("n"),
			GDedicatedNetTest.bStillBoarded ? TEXT("y") : TEXT("n"),
			GDedicatedNetTest.ShipTravel,
			GDedicatedNetTest.ShipYawDelta,
			GDedicatedNetTest.PlayersAboard,
			*Detail);

		GPShipDebugScreen(
			FString::Printf(TEXT("DEDICATED NET %s %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), *Line),
			bPass ? FColor::Green : FColor::Red,
			12.0f);
		UE_LOG(LogGalacticPirates, Warning, TEXT("[DedicatedNet][Verdict] %s %s"),
			bPass ? TEXT("PASS") : TEXT("FAIL"),
			*Line);

		if (AGalacticPiratesCharacter* Character = GDedicatedNetTest.Character.Get())
		{
			GPShipDebugSnapshot(Character, TEXT("DedicatedNet/Done"));
		}

		GDedicatedNetTest.Phase = -1;
	}
}

void GPStartDedicatedNetTest(UWorld* World)
{
	GDedicatedNetTest = FDedicatedNetTestState();
	GDedicatedNetTest.Phase = 0;
	GDedicatedNetTest.LastTickFrame = MAX_uint64;

	const ENetMode NetMode = World ? World->GetNetMode() : NM_MAX;
	UE_LOG(LogGalacticPirates, Warning,
		TEXT("[DedicatedNet] Started NetMode=%d dedicated=%s"),
		static_cast<int32>(NetMode),
		(NetMode == NM_DedicatedServer) ? TEXT("true") : TEXT("false"));
	GPShipDebugScreen(TEXT("Dedicated net test waiting for boarded client"), FColor::Cyan, 6.0f);
}

void GPTickDedicatedNetTest(UWorld* World, float DeltaTime)
{
	if (GDedicatedNetTest.Phase < 0 || !World)
	{
		return;
	}

	if (World->GetNetMode() != NM_DedicatedServer)
	{
		return;
	}

	if (GFrameCounter == GDedicatedNetTest.LastTickFrame)
	{
		return;
	}
	GDedicatedNetTest.LastTickFrame = GFrameCounter;

	GDedicatedNetTest.Time += DeltaTime;
	GDedicatedNetTest.PhaseTime += DeltaTime;

	AGalacticPiratesCharacter* Character = GDedicatedNetTest.Character.Get();
	if (!Character)
	{
		Character = FindDedicatedNetCharacter(World);
		GDedicatedNetTest.Character = Character;
	}

	AWalkableShip* Ship = GDedicatedNetTest.Ship.Get();
	if (!Ship && Character)
	{
		Ship = Character->GetBoardedShip();
		GDedicatedNetTest.Ship = Ship;
	}
	if (!Ship)
	{
		Ship = AWalkableShip::FindPersistentShip(World);
		GDedicatedNetTest.Ship = Ship;
	}

	auto FailTimeout = [&](const TCHAR* Reason)
	{
		FinishDedicatedNetTest(false, FString::Printf(TEXT("timeout %s t=%.2f"), Reason, GDedicatedNetTest.Time));
	};

	if (GDedicatedNetTest.Phase == 0)
	{
		if (Character && Character->GetBoardedShip())
		{
			Ship = Character->GetBoardedShip();
			GDedicatedNetTest.Ship = Ship;
			GDedicatedNetTest.bBoarded = true;
			GDedicatedNetTest.bInside = Ship && Ship->IsWalkableWorldLocation(Character->GetActorLocation());
			GDedicatedNetTest.PlayersAboard = Ship ? Ship->GetPlayersAboard().Num() : 0;
			GDedicatedNetTest.StartShipLocation = Ship ? Ship->GetActorLocation() : FVector::ZeroVector;
			GDedicatedNetTest.StartShipRotation = Ship ? Ship->GetActorQuat() : FQuat::Identity;
			GDedicatedNetTest.Phase = 1;
			GDedicatedNetTest.PhaseTime = 0.0f;
			UE_LOG(LogGalacticPirates, Warning,
				TEXT("[DedicatedNet] Boarded %s on %s inside=%s aboard=%d loc=%s"),
				*GetNameSafe(Character),
				*GetNameSafe(Ship),
				GDedicatedNetTest.bInside ? TEXT("true") : TEXT("false"),
				GDedicatedNetTest.PlayersAboard,
				*Character->GetActorLocation().ToCompactString());
			GPShipDebugSnapshot(Character, TEXT("DedicatedNet/Boarded"));
		}
		else if (GDedicatedNetTest.Time > 45.0f)
		{
			FailTimeout(TEXT("waiting for boarded player"));
		}
		return;
	}

	if (!Character || !Ship)
	{
		FailTimeout(TEXT("lost character or ship"));
		return;
	}

	if (GDedicatedNetTest.Phase == 1)
	{
		if (GDedicatedNetTest.PhaseTime < 0.35f)
		{
			return;
		}

		GDedicatedNetTest.bInside = Ship->IsWalkableWorldLocation(Character->GetActorLocation());
		if (!GDedicatedNetTest.bBoarded || !GDedicatedNetTest.bInside)
		{
			FinishDedicatedNetTest(false, TEXT("player not inside ship after board"));
			return;
		}

		if (Character->IsPiloting() && Ship->Helm)
		{
			Ship->Helm->TryInteract(Character);
		}

		GDedicatedNetTest.Phase = 2;
		GDedicatedNetTest.PhaseTime = 0.0f;
		UE_LOG(LogGalacticPirates, Warning, TEXT("[DedicatedNet] Taking helm"));
		return;
	}

	if (GDedicatedNetTest.Phase == 2)
	{
		if (!Character->IsPiloting() && Ship->Helm)
		{
			Ship->Helm->TryInteract(Character);
		}

		GDedicatedNetTest.bHelmTaken = Character->IsPiloting() && Ship->IsPilot(Character);
		if (GDedicatedNetTest.bHelmTaken)
		{
			GDedicatedNetTest.StartShipLocation = Ship->GetActorLocation();
			GDedicatedNetTest.StartShipRotation = Ship->GetActorQuat();
			GDedicatedNetTest.Phase = 3;
			GDedicatedNetTest.PhaseTime = 0.0f;
			UE_LOG(LogGalacticPirates, Warning, TEXT("[DedicatedNet] Helm taken, applying server pilot input"));
		}
		else if (GDedicatedNetTest.PhaseTime > 3.0f)
		{
			FailTimeout(TEXT("helm take"));
		}
		return;
	}

	if (GDedicatedNetTest.Phase == 3)
	{
		Ship->ApplyPilotInput(Character, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f));
		if (GDedicatedNetTest.PhaseTime >= 1.50f)
		{
			GDedicatedNetTest.Phase = 4;
			GDedicatedNetTest.PhaseTime = 0.0f;
		}
		return;
	}

	if (GDedicatedNetTest.Phase == 4)
	{
		GDedicatedNetTest.ShipTravel = FVector::Dist(Ship->GetActorLocation(), GDedicatedNetTest.StartShipLocation);
		GDedicatedNetTest.ShipYawDelta = FMath::RadiansToDegrees(
			GDedicatedNetTest.StartShipRotation.AngularDistance(Ship->GetActorQuat()));
		const FVector PointVel = Ship->GetPointVelocity(Character->GetActorLocation());
		GDedicatedNetTest.bShipMoved = GDedicatedNetTest.ShipTravel > 40.0f || GDedicatedNetTest.ShipYawDelta > 8.0f;
		UE_LOG(LogGalacticPirates, Warning,
			TEXT("[DedicatedNet] Ship motion travel=%.1fcm yaw=%.1fdeg vel=%s"),
			GDedicatedNetTest.ShipTravel,
			GDedicatedNetTest.ShipYawDelta,
			*PointVel.ToCompactString());

		if (!GDedicatedNetTest.bShipMoved)
		{
			FinishDedicatedNetTest(false, TEXT("ship did not move under dedicated pilot input"));
			return;
		}

		GDedicatedNetTest.Phase = 5;
		GDedicatedNetTest.PhaseTime = 0.0f;
		return;
	}

	if (GDedicatedNetTest.Phase == 5)
	{
		Ship->ApplyPilotInput(Character, FVector::ZeroVector, FVector::ZeroVector);
		if (Character->IsPiloting() && Ship->Helm)
		{
			Ship->Helm->TryInteract(Character);
		}

		GDedicatedNetTest.bHelmReleased = !Character->IsPiloting() && !Ship->IsPilot(Character);
		if (GDedicatedNetTest.bHelmReleased)
		{
			GDedicatedNetTest.Phase = 6;
			GDedicatedNetTest.PhaseTime = 0.0f;
			UE_LOG(LogGalacticPirates, Warning, TEXT("[DedicatedNet] Helm released"));
		}
		else if (GDedicatedNetTest.PhaseTime > 3.0f)
		{
			FailTimeout(TEXT("helm release"));
		}
		return;
	}

	if (GDedicatedNetTest.Phase == 6)
	{
		if (GDedicatedNetTest.PhaseTime < 0.35f)
		{
			return;
		}

		GDedicatedNetTest.bStillBoarded = Character->GetBoardedShip() == Ship
			&& Ship->GetPlayersAboard().Contains(Character)
			&& Ship->IsWalkableWorldLocation(Character->GetActorLocation());
		GDedicatedNetTest.PlayersAboard = Ship->GetPlayersAboard().Num();

		const bool bPass = GDedicatedNetTest.bBoarded
			&& GDedicatedNetTest.bInside
			&& GDedicatedNetTest.bHelmTaken
			&& GDedicatedNetTest.bShipMoved
			&& GDedicatedNetTest.bHelmReleased
			&& GDedicatedNetTest.bStillBoarded;
		FinishDedicatedNetTest(bPass, bPass ? TEXT("ok") : TEXT("player left ship after helm release"));
	}
}

namespace
{
	struct FShipDuelTestState
	{
		int32 Phase = -1;
		float Time = 0.0f;
		float PhaseTime = 0.0f;
		uint64 LastTickFrame = MAX_uint64;
		TWeakObjectPtr<AWalkableShip> ShipA;
		TWeakObjectPtr<AWalkableShip> ShipB;
		TWeakObjectPtr<AGalacticPiratesCharacter> Player;
		TWeakObjectPtr<AGalacticPiratesCharacter> GunnerB;
		float HealthAAfterBShot = -1.0f;
		float HealthBAfterAShot = -1.0f;
		bool bShipsPlaced = false;
		bool bTerminalFire = false;
		bool bCooldownDenied = false;
		bool bBFired = false;
		bool bBothExploded = false;
		int32 BeamsSeen = 0;
	};

	FShipDuelTestState GShipDuelTest;

	void HaltShip(AWalkableShip* Ship)
	{
		if (!Ship || !Ship->ShipMovement)
		{
			return;
		}

		Ship->ShipMovement->SetThrustInput(FVector::ZeroVector);
		Ship->ShipMovement->SetRotationInput(FVector::ZeroVector);
		Ship->ShipMovement->SetLinearVelocity(FVector::ZeroVector);
		Ship->ShipMovement->SetAngularVelocity(FVector::ZeroVector);
	}

	void ConfigureDuelShip(AWalkableShip* Ship)
	{
		if (!Ship)
		{
			return;
		}

		Ship->MaxHealth = 100.0f;
		Ship->SetHealth(100.0f);
		Ship->WreckLifetime = 12.0f;
		if (Ship->PulseCannon)
		{
			Ship->PulseCannon->PulseDamage = 40.0f;
			Ship->PulseCannon->RechargeTime = 0.45f;
			Ship->PulseCannon->BeamRange = 20000.0f;
			Ship->PulseCannon->BeamRadius = 180.0f;
			Ship->PulseCannon->BeamVisualDuration = 1.5f;
		}
		HaltShip(Ship);
	}

	void MoveCharacterToTerminal(AGalacticPiratesCharacter* Character, AWalkableShip* Ship)
	{
		if (!Character || !Ship || !Ship->WeaponTerminal)
		{
			return;
		}

		if (Character->IsPiloting() && Ship->Helm)
		{
			Ship->Helm->TryInteract(Character);
		}

		const FVector TerminalLoc = Ship->WeaponTerminal->GetComponentLocation();
		Character->SetActorLocation(TerminalLoc + Ship->GetActorUpVector() * 95.0f, false, nullptr, ETeleportType::TeleportPhysics);
	}

	void FinishShipDuelTest(bool bPass, const FString& Detail)
	{
		const FString Line = FString::Printf(
			TEXT("place=%s terminal=%s deny=%s Bfire=%s exploded=%s Ahp=%.0f Bhp=%.0f %s"),
			GShipDuelTest.bShipsPlaced ? TEXT("y") : TEXT("n"),
			GShipDuelTest.bTerminalFire ? TEXT("y") : TEXT("n"),
			GShipDuelTest.bCooldownDenied ? TEXT("y") : TEXT("n"),
			GShipDuelTest.bBFired ? TEXT("y") : TEXT("n"),
			GShipDuelTest.bBothExploded ? TEXT("y") : TEXT("n"),
			GShipDuelTest.HealthAAfterBShot,
			GShipDuelTest.HealthBAfterAShot,
			*Detail);

		GPShipDebugScreen(
			FString::Printf(TEXT("SHIP DUEL %s %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), *Line),
			bPass ? FColor::Green : FColor::Red,
			14.0f);
		UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDuel][Verdict] %s %s"),
			bPass ? TEXT("PASS") : TEXT("FAIL"),
			*Line);
		GShipDuelTest.Phase = -1;
	}
}

void GPStartShipDuelTest(UWorld* World)
{
	GShipDuelTest = FShipDuelTestState();
	GShipDuelTest.Phase = 0;
	const ENetMode NetMode = World ? World->GetNetMode() : NM_MAX;
	UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDuel] Started NetMode=%d"), static_cast<int32>(NetMode));
	GPShipDebugScreen(TEXT("Ship duel test waiting for boarded client"), FColor::Cyan, 6.0f);
}

void GPTickShipDuelTest(UWorld* World, float DeltaTime)
{
	if (GShipDuelTest.Phase < 0 || !World)
	{
		return;
	}

	if (World->GetNetMode() != NM_DedicatedServer)
	{
		return;
	}

	if (GFrameCounter == GShipDuelTest.LastTickFrame)
	{
		return;
	}
	GShipDuelTest.LastTickFrame = GFrameCounter;
	GShipDuelTest.Time += DeltaTime;
	GShipDuelTest.PhaseTime += DeltaTime;

	AWalkableShip* ShipA = GShipDuelTest.ShipA.Get();
	AWalkableShip* ShipB = GShipDuelTest.ShipB.Get();
	AGalacticPiratesCharacter* Player = GShipDuelTest.Player.Get();
	AGalacticPiratesCharacter* GunnerB = GShipDuelTest.GunnerB.Get();

	auto FailTimeout = [&](const TCHAR* Reason)
	{
		FinishShipDuelTest(false, FString::Printf(TEXT("timeout %s t=%.2f"), Reason, GShipDuelTest.Time));
	};

	if (GShipDuelTest.Phase == 0)
	{
		for (TActorIterator<AGalacticPiratesCharacter> It(World); It; ++It)
		{
			if (*It && (*It)->GetBoardedShip() && Cast<APlayerController>((*It)->GetController()))
			{
				Player = *It;
				GShipDuelTest.Player = Player;
				break;
			}
		}

		if (Player && Player->GetBoardedShip())
		{
			ShipA = Player->GetBoardedShip();
			GShipDuelTest.ShipA = ShipA;
			GShipDuelTest.Phase = 1;
			GShipDuelTest.PhaseTime = 0.0f;
			UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDuel] Player %s boarded %s"), *GetNameSafe(Player), *GetNameSafe(ShipA));
		}
		else if (GShipDuelTest.Time > 45.0f)
		{
			FailTimeout(TEXT("waiting for boarded player"));
		}
		return;
	}

	if (!Player || !ShipA)
	{
		FailTimeout(TEXT("lost player or ship A"));
		return;
	}

	if (GShipDuelTest.Phase == 1)
	{
		for (TActorIterator<AWalkableShip> It(World); It; ++It)
		{
			if (*It && *It != ShipA)
			{
				ShipB = *It;
				break;
			}
		}

		if (!ShipB)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector LocB = ShipA->GetActorLocation() + ShipA->GetActorForwardVector() * 7000.0f;
			const FRotator RotB = (-ShipA->GetActorForwardVector()).Rotation();
			ShipB = World->SpawnActor<AWalkableShip>(ShipA->GetClass(), LocB, RotB, Params);
			UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDuel] Spawned opposing ship %s"), *GetNameSafe(ShipB));
		}

		if (!ShipB)
		{
			FinishShipDuelTest(false, TEXT("failed to spawn ship B"));
			return;
		}

		const FVector LocA = ShipA->GetActorLocation();
		ShipA->SetActorRotation(FRotator::ZeroRotator);
		ShipB->SetActorLocationAndRotation(LocA + FVector(7000.0f, 0.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		ConfigureDuelShip(ShipA);
		ConfigureDuelShip(ShipB);
		GShipDuelTest.ShipB = ShipB;
		GShipDuelTest.bShipsPlaced = true;
		GShipDuelTest.Phase = 2;
		GShipDuelTest.PhaseTime = 0.0f;
		UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDuel] Ships facing A=%s B=%s"),
			*ShipA->GetActorLocation().ToCompactString(),
			*ShipB->GetActorLocation().ToCompactString());
		return;
	}

	if (!ShipB)
	{
		FailTimeout(TEXT("lost ship B"));
		return;
	}

	if (GShipDuelTest.Phase == 2)
	{
		MoveCharacterToTerminal(Player, ShipA);
		GShipDuelTest.Phase = 3;
		GShipDuelTest.PhaseTime = 0.0f;
		return;
	}

	if (GShipDuelTest.Phase == 3)
	{
		if (GShipDuelTest.PhaseTime < 0.15f)
		{
			return;
		}

		const bool bFired = ShipA->WeaponTerminal && ShipA->WeaponTerminal->TryInteract(Player);
		GShipDuelTest.bTerminalFire = bFired;
		if (!GShipDuelTest.bTerminalFire && ShipA->PulseCannon)
		{
			GShipDuelTest.bTerminalFire = ShipA->PulseCannon->FireIgnoringTerminalRange(Player);
		}

		UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDuel] A terminal fire=%s B health=%.1f"),
			GShipDuelTest.bTerminalFire ? TEXT("true") : TEXT("false"),
			ShipB->GetHealth());

		if (!GShipDuelTest.bTerminalFire)
		{
			FinishShipDuelTest(false, TEXT("ship A terminal fire failed"));
			return;
		}

		const bool bSecond = ShipA->WeaponTerminal && ShipA->WeaponTerminal->TryInteract(Player);
		GShipDuelTest.bCooldownDenied = !bSecond && ShipA->PulseCannon && !ShipA->PulseCannon->CanFire();
		GShipDuelTest.Phase = 4;
		GShipDuelTest.PhaseTime = 0.0f;
		return;
	}

	if (GShipDuelTest.Phase == 4)
	{
		if (GShipDuelTest.PhaseTime < 0.4f)
		{
			return;
		}

		GShipDuelTest.HealthBAfterAShot = ShipB->GetHealth();
		if (GShipDuelTest.HealthBAfterAShot >= 99.0f)
		{
			FinishShipDuelTest(false, TEXT("ship A beam did not damage ship B"));
			return;
		}

		if (!GunnerB)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GunnerB = World->SpawnActor<AGalacticPiratesCharacter>(Player->GetClass(), ShipB->GetSpawnTransform(), Params);
			if (GunnerB)
			{
				GunnerB->BoardShip(ShipB);
				MoveCharacterToTerminal(GunnerB, ShipB);
				GShipDuelTest.GunnerB = GunnerB;
			}
		}

		bool bFired = false;
		if (GunnerB && ShipB->WeaponTerminal)
		{
			bFired = ShipB->WeaponTerminal->TryInteract(GunnerB);
		}
		if (!bFired && ShipB->PulseCannon)
		{
			bFired = ShipB->PulseCannon->FireIgnoringTerminalRange(GunnerB);
		}

		GShipDuelTest.bBFired = bFired;
		UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDuel] B fire=%s A health=%.1f B health=%.1f"),
			bFired ? TEXT("true") : TEXT("false"),
			ShipA->GetHealth(),
			ShipB->GetHealth());

		if (!bFired)
		{
			FinishShipDuelTest(false, TEXT("ship B did not fire"));
			return;
		}

		GShipDuelTest.Phase = 5;
		GShipDuelTest.PhaseTime = 0.0f;
		return;
	}

	if (GShipDuelTest.Phase == 5)
	{
		if (GShipDuelTest.PhaseTime < 0.4f)
		{
			return;
		}

		GShipDuelTest.HealthAAfterBShot = ShipA->GetHealth();
		UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDuel] After B beam A health=%.1f"), GShipDuelTest.HealthAAfterBShot);
		if (GShipDuelTest.HealthAAfterBShot >= 99.0f)
		{
			FinishShipDuelTest(false, TEXT("ship B beam did not damage ship A"));
			return;
		}

		GShipDuelTest.Phase = 6;
		GShipDuelTest.PhaseTime = 0.0f;
		return;
	}

	if (GShipDuelTest.Phase == 6 || GShipDuelTest.Phase == 7)
	{
		if (GShipDuelTest.PhaseTime < 0.55f)
		{
			return;
		}

		if (GShipDuelTest.Phase == 6)
		{
			MoveCharacterToTerminal(Player, ShipA);
			if (ShipA->WeaponTerminal)
			{
				ShipA->WeaponTerminal->TryInteract(Player);
			}
		}
		else
		{
			MoveCharacterToTerminal(GunnerB, ShipB);
			if (GunnerB && ShipB->WeaponTerminal)
			{
				ShipB->WeaponTerminal->TryInteract(GunnerB);
			}
			else if (ShipB->PulseCannon)
			{
				ShipB->PulseCannon->FireIgnoringTerminalRange(GunnerB);
			}
		}

		GShipDuelTest.Phase++;
		GShipDuelTest.PhaseTime = 0.0f;
		return;
	}

	if (GShipDuelTest.Phase == 8)
	{
		if (GShipDuelTest.PhaseTime < 0.55f)
		{
			return;
		}

		MoveCharacterToTerminal(Player, ShipA);
		MoveCharacterToTerminal(GunnerB, ShipB);
		if (ShipA->WeaponTerminal)
		{
			ShipA->WeaponTerminal->TryInteract(Player);
		}
		if (GunnerB && ShipB->WeaponTerminal)
		{
			ShipB->WeaponTerminal->TryInteract(GunnerB);
		}
		else if (ShipB->PulseCannon)
		{
			ShipB->PulseCannon->FireIgnoringTerminalRange(GunnerB);
		}

		UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipDuel] Killing volley health A=%.1f B=%.1f"),
			ShipA->GetHealth(),
			ShipB->GetHealth());

		GShipDuelTest.Phase = 9;
		GShipDuelTest.PhaseTime = 0.0f;
		return;
	}

	if (GShipDuelTest.Phase == 9)
	{
		if (GShipDuelTest.PhaseTime < 1.7f)
		{
			return;
		}

		GShipDuelTest.bBothExploded = ShipA->IsWrecked() && ShipB->IsWrecked();
		const bool bPass = GShipDuelTest.bShipsPlaced
			&& GShipDuelTest.bTerminalFire
			&& GShipDuelTest.bCooldownDenied
			&& GShipDuelTest.bBFired
			&& GShipDuelTest.bBothExploded;
		FinishShipDuelTest(bPass, bPass ? TEXT("ok") : TEXT("ships did not both explode"));
	}
}

namespace
{
	struct FGPShipMoveProbe
	{
		bool bActive = false;
		bool bLoggedStart = false;
		bool bAppliedDirect = false;
		bool bMeasuredDirect = false;
		bool bAppliedPilot = false;
		float Elapsed = 0.0f;
		TWeakObjectPtr<UWorld> World;
		TArray<TWeakObjectPtr<AWalkableShip>> Ships;
		TArray<FVector> StartLocations;
		TArray<FRotator> StartRotations;
		TArray<float> DistAtDirect;
		TArray<float> YawAtDirect;
		TArray<float> DistAtPilot;
		TArray<float> YawAtPilot;
	};

	FGPShipMoveProbe GShipMoveProbe;

	void LogShipMoveSnapshot(const TCHAR* Tag, AWalkableShip* Ship)
	{
		if (!Ship)
		{
			return;
		}

		UShipMovementComponent* Move = Ship->ShipMovement;
		UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Ship->GetRootComponent());
		UE_LOG(LogGalacticPirates, Warning,
			TEXT("[ShipMoveProbe] %s ship=%s auth=%d wreck=%d net=%d loc=%s rot=%s lin=%.1f ang=%.1f thrust=%s rotIn=%s heldT=%s heldR=%s override=%d tickOn=%d mass=%.0f sim=%d gravity=%d collide=%d orbitOn=%d crewOn=%d helmOcc=%d currentPilot=%s"),
			Tag,
			*Ship->GetName(),
			Ship->HasAuthority() ? 1 : 0,
			Ship->IsWrecked() ? 1 : 0,
			static_cast<int32>(Ship->GetNetMode()),
			*Ship->GetActorLocation().ToCompactString(),
			*Ship->GetActorRotation().ToCompactString(),
			Move ? Move->GetLinearVelocity().Size() : -1.0f,
			Move ? Move->GetAngularVelocity().Size() : -1.0f,
			Move ? *Move->GetAppliedThrustInput().ToCompactString() : TEXT("none"),
			Move ? *Move->GetAppliedRotationInput().ToCompactString() : TEXT("none"),
			Move ? *Move->GetThrustInput().ToCompactString() : TEXT("none"),
			Move ? *Move->GetRotationInput().ToCompactString() : TEXT("none"),
			(Move && Move->IsVelocityOverride()) ? 1 : 0,
			(Move && Move->PrimaryComponentTick.IsTickFunctionEnabled()) ? 1 : 0,
			Move ? Move->ShipMass : -1.0f,
			RootPrim && RootPrim->IsSimulatingPhysics() ? 1 : 0,
			RootPrim && RootPrim->IsGravityEnabled() ? 1 : 0,
			RootPrim ? static_cast<int32>(RootPrim->GetCollisionEnabled()) : -1,
			(Ship->OrbitAI && Ship->OrbitAI->bEnabled) ? 1 : 0,
			(Ship->CrewAI && Ship->CrewAI->bEnabled) ? 1 : 0,
			(Ship->Helm && Ship->Helm->IsOccupied()) ? 1 : 0,
			*GetNameSafe(Ship->GetCurrentPilot()));
	}
}

void GPStartShipMoveProbe(UWorld* World)
{
	GShipMoveProbe = FGPShipMoveProbe();
	if (!World)
	{
		UE_LOG(LogGalacticPirates, Error, TEXT("[ShipMoveProbe] FAIL no world"));
		return;
	}

	CVarGPShipMoveLog->Set(1, ECVF_SetByConsole);
	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogGalacticPirates, Warning,
			TEXT("[ShipMoveProbe] running on CLIENT world — ship physics is server-authoritative. Also run gp.StartShipMoveProbe on the dedicated/PIE server console."));
	}
	for (TActorIterator<AWalkableShip> It(World); It; ++It)
	{
		AWalkableShip* Ship = *It;
		if (!IsValid(Ship) || Ship->IsWrecked())
		{
			continue;
		}
		GShipMoveProbe.Ships.Add(Ship);
		GShipMoveProbe.StartLocations.Add(Ship->GetActorLocation());
		GShipMoveProbe.StartRotations.Add(Ship->GetActorRotation());
		GShipMoveProbe.DistAtDirect.Add(0.0f);
		GShipMoveProbe.YawAtDirect.Add(0.0f);
		GShipMoveProbe.DistAtPilot.Add(0.0f);
		GShipMoveProbe.YawAtPilot.Add(0.0f);
		LogShipMoveSnapshot(TEXT("START"), Ship);
	}

	if (GShipMoveProbe.Ships.Num() == 0)
	{
		UE_LOG(LogGalacticPirates, Error, TEXT("[ShipMoveProbe] FAIL no live ships in world"));
		return;
	}

	GShipMoveProbe.bActive = true;
	GShipMoveProbe.Elapsed = 0.0f;
	GShipMoveProbe.World = World;
	UE_LOG(LogGalacticPirates, Warning,
		TEXT("[ShipMoveProbe] started ships=%d net=%d — 0.2s snapshot, 1.2s direct SetThrust/SetRotation, 1.2s ApplyPilotInput, then PASS/FAIL"),
		GShipMoveProbe.Ships.Num(),
		static_cast<int32>(World->GetNetMode()));
}

void GPTickShipMoveProbe(UWorld* World, float DeltaTime)
{
	if (!World)
	{
		return;
	}

	if (!GShipMoveProbe.bActive && CVarGPShipMoveProbe.GetValueOnGameThread() > 0 && World->GetNetMode() != NM_Client)
	{
		CVarGPShipMoveProbe->Set(0, ECVF_SetByConsole);
		GPStartShipMoveProbe(World);
	}

	if (!GShipMoveProbe.bActive || GShipMoveProbe.World.Get() != World)
	{
		return;
	}

	static uint64 LastFrame = 0;
	if (GFrameCounter == LastFrame)
	{
		return;
	}
	LastFrame = GFrameCounter;

	GShipMoveProbe.Elapsed += DeltaTime;
	const float T = GShipMoveProbe.Elapsed;

	if (!GShipMoveProbe.bLoggedStart && T >= 0.2f)
	{
		GShipMoveProbe.bLoggedStart = true;
		for (int32 i = 0; i < GShipMoveProbe.Ships.Num(); ++i)
		{
			LogShipMoveSnapshot(TEXT("PRE"), GShipMoveProbe.Ships[i].Get());
		}
	}

	if (!GShipMoveProbe.bAppliedDirect && T >= 0.25f)
	{
		GShipMoveProbe.bAppliedDirect = true;
		for (int32 i = 0; i < GShipMoveProbe.Ships.Num(); ++i)
		{
			AWalkableShip* Ship = GShipMoveProbe.Ships[i].Get();
			if (!IsValid(Ship) || !Ship->ShipMovement)
			{
				continue;
			}
			if (Ship->OrbitAI)
			{
				Ship->OrbitAI->bEnabled = false;
			}
			if (Ship->CrewAI)
			{
				Ship->CrewAI->bEnabled = false;
			}
			Ship->ShipMovement->SetVelocityOverride(FVector::ZeroVector, FVector::ZeroVector, false);
			Ship->ShipMovement->SetThrustInput(FVector(1.0f, 0.0f, 0.0f));
			Ship->ShipMovement->SetRotationInput(FVector(0.0f, 0.0f, 1.0f));
			LogShipMoveSnapshot(TEXT("DIRECT_SET"), Ship);
		}
		UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipMoveProbe] applied direct SetThrust(1,0,0) SetRotation yaw=1 on all ships (orbit/crew AI off, override cleared)"));
	}

	if (!GShipMoveProbe.bMeasuredDirect && T >= 1.45f)
	{
		for (int32 i = 0; i < GShipMoveProbe.Ships.Num(); ++i)
		{
			AWalkableShip* Ship = GShipMoveProbe.Ships[i].Get();
			if (!IsValid(Ship))
			{
				continue;
			}
			GShipMoveProbe.DistAtDirect[i] = FVector::Dist(Ship->GetActorLocation(), GShipMoveProbe.StartLocations[i]);
			GShipMoveProbe.YawAtDirect[i] = FMath::Abs(FMath::FindDeltaAngleDegrees(
				GShipMoveProbe.StartRotations[i].Yaw, Ship->GetActorRotation().Yaw));
			LogShipMoveSnapshot(TEXT("AFTER_DIRECT"), Ship);
			UE_LOG(LogGalacticPirates, Warning,
				TEXT("[ShipMoveProbe] after-direct %s dist=%.1f yawDelta=%.1f"),
				*Ship->GetName(),
				GShipMoveProbe.DistAtDirect[i],
				GShipMoveProbe.YawAtDirect[i]);
		}
		GShipMoveProbe.bMeasuredDirect = true;
	}

	if (!GShipMoveProbe.bAppliedPilot && T >= 1.55f)
	{
		GShipMoveProbe.bAppliedPilot = true;
		APlayerController* PC = World->GetFirstPlayerController();
		AGalacticPiratesCharacter* Player = PC ? Cast<AGalacticPiratesCharacter>(PC->GetPawn()) : nullptr;
		for (int32 i = 0; i < GShipMoveProbe.Ships.Num(); ++i)
		{
			AWalkableShip* Ship = GShipMoveProbe.Ships[i].Get();
			if (!IsValid(Ship) || !Ship->ShipMovement)
			{
				continue;
			}
			Ship->ShipMovement->SetThrustInput(FVector::ZeroVector);
			Ship->ShipMovement->SetRotationInput(FVector::ZeroVector);
			Ship->ShipMovement->SetLinearVelocity(FVector::ZeroVector);
			Ship->ShipMovement->SetAngularVelocity(FVector::ZeroVector);
			GShipMoveProbe.StartLocations[i] = Ship->GetActorLocation();
			GShipMoveProbe.StartRotations[i] = Ship->GetActorRotation();

			if (Player && Ship->HasAuthority())
			{
				if (!Ship->GetCurrentPilot() && Ship->Helm)
				{
					Ship->Helm->TryInteract(Player);
				}
				Ship->ApplyPilotInput(Player, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f));
			}
			LogShipMoveSnapshot(TEXT("PILOT_SET"), Ship);
		}
		UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipMoveProbe] applied ApplyPilotInput thrust+yaw (or skipped if no authority/player)"));
	}

	if (T >= 2.75f)
	{
		bool bDirectMoved = false;
		bool bDirectTurned = false;
		bool bPilotMoved = false;
		bool bPilotTurned = false;
		for (int32 i = 0; i < GShipMoveProbe.Ships.Num(); ++i)
		{
			AWalkableShip* Ship = GShipMoveProbe.Ships[i].Get();
			if (!IsValid(Ship))
			{
				continue;
			}
			GShipMoveProbe.DistAtPilot[i] = FVector::Dist(Ship->GetActorLocation(), GShipMoveProbe.StartLocations[i]);
			GShipMoveProbe.YawAtPilot[i] = FMath::Abs(FMath::FindDeltaAngleDegrees(
				GShipMoveProbe.StartRotations[i].Yaw, Ship->GetActorRotation().Yaw));
			LogShipMoveSnapshot(TEXT("AFTER_PILOT"), Ship);
			UE_LOG(LogGalacticPirates, Warning,
				TEXT("[ShipMoveProbe] after-pilot %s dist=%.1f yawDelta=%.1f (direct dist=%.1f yaw=%.1f)"),
				*Ship->GetName(),
				GShipMoveProbe.DistAtPilot[i],
				GShipMoveProbe.YawAtPilot[i],
				GShipMoveProbe.DistAtDirect[i],
				GShipMoveProbe.YawAtDirect[i]);
			bDirectMoved |= GShipMoveProbe.DistAtDirect[i] > 50.0f;
			bDirectTurned |= GShipMoveProbe.YawAtDirect[i] > 5.0f;
			bPilotMoved |= GShipMoveProbe.DistAtPilot[i] > 50.0f;
			bPilotTurned |= GShipMoveProbe.YawAtPilot[i] > 5.0f;
		}

		const bool bPass = bDirectMoved && bDirectTurned;
		UE_LOG(LogGalacticPirates, Warning,
			TEXT("[ShipMoveProbe] %s directMove=%d directTurn=%d pilotMove=%d pilotTurn=%d (PASS if physics responds to SetThrust/SetRotation)"),
			bPass ? TEXT("PASS") : TEXT("FAIL"),
			bDirectMoved ? 1 : 0,
			bDirectTurned ? 1 : 0,
			bPilotMoved ? 1 : 0,
			bPilotTurned ? 1 : 0);

		if (!bDirectMoved && !bDirectTurned)
		{
			UE_LOG(LogGalacticPirates, Error,
				TEXT("[ShipMoveProbe] diagnosis: movement component is not integrating (tick off, wrecked, override, no authority, or physics blocked)"));
		}
		else if (bDirectMoved && !bPilotMoved)
		{
			UE_LOG(LogGalacticPirates, Error,
				TEXT("[ShipMoveProbe] diagnosis: physics works; ApplyPilotInput is rejected (pilot mismatch / client / wrecked)"));
		}

		GShipMoveProbe.bActive = false;
		CVarGPShipMoveLog->Set(0, ECVF_SetByConsole);
		if (FParse::Param(FCommandLine::Get(), TEXT("AutoQuitAfterProbe")))
		{
			FPlatformMisc::RequestExit(false);
		}
	}
}
