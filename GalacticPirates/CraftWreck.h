#pragma once

#include "CoreMinimal.h"

class AActor;
class APawn;
class UOccupancyComponent;
class UShipMovementComponent;

void GPDisableCraftMovement(AActor* Actor);
void GPReleaseAllOccupancy(AActor* Actor);
void GPKillOccupantsInWreck(AActor* Actor, const FVector& Epicenter);
void GPBeginCraftWreck(AActor* Actor, float LifeSpanSeconds);
UOccupancyComponent* GPFindPilotOccupancy(AActor* Actor);
AActor* GPFindOccupiableCraftInRange(APawn* Pawn);
