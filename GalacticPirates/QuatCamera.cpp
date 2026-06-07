#include "QuatCamera.h"

UQuatCamera::UQuatCamera()
{
	PrimaryComponentTick.bCanEverTick = true;
	bUsePawnControlRotation = false;
	
	CurrentUpDirection = FVector::UpVector;
	TargetUpDirection = FVector::UpVector;
	LocalYaw = 0.0f;
	LocalPitch = 0.0f;
}

void UQuatCamera::BeginPlay()
{
	Super::BeginPlay();
	
	CurrentUpDirection = FVector::UpVector;
	TargetUpDirection = FVector::UpVector;
	LocalYaw = 0.0f;
	LocalPitch = 0.0f;
}

void UQuatCamera::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateUpDirection(DeltaTime);

	FQuat WorldRotation = ComputeWorldRotation();
	SetWorldRotation(WorldRotation);
}

void UQuatCamera::SetReferenceUpDirection(const FVector& NewUp, bool bInstant)
{
	FVector NormalizedUp = NewUp.GetSafeNormal();
	if (NormalizedUp.IsNearlyZero())
	{
		NormalizedUp = FVector::UpVector;
	}

	TargetUpDirection = NormalizedUp;

	if (bInstant)
	{
		FVector OldUp = CurrentUpDirection;
		CurrentUpDirection = TargetUpDirection;
		PreserveWorldOrientationOnUpChange(OldUp, CurrentUpDirection);
	}
}

void UQuatCamera::AddLookInput(float DeltaYaw, float DeltaPitch)
{
	LocalYaw += DeltaYaw;
	LocalPitch = FMath::Clamp(LocalPitch + DeltaPitch, PitchClampMin, PitchClampMax);

	while (LocalYaw > 180.0f) LocalYaw -= 360.0f;
	while (LocalYaw < -180.0f) LocalYaw += 360.0f;
}

void UQuatCamera::ResetOrientation()
{
	LocalYaw = 0.0f;
	LocalPitch = 0.0f;
}

void UQuatCamera::UpdateUpDirection(float DeltaTime)
{
	if (CurrentUpDirection.Equals(TargetUpDirection, 0.0001f))
	{
		return;
	}

	FVector OldUp = CurrentUpDirection;

	float Alpha = FMath::Clamp(UpTransitionSpeed * DeltaTime, 0.0f, 1.0f);
	FVector InterpolatedUp = FMath::Lerp(CurrentUpDirection, TargetUpDirection, Alpha);
	CurrentUpDirection = InterpolatedUp.GetSafeNormal();

	PreserveWorldOrientationOnUpChange(OldUp, CurrentUpDirection);
}

void UQuatCamera::PreserveWorldOrientationOnUpChange(const FVector& OldUp, const FVector& NewUp)
{
	if (OldUp.Equals(NewUp, 0.0001f))
	{
		return;
	}

	FQuat CurrentWorldRotation = ComputeWorldRotation();
	
	float NewYaw, NewPitch;
	DecomposeWorldRotation(CurrentWorldRotation, NewUp, NewYaw, NewPitch);

	LocalYaw = NewYaw;
	LocalPitch = FMath::Clamp(NewPitch, PitchClampMin, PitchClampMax);
}

FQuat UQuatCamera::ComputeWorldRotation() const
{
	FVector Forward = FVector::ForwardVector;
	FVector Right = FVector::RightVector;
	
	float CrossZ = FVector::UpVector.X * CurrentUpDirection.Y - FVector::UpVector.Y * CurrentUpDirection.X;
	float DotUp = FVector::UpVector | CurrentUpDirection;
	
	FQuat AlignToUp = FQuat::FindBetweenNormals(FVector::UpVector, CurrentUpDirection);
	
	FQuat YawRotation = FQuat(CurrentUpDirection, FMath::DegreesToRadians(LocalYaw));
	
	FVector RightAfterYaw = YawRotation.RotateVector(AlignToUp.RotateVector(Right));
	
	FQuat PitchRotation = FQuat(RightAfterYaw, FMath::DegreesToRadians(-LocalPitch));
	
	return (PitchRotation * YawRotation * AlignToUp).GetNormalized();
}

void UQuatCamera::DecomposeWorldRotation(const FQuat& WorldRotation, const FVector& UpDir, float& OutYaw, float& OutPitch) const
{
	FVector WorldForward = WorldRotation.GetForwardVector();
	
	FVector ProjectedForward = FVector::VectorPlaneProject(WorldForward, UpDir);
	
	if (ProjectedForward.IsNearlyZero())
	{
		OutYaw = LocalYaw;
		OutPitch = (WorldForward | UpDir) > 0.0f ? PitchClampMax : PitchClampMin;
		return;
	}
	
	ProjectedForward.Normalize();
	
	FQuat AlignToUp = FQuat::FindBetweenNormals(FVector::UpVector, UpDir);
	FVector BaseForward = AlignToUp.RotateVector(FVector::ForwardVector);
	FVector BaseRight = AlignToUp.RotateVector(FVector::RightVector);
	
	float ForwardDot = ProjectedForward | BaseForward;
	float RightDot = ProjectedForward | BaseRight;
	OutYaw = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));
	
	float PitchSin = WorldForward | UpDir;
	PitchSin = FMath::Clamp(PitchSin, -1.0f, 1.0f);
	OutPitch = FMath::RadiansToDegrees(FMath::Asin(-PitchSin));
}
