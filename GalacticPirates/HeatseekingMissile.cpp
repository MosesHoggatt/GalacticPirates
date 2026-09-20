#include "HeatseekingMissile.h"
#include "WalkableShip.h"
#include "GalacticPiratesCharacter.h"
#include "HoloMapPoiComponent.h"
#include "HoloMapTypes.h"
#include "ShipPolish.h"
#include "GalacticPirates.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

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
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(60.0f);
	SetMinNetUpdateFrequency(20.0f);
	bAlwaysRelevant = true;

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

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = MissileMesh;
	ProjectileMovement->InitialSpeed = 4200.0f;
	ProjectileMovement->MaxSpeed = 7200.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bIsHomingProjectile = true;
	ProjectileMovement->HomingAccelerationMagnitude = HomingAcceleration;
	ProjectileMovement->bSweepCollision = true;
}

void AHeatseekingMissile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHeatseekingMissile, SourceShip);
	DOREPLIFETIME(AHeatseekingMissile, LockedTarget);
}

void AHeatseekingMissile::BeginPlay()
{
	Super::BeginPlay();
	GPApplyPolishVfxMaterial(MissileMesh, TEXT("circle_05"), FLinearColor(1.0f, 0.28f, 0.06f, 1.0f));
	IgnoreSourceCollision();
	if (HasAuthority())
	{
		RemainingHealth = MissileHealth;
		SetLifeSpan(FuseSeconds);
	}
}

void AHeatseekingMissile::InitializeMissile(AWalkableShip* InSourceShip, AWalkableShip* InTarget, APawn* InInstigator, float InDamage, float Speed)
{
	SourceShip = InSourceShip;
	InstigatorPawn = InInstigator;
	SetInstigator(InInstigator);
	SetOwner(InSourceShip);
	Damage = InDamage;
	RemainingHealth = MissileHealth;
	IgnoreSourceCollision();

	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = Speed;
		ProjectileMovement->MaxSpeed = FMath::Max(Speed, ProjectileMovement->MaxSpeed);
		ProjectileMovement->Velocity = GetActorForwardVector() * Speed;
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
	if (!SourceShip || !MissileMesh)
	{
		return;
	}

	MissileMesh->IgnoreActorWhenMoving(SourceShip, true);
	for (AGalacticPiratesCharacter* Aboard : SourceShip->GetPlayersAboard())
	{
		if (Aboard)
		{
			MissileMesh->IgnoreActorWhenMoving(Aboard, true);
		}
	}
}

AWalkableShip* AHeatseekingMissile::FindHottestTarget() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	AWalkableShip* Best = nullptr;
	float BestScore = 0.0f;
	const FVector Origin = GetActorLocation();
	const FVector Forward = GetActorForwardVector();

	for (TActorIterator<AWalkableShip> It(World); It; ++It)
	{
		AWalkableShip* Ship = *It;
		if (!Ship || Ship == SourceShip || Ship->IsWrecked())
		{
			continue;
		}

		if (UHoloMapPoiComponent* Poi = Ship->HoloPoi)
		{
			if (!Poi->IsHostileTo(SourceShip))
			{
				continue;
			}
		}

		const float SpeedCm = Ship->GetPointVelocity(Ship->GetActorLocation()).Size();
		const float Heat = Ship->GetHealth() + SpeedCm * 0.15f + 200.0f;
		const float Score = GPComputeMissileHeatScore(Origin, Forward, Ship->GetActorLocation(), Heat, HeatSeekMinDot);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Ship;
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

	if (LockedTarget && (LockedTarget->IsWrecked() || !IsValid(LockedTarget)))
	{
		LockedTarget = nullptr;
	}

	AWalkableShip* Hottest = FindHottestTarget();
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

	const FVector Origin = GetActorLocation();
	const FVector Forward = GetActorForwardVector();
	const float LockedHeat = LockedTarget->GetHealth() + 200.0f;
	const float NewHeat = Hottest->GetHealth() + 200.0f;
	const float LockedScore = GPComputeMissileHeatScore(Origin, Forward, LockedTarget->GetActorLocation(), LockedHeat, HeatSeekMinDot * 0.5f);
	const float NewScore = GPComputeMissileHeatScore(Origin, Forward, Hottest->GetActorLocation(), NewHeat, HeatSeekMinDot);
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

	ProjectileMovement->bIsHomingProjectile = LockedTarget != nullptr;
	ProjectileMovement->HomingTargetComponent = LockedTarget && LockedTarget->CombatHull
		? static_cast<USceneComponent*>(LockedTarget->CombatHull)
		: (LockedTarget ? LockedTarget->GetRootComponent() : nullptr);
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
	AWalkableShip* Ship = Cast<AWalkableShip>(OtherActor);
	if (Ship && Ship != SourceShip)
	{
		Detonate(Ship);
	}
}

void AHeatseekingMissile::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
	AWalkableShip* Ship = Cast<AWalkableShip>(Other);
	if (!Ship && OtherComp)
	{
		Ship = Cast<AWalkableShip>(OtherComp->GetOwner());
	}
	if (Ship == SourceShip)
	{
		return;
	}
	Detonate(Ship);
}

void AHeatseekingMissile::Detonate(AWalkableShip* HitShip)
{
	if (bDetonated)
	{
		return;
	}
	bDetonated = true;

	if (HasAuthority() && HitShip && HitShip != SourceShip)
	{
		HitShip->ApplyShipDamage(Damage, Cast<AGalacticPiratesCharacter>(InstigatorPawn), this);
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		GPPlayPolishSoundAt(this, TEXT("SFX_Impact"), GetActorLocation(), 1.1f);
		GPPlayExplosionCameraShake(GetWorld(), GetActorLocation(), 180.0f, 3500.0f, 0.45f);
	}

	UE_LOG(LogGalacticPirates, Warning, TEXT("[Missile] Detonate hit=%s src=%s dmg=%.1f net=%d"),
		*GetNameSafe(HitShip),
		*GetNameSafe(SourceShip),
		Damage,
		static_cast<int32>(GetNetMode()));

	Destroy();
}

bool AHeatseekingMissile::ApplyMinigunHit(float InDamage, APawn* InInstigator)
{
	if (!HasAuthority() || bDetonated || InDamage <= 0.0f)
	{
		return false;
	}

	InstigatorPawn = InInstigator;
	RemainingHealth -= InDamage;
	if (RemainingHealth <= 0.0f)
	{
		Detonate(nullptr);
		return true;
	}
	return true;
}
