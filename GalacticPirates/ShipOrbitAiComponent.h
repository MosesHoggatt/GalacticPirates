#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShipOrbitAiComponent.generated.h"

class AWalkableShip;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GALACTICPIRATES_API UShipOrbitAiComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShipOrbitAiComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|AI")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|AI", meta = (ClampMin = "500.0"))
	float OrbitRadius = 6500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|AI")
	float OrbitHeightOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|AI")
	float OrbitDirection = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|AI", meta = (ClampMin = "0.1"))
	float SteeringGain = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|AI", meta = (ClampMin = "0.1"))
	float RadiusGain = 0.0012f;

	UFUNCTION(BlueprintCallable, Category = "Ship|AI")
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY()
	TObjectPtr<AWalkableShip> OwningShip;

	AActor* FindOrbitTarget() const;
	void ApplyOrbitSteering(AActor* Target);
};
