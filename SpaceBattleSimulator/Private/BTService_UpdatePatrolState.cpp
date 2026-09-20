#include "BTService_UpdatePatrolState.h"
#include "StarshipAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/Engine.h"

UBTService_UpdatePatrolState::UBTService_UpdatePatrolState()
{
    NodeName = TEXT("UpdatePatrolState");
    Interval = 1.0f;
}

void UBTService_UpdatePatrolState::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    AStarshipAIController* AIController = Cast<AStarshipAIController>(OwnerComp.GetAIOwner());
    UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
    if (!AIController || !Blackboard)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, TEXT("BTService_UpdatePatrolState: AIController or Blackboard Missing"));
        return;
    }

    APawn* ControlledPawn = AIController->GetPawn();
    if (!ControlledPawn)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, TEXT("BTService_UpdatePatrolState: ControlledPawn Missing"));
        return;
    }

    TimeInCurrentState += DeltaSeconds;
    uint8 PatrolStateValue = Blackboard->GetValueAsEnum(PatrolStateKey.SelectedKeyName);

    const TCHAR* PatrolStateNames[] = { TEXT("Idle"), TEXT("Moving"), TEXT("Searching") };
    FString StateName = PatrolStateValue < 3 ? PatrolStateNames[PatrolStateValue] : TEXT("Unknown");

    float Distance = 0.0f;
    switch (PatrolStateValue)
    {
    case 0: // Idle
        if (TimeInCurrentState >= IdleDuration)
        {
            FVector CurrentLocation = ControlledPawn->GetActorLocation();
            FVector NewTarget = CurrentLocation + UKismetMathLibrary::RandomUnitVectorInConeInDegrees(FVector::ForwardVector, 45.0f) * PatrolRadius;
            Blackboard->SetValueAsVector(TargetLocationKey.SelectedKeyName, NewTarget);
            Blackboard->SetValueAsEnum(PatrolStateKey.SelectedKeyName, 1);
            TimeInCurrentState = 0.0f;
            if (GEngine) GEngine->AddOnScreenDebugMessage(13, 5.0f, FColor::Cyan, FString::Printf(TEXT("%s: AI Transitioned to Moving - New Patrol Target: %s"), *ControlledPawn->GetName(), *NewTarget.ToString()));
        }
        else
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(12, 1.0f, FColor::Yellow, FString::Printf(TEXT("%s: AI in Idle State - Time Remaining: %.2f"), *ControlledPawn->GetName(), IdleDuration - TimeInCurrentState));
        }
        break;

    case 1: // Moving
        FVector TargetLocation = Blackboard->GetValueAsVector(TargetLocationKey.SelectedKeyName);
        Distance = FVector::Distance(ControlledPawn->GetActorLocation(), TargetLocation);
        if (Distance < 100.0f)
        {
            Blackboard->SetValueAsEnum(PatrolStateKey.SelectedKeyName, 2);
            TimeInCurrentState = 0.0f;
            if (GEngine) GEngine->AddOnScreenDebugMessage(11, 5.0f, FColor::Cyan, FString::Printf(TEXT("%s: AI Reached Patrol Point - Transitioned to Searching"), *ControlledPawn->GetName()));
        }
        else
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(10, 1.0f, FColor::Green, FString::Printf(TEXT("%s: AI Moving to Patrol Point - Distance Remaining: %.2f"), *ControlledPawn->GetName(), Distance));
        }
        break;

    case 2: // Searching
        if (TimeInCurrentState >= 5.0f)
        {
            Blackboard->SetValueAsEnum(PatrolStateKey.SelectedKeyName, 0);
            TimeInCurrentState = 0.0f;
            if (GEngine) GEngine->AddOnScreenDebugMessage(9, 5.0f, FColor::Cyan, FString::Printf(TEXT("%s: AI Completed Search - Transitioned to Idle"), *ControlledPawn->GetName()));
        }
        else
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(8, 1.0f, FColor::Orange, FString::Printf(TEXT("%s: AI Searching - Time Remaining: %.2f"), *ControlledPawn->GetName(), 5.0f - TimeInCurrentState));
        }
        break;
    }
}