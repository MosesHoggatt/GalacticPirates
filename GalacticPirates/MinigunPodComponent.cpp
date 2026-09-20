#include "MinigunPodComponent.h"
#include "WalkableShip.h"
#include "GalacticPiratesCharacter.h"
#include "HeatseekingMissile.h"
#include "CombatTypes.h"
#include "ShipPolish.h"
#include "GalacticPirates.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "CollisionQueryParams.h"
#include "UObject/ConstructorHelpers.h"
#include "Interfaces/MovementBaseInterface.h"
#include "GameFramework/CharacterMovementComponent.h"

UMinigunPodComponent::UMinigunPodComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	SetMobility(EComponentMobility::Movable);

	PodMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PodMesh"));
	PodMesh->SetupAttachment(this);
	PodMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -20.0f));
	PodMesh->SetRelativeScale3D(FVector(1.6f, 1.6f, 0.35f));
	PodMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PodMesh->SetCastShadow(false);

	YawMount = CreateDefaultSubobject<USceneComponent>(TEXT("YawMount"));
	YawMount->SetupAttachment(this);
	YawMount->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));

	PitchMount = CreateDefaultSubobject<USceneComponent>(TEXT("PitchMount"));
	PitchMount->SetupAttachment(YawMount);

	BarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrelMesh"));
	BarrelMesh->SetupAttachment(PitchMount);
	BarrelMesh->SetRelativeLocation(FVector(90.0f, 0.0f, 0.0f));
	BarrelMesh->SetRelativeScale3D(FVector(2.2f, 0.22f, 0.22f));
	BarrelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BarrelMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere"));
	if (CubeMesh.Succeeded())
	{
		PodMesh->SetStaticMesh(CubeMesh.Object);
	}
	if (CylinderMesh.Succeeded())
	{
		BarrelMesh->SetStaticMesh(CylinderMesh.Object);
		BarrelMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
	}

	MuzzleFlashMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MuzzleFlashMesh"));
	MuzzleFlashMesh->SetupAttachment(PitchMount);
	MuzzleFlashMesh->SetRelativeLocation(FVector(190.0f, 0.0f, 0.0f));
	MuzzleFlashMesh->SetRelativeScale3D(FVector(0.55f, 0.35f, 0.35f));
	MuzzleFlashMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MuzzleFlashMesh->SetCastShadow(false);
	MuzzleFlashMesh->SetVisibility(false);
	MuzzleFlashMesh->SetStaticMesh(SphereMesh.Succeeded() ? SphereMesh.Object : nullptr);

	ImpactFlashMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ImpactFlashMesh"));
	ImpactFlashMesh->SetupAttachment(this);
	ImpactFlashMesh->SetRelativeScale3D(FVector(0.9f));
	ImpactFlashMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ImpactFlashMesh->SetCastShadow(false);
	ImpactFlashMesh->SetVisibility(false);
	ImpactFlashMesh->SetStaticMesh(SphereMesh.Succeeded() ? SphereMesh.Object : nullptr);

	TracerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TracerMesh"));
	TracerMesh->SetupAttachment(this);
	TracerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TracerMesh->SetCastShadow(false);
	TracerMesh->SetVisibility(false);
	TracerMesh->SetStaticMesh(CylinderMesh.Succeeded() ? CylinderMesh.Object : nullptr);

	MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
	MuzzleLight->SetupAttachment(PitchMount);
	MuzzleLight->SetRelativeLocation(FVector(190.0f, 0.0f, 0.0f));
	MuzzleLight->SetAttenuationRadius(900.0f);
	MuzzleLight->SetLightColor(FLinearColor(1.0f, 0.72f, 0.28f));
	MuzzleLight->SetIntensity(0.0f);
	MuzzleLight->SetCastShadows(false);
}

void UMinigunPodComponent::BeginPlay()
{
	Super::BeginPlay();
	OwningShip = Cast<AWalkableShip>(GetOwner());
	GPApplyPolishVfxMaterial(PodMesh, TEXT("circle_05"), FLinearColor(0.25f, 0.28f, 0.32f, 1.0f));
	GPApplyPolishVfxMaterial(BarrelMesh, TEXT("circle_05"), FLinearColor(0.55f, 0.45f, 0.2f, 1.0f));
	GPApplyPolishVfxMaterial(MuzzleFlashMesh, TEXT("muzzle_01"), FLinearColor(6.0f, 3.4f, 1.1f, 1.0f));
	GPApplyPolishVfxMaterial(ImpactFlashMesh, TEXT("flare_01"), FLinearColor(5.0f, 2.6f, 0.8f, 1.0f));
	GPApplyPolishVfxMaterial(TracerMesh, TEXT("light_01"), FLinearColor(4.5f, 3.0f, 1.0f, 1.0f));
	ApplyMountRotation();
}

void UMinigunPodComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMinigunPodComponent, Gunner);
	DOREPLIFETIME(UMinigunPodComponent, AimYaw);
	DOREPLIFETIME(UMinigunPodComponent, AimPitch);
}

void UMinigunPodComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateShotFx(DeltaTime);

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (OwningShip && OwningShip->IsWrecked() && Gunner)
	{
		ForceRelease();
		return;
	}

	if (!bFiring || !Gunner)
	{
		FireTimer = 0.0f;
		return;
	}

	FireTimer -= DeltaTime;
	while (FireTimer <= 0.0f)
	{
		FireTrace();
		FireTimer += FireInterval;
	}
}

bool UMinigunPodComponent::IsCharacterInRange(const AGalacticPiratesCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	return FVector::Dist(Character->GetActorLocation(), GetComponentLocation()) <= InteractRange;
}

bool UMinigunPodComponent::TryInteract(AGalacticPiratesCharacter* Character)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Character || !OwningShip)
	{
		return false;
	}
	if (OwningShip->IsWrecked() || Character->GetBoardedShip() != OwningShip)
	{
		return false;
	}

	if (Gunner == Character)
	{
		Vacate();
		return true;
	}

	if (Gunner)
	{
		return false;
	}

	if (Character->IsPiloting() || Character->IsManningMinigun())
	{
		return false;
	}

	if (!IsCharacterInRange(Character))
	{
		return false;
	}

	Occupy(Character);
	return true;
}

void UMinigunPodComponent::ForceRelease()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Vacate();
	}
}

void UMinigunPodComponent::Occupy(AGalacticPiratesCharacter* Character)
{
	Gunner = Character;
	bFiring = false;
	AimYaw = 0.0f;
	AimPitch = 0.0f;
	ApplyMountRotation();
	Character->SetManningMinigun(this);
	LockGunner(Character);
	OnRep_Gunner();
}

void UMinigunPodComponent::Vacate()
{
	AGalacticPiratesCharacter* Previous = Gunner;
	bFiring = false;
	Gunner = nullptr;
	if (Previous)
	{
		UnlockGunner(Previous);
		Previous->SetManningMinigun(nullptr);
	}
	OnRep_Gunner();
}

void UMinigunPodComponent::LockGunner(AGalacticPiratesCharacter* Character)
{
	if (!Character)
	{
		return;
	}
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetComponentTickEnabled(false);
		Movement->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));
	}
	Character->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Character->SetActorRelativeLocation(FVector(-40.0f, 0.0f, 10.0f));
	Character->SetActorRelativeRotation(FRotator::ZeroRotator);
}

void UMinigunPodComponent::UnlockGunner(AGalacticPiratesCharacter* Character)
{
	if (!Character)
	{
		return;
	}
	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->SetComponentTickEnabled(true);
	}
	Character->RestoreWalkingOnShip();
}

void UMinigunPodComponent::OnRep_Gunner()
{
	ApplyMountRotation();
}

void UMinigunPodComponent::OnRep_Aim()
{
	ApplyMountRotation();
}

void UMinigunPodComponent::ApplyAim(float NewYaw, float NewPitch)
{
	AimYaw = FMath::Clamp(NewYaw, YawMin, YawMax);
	AimPitch = FMath::Clamp(NewPitch, PitchMin, PitchMax);
	ApplyMountRotation();
}

void UMinigunPodComponent::AddAimInput(float YawDelta, float PitchDelta)
{
	ApplyAim(AimYaw + YawDelta * AimSensitivity, AimPitch + PitchDelta * AimSensitivity);
}

void UMinigunPodComponent::SetFiring(bool bNewFiring)
{
	bFiring = bNewFiring && Gunner != nullptr;
}

void UMinigunPodComponent::ApplyMountRotation()
{
	if (YawMount)
	{
		YawMount->SetRelativeRotation(FRotator(0.0f, AimYaw, 0.0f));
	}
	if (PitchMount)
	{
		PitchMount->SetRelativeRotation(FRotator(AimPitch, 0.0f, 0.0f));
	}
}

FVector UMinigunPodComponent::GetMuzzleLocation() const
{
	if (BarrelMesh)
	{
		return BarrelMesh->GetComponentLocation() + GetMuzzleForward() * 80.0f;
	}
	return GetComponentLocation();
}

FVector UMinigunPodComponent::GetMuzzleForward() const
{
	return PitchMount ? PitchMount->GetForwardVector() : GetForwardVector();
}

float UMinigunPodComponent::DamageForActor(AActor* HitActor) const
{
	if (Cast<AHeatseekingMissile>(HitActor))
	{
		return MissileDamage;
	}
	if (AWalkableShip* Ship = Cast<AWalkableShip>(HitActor))
	{
		if (Ship->ArmorClass == EShipArmorClass::Light)
		{
			return LightShipDamage;
		}
		return ArmoredShipDamage;
	}
	return 0.0f;
}

void UMinigunPodComponent::FireTrace()
{
	UWorld* World = GetWorld();
	if (!World || !OwningShip)
	{
		return;
	}

	const FVector Start = GetMuzzleLocation();
	const FVector End = Start + GetMuzzleForward() * TraceRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MinigunTrace), false, OwningShip);
	Params.AddIgnoredActor(OwningShip);
	if (Gunner)
	{
		Params.AddIgnoredActor(Gunner);
	}
	for (AGalacticPiratesCharacter* Aboard : OwningShip->GetPlayersAboard())
	{
		if (Aboard)
		{
			Params.AddIgnoredActor(Aboard);
		}
	}

	FHitResult Hit;
	const bool bHit = World->SweepSingleByChannel(
		Hit,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(TraceRadius),
		Params);

	FVector TracerEnd = End;
	bool bDidHit = false;
	if (bHit)
	{
		TracerEnd = Hit.ImpactPoint;
		AActor* HitActor = Hit.GetActor();
		if (AHeatseekingMissile* Missile = Cast<AHeatseekingMissile>(HitActor))
		{
			bDidHit = Missile->ApplyMinigunHit(MissileDamage, Gunner);
		}
		else if (AWalkableShip* Ship = Cast<AWalkableShip>(HitActor))
		{
			if (Ship != OwningShip)
			{
				const float Damage = DamageForActor(Ship);
				if (Damage > 0.0f)
				{
					Ship->ApplyShipDamage(Damage, Gunner, OwningShip);
					bDidHit = true;
				}
			}
		}
		else if (Hit.GetComponent())
		{
			if (AHeatseekingMissile* CompMissile = Cast<AHeatseekingMissile>(Hit.GetComponent()->GetOwner()))
			{
				bDidHit = CompMissile->ApplyMinigunHit(MissileDamage, Gunner);
			}
			else if (AWalkableShip* CompShip = Cast<AWalkableShip>(Hit.GetComponent()->GetOwner()))
			{
				if (CompShip != OwningShip)
				{
					const float Damage = DamageForActor(CompShip);
					if (Damage > 0.0f)
					{
						CompShip->ApplyShipDamage(Damage, Gunner, OwningShip);
						bDidHit = true;
					}
				}
			}
		}
	}

	Multicast_Tracer(Start, TracerEnd, bDidHit);
}

void UMinigunPodComponent::Multicast_Tracer_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	PlayShotFx(Start, End, bHit);
}

void UMinigunPodComponent::PlayShotFx(const FVector& Start, const FVector& End, bool bHit)
{
	FlashTimer = MuzzleFlashSeconds;
	++ShotsPlayed;

	if (MuzzleFlashMesh)
	{
		const float Spin = static_cast<float>(ShotsPlayed) * 47.0f;
		const float Jitter = FMath::FRandRange(0.8f, 1.25f);
		MuzzleFlashMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, Spin));
		MuzzleFlashMesh->SetRelativeScale3D(FVector(0.55f, 0.35f, 0.35f) * Jitter);
		MuzzleFlashMesh->SetVisibility(true);
	}

	if (MuzzleLight)
	{
		MuzzleLight->SetIntensity(9000.0f);
	}

	if (TracerMesh)
	{
		const FVector Delta = End - Start;
		const float Length = Delta.Size();
		if (Length > 1.0f)
		{
			// The cylinder is 100cm tall on Z, so aim its Z axis down the shot.
			TracerMesh->SetWorldLocation(Start + Delta * 0.5f);
			TracerMesh->SetWorldRotation(FRotationMatrix::MakeFromZ(Delta / Length).ToQuat());
			TracerMesh->SetWorldScale3D(FVector(0.06f, 0.06f, Length / 100.0f));
			TracerMesh->SetVisibility(true);
		}
	}

	if (ImpactFlashMesh)
	{
		ImpactFlashMesh->SetVisibility(bHit);
		if (bHit)
		{
			ImpactFlashMesh->SetWorldLocation(End);
			ImpactFlashMesh->SetWorldScale3D(FVector(FMath::FRandRange(0.7f, 1.2f)));
		}
	}

	// One sample per shot at 14 rounds/sec turns to mush, so play every other round.
	if (ShotsPlayed % 2 == 0)
	{
		GPPlayPolishSoundAt(this, TEXT("SFX_PulseFire"), Start, 0.4f);
	}
}

void UMinigunPodComponent::UpdateShotFx(float DeltaTime)
{
	if (FlashTimer <= 0.0f)
	{
		return;
	}

	FlashTimer -= DeltaTime;
	if (FlashTimer > 0.0f)
	{
		if (MuzzleLight)
		{
			MuzzleLight->SetIntensity(9000.0f * FMath::Max(FlashTimer / MuzzleFlashSeconds, 0.0f));
		}
		return;
	}

	FlashTimer = 0.0f;
	if (MuzzleFlashMesh)
	{
		MuzzleFlashMesh->SetVisibility(false);
	}
	if (TracerMesh)
	{
		TracerMesh->SetVisibility(false);
	}
	if (ImpactFlashMesh)
	{
		ImpactFlashMesh->SetVisibility(false);
	}
	if (MuzzleLight)
	{
		MuzzleLight->SetIntensity(0.0f);
	}
}
