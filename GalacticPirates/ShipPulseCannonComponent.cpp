#include "ShipPulseCannonComponent.h"
#include "WalkableShip.h"
#include "SpaceCraft.h"
#include "WeaponTerminalComponent.h"
#include "GalacticPiratesCharacter.h"
#include "ShipPulseBeamVisual.h"
#include "ShipPolish.h"
#include "GalacticPirates.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionQueryParams.h"

UShipPulseCannonComponent::UShipPulseCannonComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	SetUsingAbsoluteRotation(false);
}

void UShipPulseCannonComponent::BeginPlay()
{
	Super::BeginPlay();
	OwningShip = Cast<AWalkableShip>(GetOwner());
}

void UShipPulseCannonComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UShipPulseCannonComponent, CooldownRemaining);
	DOREPLIFETIME(UShipPulseCannonComponent, ShotsFired);
}

void UShipPulseCannonComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (CooldownRemaining > 0.0f)
	{
		CooldownRemaining = FMath::Max(0.0f, CooldownRemaining - DeltaTime);
		if (CooldownRemaining <= 0.0f)
		{
			bRechargeBroadcastPending = true;
		}
	}

	if (bRechargeBroadcastPending && CooldownRemaining <= 0.0f)
	{
		bRechargeBroadcastPending = false;
		OnPulseCannonRecharged.Broadcast();
		Multicast_Recharged();
		if (GPCombatLogEnabled())
		{
			UE_LOG(LogGalacticPirates, Log, TEXT("[PulseCannon] Recharged on %s"), *GetNameSafe(GetOwner()));
		}
	}
}

bool UShipPulseCannonComponent::CanFire() const
{
	if (!GetOwner() || GPIsCraftWrecked(GetOwner()))
	{
		return false;
	}
	return CooldownRemaining <= KINDA_SMALL_NUMBER;
}

bool UShipPulseCannonComponent::CanFireWeapon() const
{
	return CanFire();
}

bool UShipPulseCannonComponent::TryFireWeapon(APawn* InstigatorPawn)
{
	return Fire(Cast<AGalacticPiratesCharacter>(InstigatorPawn));
}

float UShipPulseCannonComponent::GetRechargeAlpha() const
{
	if (RechargeTime <= KINDA_SMALL_NUMBER)
	{
		return 1.0f;
	}
	return 1.0f - FMath::Clamp(CooldownRemaining / RechargeTime, 0.0f, 1.0f);
}

FVector UShipPulseCannonComponent::GetMuzzleWorldLocation() const
{
	FVector Muzzle = GetComponentTransform().TransformPosition(MuzzleOffset);
	const FVector Forward = GetMuzzleForward();
	AWalkableShip* Ship = Cast<AWalkableShip>(GetOwner());
	if (!Ship)
	{
		return Muzzle;
	}

	auto PushPastBounds = [&](const FBoxSphereBounds& MeshBounds)
	{
		const FVector Origin = MeshBounds.Origin;
		const FVector Extent = MeshBounds.BoxExtent;
		float MaxAlong = -TNumericLimits<float>::Max();
		for (int32 IX = -1; IX <= 1; IX += 2)
		{
			for (int32 IY = -1; IY <= 1; IY += 2)
			{
				for (int32 IZ = -1; IZ <= 1; IZ += 2)
				{
					const FVector Corner = Origin + FVector(Extent.X * IX, Extent.Y * IY, Extent.Z * IZ);
					MaxAlong = FMath::Max(MaxAlong, FVector::DotProduct(Corner, Forward));
				}
			}
		}

		const float Desired = MaxAlong + BeamRadius + MuzzleHullClearance;
		const float Current = FVector::DotProduct(Muzzle, Forward);
		if (Current < Desired)
		{
			Muzzle += Forward * (Desired - Current);
		}
	};

	if (Ship->GetInteriorMesh() && Ship->GetInteriorMesh()->GetStaticMesh())
	{
		PushPastBounds(Ship->GetInteriorMesh()->Bounds);
	}
	if (Ship->HullMesh && Ship->HullMesh->GetStaticMesh())
	{
		PushPastBounds(Ship->HullMesh->Bounds);
	}
	if (Ship->CombatHull)
	{
		PushPastBounds(Ship->CombatHull->Bounds);
	}

	return Muzzle;
}

FVector UShipPulseCannonComponent::GetMuzzleForward() const
{
	return GetComponentQuat().GetForwardVector();
}

bool UShipPulseCannonComponent::Fire(AGalacticPiratesCharacter* Operator)
{
	return FireInternal(Operator, true);
}

bool UShipPulseCannonComponent::FireIgnoringTerminalRange(AGalacticPiratesCharacter* Operator)
{
	return FireInternal(Operator, false);
}

bool UShipPulseCannonComponent::FireInternal(AGalacticPiratesCharacter* Operator, bool bRequireTerminalRange)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld())
	{
		return false;
	}

	OwningShip = Cast<AWalkableShip>(GetOwner());
	if (!CanFire())
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[PulseCannon] Fire denied on %s cooldown=%.2f wrecked=%s"),
			*GetNameSafe(OwningShip),
			CooldownRemaining,
			GPIsCraftWrecked(GetOwner()) ? TEXT("true") : TEXT("false"));
		return false;
	}

	if (Operator)
	{
		if (Operator->GetBoardedShip() != OwningShip)
		{
			UE_LOG(LogGalacticPirates, Warning, TEXT("[PulseCannon] Operator %s is not aboard %s"),
				*GetNameSafe(Operator), *GetNameSafe(OwningShip));
			return false;
		}

		if (bRequireTerminalRange && OwningShip->WeaponTerminal && !OwningShip->WeaponTerminal->IsCharacterInRange(Operator))
		{
			UE_LOG(LogGalacticPirates, Warning, TEXT("[PulseCannon] Operator %s is not at the weapon terminal"),
				*GetNameSafe(Operator));
			return false;
		}
	}

	const FVector Start = GetMuzzleWorldLocation();
	const FVector End = Start + GetMuzzleForward() * BeamRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShipPulseCannon), false, OwningShip);
	if (OwningShip)
	{
		for (AGalacticPiratesCharacter* Aboard : OwningShip->GetPlayersAboard())
		{
			Params.AddIgnoredActor(Aboard);
		}
	}

	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByChannel(
		Hits,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(BeamRadius),
		Params);

	FVector BeamEnd = End;
	AWalkableShip* HitShip = nullptr;
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor)
		{
			continue;
		}

		AActor* CandidateCraft = GPAsSpaceCraft(HitActor) ? HitActor : nullptr;
		if (!CandidateCraft && Hit.GetComponent() && Hit.GetComponent()->GetOwner())
		{
			AActor* OwnerActor = Hit.GetComponent()->GetOwner();
			if (GPAsSpaceCraft(OwnerActor))
			{
				CandidateCraft = OwnerActor;
			}
		}

		if (CandidateCraft && CandidateCraft != GetOwner())
		{
			if (!HitShip)
			{
				HitShip = Cast<AWalkableShip>(CandidateCraft);
			}
			continue;
		}

		if (Cast<APawn>(HitActor))
		{
			continue;
		}

		BeamEnd = Hit.ImpactPoint;
		break;
	}

	LastHitShip = HitShip;
	LastOperator = Operator;
	ShotsFired++;
	CooldownRemaining = RechargeTime;
	bRechargeBroadcastPending = true;

	Multicast_PulseFired(Start, BeamEnd, HitShip != nullptr);
	OnPulseCannonFired.Broadcast(HitShip, 0.0f, HitShip != nullptr);

	UE_LOG(LogGalacticPirates, Warning,
		TEXT("[PulseCannon] Fired from %s by %s hit=%s cooldown=%.2f start=%s end=%s duration=%.2f"),
		*GetNameSafe(OwningShip),
		*GetNameSafe(Operator),
		*GetNameSafe(HitShip),
		CooldownRemaining,
		*Start.ToCompactString(),
		*BeamEnd.ToCompactString(),
		BeamVisualDuration);

	return true;
}

void UShipPulseCannonComponent::Multicast_PulseFired_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit)
{
	PlayBeamVisual(Start, End, bHit);

	if (GetNetMode() != NM_DedicatedServer)
	{
		GPPlayPolishSound2D(this, TEXT("SFX_PulseFire"), 0.95f);
		GPPlayPolishSoundAt(this, TEXT("SFX_PulseFire"), Start, 1.0f);
		if (bHit)
		{
			GPPlayPolishSound2D(this, TEXT("SFX_ExplosionHit"), 0.55f);
			GPPlayPolishSoundAt(this, TEXT("SFX_ExplosionHit"), End, 0.7f);
			GPPlayPolishSound2D(this, TEXT("SFX_Impact"), 0.75f);
			GPPlayPolishSoundAt(this, TEXT("SFX_Impact"), End, 0.9f);
		}
		GPPlayCannonCameraShake(GetWorld(), Start, 200.0f, 4500.0f, 0.85f);
	}

	UE_LOG(LogGalacticPirates, Warning,
		TEXT("[PulseCannon] Beam visual net=%d hit=%s start=%s end=%s"),
		static_cast<int32>(GetNetMode()),
		bHit ? TEXT("true") : TEXT("false"),
		*Start.ToCompactString(),
		*End.ToCompactString());
}

void UShipPulseCannonComponent::Multicast_Recharged_Implementation()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	GPPlayPolishSoundAt(this, TEXT("SFX_RechargeReady"), GetComponentLocation(), 0.9f);
}

void UShipPulseCannonComponent::PlayBeamVisual(const FVector& Start, const FVector& End, bool bHit)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	DrawDebugLine(World, Start, End, bHit ? FColor::Cyan : FColor::Orange, false, BeamVisualDuration, 0, BeamThickness * 0.15f);
	DrawDebugSphere(World, End, BeamRadius, 8, bHit ? FColor::Red : FColor::Yellow, false, BeamVisualDuration);

	const float DamagePerSecond = PulseDamage / FMath::Max(BeamVisualDuration, 0.05f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AShipPulseBeamVisual* Beam = World->SpawnActor<AShipPulseBeamVisual>(AShipPulseBeamVisual::StaticClass(), Start, FRotator::ZeroRotator, Params))
	{
		Beam->InitializeBeam(
			Start,
			End,
			BeamThickness,
			BeamVisualDuration,
			BeamColor,
			BeamRadius,
			DamagePerSecond,
			OwningShip,
			LastOperator);
	}
}
