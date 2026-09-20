#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "ShipPulseCannonComponent.generated.h"

class AWalkableShip;
class AGalacticPiratesCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPulseCannonFired, AWalkableShip*, TargetShip, float, DamageDealt, bool, bHit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPulseCannonRecharged);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GALACTICPIRATES_API UShipPulseCannonComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UShipPulseCannonComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse Cannon", meta = (ClampMin = "1.0"))
	float PulseDamage = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse Cannon", meta = (ClampMin = "100.0"))
	float BeamRange = 25000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse Cannon", meta = (ClampMin = "1.0"))
	float BeamRadius = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse Cannon", meta = (ClampMin = "0.1"))
	float RechargeTime = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse Cannon", meta = (ClampMin = "0.05"))
	float BeamVisualDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse Cannon", meta = (ClampMin = "1.0"))
	float BeamThickness = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse Cannon")
	FLinearColor BeamColor = FLinearColor(0.2f, 0.85f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse Cannon")
	FVector MuzzleOffset = FVector(200.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pulse Cannon", meta = (ClampMin = "0.0"))
	float MuzzleHullClearance = 80.0f;

	UPROPERTY(BlueprintAssignable, Category = "Pulse Cannon")
	FOnPulseCannonFired OnPulseCannonFired;

	UPROPERTY(BlueprintAssignable, Category = "Pulse Cannon")
	FOnPulseCannonRecharged OnPulseCannonRecharged;

	UFUNCTION(BlueprintCallable, Category = "Pulse Cannon")
	bool CanFire() const;

	UFUNCTION(BlueprintCallable, Category = "Pulse Cannon")
	float GetCooldownRemaining() const { return CooldownRemaining; }

	UFUNCTION(BlueprintCallable, Category = "Pulse Cannon")
	float GetRechargeAlpha() const;

	UFUNCTION(BlueprintCallable, Category = "Pulse Cannon")
	FVector GetMuzzleWorldLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Pulse Cannon")
	FVector GetMuzzleForward() const;

	UFUNCTION(BlueprintCallable, Category = "Pulse Cannon")
	bool Fire(AGalacticPiratesCharacter* Operator);

	UFUNCTION(BlueprintCallable, Category = "Pulse Cannon")
	bool FireIgnoringTerminalRange(AGalacticPiratesCharacter* Operator);

	UFUNCTION(BlueprintPure, Category = "Pulse Cannon")
	int32 GetShotsFired() const { return ShotsFired; }

	UFUNCTION(BlueprintPure, Category = "Pulse Cannon")
	AWalkableShip* GetLastHitShip() const { return LastHitShip; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Pulse Cannon")
	float CooldownRemaining = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Pulse Cannon")
	int32 ShotsFired = 0;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PulseFired(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Recharged();

private:
	UPROPERTY()
	AWalkableShip* OwningShip = nullptr;

	UPROPERTY()
	AWalkableShip* LastHitShip = nullptr;

	UPROPERTY()
	AGalacticPiratesCharacter* LastOperator = nullptr;

	bool bRechargeBroadcastPending = false;

	bool FireInternal(AGalacticPiratesCharacter* Operator, bool bRequireTerminalRange);
	void PlayBeamVisual(const FVector& Start, const FVector& End, bool bHit);
};
