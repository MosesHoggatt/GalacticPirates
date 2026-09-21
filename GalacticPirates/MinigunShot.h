#pragma once

#include "CoreMinimal.h"

class AActor;
class APawn;
class UWorld;
class UObject;

struct FMinigunShotRequest
{
	AActor* Causer = nullptr;
	APawn* Instigator = nullptr;
	FVector Start = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector;
	float Range = 18000.0f;
	float Radius = 12.0f;
	float LightShipDamage = 40.0f;
	float MissileDamage = 50.0f;
};

/** Shared minigun round: visibility sweep, ballistic hull damage, missile hits. */
bool GPFireMinigunShot(const FMinigunShotRequest& Request, FVector& OutTracerEnd, bool& bHit);

void GPPlayMinigunShotAudio(const UObject* WorldContext, const FVector& Location);
