#include "StarshipMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "StarshipAIController.h"

UStarshipMovementComponent::UStarshipMovementComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickGroup = TG_DuringPhysics;
    PrimaryComponentTick.bAllowTickOnDedicatedServer = true;
    IsFirstTick = true;

    CurrentOrientation = FQuat::Identity;
    AngularVelocity = FVector::ZeroVector;
    CurrentVelocity = FVector::ZeroVector;
    CurrentThrusterPitch = 0.0f;
    CurrentThrusterYaw = 0.0f;
    CurrentThrusterRoll = 0.0f;
    CurrentThrottle = 0.0f;
    CurrentPitchInput = 0.0f;
    CurrentYawInput = 0.0f;
    CurrentRollInput = 0.0f;
    IsAirbrakeActive = false;
}

void UStarshipMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    FString ControllerType = Owner && Owner->GetInstigatorController() && Owner->GetInstigatorController()->IsA<AStarshipAIController>() ? TEXT("AI") : TEXT("Player");
    UE_LOG(LogTemp, Log, TEXT("%s StarshipMovementComponent: Initialized on %s"), *ControllerType, *GetOwner()->GetName());
    if (bEnableDebugVisuals)
    {
        DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + FVector(0, 0, 100), FString::Printf(TEXT("Initialized %s"), *GetOwner()->GetName()), nullptr, FColor::Green, 5.0f);
    }

    if (Owner)
    {
        Owner->OnActorHit.AddDynamic(this, &UStarshipMovementComponent::OnActorHit);
        UE_LOG(LogTemp, Log, TEXT("%s Starship %s registered for hit events"), *ControllerType, *Owner->GetName());
        if (bEnableDebugVisuals)
        {
            DrawDebugString(GetWorld(), Owner->GetActorLocation() + FVector(0, 0, 150), TEXT("Registered for Hits"), nullptr, FColor::White, 5.0f);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("%s StarshipMovementComponent has no owner!"), *ControllerType);
        if (bEnableDebugVisuals && GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("StarshipMovementComponent: No owner"));
        }
    }
}

void UStarshipMovementComponent::SetThrottleInput(float ThrottleInput)
{
    CurrentThrottle = FMath::Clamp(ThrottleInput, -1.0f, 1.0f);
    AActor* Owner = GetOwner();
    FString ControllerType = Owner && Owner->GetInstigatorController() && Owner->GetInstigatorController()->IsA<AStarshipAIController>() ? TEXT("AI") : TEXT("Player");
    if (bEnableDebugVisuals)
    {
        DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + FVector(0, 0, 200), FString::Printf(TEXT("Throttle: %.2f"), CurrentThrottle), nullptr, FColor::Cyan, 0.0f);
        UE_LOG(LogTemp, Log, TEXT("%s Throttle Input: %.2f"), *ControllerType, CurrentThrottle);
    }
}

void UStarshipMovementComponent::SetSteeringInput(float PitchInput, float YawInput, float RollInput)
{
    CurrentPitchInput = FMath::Clamp(PitchInput, -1.0f, 1.0f);
    CurrentYawInput = FMath::Clamp(YawInput, -1.0f, 1.0f);
    CurrentRollInput = FMath::Clamp(RollInput, -1.0f, 1.0f);
    AActor* Owner = GetOwner();
    FString ControllerType = Owner && Owner->GetInstigatorController() && Owner->GetInstigatorController()->IsA<AStarshipAIController>() ? TEXT("AI") : TEXT("Player");
    if (bEnableDebugVisuals)
    {
        DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + FVector(0, 0, 250), FString::Printf(TEXT("Steering: %.2f, %.2f, %.2f"), CurrentPitchInput, CurrentYawInput, CurrentRollInput), nullptr, FColor::Cyan, 0.0f);
        UE_LOG(LogTemp, Log, TEXT("%s Steering Input - Pitch: %.2f, Yaw: %.2f, Roll: %.2f"), *ControllerType, CurrentPitchInput, CurrentYawInput, CurrentRollInput);
    }
}

void UStarshipMovementComponent::StartAirbrake()
{
    IsAirbrakeActive = true;
    AActor* Owner = GetOwner();
    FString ControllerType = Owner && Owner->GetInstigatorController() && Owner->GetInstigatorController()->IsA<AStarshipAIController>() ? TEXT("AI") : TEXT("Player");
    if (bEnableDebugVisuals)
    {
        DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + FVector(0, 0, 300), TEXT("Airbrake On"), nullptr, FColor::Yellow, 0.0f);
        UE_LOG(LogTemp, Log, TEXT("%s Airbrake activated for %s"), *ControllerType, *GetOwner()->GetName());
    }
}

void UStarshipMovementComponent::StopAirbrake()
{
    IsAirbrakeActive = false;
    AActor* Owner = GetOwner();
    FString ControllerType = Owner && Owner->GetInstigatorController() && Owner->GetInstigatorController()->IsA<AStarshipAIController>() ? TEXT("AI") : TEXT("Player");
    if (bEnableDebugVisuals)
    {
        DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + FVector(0, 0, 300), TEXT("Airbrake Off"), nullptr, FColor::Yellow, 0.0f);
        UE_LOG(LogTemp, Log, TEXT("%s Airbrake deactivated for %s"), *ControllerType, *GetOwner()->GetName());
    }
}

void UStarshipMovementComponent::OnActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
    AActor* Owner = GetOwner();
    FString ControllerType = Owner && Owner->GetInstigatorController() && Owner->GetInstigatorController()->IsA<AStarshipAIController>() ? TEXT("AI") : TEXT("Player");
    if (OtherActor && OtherActor->ActorHasTag(TEXT("Projectile")))
    {
        UE_LOG(LogTemp, Log, TEXT("%s Ignoring projectile hit from %s"), *ControllerType, *OtherActor->GetName());
        if (bEnableDebugVisuals)
        {
            DrawDebugString(GetWorld(), Hit.ImpactPoint + FVector(0, 0, 50), FString::Printf(TEXT("Ignored %s"), *OtherActor->GetName()), nullptr, FColor::White, 0.0f);
        }
        return;
    }

    FString OtherActorName = OtherActor ? OtherActor->GetName() : TEXT("Non-Actor (Static Mesh)");
    FVector HitLocation = Hit.ImpactPoint;
    FVector SurfaceNormal = Hit.Normal.GetSafeNormal();

    UE_LOG(LogTemp, Log, TEXT("%s Starship hit: %s, HitLocation: %s, Normal: %s, Impulse: %s"),
        *ControllerType, *OtherActorName, *HitLocation.ToString(), *SurfaceNormal.ToString(), *NormalImpulse.ToString());

    if (bEnableDebugVisuals)
    {
        DrawDebugSphere(GetWorld(), HitLocation, 50.0f, 12, FColor::Red, false, 0.0f);
        DrawDebugLine(GetWorld(), HitLocation, HitLocation + SurfaceNormal * 100.0f, FColor::Red, false, 0.0f);
        DrawDebugString(GetWorld(), HitLocation + FVector(0, 0, 50), FString::Printf(TEXT("Hit by %s"), *OtherActorName), nullptr, FColor::Red, 0.0f);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(15, 0.0f, FColor::Red, FString::Printf(TEXT("%s: Hit by %s at %s"), *GetOwner()->GetName(), *OtherActorName, *HitLocation.ToString()));
        }
    }

    float VelocityNormalComponent = FVector::DotProduct(CurrentVelocity, SurfaceNormal);
    FVector VelocityNormal = VelocityNormalComponent * SurfaceNormal;
    FVector VelocityTangential = CurrentVelocity - VelocityNormal;
    FVector NewVelocityNormal = -VelocityNormal * LinearRestitution;
    FVector NewVelocityTangential = VelocityTangential * SurfaceFriction;
    CurrentVelocity = NewVelocityNormal + NewVelocityTangential;

    float Speed = CurrentVelocity.Size();
    if (Speed > MaxSpeed)
    {
        CurrentVelocity = CurrentVelocity.GetSafeNormal() * MaxSpeed;
    }

    FVector CenterOfMass = GetOwner()->GetActorLocation();
    FVector RelativeHitPosition = HitLocation - CenterOfMass;
    FVector ImpulseDirection = SurfaceNormal;
    FVector TorqueImpulse = FVector::CrossProduct(RelativeHitPosition, ImpulseDirection);
    float IncidentSpeed = VelocityNormalComponent < 0.0f ? -VelocityNormalComponent : 0.0f;
    FVector AngularImpulse = TorqueImpulse * AngularImpulseStrength * IncidentSpeed;

    AngularVelocity.X += AngularImpulse.X / InertiaRoll;
    AngularVelocity.Y += AngularImpulse.Y / InertiaPitch;
    AngularVelocity.Z += AngularImpulse.Z / InertiaYaw;

    if (AngularVelocity.Size() > MaxAngularSpeed)
    {
        AngularVelocity = AngularVelocity.GetSafeNormal() * MaxAngularSpeed;
    }

    UE_LOG(LogTemp, Log, TEXT("%s Post-rebound - NewVelocity: %s, NewAngularVelocity: %s"),
        *ControllerType, *CurrentVelocity.ToString(), *AngularVelocity.ToString());
    if (bEnableDebugVisuals)
    {
        DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + FVector(0, 0, 350), FString::Printf(TEXT("Velocity: %s"), *CurrentVelocity.ToString()), nullptr, FColor::White, 0.0f);
    }
}

void UStarshipMovementComponent::TickComponent(float DeltaTimeSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTimeSeconds, TickType, ThisTickFunction);

    DeltaTimeSeconds = FMath::Min(DeltaTimeSeconds, 0.033f);

    AActor* Owner = GetOwner();
    FString ControllerType = Owner && Owner->GetInstigatorController() && Owner->GetInstigatorController()->IsA<AStarshipAIController>() ? TEXT("AI") : TEXT("Player");
    if (IsFirstTick)
    {
        CurrentOrientation = GetOwner()->GetActorQuat();
        IsFirstTick = false;
        UE_LOG(LogTemp, Log, TEXT("%s StarshipMovementComponent: First tick initialized for %s"), *ControllerType, *GetOwner()->GetName());
    }

    FVector LocalAngularVelocity = CurrentOrientation.Inverse().RotateVector(AngularVelocity);

    CurrentThrusterPitch = CurrentPitchInput != 0.0f
        ? CurrentPitchInput * MaxTorquePitch
        : -FMath::Clamp(LocalAngularVelocity.Y * StabilizationGainPitch, -MaxTorquePitch, MaxTorquePitch);
    CurrentThrusterYaw = CurrentYawInput != 0.0f
        ? CurrentYawInput * MaxTorqueYaw
        : -FMath::Clamp(LocalAngularVelocity.Z * StabilizationGainYaw, -MaxTorqueYaw, MaxTorqueYaw);
    CurrentThrusterRoll = CurrentRollInput != 0.0f
        ? CurrentRollInput * MaxTorqueRoll
        : -FMath::Clamp(LocalAngularVelocity.X * StabilizationGainRoll, -MaxTorqueRoll, MaxTorqueRoll);

    FVector LocalTorque = FVector(CurrentThrusterRoll, CurrentThrusterPitch, CurrentThrusterYaw);
    FVector WorldTorque = CurrentOrientation.RotateVector(LocalTorque);

    FVector AngularAcceleration;
    AngularAcceleration.X = WorldTorque.X / InertiaRoll;
    AngularAcceleration.Y = WorldTorque.Y / InertiaPitch;
    AngularAcceleration.Z = WorldTorque.Z / InertiaYaw;

    AngularVelocity += AngularAcceleration * DeltaTimeSeconds;
    float DampingFactor = FMath::Exp(-AngularDampingFactor * DeltaTimeSeconds);
    AngularVelocity *= DampingFactor;

    if (AngularVelocity.Size() > MaxAngularSpeed)
    {
        AngularVelocity = AngularVelocity.GetSafeNormal() * MaxAngularSpeed;
    }

    FVector RotationDelta = AngularVelocity * DeltaTimeSeconds;
    if (RotationDelta.SizeSquared() > 0.0f)
    {
        FQuat DeltaQuat = FQuat(RotationDelta.GetSafeNormal(), RotationDelta.Size());
        CurrentOrientation = DeltaQuat * CurrentOrientation;
        CurrentOrientation.Normalize();
    }

    UE_LOG(LogTemp, Log, TEXT("%s TickComponent: Throttle %.2f, Pitch %.2f, Yaw %.2f, Roll %.2f"),
        *ControllerType, CurrentThrottle, CurrentThrusterPitch, CurrentThrusterYaw, CurrentThrusterRoll);

    GetOwner()->GetRootComponent()->SetWorldRotation(CurrentOrientation);

    FVector ForwardVector = CurrentOrientation.RotateVector(FVector(1.0f, 0.0f, 0.0f));
    FVector Acceleration = ForwardVector * (CurrentThrottle * MaxAcceleration);

    if (IsAirbrakeActive && CurrentVelocity.SizeSquared() > 0.0f)
    {
        if (CurrentThrottle > 0.0f)
        {
            FVector ForwardVelocity = ForwardVector * FVector::DotProduct(CurrentVelocity, ForwardVector);
            FVector NonForwardVelocity = CurrentVelocity - ForwardVelocity;
            if (NonForwardVelocity.SizeSquared() > 0.0f)
            {
                FVector NonForwardDirection = NonForwardVelocity.GetSafeNormal();
                FVector NonForwardDamping = -NonForwardDirection * DriftDampingStrength;
                Acceleration += NonForwardDamping;
            }
            if (ForwardVelocity.SizeSquared() > 0.0f)
            {
                FVector ForwardDirection = ForwardVelocity.GetSafeNormal();
                FVector ForwardDamping = -ForwardDirection * (AirbrakeStrength * DriftForwardDampingFactor);
                Acceleration += ForwardDamping;
            }
        }
        else
        {
            FVector VelocityDirection = CurrentVelocity.GetSafeNormal();
            FVector AirbrakeDeceleration = -VelocityDirection * AirbrakeStrength;
            Acceleration += AirbrakeDeceleration;
        }
    }

    CurrentVelocity += Acceleration * DeltaTimeSeconds;

    if (bEnableDebugVisuals)
    {
        FVector OwnerLocation = GetOwner()->GetActorLocation();
        DrawDebugLine(GetWorld(), OwnerLocation, OwnerLocation + CurrentVelocity * 0.1f, FColor::Blue, false, 0.0f);
        DrawDebugLine(GetWorld(), OwnerLocation, OwnerLocation + Acceleration * 0.1f, FColor::Yellow, false, 0.0f);
        DrawDebugString(GetWorld(), OwnerLocation + FVector(0, 0, 400), FString::Printf(TEXT("Vel: %s, Acc: %s"), *CurrentVelocity.ToString(), *Acceleration.ToString()), nullptr, FColor::Blue, 0.0f);
    }

    float Speed = CurrentVelocity.Size();
    if (Speed > MaxSpeed)
    {
        CurrentVelocity = CurrentVelocity.GetSafeNormal() * MaxSpeed;
    }

    FVector NewLocation = GetOwner()->GetActorLocation() + CurrentVelocity * DeltaTimeSeconds;
    GetOwner()->SetActorLocation(NewLocation, true);

    if (bEnableDebugVisuals)
    {
        DrawDebugString(GetWorld(), NewLocation + FVector(0, 0, 450), FString::Printf(TEXT("Speed: %.2f"), Speed), nullptr, FColor::Green, 0.0f);
    }
}