#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StarshipMovementComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SPACEBATTLESIMULATOR_API UStarshipMovementComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UStarshipMovementComponent();

    UFUNCTION(BlueprintCallable, Category = "StarshipMovement")
    float GetMaxAngularSpeed() const { return MaxAngularSpeed; }

    UFUNCTION(BlueprintCallable, Category = "StarshipMovement")
    float GetMaxSpeed() const { return MaxSpeed; }

    UFUNCTION(BlueprintCallable, Category = "StarshipMovement")
    float GetCurrentThrottle() const { return CurrentThrottle; }

    UFUNCTION(BlueprintCallable, Category = "StarshipMovement")
    bool IsAirbrakeEngaged() const { return IsAirbrakeActive; }

    UFUNCTION(BlueprintCallable, Category = "StarshipMovement")
    void SetThrottleInput(float ThrottleInput);

    UFUNCTION(BlueprintCallable, Category = "StarshipMovement")
    void SetSteeringInput(float PitchInput, float YawInput, float RollInput);

    UFUNCTION(BlueprintCallable, Category = "StarshipMovement")
    void StartAirbrake();

    UFUNCTION(BlueprintCallable, Category = "StarshipMovement")
    void StopAirbrake();

    virtual void TickComponent(float DeltaTimeSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    virtual void BeginPlay() override;

protected:
    UPROPERTY(EditAnywhere, Category = "Movement")
    float MaxAcceleration = 3500.0f;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float MaxSpeed = 5000.0f;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float AirbrakeStrength = 5000.0f;

    UPROPERTY(EditAnywhere, Category = "Drift")
    float DriftDampingStrength = 3000.0f;

    UPROPERTY(EditAnywhere, Category = "Drift", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DriftForwardDampingFactor = 0.5f;

    UPROPERTY(EditAnywhere, Category = "Collision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LinearRestitution = 0.5f;

    UPROPERTY(EditAnywhere, Category = "Collision", meta = (ClampMin = "0.0"))
    float AngularImpulseStrength = 0.5f;

    UPROPERTY(EditAnywhere, Category = "Collision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SurfaceFriction = 0.7f;

    UPROPERTY(EditAnywhere, Category = "Rotation")
    float MaxTorquePitch = 1000.0f;

    UPROPERTY(EditAnywhere, Category = "Rotation")
    float MaxTorqueYaw = 1000.0f;

    UPROPERTY(EditAnywhere, Category = "Rotation")
    float MaxTorqueRoll = 1000.0f;

    UPROPERTY(EditAnywhere, Category = "Rotation")
    float InertiaPitch = 100.0f;

    UPROPERTY(EditAnywhere, Category = "Rotation")
    float InertiaYaw = 100.0f;

    UPROPERTY(EditAnywhere, Category = "Rotation")
    float InertiaRoll = 100.0f;

    UPROPERTY(EditAnywhere, Category = "Rotation")
    float StabilizationGainPitch = 10.0f;

    UPROPERTY(EditAnywhere, Category = "Rotation")
    float StabilizationGainYaw = 10.0f;

    UPROPERTY(EditAnywhere, Category = "Rotation")
    float StabilizationGainRoll = 10.0f;

    UPROPERTY(EditAnywhere, Category = "Rotation")
    float MaxAngularSpeed = 360.0f;

    UPROPERTY(EditAnywhere, Category = "Rotation")
    float AngularDampingFactor = 10.0f;

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bEnableDebugVisuals = true;

private:
    FQuat CurrentOrientation;
    FVector AngularVelocity;
    FVector CurrentVelocity;
    float CurrentThrusterPitch;
    float CurrentThrusterYaw;
    float CurrentThrusterRoll;
    float CurrentThrottle;
    float CurrentPitchInput;
    float CurrentYawInput;
    float CurrentRollInput;
    bool IsFirstTick = true;
    bool IsAirbrakeActive = false;

    UFUNCTION()
    void OnActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);
};