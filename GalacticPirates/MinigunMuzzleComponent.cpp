#include "MinigunMuzzleComponent.h"
#include "MinigunShot.h"
#include "SpaceCraft.h"
#include "Engine/World.h"

UMinigunMuzzleComponent::UMinigunMuzzleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	SetMobility(EComponentMobility::Movable);
}

void UMinigunMuzzleComponent::SetFiring(bool bNewFiring)
{
	bFiring = bNewFiring;
	if (!bFiring)
	{
		FireTimer = 0.0f;
	}
}

bool UMinigunMuzzleComponent::CanFireWeapon() const
{
	return GetOwner() && GetOwner()->HasAuthority() && !GPIsCraftWrecked(GetOwner());
}

bool UMinigunMuzzleComponent::TryFireWeapon(APawn* InstigatorPawn)
{
	(void)InstigatorPawn;
	if (!CanFireWeapon())
	{
		return false;
	}
	FireRound();
	return true;
}

void UMinigunMuzzleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!CanFireWeapon() || !bFiring)
	{
		return;
	}

	FireTimer -= DeltaTime;
	if (FireTimer <= 0.0f)
	{
		FireRound();
		FireTimer += FMath::Max(FireInterval, 0.05f);
	}
}

void UMinigunMuzzleComponent::FireRound()
{
	AActor* Craft = GetOwner();
	if (!Craft)
	{
		return;
	}

	FMinigunShotRequest Request;
	Request.Causer = Craft;
	Request.Instigator = Cast<APawn>(Craft);
	Request.Start = GetComponentLocation() + GetForwardVector() * MuzzleOffset.X
		+ GetRightVector() * MuzzleOffset.Y
		+ GetUpVector() * MuzzleOffset.Z;
	Request.Forward = GetForwardVector();
	Request.Range = TraceRange;
	Request.Radius = TraceRadius;
	Request.LightShipDamage = LightShipDamage;
	Request.MissileDamage = MissileDamage;

	FVector TracerEnd;
	bool bHit = false;
	if (!GPFireMinigunShot(Request, TracerEnd, bHit))
	{
		return;
	}
	Multicast_Tracer(Request.Start, TracerEnd, bHit);
}

void UMinigunMuzzleComponent::Multicast_Tracer_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit)
{
	(void)End;
	(void)bHit;
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	GPPlayMinigunShotAudio(this, Start);
}
