#include "BTTask_Patrol.h"
#include "StarshipAIController.h"
#include "StarshipMovementComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/Engine.h"

UBTTask_Patrol::UBTTask_Patrol()
{
    NodeName = TEXT("Patrol");
}

EBTNodeResult::Type UBTTask_Patrol::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AStarshipAIController* AIController = Cast<AStarshipAIController>(OwnerComp.GetAIOwner());
    if (!AIController)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(14, 5.0f, FColor::Red, TEXT("BTTask_Patrol: AIController Missing - Task Failed"));
        return EBTNodeResult::Failed;
    }

    UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
    APawn* ControlledPawn = AIController->GetPawn();
    if (!ControlledPawn)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(16, 5.0f, FColor::Red, TEXT("BTTask_Patrol: ControlledPawn Missing - Task Failed"));
        return EBTNodeResult::Failed;
    }
    if (!Blackboard)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(15, 5.0f, FColor::Red, TEXT("BTTask_Patrol: Blackboard Missing - Task Failed"));
        return EBTNodeResult::Failed;
    }

    UStarshipMovementComponent* MovementComponent = Cast<UStarshipMovementComponent>(Blackboard->GetValueAsObject(MovementComponentKey.SelectedKeyName));
    if (!MovementComponent)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(16, 5.0f, FColor::Red, TEXT("BTTask_Patrol: MovementComponent Missing - Task Failed"));
        return EBTNodeResult::Failed;
    }

    uint8 PatrolStateValue = Blackboard->GetValueAsEnum(PatrolStateKey.SelectedKeyName);
    const TCHAR* PatrolStateNames[] = { TEXT("Idle"), TEXT("Moving"), TEXT("Searching") };
    FString StateName = PatrolStateValue < 3 ? PatrolStateNames[PatrolStateValue] : TEXT("Unknown");

    switch (PatrolStateValue)
    {
    case 0: // Idle
        if (GEngine) GEngine->AddOnScreenDebugMessage(17, 1.0f, FColor::Yellow, FString::Printf(TEXT("%s: AI in Idle State - Awaiting Next Patrol Point"), *ControlledPawn->GetName()));
        MovementComponent->SetThrottleInput(0.0f);
        MovementComponent->StartAirbrake();
        return EBTNodeResult::Succeeded;

    case 1: // Moving
    {
        FVector TargetLocation = Blackboard->GetValueAsVector(TargetLocationKey.SelectedKeyName);
        FVector CurrentLocation = ControlledPawn->GetActorLocation();
        FVector Direction = (TargetLocation - CurrentLocation).GetSafeNormal();
        float Distance = FVector::Distance(TargetLocation, CurrentLocation);

        if (GEngine) GEngine->AddOnScreenDebugMessage(18, 1.0f, FColor::Green, FString::Printf(TEXT("%s: AI Moving to Patrol Point - Distance: %.2f, Target: %s"), *ControlledPawn->GetName(), Distance, *TargetLocation.ToString()));

        if (Distance < 100.0f)
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(19, 1.0f, FColor::Cyan, FString::Printf(TEXT("%s: AI Reached Patrol Point - Transitioning to Search"), *ControlledPawn->GetName()));
            return EBTNodeResult::Succeeded;
        }

        // Add steering to face TargetLocation
        FQuat TargetOrientation = Direction.ToOrientationQuat();
        FQuat CurrentOrientation = ControlledPawn->GetActorQuat();
        FQuat DeltaRotation = TargetOrientation * CurrentOrientation.Inverse();
        FVector EulerAngles = DeltaRotation.Euler();
        float DeltaTime = GetWorld()->GetDeltaSeconds();
        float PitchInput = FMath::Clamp(EulerAngles.Y / DeltaTime / MovementComponent->GetMaxAngularSpeed(), -1.0f, 1.0f);
        float YawInput = FMath::Clamp(EulerAngles.Z / DeltaTime / MovementComponent->GetMaxAngularSpeed(), -1.0f, 1.0f);
        float RollInput = 0.0f;

        MovementComponent->SetSteeringInput(PitchInput, YawInput, RollInput);
        MovementComponent->SetThrottleInput(1.0f);
        MovementComponent->StopAirbrake();
        return EBTNodeResult::InProgress;
    }

    case 2: // Searching
        if (GEngine) GEngine->AddOnScreenDebugMessage(20, 1.0f, FColor::Orange, FString::Printf(TEXT("%s: AI Searching - Scanning for Threats"), *ControlledPawn->GetName()));
        MovementComponent->SetThrottleInput(0.0f);
        MovementComponent->SetSteeringInput(0.0f, 0.2f, 0.0f);
        return EBTNodeResult::InProgress;

    default:
        if (GEngine) GEngine->AddOnScreenDebugMessage(21, 5.0f, FColor::Red, FString::Printf(TEXT("%s: AI in Invalid Patrol State %d - Task Failed"), *ControlledPawn->GetName(), PatrolStateValue));
        return EBTNodeResult::Failed;
    }
}