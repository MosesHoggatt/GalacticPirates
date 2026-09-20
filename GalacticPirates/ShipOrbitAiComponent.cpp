#include "ShipOrbitAiComponent.h"
#include "WalkableShip.h"
#include "ShipMovementComponent.h"
#include "GalacticPirates.h"
#include "EngineUtils.h"

UShipOrbitAiComponent::UShipOrbitAiComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UShipOrbitAiComponent::BeginPlay()
{
	Super::BeginPlay();
	OwningShip = Cast<AWalkableShip>(GetOwner());
	if (OwningShip && OwningShip->ShipMovement)
	{
		OwningShip->ShipMovement->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);
	}

	const uint32 Hash = GetTypeHash(GetOwner() ? GetOwner()->GetFName() : GetFName());
	OrbitDirection = (Hash & 1) ? 1.0f : -1.0f;
	OrbitRadius += static_cast<float>(Hash % 9) * 250.0f;
}

AWalkableShip* UShipOrbitAiComponent::FindOrbitTarget() const
{
	UWorld* World = GetWorld();
	if (!World || !OwningShip)
	{
		return nullptr;
	}

	AWalkableShip* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<AWalkableShip> It(World); It; ++It)
	{
		AWalkableShip* Candidate = *It;
		if (!Candidate || Candidate == OwningShip || Candidate->IsWrecked())
		{
			continue;
		}

		if (Candidate->GetPlayersAboard().Num() <= 0 && Candidate->GetCurrentPilot() == nullptr)
		{
			continue;
		}

		const float Dist = FVector::Dist(OwningShip->GetActorLocation(), Candidate->GetActorLocation());
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = Candidate;
		}
	}
	return Best;
}

void UShipOrbitAiComponent::ApplyOrbitSteering(AWalkableShip* Target)
{
	if (!OwningShip || !Target || !OwningShip->ShipMovement)
	{
		return;
	}

	const FVector SelfLoc = OwningShip->GetActorLocation();
	const FVector TargetLoc = Target->GetActorLocation() + Target->GetActorUpVector() * OrbitHeightOffset;
	FVector Offset = SelfLoc - TargetLoc;
	if (Offset.SizeSquared() < 100.0f)
	{
		Offset = OwningShip->GetActorRightVector() * 200.0f;
	}

	const float Distance = Offset.Size();
	const FVector Radial = Offset / Distance;
	FVector OrbitAxis = Target->GetActorUpVector();
	if (OrbitAxis.IsNearlyZero())
	{
		OrbitAxis = FVector::UpVector;
	}

	FVector Tangent = FVector::CrossProduct(OrbitAxis, Radial) * OrbitDirection;
	if (Tangent.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		Tangent = FVector::CrossProduct(FVector::ForwardVector, Radial) * OrbitDirection;
	}
	Tangent.Normalize();

	const float RadiusError = Distance - OrbitRadius;
	FVector DesiredDir = Tangent * 1.0f - Radial * FMath::Clamp(RadiusError * RadiusGain, -1.15f, 1.15f);
	DesiredDir += OrbitAxis * FMath::Clamp(-RadiusError * 0.00015f, -0.25f, 0.25f);
	DesiredDir.Normalize();

	const FVector LookDir = (DesiredDir * 0.7f + (TargetLoc - SelfLoc).GetSafeNormal() * 0.3f).GetSafeNormal();
	const FVector LocalLook = OwningShip->GetActorQuat().UnrotateVector(LookDir);
	const float Yaw = FMath::Clamp(FMath::Atan2(LocalLook.Y, LocalLook.X) / (PI * 0.35f), -1.0f, 1.0f) * SteeringGain;
	const float Horizontal = FMath::Sqrt(LocalLook.X * LocalLook.X + LocalLook.Y * LocalLook.Y);
	const float Pitch = FMath::Clamp(FMath::Atan2(-LocalLook.Z, Horizontal) / (PI * 0.35f), -1.0f, 1.0f) * SteeringGain;

	FVector LocalThrust = OwningShip->GetActorQuat().UnrotateVector(DesiredDir);
	LocalThrust.X = FMath::Clamp(LocalThrust.X + 0.35f, -1.0f, 1.0f);
	LocalThrust.Y = FMath::Clamp(LocalThrust.Y, -1.0f, 1.0f);
	LocalThrust.Z = FMath::Clamp(LocalThrust.Z, -1.0f, 1.0f);

	OwningShip->ShipMovement->SetThrustInput(LocalThrust);
	OwningShip->ShipMovement->SetRotationInput(FVector(0.0f, Pitch, FMath::Clamp(Yaw, -1.0f, 1.0f)));
}

void UShipOrbitAiComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnabled || !OwningShip || !OwningShip->HasAuthority() || OwningShip->IsWrecked())
	{
		return;
	}

	if (OwningShip->GetCurrentPilot() != nullptr || OwningShip->GetPlayersAboard().Num() > 0)
	{
		return;
	}

	if (AWalkableShip* Target = FindOrbitTarget())
	{
		ApplyOrbitSteering(Target);
	}
}
