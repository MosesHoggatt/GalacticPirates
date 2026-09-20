#pragma once

#include "CoreMinimal.h"
#include "WeaponComponent.h"
#include "ShipMissileSalvoComponent.generated.h"

class AWalkableShip;
class AGalacticPiratesCharacter;
class AHeatseekingMissile;
class UMissileSalvoTerminalComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GALACTICPIRATES_API UShipMissileSalvoComponent : public UWeaponComponent
{
	GENERATED_BODY()

public:
	UShipMissileSalvoComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Salvo", meta = (ClampMin = "1"))
	int32 MissilesPerSalvo = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Salvo", meta = (ClampMin = "0.02"))
	float StaggerSeconds = 0.14f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Salvo", meta = (ClampMin = "100.0"))
	float LaunchSpeed = 5760.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Salvo", meta = (ClampMin = "1.0"))
	float MissileDamage = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Salvo", meta = (ClampMin = "0.1"))
	float RechargeTime = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Salvo")
	FVector MuzzleOffset = FVector(220.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Salvo", meta = (ClampMin = "0.0"))
	float SpreadYawDegrees = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Salvo")
	bool bInheritOwnerVelocity = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Salvo")
	bool bLaunchAlongOwnerForward = false;

	UFUNCTION(BlueprintCallable, Category = "Missile Salvo")
	bool CanFire() const;

	virtual bool CanFireWeapon() const override;
	virtual bool TryFireWeapon(APawn* InstigatorPawn) override;

	UFUNCTION(BlueprintPure, Category = "Missile Salvo")
	float GetCooldownRemaining() const { return CooldownRemaining; }

	UFUNCTION(BlueprintPure, Category = "Missile Salvo")
	float GetRechargeAlpha() const;

	UFUNCTION(BlueprintPure, Category = "Missile Salvo")
	int32 GetSalvosFired() const { return SalvosFired; }

	UFUNCTION(BlueprintPure, Category = "Missile Salvo")
	int32 GetMissilesInFlight() const { return MissilesInFlight; }

	UFUNCTION(BlueprintCallable, Category = "Missile Salvo")
	bool Fire(AGalacticPiratesCharacter* Operator);

	UFUNCTION(BlueprintCallable, Category = "Missile Salvo")
	bool FireIgnoringTerminalRange(AGalacticPiratesCharacter* Operator);

	UFUNCTION(BlueprintCallable, Category = "Missile Salvo")
	bool FireAt(AActor* Target, APawn* InstigatorPawn);

	void AdvanceSalvo(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "Missile Salvo")
	AActor* ResolveSalvoTarget() const;

	FVector GetMuzzleWorldLocation() const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Missile Salvo")
	float CooldownRemaining = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Missile Salvo")
	int32 SalvosFired = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Missile Salvo")
	int32 MissilesInFlight = 0;

private:
	UPROPERTY()
	TObjectPtr<AActor> OwningCraft;

	int32 PendingLaunches = 0;
	float StaggerTimer = 0.0f;
	TWeakObjectPtr<AActor> PendingTarget;
	TWeakObjectPtr<APawn> PendingOperator;

	bool FireInternal(APawn* Operator, bool bRequireTerminalRange, AActor* ForcedTarget);
	void LaunchOneMissile(int32 IndexInSalvo, AActor* Target, APawn* Operator);
};
