#include "BTTask_EngageEnemy.h"
#include "StarshipAIController.h"
#include "StarshipMovementComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_EngageEnemy::UBTTask_EngageEnemy()
{
    NodeName = TEXT("EngageEnemy");
    bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_EngageEnemy::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    FBTEngageEnemyMemory* TaskMemory = reinterpret_cast<FBTEngageEnemyMemory*>(NodeMemory);
    TaskMemory->IsInitialized = false;
    TaskMemory->LastTargetUpdateTime = -1000.0f;

    AStarshipAIController* AIController = Cast<AStarshipAIController>(OwnerComp.GetAIOwner());
    if (!AIController)
    {
        return EBTNodeResult::Failed;
    }

    UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
    APawn* ControlledPawn = AIController->GetPawn();
    if (!ControlledPawn || !Blackboard)
    {
        return EBTNodeResult::Failed;
    }

    UStarshipMovementComponent* MovementComponent = ControlledPawn->FindComponentByClass<UStarshipMovementComponent>();
    if (!MovementComponent)
    {
        return EBTNodeResult::Failed;
    }

    AActor* EnemyStarship = Cast<AActor>(Blackboard->GetValueAsObject(EnemyStarshipKey.SelectedKeyName));
    if (EnemyStarship)
    {
        TaskMemory->PersistentEnemyStarship = EnemyStarship;
        TaskMemory->LastTargetUpdateTime = GetWorld()->GetTimeSeconds();
    }

    TaskMemory->IsInitialized = true;
    return EBTNodeResult::InProgress;
}

void UBTTask_EngageEnemy::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    FBTEngageEnemyMemory* TaskMemory = reinterpret_cast<FBTEngageEnemyMemory*>(NodeMemory);
    if (!TaskMemory->IsInitialized)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    AStarshipAIController* AIController = Cast<AStarshipAIController>(OwnerComp.GetAIOwner());
    APawn* ControlledPawn = AIController->GetPawn();
    UStarshipMovementComponent* MovementComponent = ControlledPawn->FindComponentByClass<UStarshipMovementComponent>();

    if (!MovementComponent)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    TaskMemory->TickCounter++;
    ExecuteMovementLogic(OwnerComp, NodeMemory, DeltaSeconds);
    FinishLatentTask(OwnerComp, EBTNodeResult::InProgress);
}

void UBTTask_EngageEnemy::ExecuteMovementLogic(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    FBTEngageEnemyMemory* TaskMemory = reinterpret_cast<FBTEngageEnemyMemory*>(NodeMemory);
    AStarshipAIController* AIController = Cast<AStarshipAIController>(OwnerComp.GetAIOwner());
    UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
    APawn* ControlledPawn = AIController->GetPawn();
    UStarshipMovementComponent* MovementComponent = ControlledPawn->FindComponentByClass<UStarshipMovementComponent>();

    TaskMemory->TickCounter++;

    if (!AIController || !ControlledPawn || !Blackboard || !MovementComponent)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    AActor* EnemyStarship = Cast<AActor>(Blackboard->GetValueAsObject(EnemyStarshipKey.SelectedKeyName));
    if (EnemyStarship && EnemyStarship->IsValidLowLevel())
    {
        TaskMemory->PersistentEnemyStarship = EnemyStarship;
        TaskMemory->LastTargetUpdateTime = GetWorld()->GetTimeSeconds();
    }

    EnemyStarship = TaskMemory->PersistentEnemyStarship.IsValid() ? TaskMemory->PersistentEnemyStarship.Get() : nullptr;
    if (!EnemyStarship || !EnemyStarship->IsValidLowLevel())
    {
        if (TaskMemory->LastTargetUpdateTime < 0.0f ||
            (GetWorld()->GetTimeSeconds() - TaskMemory->LastTargetUpdateTime) > TaskMemory->TargetLostTimeout)
        {
            FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
            return;
        }
        MovementComponent->SetThrottleInput(0.0f);
        MovementComponent->SetSteeringInput(0.0f, 0.0f, 0.0f);
        MovementComponent->StopAirbrake();
        TaskMemory->TickCounter++;
        FinishLatentTask(OwnerComp, EBTNodeResult::InProgress);
        return;
    }

    FVector CurrentLocation = ControlledPawn->GetActorLocation();
    FVector EnemyLocation = EnemyStarship->GetActorLocation();
    FVector DirectionToEnemy = (EnemyLocation - CurrentLocation).GetSafeNormal();

    FRotator DesiredRotation = DirectionToEnemy.Rotation();
    FRotator CurrentRotation = ControlledPawn->GetActorRotation();
    FRotator DeltaRotation = (DesiredRotation - CurrentRotation).GetNormalized();

    float PitchInput = FMath::Clamp(DeltaRotation.Pitch / MovementComponent->GetMaxAngularSpeed(), -1.0f, 1.0f);
    float YawInput = FMath::Clamp(DeltaRotation.Yaw / MovementComponent->GetMaxAngularSpeed(), -1.0f, 1.0f);
    float RollInput = 0.0f;
    float TargetThrottle = 1.0f;

    MovementComponent->SetSteeringInput(PitchInput, YawInput, RollInput);
    MovementComponent->SetThrottleInput(TargetThrottle);
    MovementComponent->StopAirbrake();

    TaskMemory->TickCounter++;
    FinishLatentTask(OwnerComp, EBTNodeResult::InProgress);
}