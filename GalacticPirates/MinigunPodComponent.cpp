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
#include "Interfaces/MovementBaseInterface.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "QuatCamera.h"

UMinigunPodComponent::UMinigunPodComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	SetMobility(EComponentMobility::Movable);

	// The gun rig is built in BuildRig() at BeginPlay, not here. Default subobjects created
	// inside a component constructor cannot be instanced on a blueprint-derived actor: the
	// children end up owned by the CDO and refuse to attach ("Template Mismatch").
}

void UMinigunPodComponent::BuildRig()
{
	AActor* Owner = GetOwner();
	if (bRigBuilt || !Owner)
	{
		return;
	}
	bRigBuilt = true;

	// A blueprint saved against the old constructor still hands us its own copies of these
	// components, empty and misparented. Throw those away before building the real rig.
	USceneComponent* Inherited[] = { YawMount, PitchMount, PodMesh, BarrelMesh, MuzzleFlashMesh, TracerMesh, TracerRibbon, ImpactFlashMesh, MuzzleLight, ImpactLight };
	for (USceneComponent* Stale : Inherited)
	{
		if (Stale && Stale->GetOwner() == Owner)
		{
			Stale->DestroyComponent();
		}
	}
	YawMount = nullptr;
	PitchMount = nullptr;
	PodMesh = nullptr;
	BarrelMesh = nullptr;
	MuzzleFlashMesh = nullptr;
	TracerMesh = nullptr;
	TracerRibbon = nullptr;
	ImpactFlashMesh = nullptr;
	MuzzleLight = nullptr;
	ImpactLight = nullptr;

	auto NameFor = [this](const TCHAR* Suffix)
	{
		return FName(*FString::Printf(TEXT("%s_%s"), *GetName(), Suffix));
	};

	auto Attach = [](USceneComponent* Component, USceneComponent* Parent)
	{
		Component->SetMobility(EComponentMobility::Movable);
		Component->RegisterComponent();
		Component->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
	};

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));

	auto MakeMesh = [&](const TCHAR* Suffix, USceneComponent* Parent, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Owner, NameFor(Suffix));
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Attach(Component, Parent);
		return Component;
	};

	YawMount = NewObject<USceneComponent>(Owner, NameFor(TEXT("YawMount")));
	Attach(YawMount, this);
	YawMount->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));

	PitchMount = NewObject<USceneComponent>(Owner, NameFor(TEXT("PitchMount")));
	Attach(PitchMount, YawMount);

	PodMesh = MakeMesh(TEXT("PodMesh"), this, CubeMesh);
	PodMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -20.0f));
	PodMesh->SetRelativeScale3D(FVector(1.6f, 1.6f, 0.35f));

	BarrelMesh = MakeMesh(TEXT("BarrelMesh"), PitchMount, CylinderMesh);
	BarrelMesh->SetRelativeLocation(FVector(90.0f, 0.0f, 0.0f));
	BarrelMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
	BarrelMesh->SetRelativeScale3D(FVector(2.2f, 0.22f, 0.22f));

	MuzzleFlashMesh = MakeMesh(TEXT("MuzzleFlashMesh"), PitchMount, SphereMesh);
	MuzzleFlashMesh->SetRelativeLocation(FVector(190.0f, 0.0f, 0.0f));
	MuzzleFlashMesh->SetRelativeScale3D(FVector(0.55f, 0.35f, 0.35f));
	MuzzleFlashMesh->SetVisibility(false);

	ImpactFlashMesh = MakeMesh(TEXT("ImpactFlashMesh"), this, SphereMesh);
	ImpactFlashMesh->SetRelativeScale3D(FVector(1.4f));
	ImpactFlashMesh->SetVisibility(false);
	ImpactFlashMesh->SetAbsolute(true, true, true);

	TracerMesh = MakeMesh(TEXT("TracerMesh"), this, CylinderMesh);
	TracerMesh->SetVisibility(false);
	TracerMesh->SetAbsolute(true, true, true);

	TracerRibbon = MakeMesh(TEXT("TracerRibbon"), this, PlaneMesh);
	TracerRibbon->SetVisibility(false);
	TracerRibbon->SetAbsolute(true, true, true);

	MuzzleLight = NewObject<UPointLightComponent>(Owner, NameFor(TEXT("MuzzleLight")));
	MuzzleLight->SetAttenuationRadius(160.0f);
	MuzzleLight->SetLightColor(FLinearColor(1.0f, 0.72f, 0.28f));
	MuzzleLight->SetIntensity(0.0f);
	MuzzleLight->SetCastShadows(false);
	Attach(MuzzleLight, PitchMount);
	MuzzleLight->SetRelativeLocation(FVector(190.0f, 0.0f, 0.0f));

	ImpactLight = NewObject<UPointLightComponent>(Owner, NameFor(TEXT("ImpactLight")));
	ImpactLight->SetAttenuationRadius(1200.0f);
	ImpactLight->SetLightColor(FLinearColor(1.0f, 0.55f, 0.15f));
	ImpactLight->SetIntensity(0.0f);
	ImpactLight->SetCastShadows(false);
	Attach(ImpactLight, this);
	ImpactLight->SetAbsolute(true, true, true);

	SparkMeshes.Reset();
	SparkVelocity.Reset();
	SparkLife.Reset();
	for (int32 Index = 0; Index < 16; ++Index)
	{
		UStaticMeshComponent* Spark = MakeMesh(*FString::Printf(TEXT("Spark%d"), Index), this, SphereMesh);
		Spark->SetVisibility(false);
		Spark->SetAbsolute(true, true, true);
		SparkMeshes.Add(Spark);
		SparkVelocity.Add(FVector::ZeroVector);
		SparkLife.Add(0.0f);
	}
}

void UMinigunPodComponent::BeginPlay()
{
	Super::BeginPlay();
	OwningShip = Cast<AWalkableShip>(GetOwner());
	BuildRig();
	GPApplyPolishVfxMaterial(PodMesh, TEXT("circle_05"), FLinearColor(0.25f, 0.28f, 0.32f, 1.0f));
	GPApplyPolishVfxMaterial(BarrelMesh, TEXT("circle_05"), FLinearColor(0.55f, 0.45f, 0.2f, 1.0f));
	GPApplyPolishVfxMaterial(MuzzleFlashMesh, TEXT("muzzle_01"), FLinearColor(6.0f, 3.4f, 1.1f, 1.0f));
	GPApplyPolishVfxMaterial(ImpactFlashMesh, TEXT("flare_01"), FLinearColor(8.0f, 3.2f, 0.6f, 1.0f));
	GPApplyPolishVfxMaterial(TracerMesh, TEXT("light_01"), FLinearColor(12.0f, 8.0f, 2.0f, 1.0f));
	GPApplyPolishVfxMaterial(TracerRibbon, TEXT("muzzle_01"), FLinearColor(14.0f, 9.0f, 2.2f, 1.0f));
	for (UStaticMeshComponent* Spark : SparkMeshes)
	{
		GPApplyPolishVfxMaterial(Spark, TEXT("VFX_Ember"), FLinearColor(10.0f, 4.5f, 0.8f, 1.0f));
	}
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
	TickSparks(DeltaTime);
	ApplyGunnerCamera();

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
	const float Interval = FMath::Max(FireInterval, 0.05f);
	if (FireTimer <= 0.0f)
	{
		FireTrace();
		FireTimer += Interval;
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
	ApplyGunnerCamera();
}

void UMinigunPodComponent::UnlockGunner(AGalacticPiratesCharacter* Character)
{
	if (!Character)
	{
		return;
	}
	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Character->RestoreWalkCamera();
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->SetComponentTickEnabled(true);
	}
	Character->RestoreWalkingOnShip();
}

void UMinigunPodComponent::OnRep_Gunner()
{
	ApplyMountRotation();
	ApplyGunnerCamera();
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

void UMinigunPodComponent::AimAtWorldLocation(const FVector& WorldLocation)
{
	const FVector WorldDir = (WorldLocation - GetMuzzleLocation()).GetSafeNormal();
	if (WorldDir.IsNearlyZero())
	{
		return;
	}

	const FVector LocalDir = GetComponentTransform().InverseTransformVectorNoScale(WorldDir).GetSafeNormal();
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(LocalDir.Y, LocalDir.X));
	const float Horizontal = FMath::Sqrt(LocalDir.X * LocalDir.X + LocalDir.Y * LocalDir.Y);
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(LocalDir.Z, Horizontal));
	ApplyAim(Yaw, Pitch);
}

void UMinigunPodComponent::SetFiring(bool bNewFiring)
{
	bFiring = bNewFiring && Gunner != nullptr;
}

void UMinigunPodComponent::ApplyGunnerCamera()
{
	if (!Gunner || !PitchMount)
	{
		return;
	}
	if (Gunner->GetController() && !Gunner->IsLocallyControlled())
	{
		return;
	}

	UQuatCamera* Camera = Gunner->GetQuatCameraComponent();
	if (!Camera)
	{
		return;
	}

	Camera->SetGunSightLock(true);
	if (Camera->GetAttachParent() != PitchMount)
	{
		Camera->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Camera->AttachToComponent(PitchMount, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	Camera->SetRelativeLocation(GunSightOffset);
	Camera->SetRelativeRotation(FRotator::ZeroRotator);
	if (USkeletalMeshComponent* Arms = Gunner->GetFirstPersonMesh())
	{
		Arms->SetVisibility(false, true);
	}
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
	if (bHit)
	{
		TracerEnd = Hit.ImpactPoint;
		AActor* HitActor = Hit.GetActor();
		if (AHeatseekingMissile* Missile = Cast<AHeatseekingMissile>(HitActor))
		{
			Missile->ApplyMinigunHit(MissileDamage, Gunner);
		}
		else if (AWalkableShip* Ship = Cast<AWalkableShip>(HitActor))
		{
			if (Ship != OwningShip)
			{
				const float Damage = DamageForActor(Ship);
				if (Damage > 0.0f)
				{
					Ship->ApplyShipDamage(Damage, Gunner, OwningShip);
				}
			}
		}
		else if (Hit.GetComponent())
		{
			if (AHeatseekingMissile* CompMissile = Cast<AHeatseekingMissile>(Hit.GetComponent()->GetOwner()))
			{
				CompMissile->ApplyMinigunHit(MissileDamage, Gunner);
			}
			else if (AWalkableShip* CompShip = Cast<AWalkableShip>(Hit.GetComponent()->GetOwner()))
			{
				if (CompShip != OwningShip)
				{
					const float Damage = DamageForActor(CompShip);
					if (Damage > 0.0f)
					{
						CompShip->ApplyShipDamage(Damage, Gunner, OwningShip);
					}
				}
			}
		}
	}

	Multicast_Tracer(Start, TracerEnd, bHit);
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
	TracerTimer = TracerSeconds;
	++ShotsPlayed;

	if (MuzzleFlashMesh)
	{
		const float Spin = static_cast<float>(ShotsPlayed) * 47.0f;
		MuzzleFlashMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, Spin));
		MuzzleFlashMesh->SetRelativeScale3D(FVector(0.22f, 0.14f, 0.14f));
		MuzzleFlashMesh->SetVisibility(true);
		MuzzleFlashMesh->SetHiddenInGame(false);
	}

	if (MuzzleLight)
	{
		MuzzleLight->SetIntensity(600.0f);
	}

	PlaceTracer(Start, End);

	const FVector Incoming = (End - Start).GetSafeNormal();
	if (ImpactFlashMesh)
	{
		ImpactFlashMesh->SetVisibility(bHit);
		ImpactFlashMesh->SetHiddenInGame(!bHit);
		if (bHit)
		{
			ImpactFlashMesh->SetWorldLocation(End);
			ImpactFlashMesh->SetWorldScale3D(FVector(FMath::FRandRange(1.6f, 2.4f)));
		}
	}

	if (ImpactLight)
	{
		ImpactLight->SetWorldLocation(End);
		ImpactLight->SetIntensity(bHit ? 14000.0f : 0.0f);
	}

	if (bHit)
	{
		SpawnImpactSparks(End, Incoming);
	}

	if (ShotsPlayed % 2 == 0)
	{
		GPPlayPolishSoundAt(this, TEXT("SFX_PulseFire"), Start, 0.4f);
	}
}

void UMinigunPodComponent::PlaceTracer(const FVector& Start, const FVector& End)
{
	const FVector Delta = End - Start;
	const float Length = Delta.Size();
	if (Length <= 1.0f)
	{
		return;
	}

	const FVector Dir = Delta / Length;
	const float Bolt = FMath::Min(Length, TracerVisibleLength);
	// Drop the streak just under the bore so a seated gunner sees it instead of looking down the tube.
	const FVector Drop = PitchMount ? -PitchMount->GetUpVector() * 22.0f : FVector(0.0f, 0.0f, -22.0f);
	const FVector A = Start + Drop;
	const FVector Mid = A + Dir * (Bolt * 0.5f);

	if (TracerMesh)
	{
		TracerMesh->SetWorldLocation(Mid);
		TracerMesh->SetWorldRotation(FRotationMatrix::MakeFromZ(Dir).ToQuat());
		TracerMesh->SetWorldScale3D(FVector(0.55f, 0.55f, Bolt / 100.0f));
		TracerMesh->SetVisibility(true);
		TracerMesh->SetHiddenInGame(false);
	}

	if (TracerRibbon)
	{
		FVector ViewUp = PitchMount ? PitchMount->GetUpVector() : FVector::UpVector;
		FVector Across = FVector::CrossProduct(ViewUp, Dir).GetSafeNormal();
		if (Across.IsNearlyZero())
		{
			Across = FVector::CrossProduct(FVector::RightVector, Dir).GetSafeNormal();
		}
		TracerRibbon->SetWorldLocation(Mid);
		TracerRibbon->SetWorldRotation(FRotationMatrix::MakeFromXY(Across, Dir).ToQuat());
		TracerRibbon->SetWorldScale3D(FVector(0.7f, Bolt / 100.0f, 1.0f));
		TracerRibbon->SetVisibility(true);
		TracerRibbon->SetHiddenInGame(false);
	}
}

void UMinigunPodComponent::SpawnImpactSparks(const FVector& ImpactPoint, const FVector& IncomingDir)
{
	const FVector Out = IncomingDir.IsNearlyZero() ? FVector::UpVector : -IncomingDir;
	for (int32 Index = 0; Index < SparkMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Spark = SparkMeshes[Index];
		if (!Spark)
		{
			continue;
		}

		const FVector Spray = FMath::VRandCone(Out, FMath::DegreesToRadians(55.0f));
		Spark->SetWorldLocation(ImpactPoint + Spray * FMath::FRandRange(4.0f, 18.0f));
		Spark->SetWorldScale3D(FVector(FMath::FRandRange(0.12f, 0.28f)));
		Spark->SetVisibility(true);
		Spark->SetHiddenInGame(false);
		SparkVelocity[Index] = Spray * FMath::FRandRange(900.0f, 2200.0f);
		SparkLife[Index] = FMath::FRandRange(0.12f, 0.28f);
	}
}

void UMinigunPodComponent::TickSparks(float DeltaTime)
{
	for (int32 Index = 0; Index < SparkMeshes.Num(); ++Index)
	{
		if (SparkLife[Index] <= 0.0f)
		{
			continue;
		}

		SparkLife[Index] -= DeltaTime;
		UStaticMeshComponent* Spark = SparkMeshes[Index];
		if (!Spark)
		{
			continue;
		}

		if (SparkLife[Index] <= 0.0f)
		{
			SparkLife[Index] = 0.0f;
			Spark->SetVisibility(false);
			continue;
		}

		SparkVelocity[Index] += FVector(0.0f, 0.0f, -600.0f) * DeltaTime;
		Spark->AddWorldOffset(SparkVelocity[Index] * DeltaTime);
		Spark->SetWorldScale3D(FVector(0.08f + 0.2f * (SparkLife[Index] / 0.28f)));
	}
}

void UMinigunPodComponent::HideMuzzleFlash()
{
	if (MuzzleFlashMesh)
	{
		MuzzleFlashMesh->SetVisibility(false);
		MuzzleFlashMesh->SetHiddenInGame(true);
	}
	if (MuzzleLight)
	{
		MuzzleLight->SetIntensity(0.0f);
	}
}

void UMinigunPodComponent::UpdateShotFx(float DeltaTime)
{
	// One-frame strobe: last tick's muzzle sprite/light is always gone before this tick may fire again.
	HideMuzzleFlash();

	if (FlashTimer > 0.0f)
	{
		FlashTimer -= DeltaTime;
		if (FlashTimer <= 0.0f)
		{
			FlashTimer = 0.0f;
			if (ImpactLight)
			{
				ImpactLight->SetIntensity(0.0f);
			}
			if (ImpactFlashMesh)
			{
				ImpactFlashMesh->SetVisibility(false);
			}
		}
	}

	if (TracerTimer > 0.0f)
	{
		TracerTimer -= DeltaTime;
		if (TracerTimer <= 0.0f)
		{
			TracerTimer = 0.0f;
			if (TracerMesh)
			{
				TracerMesh->SetVisibility(false);
			}
			if (TracerRibbon)
			{
				TracerRibbon->SetVisibility(false);
			}
		}
	}
}
