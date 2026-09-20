#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShipCrewAiComponent.generated.h"

class AWalkableShip;
class AGalacticPiratesCharacter;
class UMinigunPodComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GALACTICPIRATES_API UShipCrewAiComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShipCrewAiComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|CrewAI")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|CrewAI", meta = (ClampMin = "1.0"))
	float SalvoInterval = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|CrewAI", meta = (ClampMin = "0.5"))
	float GunEngageDot = 0.72f;

	UFUNCTION(BlueprintCallable, Category = "Ship|CrewAI")
	int32 GetCrewCount() const;

	UFUNCTION(BlueprintCallable, Category = "Ship|CrewAI")
	void SpawnCrew();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY()
	TObjectPtr<AWalkableShip> OwningShip;

	UPROPERTY()
	TObjectPtr<AGalacticPiratesCharacter> Pilot;

	UPROPERTY()
	TObjectPtr<AGalacticPiratesCharacter> SalvoOperator;

	UPROPERTY()
	TObjectPtr<AGalacticPiratesCharacter> PortGunner;

	UPROPERTY()
	TObjectPtr<AGalacticPiratesCharacter> StarboardGunner;

	float SalvoTimer = 2.0f;
	bool bCrewSpawned = false;

	bool ShouldCrewThisShip() const;
	AGalacticPiratesCharacter* SpawnCrewMember(const TCHAR* Name);
	void SeatCrew();
	void DriveMinigun(UMinigunPodComponent* Pod, AWalkableShip* Target);
	AWalkableShip* FindAttackTarget() const;
	void DestroyCrew();
};
