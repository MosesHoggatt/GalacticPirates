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
	void SetReferenceOrientation(const FQuat& NewFrame, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "Quaternion Camera")
	void AddLookInput(float DeltaYaw, float DeltaPitch);

	UFUNCTION(BlueprintCallable, Category = "Quaternion Camera")
	FVector GetReferenceUpDirection() const { return CurrentFrame.GetUpVector(); }

	UFUNCTION(BlueprintCallable, Category = "Quaternion Camera")
	FQuat GetReferenceOrientation() const { return CurrentFrame; }

	UFUNCTION(BlueprintCallable, Category = "Quaternion Camera")
	float GetCurrentYaw() const { return LocalYaw; }

	UFUNCTION(BlueprintCallable, Category = "Quaternion Camera")
	float GetCurrentPitch() const { return LocalPitch; }

	UFUNCTION(BlueprintCallable, Category = "Quaternion Camera")
	void ResetOrientation();

	/** Skip local yaw/pitch and leave world transform to an external owner (minigun sight). */
	void SetGunSightLock(bool bLocked);

	void SetDeathFollow(bool bFollow);
	bool IsDeathFollow() const { return bDeathFollow; }

	UFUNCTION(BlueprintCallable, Category = "Quaternion Camera")
	FVector GetPlanarLookForward() const;

	FQuat GetLastComputedWorldRotation() const { return LastComputedWorldRotation; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	FQuat CurrentFrame;
	FQuat TargetFrame;
	float LocalYaw;
	float LocalPitch;
	FQuat LastComputedWorldRotation;
	bool bGunSightLock = false;
	bool bDeathFollow = false;

	void UpdateReferenceFrame(float DeltaTime);
	FQuat ComputeWorldRotation() const;
};
