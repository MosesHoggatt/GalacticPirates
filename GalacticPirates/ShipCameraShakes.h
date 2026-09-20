#pragma once

#include "CoreMinimal.h"
#include "Shakes/LegacyCameraShake.h"
#include "ShipCameraShakes.generated.h"

UCLASS()
class GALACTICPIRATES_API UShipCannonCameraShake : public ULegacyCameraShake
{
	GENERATED_BODY()

public:
	UShipCannonCameraShake();
};

UCLASS()
class GALACTICPIRATES_API UShipExplosionCameraShake : public ULegacyCameraShake
{
	GENERATED_BODY()

public:
	UShipExplosionCameraShake();
};
