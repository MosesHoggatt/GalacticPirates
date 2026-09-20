#include "HeatseekingMissile.h"
#include "WalkableShip.h"
#include "GalacticPiratesCharacter.h"
#include "HoloMapPoiComponent.h"
#include "HoloMapTypes.h"
#include "ShipPolish.h"
#include "GalacticPirates.h"
#include "SpaceCraft.h"
#include "HullHealthComponent.h"
#include "CraftReplication.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "ShipPulseBeamVisual.h"

float GPComputeMissileHeatScore(const FVector& Origin, const FVector& Forward, const FVector& TargetLocation, float TargetHeat, float MinDot)
{
	const FVector ToTarget = TargetLocation - Origin;
	const float DistSq = ToTarget.SizeSquared();
	if (DistSq < 1.0f || TargetHeat <= 0.0f)
	{
		return -1.0f;
	}

	const FVector Dir = ToTarget.GetSafeNormal();
	const float Dot = FVector::DotProduct(Forward.GetSafeNormal(), Dir);
	if (Dot < MinDot)
	{
		return -1.0f;
	}

	return (TargetHeat * Dot) / DistSq;
}

AHeatseekingMissile::AHeatseekingMissile()
{
	PrimaryActorTick.bCanEverTick = true;
	GPCraftNet::Apply(this, GPCraftNet::Missile());

	MissileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MissileMesh"));
	SetRootComponent(MissileMesh);
	MissileMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MissileMesh->SetCollisionObjectType(ECC_WorldDynamic);
	MissileMesh->SetCollisionResponseToAllChannels(ECR_Block);
	MissileMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	MissileMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	MissileMesh->SetNotifyRigidBodyCollision(true);
	MissileMesh->SetGenerateOverlapEvents(true);
	MissileMesh->SetRelativeScale3D(FVector(2.4f, 0.35f, 0.35f));
	MissileMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone"));
	if (ConeMesh.Succeeded())
	{
		MissileMesh->SetStaticMesh(ConeMesh.Object);
	}

	ExhaustLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ExhaustLight"));
	ExhaustLight->SetupAttachment(MissileMesh);
	ExhaustLight->SetRelativeLocation(FVector(-80.0f, 0.0f, 0.0f));
	ExhaustLight->SetIntensity(2500.0f);
	ExhaustLight->SetAttenuationRadius(420.0f);
	ExhaustLight->SetLightColor(FLinearColor(1.0f, 0.35f, 0.08f));
	ExhaustLight->SetCastShadows(false);

	HoloPoi = CreateDefaultSubobject<UHoloMapPoiComponent>(TEXT("HoloPoi"));
	HoloPoi->SetupAttachment(MissileMesh);
	HoloPoi->Kind = EHoloMapPoiKind::Missile;
	HoloPoi->Primitive = EHoloMapPrimitive::Sphere;
	HoloPoi->bOverridePrimitive = true;
	HoloPoi->MarkerColor = FLinearColor(1.0f, 0.92f, 0.15f, 1.0f);
	HoloPoi->bOverrideColor = true;
	HoloPoi->MarkerScale = FVector(0.04f, 0.04f, 0.04f);
	HoloPoi->bVisibleOnMaps = true;

	HullHealth = CreateDefaultSubobject<UHullHealthComponent>(TEXT("HullHealth"));
	HullHealth->MaxHealth = MissileHealth;
	HullHealth->ArmorClass = EShipArmorClass::Unarmored;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = MissileMesh;
	ProjectileMovement->InitialSpeed = 5040.0f;
	ProjectileMovement->MaxSpeed = 8640.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bIsHomingProjectile = true;
	ProjectileMovement->HomingAccelerationMagnitude = HomingAcceleration;
	ProjectileMovement->bSweepCollision = true;
}

void AHeatseekingMissile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHeatseekingMissile, SourceActor);
	DOREPLIFETIME(AHeatseekingMissile, LockedTarget);
}

void AHeatseekingMissile::BeginPlay()
{
	Super::BeginPlay();
	GPApplyPolishVfxMaterial(MissileMesh, TEXT("circle_05"), FLinearColor(1.0f, 0.28f, 0.06f, 1.0f));
	IgnoreSourceCollision();
	FuseElapsed = 0.0f;
	if (HullHealth)
	{
		HullHealth->MaxHealth = MissileHealth;
		HullHealth->OnHullDestroyed.AddDynamic(this, &AHeatseekingMissile::HandleHullDestroyed);
		if (HasAuthority())
		{
			HullHealth->ResetToFull();
		}
	}
	if (HasAuthority())
	{
		SetLifeSpan(0.0f);
	}
}

void AHeatseekingMissile::InitializeMissile(AActor* InSource, AActor* InTarget, APawn* InInstigator, float InDamage, float Speed, const FVector& InheritedVelocity)
{
	SourceActor = InSource;
	InstigatorPawn = InInstigator;
	SetInstigator(InInstigator);
	SetOwner(InSource ? InSource : InInstigator);
	Damage = InDamage;
	if (HullHealth)
	{
		HullHealth->MaxHealth = MissileHealth;
		HullHealth->ResetToFull();
	}
	IgnoreSourceCollision();

	if (ProjectileMovement)
	{
		const FVector LaunchVelocity = GetActorForwardVector() * Speed + InheritedVelocity;
		ProjectileMovement->InitialSpeed = LaunchVelocity.Size();
		ProjectileMovement->MaxSpeed = FMath::Max(LaunchVelocity.Size() * 1.5f, 8640.0f);
		ProjectileMovement->Velocity = LaunchVelocity;
		ProjectileMovement->HomingAccelerationMagnitude = HomingAcceleration;
	}

	if (HasAuthority())
	{
		LockedTarget = InTarget;
		if (!LockedTarget)
		{
			LockedTarget = FindHottestTarget();
		}
		ApplyHoming();
	}

	OnRep_LockedTarget();
}

void AHeatseekingMissile::IgnoreSourceCollision()
{
	if (!MissileMesh)
	{
		return;
	}

	if (AWalkableShip* SourceShip = Cast<AWalkableShip>(SourceActor))
	{
		MissileMesh->IgnoreActorWhenMoving(SourceShip, true);
		for (AGalacticPiratesCharacter* Aboard : SourceShip->GetPlayersAboard())
		{
			if (Aboard)
			{
				MissileMesh->IgnoreActorWhenMoving(Aboard, true);
			}
		}
	}
	else if (SourceActor)
	{
		MissileMesh->IgnoreActorWhenMoving(SourceActor, true);
	}

	if (AActor* SourceOwner = GetOwner())
	{
		MissileMesh->IgnoreActorWhenMoving(SourceOwner, true);
	}
	if (InstigatorPawn)
	{
		MissileMesh->IgnoreActorWhenMoving(InstigatorPawn, true);
	}
}

AWalkableShip* AHeatseekingMissile::GetLockedTarget() const
{
	return Cast<AWalkableShip>(LockedTarget);
}

AWalkableShip* AHeatseekingMissile::GetSourceShip() const
{
	return Cast<AWalkableShip>(SourceActor);
}

AActor* AHeatseekingMissile::FindHottestTarget() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	AActor* Best = nullptr;
	float BestScore = 0.0f;
	const FVector Origin = GetActorLocation();
	const FVector Forward = GetActorForwardVector();

	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		ISpaceCraft* Craft = GPAsSpaceCraft(Pawn);
		if (!Pawn || Pawn == SourceActor || !Craft || Craft->IsCraftWrecked())
		{
			continue;
		}

		if (SourceActor && !GPAreHostile(SourceActor, Pawn))
		{
			continue;
		}
		if (ISpaceCraft* SourceCraft = GPAsSpaceCraft(SourceActor))
		{
			if (SourceCraft->GetHomeCraft() == Pawn)
			{
				continue;
			}
		}

		const float Health = Craft->GetHullHealth() ? Craft->GetHullHealth()->GetHealth() : 0.0f;
		const float SpeedCm = Craft->GetCraftVelocity().Size();
		const float Heat = Health + SpeedCm * 0.15f + 200.0f;
		const float Score = GPComputeMissileHeatScore(Origin, Forward, Pawn->GetActorLocation(), Heat, HeatSeekMinDot);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Pawn;
		}
	}

	return Best;
}

void AHeatseekingMissile::AcquireOrRefreshTarget()
{
	if (!HasAuthority())
	{
		return;
	}

	if (LockedTarget && (!IsValid(LockedTarget) || GPIsCraftWrecked(LockedTarget)))
	{
		LockedTarget = nullptr;
	}

	AActor* Hottest = FindHottestTarget();
	if (!Hottest)
	{
		return;
	}

	if (!LockedTarget)
	{
		LockedTarget = Hottest;
		ApplyHoming();
		return;
	}

	auto HeatOf = [](AActor* Actor) -> float
	{
		if (ISpaceCraft* Craft = GPAsSpaceCraft(Actor))
		{
			return Craft->GetHullHealth() ? Craft->GetHullHealth()->GetHealth() + 200.0f : 200.0f;
		}
		return 200.0f;
	};

	const FVector Origin = GetActorLocation();
	const FVector Forward = GetActorForwardVector();
	const float LockedScore = GPComputeMissileHeatScore(Origin, Forward, LockedTarget->GetActorLocation(), HeatOf(LockedTarget), HeatSeekMinDot * 0.5f);
	const float NewScore = GPComputeMissileHeatScore(Origin, Forward, Hottest->GetActorLocation(), HeatOf(Hottest), HeatSeekMinDot);
	if (NewScore > LockedScore * 1.35f)
	{
		LockedTarget = Hottest;
		ApplyHoming();
	}
}

void AHeatseekingMissile::ApplyHoming()
{
	if (!ProjectileMovement)
	{
		return;
	}

	USceneComponent* HomingComp = nullptr;
	if (ISpaceCraft* Craft = GPAsSpaceCraft(LockedTarget))
	{
		HomingComp = Craft->GetHomingSceneComponent();
	}
	else if (LockedTarget)
	{
		HomingComp = LockedTarget->GetRootComponent();
	}

	ProjectileMovement->bIsHomingProjectile = HomingComp != nullptr;
	ProjectileMovement->HomingTargetComponent = HomingComp;
	ProjectileMovement->HomingAccelerationMagnitude = HomingAcceleration;
}

void AHeatseekingMissile::OnRep_LockedTarget()
{
	ApplyHoming();
}

void AHeatseekingMissile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!HasAuthority() || bDetonated)
	{
		return;
	}

	RetargetTimer += DeltaTime;
	FuseElapsed += DeltaTime;
	if (FuseElapsed >= FuseSeconds)
	{
		Detonate(nullptr);
		return;
	}

	if (RetargetTimer >= RetargetInterval)
	{
		RetargetTimer = 0.0f;
		AcquireOrRefreshTarget();
		ApplyHoming();
	}
}

void AHeatseekingMissile::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	if (!HasAuthority())
	{
		return;
	}
	AActor* Other = OtherActor;
	if (!Other || Other == SourceActor || GPIsCraftWrecked(Other))
	{
		return;
	}
	if (GPAsSpaceCraft(Other) || Cast<AWalkableShip>(Other))
	{
		Detonate(Other);
	}
}

void AHeatseekingMissile::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
	if (!HasAuthority())
	{
		return;
	}
	AActor* HitActor = Other;
	if (!HitActor && OtherComp)
	{
		HitActor = OtherComp->GetOwner();
	}
	if (HitActor == SourceActor)
	{
		return;
	}
	Detonate(GPAsSpaceCraft(HitActor) ? HitActor : nullptr);
}

void AHeatseekingMissile::PlayDetonationFx(const FVector& Location, bool bShipHit)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (MissileMesh)
	{
		MissileMesh->SetVisibility(false, true);
		MissileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (ExhaustLight)
	{
		ExhaustLight->SetIntensity(0.0f);
	}

	const float BlastScale = bShipHit ? 380.0f : 160.0f;
	const float BlastDuration = bShipHit ? 1.05f : 0.55f;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AShipPulseBeamVisual* Burst = World->SpawnActor<AShipPulseBeamVisual>(AShipPulseBeamVisual::StaticClass(), Location, FRotator::ZeroRotator, Params))
	{
		const FLinearColor Color = bShipHit
			? FLinearColor(1.0f, 0.32f, 0.06f, 1.0f)
			: FLinearColor(1.0f, 0.62f, 0.18f, 1.0f);
		Burst->InitializeExplosion(Location, BlastScale, BlastDuration, Color);
	}

	if (bShipHit)
	{
		GPPlayPolishSoundAt(this, TEXT("SFX_ExplosionHit"), Location, 1.05f);
		GPPlayPolishSoundAt(this, TEXT("SFX_ExplosionBass"), Location, 0.7f);
		GPPlayPolishSoundAt(this, TEXT("SFX_Impact"), Location, 0.85f);
		GPPlayExplosionCameraShake(World, Location, 180.0f, 3200.0f, 0.55f);
	}
	else
	{
		GPPlayPolishSoundAt(this, TEXT("SFX_ExplosionFizzle"), Location, 0.32f);
		GPPlayExplosionCameraShake(World, Location, 80.0f, 1600.0f, 0.18f);
	}
}

void AHeatseekingMissile::Multicast_DetonateFx_Implementation(FVector_NetQuantize Location, bool bShipHit)
{
	PlayDetonationFx(Location, bShipHit);
}

void AHeatseekingMissile::HandleHullDestroyed()
{
	Detonate(nullptr);
}

void AHeatseekingMissile::Detonate(AActor* HitActor)
{
	if (bDetonated)
	{
		return;
	}
	bDetonated = true;

	const FVector Location = GetActorLocation();
	const bool bShipHit = HitActor && HitActor != SourceActor && GPAsSpaceCraft(HitActor);
	if (HasAuthority() && bShipHit)
	{
		FSpaceDamageEvent Event;
		Event.Amount = Damage;
		Event.Kind = ESpaceDamageKind::Missile;
		Event.InstigatorPawn = InstigatorPawn;
		Event.Causer = this;
		GPApplySpaceDamage(HitActor, Event);
	}

	if (HasAuthority())
	{
		Multicast_DetonateFx(Location, bShipHit);
	}
	else
	{
		PlayDetonationFx(Location, bShipHit);
	}

	if (GPCombatLogEnabled())
	{
		UE_LOG(LogGalacticPirates, Log, TEXT("[Missile] Detonate hit=%s src=%s dmg=%.1f net=%d"),
			*GetNameSafe(HitActor),
			*GetNameSafe(SourceActor),
			Damage,
			static_cast<int32>(GetNetMode()));
	}

	Destroy();
}

bool AHeatseekingMissile::ApplyMinigunHit(float InDamage, APawn* InInstigator)
{
	if (!HasAuthority() || bDetonated || InDamage <= 0.0f)
	{
		return false;
	}

	InstigatorPawn = InInstigator;
	UHullHealthComponent* Hull = HullHealth ? HullHealth.Get() : GPFindHullHealth(this);
	if (Hull)
	{
		Hull->MaxHealth = MissileHealth;
		Hull->ArmorClass = EShipArmorClass::Unarmored;
		if (Hull->GetHealth() > MissileHealth)
		{
			Hull->SetHealth(MissileHealth);
		}

		FSpaceDamageEvent Event;
		Event.Amount = InDamage;
		Event.Kind = ESpaceDamageKind::Ballistic;
		Event.InstigatorPawn = InInstigator;
		Event.Causer = InInstigator;
		Hull->ApplyDamage(Event);
	}

	if (!Hull || Hull->IsDestroyed() || (Hull && Hull->GetHealth() <= 0.0f))
	{
		Detonate(nullptr);
	}

	return bDetonated || !IsValid(this);
}
