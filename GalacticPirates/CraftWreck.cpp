#include "CraftWreck.h"
#include "OccupancyComponent.h"
#include "SpaceCraft.h"
#include "ShipMovementComponent.h"
#include "GalacticPiratesCharacter.h"
#include "EngineUtils.h"
#include "Engine/EngineTypes.h"

void GPDisableCraftMovement(AActor* Actor)
{
	ISpaceCraft* Craft = GPAsSpaceCraft(Actor);
	UShipMovementComponent* Movement = Craft ? Craft->GetSpaceMovement() : nullptr;
	if (!Movement)
	{
		return;
	}
	Movement->SetThrustInput(FVector::ZeroVector);
	Movement->SetRotationInput(FVector::ZeroVector);
	Movement->SetComponentTickEnabled(false);
}

void GPReleaseAllOccupancy(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	TArray<UOccupancyComponent*> Seats;
	Actor->GetComponents<UOccupancyComponent>(Seats);
	for (UOccupancyComponent* Seat : Seats)
	{
		if (Seat)
		{
			Seat->ForceRelease();
		}
	}
}

void GPKillOccupantsInWreck(AActor* Actor, const FVector& Epicenter)
{
	if (!Actor)
	{
		return;
	}

	TArray<UOccupancyComponent*> Seats;
	Actor->GetComponents<UOccupancyComponent>(Seats);
	for (UOccupancyComponent* Seat : Seats)
	{
		if (AGalacticPiratesCharacter* Character = Seat ? Cast<AGalacticPiratesCharacter>(Seat->GetOccupant()) : nullptr)
		{
			Character->DieInWreck(Epicenter);
		}
	}
}

void GPBeginCraftWreck(AActor* Actor, float LifeSpanSeconds)
{
	if (!Actor || !Actor->HasAuthority())
	{
		return;
	}

	GPDisableCraftMovement(Actor);
	GPKillOccupantsInWreck(Actor, Actor->GetActorLocation());
	GPReleaseAllOccupancy(Actor);
	Actor->SetNetDormancy(DORM_DormantAll);
	if (LifeSpanSeconds > 0.0f)
	{
		Actor->SetLifeSpan(LifeSpanSeconds);
	}
}

UOccupancyComponent* GPFindPilotOccupancy(AActor* Actor)
{
	if (ISpaceCraft* Craft = GPAsSpaceCraft(Actor))
	{
		return Craft->GetPilotOccupancy();
	}
	return nullptr;
}

AActor* GPFindOccupiableCraftInRange(APawn* Pawn)
{
	if (!Pawn || !Pawn->GetWorld())
	{
		return nullptr;
	}

	AActor* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<APawn> It(Pawn->GetWorld()); It; ++It)
	{
		APawn* Candidate = *It;
		if (!Candidate || Candidate == Pawn || GPIsCraftWrecked(Candidate))
		{
			continue;
		}
		UOccupancyComponent* Seat = GPFindPilotOccupancy(Candidate);
		if (!Seat || !Seat->IsInRange(Pawn))
		{
			continue;
		}
		const float Dist = FVector::Dist(Pawn->GetActorLocation(), Seat->GetComponentLocation());
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = Candidate;
		}
	}
	return Best;
}
