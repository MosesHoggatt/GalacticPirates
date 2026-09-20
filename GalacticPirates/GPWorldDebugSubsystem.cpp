#include "GPWorldDebugSubsystem.h"
#include "ShipDebug.h"
#include "Engine/World.h"

bool UGPWorldDebugSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UGPWorldDebugSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	GPTickDedicatedNetTest(World, DeltaTime);
	GPTickShipDuelTest(World, DeltaTime);
	GPTickShipMoveProbe(World, DeltaTime);
}

TStatId UGPWorldDebugSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGPWorldDebugSubsystem, STATGROUP_Tickables);
}
