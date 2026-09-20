// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GalacticPiratesGameMode.generated.h"

class AWalkableShip;

/**
 * Dedicated-server persistent world GameMode.
 * Players spawn aboard the walkable ship; there is no player-host path.
 */
UCLASS(abstract)
class AGalacticPiratesGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGalacticPiratesGameMode();

	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void StartPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Ship")
	AWalkableShip* GetOrSpawnPersistentShip();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	TSubclassOf<AWalkableShip> DefaultWalkableShipClass;
};
