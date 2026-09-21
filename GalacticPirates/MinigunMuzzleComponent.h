#pragma once

#include "CoreMinimal.h"
#include "WeaponComponent.h"
#include "MinigunMuzzleComponent.generated.h"

/** Fixed forward minigun: same traces and damage as the walkable gun pod, no pod mesh or seat. */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class GALACTICPIRATES_API UMinigunMuzzleComponent : public UWeaponComponent
{
	GENERATED_BODY()

public:
	UMinigunMuzzleComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun", meta = (ClampMin = "0.05"))
	float FireInterval = 0.07f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun", meta = (ClampMin = "100.0"))
	float TraceRange = 18000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun", meta = (ClampMin = "1.0"))
	float TraceRadius = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun")
	float LightShipDamage = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun")
	float MissileDamage = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun")
	FVector MuzzleOffset = FVector(80.0f, 0.0f, 0.0f);

	void SetFiring(bool bNewFiring);
	bool IsFiring() const { return bFiring; }

	virtual bool CanFireWeapon() const override;
	virtual bool TryFireWeapon(APawn* InstigatorPawn) override;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Tracer(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit);

private:
	bool bFiring = false;
	float FireTimer = 0.0f;

	void FireRound();
};
