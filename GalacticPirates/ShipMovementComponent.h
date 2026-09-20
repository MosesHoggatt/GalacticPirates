#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShipMovementComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GALACTICPIRATES_API UShipMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShipMovementComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship Physics|Thrust")
	float ForwardThrustPower = 500000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship Physics|Thrust")
	float StrafeThrustPower = 300000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship Physics|Thrust")
	float VerticalThrustPower = 400000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship Physics|Rotation")
	float PitchTorque = 50000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship Physics|Rotation")
	float YawTorque = 50000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship Physics|Rotation")
	float RollTorque = 30000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship Physics|Mass")
	float ShipMass = 50000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship Physics|Dampening", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TranslationDampening = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship Physics|Dampening", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RotationDampening = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship Physics|Limits")
	float MaxLinearVelocity = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship Physics|Limits")
	float MaxAngularVelocity = 90.0f;

	void SetThrustInput(const FVector& Input);
	void SetRotationInput(const FVector& Input);

	UFUNCTION(BlueprintCallable, Category = "Ship Movement")
	FVector GetLinearVelocity() const { return LinearVelocity; }

	UFUNCTION(BlueprintCallable, Category = "Ship Movement")
	FVector GetAngularVelocity() const { return AngularVelocity; }

	void SetLinearVelocity(const FVector& NewVelocity);
	void SetAngularVelocity(const FVector& NewVelocity);
	void SetVelocityOverride(const FVector& NewLinear, const FVector& NewAngular, bool bEnable);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	FVector ThrustInput;
	FVector RotationInput;
	FVector LinearVelocity;
	FVector AngularVelocity;
	FVector OverrideLinearVelocity = FVector::ZeroVector;
	FVector OverrideAngularVelocity = FVector::ZeroVector;
	bool bVelocityOverride = false;

	void ApplyThrust(float DeltaTime);
	void ApplyTorque(float DeltaTime);
	void ApplyDampening(float DeltaTime);
	void IntegrateVelocities(float DeltaTime);
	void ClampVelocities();
};
