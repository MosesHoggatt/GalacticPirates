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
#include "MinigunMuzzleComponent.h"
#include "OccupancyComponent.h"
#include "CraftReplication.h"
#include "CraftWreck.h"
#include "GalacticPiratesCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HoloMapScanRange.h"
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
	ShipMovement->ForwardThrustPower = 2400000.0f;
	ShipMovement->StrafeThrustPower = 900000.0f;
	ShipMovement->VerticalThrustPower = 900000.0f;
	ShipMovement->PitchTorque = 48000000.0f;
	ShipMovement->YawTorque = 81000000.0f;
	ShipMovement->RollTorque = 24000000.0f;
	ShipMovement->MaxLinearVelocity = 9000.0f;
	ShipMovement->MaxAngularVelocity = 480.0f;
	ShipMovement->TranslationDampening = 0.06f;
	ShipMovement->RotationDampening = 0.22f;

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

	GunHardpoint = CreateDefaultSubobject<UWeaponHardpointComponent>(TEXT("GunHardpoint"));
	GunHardpoint->SetupAttachment(HullMesh);
	GunHardpoint->SetRelativeLocation(FVector(280.0f, 0.0f, 18.0f));

	NoseGun = CreateDefaultSubobject<UMinigunMuzzleComponent>(TEXT("NoseGun"));
	NoseGun->SetupAttachment(GunHardpoint);
	NoseGun->FireInterval = 0.06f;
	GunHardpoint->EquippedWeapon = NoseGun;
	GunHardpoint->WeaponClass = UMinigunMuzzleComponent::StaticClass();

	CockpitOccupancy = CreateDefaultSubobject<UOccupancyComponent>(TEXT("CockpitOccupancy"));
	CockpitOccupancy->SetupAttachment(HullMesh);
	CockpitOccupancy->SetRelativeLocation(FVector(-240.0f, 0.0f, 40.0f));
	CockpitOccupancy->InteractRange = 180.0f;

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
	DOREPLIFETIME(ABulldogFighter, bHullDocked);
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

void ABulldogFighter::DockToHull(AWalkableShip* Host)
{
	if (!Host)
	{
		return;
	}

	RegisterHomeCraft(Host);
	USceneComponent* Dock = Host->HangarDock ? static_cast<USceneComponent*>(Host->HangarDock) : Host->GetRootComponent();
	AttachToComponent(Dock, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	bHullDocked = true;
	bEnabled = false;
	if (Collision)
	{
		Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	if (ShipMovement)
	{
		ShipMovement->SetThrustInput(FVector::ZeroVector);
		ShipMovement->SetRotationInput(FVector::ZeroVector);
		ShipMovement->SetLinearVelocity(FVector::ZeroVector);
		ShipMovement->SetAngularVelocity(FVector::ZeroVector);
	}
	UE_LOG(LogGalacticPirates, Warning, TEXT("[Hangar] %s docked to %s at %s"),
		*GetName(),
		*GetNameSafe(Host),
		*GetActorLocation().ToCompactString());
}

void ABulldogFighter::UndockFromHull()
{
	if (!bHullDocked && !GetAttachParentActor())
	{
		return;
	}

	const FVector Loc = GetActorLocation();
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	bHullDocked = false;
	if (Collision)
	{
		Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}
	UE_LOG(LogGalacticPirates, Warning, TEXT("[Hangar] %s undocked at %s"), *GetName(), *Loc.ToCompactString());
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
	if (bHullDocked)
	{
		UndockFromHull();
	}
	ShipMovement->SetThrustInput(ThrustInput);
	ShipMovement->SetRotationInput(RotationInput);
	UE_LOG(LogGalacticPirates, Warning, TEXT("[FighterPilot] %s thrust=%s rot=%s loc=%s"),
		*GetName(),
		*ThrustInput.ToCompactString(),
		*RotationInput.ToCompactString(),
		*GetActorLocation().ToCompactString());
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
		UndockFromHull();
		if (NewPilot->GetBoardedShip())
		{
			NewPilot->LeaveShip();
		}
		NewPilot->SetOccupiedVehicle(this);
		NewPilot->SetPiloting(true);
		NewPilot->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		bEnabled = false;
		if (ShipMovement)
		{
			ShipMovement->SetThrustInput(FVector::ZeroVector);
			ShipMovement->SetRotationInput(FVector::ZeroVector);
		}
		UE_LOG(LogGalacticPirates, Warning, TEXT("[FighterPilot] %s entered %s"), *GetNameSafe(NewPilot), *GetName());
	}
	else if (!NewOccupant)
	{
		bEnabled = !bHullDocked;
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
	if (NoseGun)
	{
		NoseGun->SetFiring(false);
	}
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

float ABulldogFighter::GetMapRangeCm() const
{
	return GPHoloMapScanRangeCm();
}

void ABulldogFighter::PickNewOutbound(AActor* Target)
{
	if (!Target)
	{
		return;
	}

	StrafeSide *= -1.0f;
	const FVector TargetLoc = Target->GetActorLocation();
	FVector Away = (GetActorLocation() - TargetLoc).GetSafeNormal();
	if (Away.IsNearlyZero())
	{
		Away = Target->GetActorRightVector() * StrafeSide;
	}
	FVector Right = FVector::CrossProduct(Target->GetActorUpVector(), Away).GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		Right = Target->GetActorRightVector();
	}
	RunAxis = Away;
	RunOffset = Right * (StrafeLateral * StrafeSide) + Target->GetActorUpVector() * StrafeHeight;
	RunAimPoint = TargetLoc + Away * GetMapRangeCm();
	Phase = EBulldogStrafePhase::Outbound;
}

void ABulldogFighter::SteerToward(const FVector& WorldPoint, const FVector& LookPoint, float ThrottleBoost, bool bBrake)
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
	const float Yaw = FMath::Clamp(FMath::Atan2(LocalLook.Y, LocalLook.X) / (PI * 0.22f), -1.0f, 1.0f);
	const float Horizontal = FMath::Sqrt(LocalLook.X * LocalLook.X + LocalLook.Y * LocalLook.Y);
	const float Pitch = FMath::Clamp(FMath::Atan2(-LocalLook.Z, Horizontal) / (PI * 0.22f), -1.0f, 1.0f);
	const FVector LocalRight = GetActorQuat().UnrotateVector(DesiredDir);
	const float Roll = FMath::Clamp(LocalRight.Y * 0.35f, -1.0f, 1.0f);

	FVector LocalThrust = GetActorQuat().UnrotateVector(DesiredDir);
	if (bBrake)
	{
		const FVector LocalVel = GetActorQuat().UnrotateVector(GetCraftVelocity());
		const float Speed = LocalVel.Size();
		if (Speed > 250.0f)
		{
			LocalThrust = -LocalVel / Speed;
		}
	}
	else
	{
		LocalThrust.X = FMath::Clamp(LocalThrust.X + ThrottleBoost, -1.0f, 1.0f);
	}
	LocalThrust.X = FMath::Clamp(LocalThrust.X, -1.0f, 1.0f);
	LocalThrust.Y = FMath::Clamp(LocalThrust.Y, -1.0f, 1.0f);
	LocalThrust.Z = FMath::Clamp(LocalThrust.Z, -1.0f, 1.0f);

	ShipMovement->SetThrustInput(LocalThrust);
	ShipMovement->SetRotationInput(FVector(Roll, Pitch, Yaw));
}

void ABulldogFighter::UpdateNoseGun(AActor* Target, float Dist, float AimDot)
{
	if (!NoseGun)
	{
		return;
	}

	const bool bAimed = Target
		&& Dist <= MaxFireDistance
		&& AimDot >= FireConeDot
		&& !IsCraftWrecked();
	NoseGun->SetFiring(bAimed);
}

void ABulldogFighter::TickStrafeAi(float DeltaTime)
{
	AActor* Target = FindAttackTarget();
	CachedTarget = Target;
	if (!Target)
	{
		UpdateNoseGun(nullptr, 0.0f, 0.0f);
		if (ShipMovement)
		{
			ShipMovement->SetThrustInput(FVector(0.35f, 0.0f, 0.0f));
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
	const float MapRange = GetMapRangeCm();
	UpdateNoseGun(Target, Dist, AimDot);

	AiLogTimer -= DeltaTime;
	if (AiLogTimer <= 0.0f)
	{
		AiLogTimer = 0.25f;
		const TCHAR* PhaseName = TEXT("Outbound");
		if (Phase == EBulldogStrafePhase::TurnIn)
		{
			PhaseName = TEXT("TurnIn");
		}
		else if (Phase == EBulldogStrafePhase::Strafe)
		{
			PhaseName = TEXT("Strafe");
		}
		UE_LOG(LogGalacticPirates, Warning,
			TEXT("[BulldogAi] %s phase=%s dist=%.0f map=%.0f speed=%.0f out=%.0f aim=%.2f"),
			*GetName(),
			PhaseName,
			Dist,
			MapRange,
			GetCraftVelocity().Size(),
			FVector::DotProduct(GetCraftVelocity(), -ToTargetDir),
			AimDot);
	}

	const float OutboundSpeed = FVector::DotProduct(GetCraftVelocity(), -ToTargetDir);

	if (Phase == EBulldogStrafePhase::Outbound)
	{
		if (RunAimPoint.IsNearlyZero())
		{
			PickNewOutbound(Target);
		}

		const FVector EdgePoint = TargetLoc + RunAxis * MapRange;
		const bool bNearEdge = Dist >= MapRange * 0.86f || (Dist >= MapRange * 0.70f && OutboundSpeed < 900.0f);
		SteerToward(EdgePoint, EdgePoint, Dist > MapRange * 0.55f ? 0.0f : 1.0f, Dist >= MapRange * 0.70f);
		if (bNearEdge)
		{
			Phase = EBulldogStrafePhase::TurnIn;
			UE_LOG(LogGalacticPirates, Warning, TEXT("[BulldogAi] %s reached map edge dist=%.0f speedOut=%.0f — hard turn"), *GetName(), Dist, OutboundSpeed);
		}
		return;
	}

	if (Phase == EBulldogStrafePhase::TurnIn)
	{
		FVector WorldPull = ToTargetDir * 1.6f;
		FVector LocalThrust = GetActorQuat().UnrotateVector(WorldPull);
		LocalThrust.X = FMath::Clamp(LocalThrust.X, -1.0f, 1.0f);
		LocalThrust.Y = FMath::Clamp(LocalThrust.Y, -1.0f, 1.0f);
		LocalThrust.Z = FMath::Clamp(LocalThrust.Z, -1.0f, 1.0f);
		const FVector LocalLook = GetActorQuat().UnrotateVector(ToTargetDir);
		const float Yaw = FMath::Clamp(FMath::Atan2(LocalLook.Y, LocalLook.X) / (PI * 0.10f), -1.0f, 1.0f);
		const float Horizontal = FMath::Sqrt(LocalLook.X * LocalLook.X + LocalLook.Y * LocalLook.Y);
		const float Pitch = FMath::Clamp(FMath::Atan2(-LocalLook.Z, Horizontal) / (PI * 0.10f), -1.0f, 1.0f);
		if (ShipMovement)
		{
			ShipMovement->SetThrustInput(LocalThrust);
			ShipMovement->SetRotationInput(FVector(0.0f, Pitch, Yaw));
		}
		if (AimDot >= 0.55f)
		{
			Phase = EBulldogStrafePhase::Strafe;
			RunAimPoint = TargetLoc + RunOffset;
			UE_LOG(LogGalacticPirates, Warning, TEXT("[BulldogAi] %s turned in aim=%.2f dist=%.0f speedOut=%.0f — strafe"), *GetName(), AimDot, Dist, OutboundSpeed);
		}
		return;
	}

	SteerToward(TargetLoc + RunOffset, TargetLoc, AimDot > 0.35f ? 1.0f : 0.0f, false);
	if (CanFireMissile()
		&& Dist >= MinFireDistance
		&& Dist <= MissileEngageDistance
		&& (AimDot >= MissileAimDot || Dist <= 1800.0f))
	{
		FireSeekingMissile(Target);
	}

	const bool bPassed = OutboundSpeed > 250.0f && Dist < MapRange * 0.35f && AimDot < 0.20f;
	if (bPassed)
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[BulldogAi] %s finished strafe dist=%.0f — outbound again"), *GetName(), Dist);
		PickNewOutbound(Target);
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
	if (bHullDocked)
	{
		return;
	}
	if (bEnabled && !IsPlayerOccupied())
	{
		TickStrafeAi(DeltaTime);
	}
	else if (NoseGun)
	{
		NoseGun->SetFiring(false);
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
		+ TargetShip->GetActorForwardVector() * 2800.0f
		+ TargetShip->GetActorRightVector() * 900.0f
		+ TargetShip->GetActorUpVector() * 400.0f;
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

ABulldogFighter* ABulldogFighter::SpawnDockedOnShip(UWorld* World, AWalkableShip* TargetShip)
{
	if (!World || !TargetShip)
	{
		return nullptr;
	}

	for (TActorIterator<ABulldogFighter> It(World); It; ++It)
	{
		ABulldogFighter* Existing = *It;
		if (Existing && Existing->GetHomeCraft() == TargetShip)
		{
			if (!Existing->IsHullDocked() && !Existing->HasHumanOccupant())
			{
				Existing->DockToHull(TargetShip);
			}
			return Existing;
		}
	}

	USceneComponent* Dock = TargetShip->HangarDock ? static_cast<USceneComponent*>(TargetShip->HangarDock) : TargetShip->GetRootComponent();
	const FTransform DockTM = Dock ? Dock->GetComponentTransform() : TargetShip->GetActorTransform();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABulldogFighter* Fighter = World->SpawnActor<ABulldogFighter>(DockTM.GetLocation(), DockTM.Rotator(), Params);
	if (Fighter)
	{
		Fighter->DockToHull(TargetShip);
	}
	UE_LOG(LogGalacticPirates, Warning, TEXT("[Hangar] spawned docked %s on %s"),
		*GetNameSafe(Fighter),
		*GetNameSafe(TargetShip));
	return Fighter;
}
