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

	/** Pause after finishing one weapon before another may fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|CrewAI", meta = (ClampMin = "0.1"))
	float InterWeaponDelay = 1.4f;

	/** How long an AI minigun burst lasts before they must switch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|CrewAI", meta = (ClampMin = "0.2"))
	float GunBurstSeconds = 1.6f;

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

	bool bCrewSpawned = false;

	enum class EAiWeapon : uint8
	{
		None,
		PortMinigun,
		StarboardMinigun,
		Missiles
	};

	EAiWeapon ActiveWeapon = EAiWeapon::None;
	EAiWeapon LastWeapon = EAiWeapon::None;
	float WeaponHoldTimer = 0.0f;
	float WeaponDelayTimer = 0.0f;

	bool ShouldCrewThisShip() const;
	AGalacticPiratesCharacter* SpawnCrewMember(const TCHAR* Name);
	void SeatCrew();
	bool UpdateMinigun(UMinigunPodComponent* Pod, AActor* Target, bool bAllowFire);
	bool HasGunLineOfSight(const UMinigunPodComponent* Pod, AActor* Target) const;
	bool CanEngageWithMinigun(const UMinigunPodComponent* Pod, AActor* Target) const;
	EAiWeapon ChooseWeapon(AActor* Target) const;
	void SilenceWeapons();
	void FinishWeaponUse();
	void SetGunManned(UMinigunPodComponent* Pod, AGalacticPiratesCharacter* Crew, bool bManned);
	AActor* FindAttackTarget() const;
	void DestroyCrew();
};
