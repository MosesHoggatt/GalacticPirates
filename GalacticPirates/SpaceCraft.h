#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameFramework/Actor.h"
#include "SpaceCraft.generated.h"

class UShipMovementComponent;
class UHullHealthComponent;
class USceneComponent;
class UOccupancyComponent;

UINTERFACE(MinimalAPI, BlueprintType)
class USpaceCraft : public UInterface
{
	GENERATED_BODY()
};

class GALACTICPIRATES_API ISpaceCraft
{
	GENERATED_BODY()

public:
	virtual UShipMovementComponent* GetSpaceMovement() const = 0;
	virtual UHullHealthComponent* GetHullHealth() const = 0;
	virtual bool IsCraftWrecked() const = 0;
	virtual FVector GetCraftVelocity() const = 0;
	virtual USceneComponent* GetHomingSceneComponent() const = 0;
	virtual bool HasHumanOccupant() const = 0;
	virtual FName GetAffiliationId() const = 0;
	virtual AActor* GetHomeCraft() const = 0;
	virtual UOccupancyComponent* GetPilotOccupancy() const = 0;
	virtual void NotifyCraftWrecked() = 0;
};

inline ISpaceCraft* GPAsSpaceCraft(AActor* Actor)
{
	return Actor ? Cast<ISpaceCraft>(Actor) : nullptr;
}

inline bool GPIsCraftWrecked(AActor* Actor)
{
	if (ISpaceCraft* Craft = GPAsSpaceCraft(Actor))
	{
		return Craft->IsCraftWrecked();
	}
	return false;
}

inline FName GPResolvedAffiliation(AActor* Actor)
{
	ISpaceCraft* Craft = GPAsSpaceCraft(Actor);
	if (!Craft)
	{
		return NAME_None;
	}

	const FName Tag = Craft->GetAffiliationId();
	if (!Tag.IsNone())
	{
		return Tag;
	}

	AActor* Home = Craft->GetHomeCraft();
	if (Home && Home != Actor)
	{
		if (ISpaceCraft* HomeCraft = GPAsSpaceCraft(Home))
		{
			return HomeCraft->GetAffiliationId();
		}
	}
	return NAME_None;
}

/** Own launched fighters. Unused until hangar deploy lands; keep the hook so map/FOF can stay friendly. */
inline bool GPIsOwnDeployedFighter(AActor* Viewer, AActor* Other)
{
	(void)Viewer;
	(void)Other;
	return false;
}

/** Friend/foe only. Never used for boarding or occupancy. Distinct craft are hostile until deployed-fighter FOF exists. */
inline bool GPAreHostile(AActor* A, AActor* B)
{
	if (!A || !B || A == B)
	{
		return false;
	}
	if (GPIsOwnDeployedFighter(A, B) || GPIsOwnDeployedFighter(B, A))
	{
		return false;
	}
	return true;
}
