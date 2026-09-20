#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WeaponHardpointComponent.generated.h"

class UWeaponComponent;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class GALACTICPIRATES_API UWeaponHardpointComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UWeaponHardpointComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hardpoint")
	TObjectPtr<UWeaponComponent> EquippedWeapon;

	/** Spawned onto this hardpoint when EquippedWeapon is empty. Replicated so clients match loadout. */
	UPROPERTY(ReplicatedUsing = OnRep_WeaponClass, EditAnywhere, BlueprintReadWrite, Category = "Hardpoint")
	TSubclassOf<UWeaponComponent> WeaponClass;

	UFUNCTION(BlueprintCallable, Category = "Hardpoint")
	bool TryFire(APawn* InstigatorPawn);

	UFUNCTION(BlueprintPure, Category = "Hardpoint")
	UWeaponComponent* GetEquippedWeapon() const { return EquippedWeapon; }

	UFUNCTION(BlueprintCallable, Category = "Hardpoint")
	bool EquipWeaponClass(TSubclassOf<UWeaponComponent> NewClass);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_WeaponClass();

	void DiscoverEquippedWeapon();
};
