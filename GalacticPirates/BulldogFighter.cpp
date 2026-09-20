#include "BulldogFighter.h"
#include "WalkableShip.h"
#include "ShipMovementComponent.h"
#include "HeatseekingMissile.h"
#include "HoloMapPoiComponent.h"
#include "HoloMapTypes.h"
#include "ShipPolish.h"
#include "GalacticPirates.h"
#include "HullHealthComponent.h"
#include "WeaponHardpointComponent.h"
#include "ShipMissileSalvoComponent.h"
#include "OccupancyComponent.h"
#include "CraftReplication.h"
#include "CraftWreck.h"
#include "GalacticPiratesCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ABulldogFighter::ABulldogFighter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	GPCraftNet::Apply(this, GPCraftNet::Fighter());

	HullMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HullMesh"));
	SetRootComponent(HullMesh);
	HullMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HullMesh->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(TEXT("/Game/Blueprints/Spaceships/Models/A-53Bulldog"));
	if (MeshFinder.Succeeded())
	{
		HullMesh->SetStaticMesh(MeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Game/Blueprints/Spaceships/Models/Materials/A-53Bulldog_MaterialInstance"));
	if (MatFinder.Succeeded())
	{
		HullMesh->SetMaterial(0, MatFinder.Object);
	}

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetupAttachment(HullMesh);
	Collision->SetBoxExtent(FVector(280.0f, 180.0f, 90.0f));
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_Pawn);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetGenerateOverlapEvents(true);

	ShipMovement = CreateDefaultSubobject<UShipMovementComponent>(TEXT("ShipMovement"));
	ShipMovement->ShipMass = 900.0f;
	ShipMovement->ForwardThrustPower = 520000.0f;
	ShipMovement->StrafeThrustPower = 360000.0f;
	ShipMovement->VerticalThrustPower = 360000.0f;
	ShipMovement->PitchTorque = 90000.0f;
	ShipMovement->YawTorque = 110000.0f;
	ShipMovement->RollTorque = 50000.0f;
	ShipMovement->MaxLinearVelocity = 5200.0f;
	ShipMovement->MaxAngularVelocity = 140.0f;
	ShipMovement->TranslationDampening = 0.18f;
	ShipMovement->RotationDampening = 0.28f;

	HullHealth = CreateDefaultSubobject<UHullHealthComponent>(TEXT("HullHealth"));
	HullHealth->MaxHealth = 280.0f;
	HullHealth->ArmorClass = EShipArmorClass::Light;

	MissileHardpoint = CreateDefaultSubobject<UWeaponHardpointComponent>(TEXT("MissileHardpoint"));
	MissileHardpoint->SetupAttachment(HullMesh);

	MissileSalvo = CreateDefaultSubobject<UShipMissileSalvoComponent>(TEXT("MissileSalvo"));
	MissileSalvo->SetupAttachment(MissileHardpoint);
	MissileSalvo->MissilesPerSalvo = 1;
	MissileSalvo->SpreadYawDegrees = 0.0f;
	MissileSalvo->RechargeTime = FireCooldown;
	MissileSalvo->MissileDamage = MissileDamage;
	MissileSalvo->LaunchSpeed = MissileLaunchSpeed;
	MissileSalvo->MuzzleOffset = MuzzleOffset;
	MissileSalvo->bInheritOwnerVelocity = true;
	MissileSalvo->bLaunchAlongOwnerForward = true;
	MissileHardpoint->EquippedWeapon = MissileSalvo;
	MissileHardpoint->WeaponClass = UShipMissileSalvoComponent::StaticClass();

	CockpitOccupancy = CreateDefaultSubobject<UOccupancyComponent>(TEXT("CockpitOccupancy"));
	CockpitOccupancy->SetupAttachment(HullMesh);
	CockpitOccupancy->InteractRange = 420.0f;

	HoloPoi = CreateDefaultSubobject<UHoloMapPoiComponent>(TEXT("HoloPoi"));
	HoloPoi->SetupAttachment(HullMesh);
	HoloPoi->Kind = EHoloMapPoiKind::EnemyShip;
	HoloPoi->Primitive = EHoloMapPrimitive::Triangle;
	HoloPoi->bOverridePrimitive = true;
	HoloPoi->bVisibleOnMaps = true;
	HoloPoi->MarkerScale = FVector(0.10f, 0.10f, 0.10f);
	HoloPoi->OverrideMesh = nullptr;
}

void ABulldogFighter::BeginPlay()
{
	Super::BeginPlay();

	if (ShipMovement)
	{
		ShipMovement->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
	}

	if (CockpitOccupancy)
	{
		CockpitOccupancy->OnOccupancyChanged.AddDynamic(this, &ABulldogFighter::HandleCockpitOccupancy);
	}

	if (HullHealth)
	{
		HullHealth->OnHullDestroyed.AddDynamic(this, &ABulldogFighter::HandleHullDestroyed);
		if (HasAuthority())
		{
			HullHealth->ResetToFull();
		}
	}

	if (MissileSalvo)
	{
		MissileSalvo->RechargeTime = FireCooldown;
		MissileSalvo->MissileDamage = MissileDamage;
		MissileSalvo->LaunchSpeed = MissileLaunchSpeed;
		MissileSalvo->MuzzleOffset = MuzzleOffset;
	}

	const uint32 Hash = GetTypeHash(GetFName());
	StrafeSide = (Hash & 1) ? 1.0f : -1.0f;
	StrafeHeight += static_cast<float>(static_cast<int32>(Hash % 7) - 3) * 80.0f;
}

AActor* ABulldogFighter::FindAttackTarget() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	AActor* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Candidate = *It;
		ISpaceCraft* Craft = GPAsSpaceCraft(Candidate);
		if (!Candidate || Candidate == this || Candidate == HomeCraft || !Craft || Craft->IsCraftWrecked() || !Craft->HasHumanOccupant())
		{
			continue;
		}
		if (!GPAreHostile(const_cast<ABulldogFighter*>(this), Candidate))
		{
			continue;
		}

		const float Dist = FVector::Dist(GetActorLocation(), Candidate->GetActorLocation());
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = Candidate;
		}
	}
	return Best;
}

bool ABulldogFighter::CanFireMissile() const
{
	if (!HasAuthority() || FireCooldownRemaining > KINDA_SMALL_NUMBER || ActiveMissile.IsValid())
	{
		return false;
	}
	return MissileSalvo && MissileSalvo->CanFire();
}

FVector ABulldogFighter::GetInheritedLaunchVelocity() const
{
	return ShipMovement ? ShipMovement->GetLinearVelocity() : FVector::ZeroVector;
}

UShipMovementComponent* ABulldogFighter::GetSpaceMovement() const
{
	return ShipMovement;
}

UHullHealthComponent* ABulldogFighter::GetHullHealth() const
{
	return HullHealth;
}

bool ABulldogFighter::IsCraftWrecked() const
{
	return bWrecked || (HullHealth && HullHealth->IsDestroyed());
}

FVector ABulldogFighter::GetCraftVelocity() const
{
	return GetInheritedLaunchVelocity();
}

USceneComponent* ABulldogFighter::GetHomingSceneComponent() const
{
	return Collision ? static_cast<USceneComponent*>(Collision) : GetRootComponent();
}

bool ABulldogFighter::HasHumanOccupant() const
{
	if (AGalacticPiratesCharacter* Character = CockpitOccupancy ? Cast<AGalacticPiratesCharacter>(CockpitOccupancy->GetOccupant()) : nullptr)
	{
		return Character && !Character->IsAiCrew();
	}
	return false;
}

FName ABulldogFighter::GetAffiliationId() const
{
	return AffiliationId;
}

AActor* ABulldogFighter::GetHomeCraft() const
{
	return HomeCraft ? HomeCraft.Get() : const_cast<ABulldogFighter*>(this);
}

UOccupancyComponent* ABulldogFighter::GetPilotOccupancy() const
{
	return CockpitOccupancy;
}

void ABulldogFighter::NotifyCraftWrecked()
{
	HandleHullDestroyed();
}

void ABulldogFighter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABulldogFighter, HomeCraft);
	DOREPLIFETIME(ABulldogFighter, bWrecked);
	DOREPLIFETIME(ABulldogFighter, AffiliationId);
}

void ABulldogFighter::RegisterHomeCraft(AActor* InHomeCraft)
{
	HomeCraft = InHomeCraft;
	if (ISpaceCraft* Home = GPAsSpaceCraft(InHomeCraft))
	{
		if (AffiliationId.IsNone())
		{
			AffiliationId = Home->GetAffiliationId();
		}
	}
}

bool ABulldogFighter::IsPlayerOccupied() const
{
	return HasHumanOccupant();
}

bool ABulldogFighter::TryCockpitInteract(APawn* Pawn)
{
	return CockpitOccupancy && CockpitOccupancy->TryOccupy(Pawn);
}

void ABulldogFighter::ApplyPilotInput(APawn* Pilot, const FVector& ThrustInput, const FVector& RotationInput)
{
	if (!HasAuthority() || !ShipMovement || !CockpitOccupancy || CockpitOccupancy->GetOccupant() != Pilot)
	{
		return;
	}
	ShipMovement->SetThrustInput(ThrustInput);
	ShipMovement->SetRotationInput(RotationInput);
}

void ABulldogFighter::HandleCockpitOccupancy(APawn* NewOccupant, APawn* OldOccupant)
{
	if (AGalacticPiratesCharacter* OldPilot = Cast<AGalacticPiratesCharacter>(OldOccupant))
	{
		OldPilot->SetOccupiedVehicle(nullptr);
		OldPilot->SetPiloting(false);
		OldPilot->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}

	if (AGalacticPiratesCharacter* NewPilot = Cast<AGalacticPiratesCharacter>(NewOccupant))
	{
		NewPilot->SetOccupiedVehicle(this);
		NewPilot->SetPiloting(true);
		NewPilot->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		bEnabled = false;
		if (ShipMovement)
		{
			ShipMovement->SetThrustInput(FVector::ZeroVector);
			ShipMovement->SetRotationInput(FVector::ZeroVector);
		}
	}
	else if (!NewOccupant)
	{
		bEnabled = true;
	}
}

void ABulldogFighter::HandleHullDestroyed()
{
	if (bWrecked)
	{
		return;
	}
	bWrecked = true;
	bEnabled = false;
	GPBeginCraftWreck(this, 4.0f);
}

bool ABulldogFighter::FireSeekingMissile(AActor* Target)
{
	if (!HasAuthority() || !CanFireMissile() || !MissileSalvo)
	{
		return false;
	}

	if (!MissileSalvo->FireAt(Target, this))
	{
		return false;
	}

	MissileSalvo->AdvanceSalvo(1.0f);
	FireCooldownRemaining = FireCooldown;

	ActiveMissile = nullptr;
	for (TActorIterator<AHeatseekingMissile> It(GetWorld()); It; ++It)
	{
		AHeatseekingMissile* Missile = *It;
		if (Missile && Missile->GetOwner() == this)
		{
			ActiveMissile = Missile;
			break;
		}
	}

	return ActiveMissile.IsValid();
}

void ABulldogFighter::PickNewApproach(AActor* Target)
{
	if (!Target)
	{
		return;
	}

	StrafeSide *= -1.0f;
	const FVector TargetFwd = Target->GetActorForwardVector();
	const FVector TargetRight = Target->GetActorRightVector();
	const FVector TargetUp = Target->GetActorUpVector();
	RunAimPoint = Target->GetActorLocation()
		- TargetFwd * ApproachDistance
		+ TargetRight * (StrafeLateral * StrafeSide)
		+ TargetUp * StrafeHeight;
	Phase = EBulldogStrafePhase::Approach;
}

void ABulldogFighter::SteerToward(const FVector& WorldPoint, const FVector& LookPoint, float ThrottleBoost)
{
	if (!ShipMovement)
	{
		return;
	}

	const FVector ToPoint = WorldPoint - GetActorLocation();
	FVector DesiredDir = ToPoint.GetSafeNormal();
	if (DesiredDir.IsNearlyZero())
	{
		DesiredDir = GetActorForwardVector();
	}

	FVector LookDir = (LookPoint - GetActorLocation()).GetSafeNormal();
	if (LookDir.IsNearlyZero())
	{
		LookDir = DesiredDir;
	}

	const FVector LocalLook = GetActorQuat().UnrotateVector(LookDir);
	const float Yaw = FMath::Clamp(FMath::Atan2(LocalLook.Y, LocalLook.X) / (PI * 0.28f), -1.0f, 1.0f);
	const float Horizontal = FMath::Sqrt(LocalLook.X * LocalLook.X + LocalLook.Y * LocalLook.Y);
	const float Pitch = FMath::Clamp(FMath::Atan2(-LocalLook.Z, Horizontal) / (PI * 0.28f), -1.0f, 1.0f);
	const FVector LocalRight = GetActorQuat().UnrotateVector(DesiredDir);
	const float Roll = FMath::Clamp(LocalRight.Y * 0.35f, -1.0f, 1.0f);

	FVector LocalThrust = GetActorQuat().UnrotateVector(DesiredDir);
	LocalThrust.X = FMath::Clamp(LocalThrust.X + ThrottleBoost, -1.0f, 1.0f);
	LocalThrust.Y = FMath::Clamp(LocalThrust.Y, -1.0f, 1.0f);
	LocalThrust.Z = FMath::Clamp(LocalThrust.Z, -1.0f, 1.0f);

	ShipMovement->SetThrustInput(LocalThrust);
	ShipMovement->SetRotationInput(FVector(Roll, Pitch, Yaw));
}

void ABulldogFighter::TickStrafeAi(float DeltaTime)
{
	AActor* Target = FindAttackTarget();
	CachedTarget = Target;
	if (!Target)
	{
		if (ShipMovement)
		{
			ShipMovement->SetThrustInput(FVector(0.15f, 0.0f, 0.0f));
			ShipMovement->SetRotationInput(FVector::ZeroVector);
		}
		return;
	}

	const FVector SelfLoc = GetActorLocation();
	const FVector TargetLoc = Target->GetActorLocation();
	const FVector ToTarget = TargetLoc - SelfLoc;
	const float Dist = ToTarget.Size();
	const FVector ToTargetDir = ToTarget.GetSafeNormal();
	const float AimDot = FVector::DotProduct(GetActorForwardVector(), ToTargetDir);

	if (Phase == EBulldogStrafePhase::Approach)
	{
		if (RunAimPoint.IsNearlyZero())
		{
			PickNewApproach(Target);
		}

		SteerToward(RunAimPoint, TargetLoc, 0.55f);
		if (FVector::Dist(SelfLoc, RunAimPoint) < 900.0f || (Dist < MaxFireDistance && AimDot > 0.55f))
		{
			Phase = EBulldogStrafePhase::Attack;
			RunAimPoint = TargetLoc + Target->GetActorForwardVector() * PassDistance
				+ Target->GetActorRightVector() * (StrafeLateral * StrafeSide * 0.25f);
		}
		return;
	}

	if (Phase == EBulldogStrafePhase::Attack)
	{
		SteerToward(RunAimPoint, TargetLoc, 0.85f);
		if (CanFireMissile()
			&& Dist >= MinFireDistance
			&& Dist <= MaxFireDistance
			&& AimDot >= FireConeDot)
		{
			FireSeekingMissile(Target);
		}

		const bool bPassed = FVector::DotProduct(GetActorForwardVector(), ToTargetDir) < 0.05f
			|| FVector::Dist(SelfLoc, RunAimPoint) < 700.0f;
		if (bPassed)
		{
			Phase = EBulldogStrafePhase::Breakaway;
			RunAimPoint = SelfLoc
				+ GetActorForwardVector() * 2800.0f
				+ GetActorUpVector() * 1600.0f
				+ GetActorRightVector() * (StrafeSide * 2200.0f);
		}
		return;
	}

	SteerToward(RunAimPoint, RunAimPoint + GetActorForwardVector() * 500.0f, 0.4f);
	if (FVector::Dist(SelfLoc, RunAimPoint) < 900.0f || Dist > ApproachDistance * 0.85f)
	{
		PickNewApproach(Target);
	}
}

void ABulldogFighter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!HasAuthority())
	{
		return;
	}

	FireCooldownRemaining = FMath::Max(0.0f, FireCooldownRemaining - DeltaTime);
	if (bEnabled && !IsPlayerOccupied())
	{
		TickStrafeAi(DeltaTime);
	}
}

bool ABulldogFighter::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	if (const AGalacticPiratesCharacter* Other = Cast<AGalacticPiratesCharacter>(ViewTarget))
	{
		if (Other->GetOccupiedVehicle() == this)
		{
			return true;
		}
		if (HomeCraft && Other->GetBoardedShip() == HomeCraft)
		{
			return true;
		}
	}
	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

ABulldogFighter* ABulldogFighter::SpawnNearShip(UWorld* World, AWalkableShip* TargetShip)
{
	if (!World || !TargetShip)
	{
		return nullptr;
	}

	for (TActorIterator<ABulldogFighter> It(World); It; ++It)
	{
		if (*It && (*It)->GetHomeCraft() == TargetShip)
		{
			return *It;
		}
	}

	const FVector SpawnLoc = TargetShip->GetActorLocation()
		+ TargetShip->GetActorForwardVector() * 11000.0f
		+ TargetShip->GetActorRightVector() * 4200.0f
		+ TargetShip->GetActorUpVector() * 800.0f;
	const FRotator SpawnRot = (-TargetShip->GetActorForwardVector()).Rotation();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ABulldogFighter* Fighter = World->SpawnActor<ABulldogFighter>(SpawnLoc, SpawnRot, Params);
	if (Fighter)
	{
		Fighter->RegisterHomeCraft(TargetShip);
	}
	UE_LOG(LogGalacticPirates, Warning, TEXT("[Bulldog] spawned %s near %s"),
		*GetNameSafe(Fighter),
		*GetNameSafe(TargetShip));
	return Fighter;
}
