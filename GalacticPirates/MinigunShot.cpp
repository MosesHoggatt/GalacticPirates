#include "MinigunShot.h"
#include "WalkableShip.h"
#include "GalacticPiratesCharacter.h"
#include "OccupancyComponent.h"
#include "HeatseekingMissile.h"
#include "CombatTypes.h"
#include "HullHealthComponent.h"
#include "SpaceCraft.h"
#include "ShipPolish.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"

bool GPFireMinigunShot(const FMinigunShotRequest& Request, FVector& OutTracerEnd, bool& bHit)
{
	bHit = false;
	AActor* Craft = Request.Causer;
	UWorld* World = Craft ? Craft->GetWorld() : nullptr;
	if (!World || !Craft || GPIsCraftWrecked(Craft))
	{
		return false;
	}

	const FVector Dir = Request.Forward.GetSafeNormal();
	const FVector Start = Request.Start;
	const FVector End = Start + Dir * Request.Range;
	OutTracerEnd = End;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MinigunTrace), false, Craft);
	Params.AddIgnoredActor(Craft);
	if (Request.Instigator)
	{
		Params.AddIgnoredActor(Request.Instigator);
	}
	if (AWalkableShip* Ship = Cast<AWalkableShip>(Craft))
	{
		for (AGalacticPiratesCharacter* Aboard : Ship->GetPlayersAboard())
		{
			if (Aboard)
			{
				Params.AddIgnoredActor(Aboard);
			}
		}
	}
	else if (ISpaceCraft* Space = GPAsSpaceCraft(Craft))
	{
		if (UOccupancyComponent* Occ = Space->GetPilotOccupancy())
		{
			if (APawn* Occupant = Occ->GetOccupant())
			{
				Params.AddIgnoredActor(Occupant);
			}
		}
	}

	FHitResult Hit;
	bHit = World->SweepSingleByChannel(
		Hit,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(Request.Radius),
		Params);

	if (bHit)
	{
		OutTracerEnd = Hit.ImpactPoint;
		AActor* HitActor = Hit.GetActor();
		if (!HitActor && Hit.GetComponent())
		{
			HitActor = Hit.GetComponent()->GetOwner();
		}
		if (AHeatseekingMissile* Missile = Cast<AHeatseekingMissile>(HitActor))
		{
			Missile->ApplyMinigunHit(Request.MissileDamage, Request.Instigator);
		}
		else if (HitActor && HitActor != Craft)
		{
			FSpaceDamageEvent Event;
			Event.Amount = Request.LightShipDamage;
			Event.Kind = ESpaceDamageKind::Ballistic;
			Event.InstigatorPawn = Request.Instigator;
			Event.Causer = Craft;
			GPApplySpaceDamage(HitActor, Event);
		}
	}

	return true;
}

void GPPlayMinigunShotAudio(const UObject* WorldContext, const FVector& Location)
{
	GPPlayPolishSoundAt(WorldContext, TEXT("SFX_Minigun"), Location, 0.7f);
}
