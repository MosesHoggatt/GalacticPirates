#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "QuatCamera.generated.h"

UCLASS(ClassGroup=(Camera), meta=(BlueprintSpawnableComponent))
class GALACTICPIRATES_API UQuatCamera : public UCameraComponent
{
	GENERATED_BODY()

public:
	UQuatCamera();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quaternion Camera")
	float UpTransitionSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quaternion Camera")
	float PitchClampMin = -89.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quaternion Camera")
	float PitchClampMax = 89.0f;

	UFUNCTION(BlueprintCallable, Category = "Quaternion Camera")
	void SetReferenceUpDirection(const FVector& NewUp, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "Quaternion Camera")
	void AddLookInput(float DeltaYaw, float DeltaPitch);

	UFUNCTION(BlueprintCallable, Category = "Quaternion Camera")
	FVector GetReferenceUpDirection() const { return CurrentUpDirection; }

	UFUNCTION(BlueprintCallable, Category = "Quaternion Camera")
	float GetCurrentYaw() const { return LocalYaw; }

	UFUNCTION(BlueprintCallable, Category = "Quaternion Camera")
	float GetCurrentPitch() const { return LocalPitch; }

	UFUNCTION(BlueprintCallable, Category = "Quaternion Camera")
	void ResetOrientation();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	FVector CurrentUpDirection;
	FVector TargetUpDirection;
	float LocalYaw;
	float LocalPitch;

	void UpdateUpDirection(float DeltaTime);
	void PreserveWorldOrientationOnUpChange(const FVector& OldUp, const FVector& NewUp);
	FQuat ComputeWorldRotation() const;
	void DecomposeWorldRotation(const FQuat& WorldRotation, const FVector& UpDir, float& OutYaw, float& OutPitch) const;
};
