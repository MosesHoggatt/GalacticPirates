#include "QuatCamera.h"
#include "ShipDebug.h"
#include "GalacticPirates.h"
#include "GalacticPiratesCharacter.h"
#include "GameFramework/Pawn.h"

UQuatCamera::UQuatCamera()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	bUsePawnControlRotation = false;

	CurrentFrame = FQuat::Identity;
	TargetFrame = FQuat::Identity;
	LocalYaw = 0.0f;
	LocalPitch = 0.0f;
	LastComputedWorldRotation = FQuat::Identity;
}

void UQuatCamera::BeginPlay()
{
	Super::BeginPlay();

	CurrentFrame = FQuat::Identity;
	TargetFrame = FQuat::Identity;
	LocalYaw = 0.0f;
	LocalPitch = 0.0f;
}

void UQuatCamera::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	UpdateReferenceFrame(DeltaTime);

	FQuat WorldRotation = ComputeWorldRotation();
	LastComputedWorldRotation = WorldRotation;
	SetWorldRotation(WorldRotation);

	GPSampleHelmJitter(Cast<AGalacticPiratesCharacter>(GetOwner()));

	if (GPShipDebugLevel() >= 2)
	{
		static float TimeSinceCameraLog = 0.0f;
		TimeSinceCameraLog += DeltaTime;
		if (TimeSinceCameraLog >= 0.5f)
		{
			TimeSinceCameraLog = 0.0f;
			UE_LOG(LogGalacticPirates, Warning,
				TEXT("[ShipDebug][CameraTick] LocalYaw=%.2f LocalPitch=%.2f Frame=%s Computed=%s Actual=%s AttachParent=%s"),
				LocalYaw,
				LocalPitch,
				*CurrentFrame.Rotator().ToCompactString(),
				*WorldRotation.Rotator().ToCompactString(),
				*GetComponentQuat().Rotator().ToCompactString(),
				*GetNameSafe(GetAttachParent()));
		}
	}
}

void UQuatCamera::SetReferenceUpDirection(const FVector& NewUp, bool bInstant)
{
	FVector NormalizedUp = NewUp.GetSafeNormal();
	if (NormalizedUp.IsNearlyZero())
	{
		NormalizedUp = FVector::UpVector;
	}

	SetReferenceOrientation(FQuat::FindBetweenNormals(FVector::UpVector, NormalizedUp), bInstant);
}

void UQuatCamera::SetReferenceOrientation(const FQuat& NewFrame, bool bInstant)
{
	TargetFrame = NewFrame.GetNormalized();
	if (!TargetFrame.IsNormalized())
	{
		TargetFrame = FQuat::Identity;
	}

	if (bInstant)
	{
		CurrentFrame = TargetFrame;
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

FVector UQuatCamera::GetPlanarLookForward() const
{
	const FQuat YawRotation(CurrentFrame.GetUpVector(), FMath::DegreesToRadians(LocalYaw));
	return (YawRotation * CurrentFrame).GetForwardVector();
}

void UQuatCamera::UpdateReferenceFrame(float DeltaTime)
{
	if (CurrentFrame.Equals(TargetFrame, 0.0001f))
	{
		CurrentFrame = TargetFrame;
		return;
	}

	const float Alpha = FMath::Clamp(UpTransitionSpeed * DeltaTime, 0.0f, 1.0f);
	CurrentFrame = FQuat::Slerp(CurrentFrame, TargetFrame, Alpha).GetNormalized();
}

FQuat UQuatCamera::ComputeWorldRotation() const
{
	const FQuat YawRotation(CurrentFrame.GetUpVector(), FMath::DegreesToRadians(LocalYaw));
	const FVector RightAfterYaw = YawRotation.RotateVector(CurrentFrame.GetRightVector());
	const FQuat PitchRotation(RightAfterYaw, FMath::DegreesToRadians(-LocalPitch));
	return (PitchRotation * YawRotation * CurrentFrame).GetNormalized();
}
