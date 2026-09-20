#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTypes.h"
#include "HullHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHullHealthChanged, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHullDestroyed);

class UHullHealthComponent;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class GALACTICPIRATES_API UHullHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHullHealthComponent();

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Hull", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull")
	EShipArmorClass ArmorClass = EShipArmorClass::Unarmored;

	UPROPERTY(BlueprintAssignable, Category = "Hull")
	FOnHullHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Hull")
	FOnHullDestroyed OnHullDestroyed;

	UFUNCTION(BlueprintPure, Category = "Hull")
	float GetHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Hull")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Hull")
	float GetHealthPercent() const { return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Hull")
	bool IsDestroyed() const { return bDestroyed || CurrentHealth <= 0.0f; }

	UFUNCTION(BlueprintCallable, Category = "Hull")
	void ResetToFull();

	UFUNCTION(BlueprintCallable, Category = "Hull")
	void SetHealth(float NewHealth);

	UFUNCTION(BlueprintCallable, Category = "Hull")
	float ApplyDamage(const FSpaceDamageEvent& Event);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentHealth)
	float CurrentHealth = 100.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Destroyed)
	bool bDestroyed = false;

	UFUNCTION()
	void OnRep_CurrentHealth();

	UFUNCTION()
	void OnRep_Destroyed();

private:
	void NotifyHealthChanged();
	void DestroyHull();
};

UHullHealthComponent* GPFindHullHealth(AActor* Actor);
float GPApplySpaceDamage(AActor* Target, const FSpaceDamageEvent& Event);
