#include "ShipMovementComponent.h"
#include "WalkableShip.h"
#include "SpaceCraft.h"
#include "GalacticPirates.h"
#include "ShipDebug.h"
#include "Net/UnrealNetwork.h"

UShipMovementComponent::UShipMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(false);
}

void UShipMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	
	ThrustInput = FVector::ZeroVector;
	RotationInput = FVector::ZeroVector;
	HeldThrustInput = FVector::ZeroVector;
	HeldRotationInput = FVector::ZeroVector;
	LinearVelocity = FVector::ZeroVector;
	AngularVelocity = FVector::ZeroVector;
}

void UShipMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickPhysics(DeltaTime);
}

void UShipMovementComponent::TickPhysics(float DeltaTime)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (GPIsCraftWrecked(GetOwner()))
	{
		return;
	}

	ApplyThrust(DeltaTime);
	ApplyTorque(DeltaTime);
	ApplyDampening(DeltaTime);
	if (bVelocityOverride)
	{
		LinearVelocity = OverrideLinearVelocity;
		AngularVelocity = OverrideAngularVelocity;
	}
	ClampVelocities();
	IntegrateVelocities(DeltaTime);

	if (GPShipMoveLogLevel() > 0 && (GFrameCounter % 15) == 0)
	{
		UE_LOG(LogGalacticPirates, Warning,
			TEXT("[ShipMove] Tick %s tickOn=%d wreck=%d override=%d mass=%.0f thrust=%s rot=%s heldT=%s heldR=%s lin=%.1f ang=%.1f loc=%s"),
			*GetNameSafe(GetOwner()),
			PrimaryComponentTick.IsTickFunctionEnabled() ? 1 : 0,
			GPIsCraftWrecked(GetOwner()) ? 1 : 0,
			bVelocityOverride ? 1 : 0,
			ShipMass,
			*ThrustInput.ToCompactString(),
			*RotationInput.ToCompactString(),
			*HeldThrustInput.ToCompactString(),
			*HeldRotationInput.ToCompactString(),
			LinearVelocity.Size(),
			AngularVelocity.Size(),
			*GetOwner()->GetActorLocation().ToCompactString());
	}
}

void UShipMovementComponent::SetThrustInput(const FVector& Input)
{
	ThrustInput.X = FMath::Clamp(Input.X, -1.0f, 1.0f);
	ThrustInput.Y = FMath::Clamp(Input.Y, -1.0f, 1.0f);
	ThrustInput.Z = FMath::Clamp(Input.Z, -1.0f, 1.0f);
	HeldThrustInput = ThrustInput;
	if (GPShipMoveLogLevel() > 1)
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipMove] SetThrust %s -> %s"), *GetNameSafe(GetOwner()), *ThrustInput.ToCompactString());
	}
}

void UShipMovementComponent::SetRotationInput(const FVector& Input)
{
	RotationInput.X = FMath::Clamp(Input.X, -1.0f, 1.0f);
	RotationInput.Y = FMath::Clamp(Input.Y, -1.0f, 1.0f);
	RotationInput.Z = FMath::Clamp(Input.Z, -1.0f, 1.0f);
	HeldRotationInput = RotationInput;
	if (GPShipMoveLogLevel() > 1)
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipMove] SetRotation %s -> %s"), *GetNameSafe(GetOwner()), *RotationInput.ToCompactString());
	}
}

void UShipMovementComponent::SetLinearVelocity(const FVector& NewVelocity)
{
	LinearVelocity = NewVelocity;
}

void UShipMovementComponent::SetAngularVelocity(const FVector& NewVelocity)
{
	AngularVelocity = NewVelocity;
}

void UShipMovementComponent::SetVelocityOverride(const FVector& NewLinear, const FVector& NewAngular, bool bEnable)
{
	OverrideLinearVelocity = NewLinear;
	OverrideAngularVelocity = NewAngular;
	bVelocityOverride = bEnable;
	if (bEnable)
	{
		LinearVelocity = NewLinear;
		AngularVelocity = NewAngular;
	}
}

void UShipMovementComponent::ApplyThrust(float DeltaTime)
{
	if (ShipMass <= 0.0f)
	{
		return;
	}

	FVector LocalForce;
	LocalForce.X = ThrustInput.X * ForwardThrustPower;
	LocalForce.Y = ThrustInput.Y * StrafeThrustPower;
	LocalForce.Z = ThrustInput.Z * VerticalThrustPower;

	FVector WorldForce = GetOwner()->GetActorQuat().RotateVector(LocalForce);
	FVector Acceleration = WorldForce / ShipMass;
	LinearVelocity += Acceleration * DeltaTime;
}

void UShipMovementComponent::ApplyTorque(float DeltaTime)
{
	if (ShipMass <= 0.0f)
	{
		return;
	}

	const float MomentOfInertia = ShipMass * 100.0f;

	FVector LocalTorque;
	LocalTorque.X = RotationInput.X * RollTorque;
	LocalTorque.Y = RotationInput.Y * PitchTorque;
	LocalTorque.Z = RotationInput.Z * YawTorque;

	FVector AngularAcceleration = LocalTorque / MomentOfInertia;
	AngularVelocity += AngularAcceleration * DeltaTime;
}

void UShipMovementComponent::ApplyDampening(float DeltaTime)
{
	float TranslationFactor = FMath::Pow(1.0f - TranslationDampening, DeltaTime);
	LinearVelocity *= TranslationFactor;

	float RotationFactor = FMath::Pow(1.0f - RotationDampening, DeltaTime);
	AngularVelocity *= RotationFactor;
}

void UShipMovementComponent::ClampVelocities()
{
	if (LinearVelocity.SizeSquared() > MaxLinearVelocity * MaxLinearVelocity)
	{
		LinearVelocity = LinearVelocity.GetSafeNormal() * MaxLinearVelocity;
	}

	if (AngularVelocity.SizeSquared() > MaxAngularVelocity * MaxAngularVelocity)
	{
		AngularVelocity = AngularVelocity.GetSafeNormal() * MaxAngularVelocity;
	}
}

void UShipMovementComponent::IntegrateVelocities(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	FVector NewLocation = Owner->GetActorLocation() + LinearVelocity * DeltaTime;

	FQuat CurrentRotation = Owner->GetActorQuat();
	
	FVector AngularVelocityRadians = FMath::DegreesToRadians(AngularVelocity);
	float AngleMagnitude = AngularVelocityRadians.Size() * DeltaTime;
	
	if (AngleMagnitude > KINDA_SMALL_NUMBER)
	{
		FVector RotationAxis = AngularVelocityRadians.GetSafeNormal();
		FQuat DeltaRotation = FQuat(RotationAxis, AngleMagnitude);
		FQuat NewRotation = (DeltaRotation * CurrentRotation).GetNormalized();
		Owner->SetActorLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	else
	{
		Owner->SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}
