#include "BTService_DetectEnemy.h"
#include "StarshipAIController.h"
#include "StarshipMovementComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

UBTService_DetectEnemy::UBTService_DetectEnemy()
{
    NodeName = TEXT("DetectEnemy");
    Interval = 0.5f;
}

void UBTService_DetectEnemy::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    AStarshipAIController* AIController = Cast<AStarshipAIController>(OwnerComp.GetAIOwner());
    UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
    if (!AIController || !Blackboard)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, TEXT("BTService_DetectEnemy: AIController or Blackboard Missing"));
        return;
    }

    APawn* ControlledPawn = AIController->GetPawn();
    if (!ControlledPawn)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, TEXT("BTService_DetectEnemy: ControlledPawn Missing"));
        return;
    }

    TArray<AActor*> NearbyActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), NearbyActors);

    AActor* ClosestEnemy = nullptr;
    float MinDistance = DetectionRange;

    for (AActor* Actor : NearbyActors)
    {
        if (Actor == ControlledPawn || !Actor->FindComponentByClass<UStarshipMovementComponent>()) continue;

        float Distance = FVector::Distance(ControlledPawn->GetActorLocation(), Actor->GetActorLocation());
        if (Distance < MinDistance)
        {
            MinDistance = Distance;
            ClosestEnemy = Actor;
        }
    }

    bool IsEngaging = ClosestEnemy != nullptr;
    Blackboard->SetValueAsObject(EnemyStarshipKey.SelectedKeyName, ClosestEnemy);
    Blackboard->SetValueAsBool(IsEngagingKey.SelectedKeyName, IsEngaging);

    if (GEngine)
    {
        if (IsEngaging)
        {
            GEngine->AddOnScreenDebugMessage(23, 1.0f, FColor::Magenta, FString::Printf(TEXT("%s: AI Detects Enemy - %s (Distance: %.2f, Threat Level: High)"), *ControlledPawn->GetName(), *ClosestEnemy->GetName(), MinDistance));
        }
        else
        {
            GEngine->AddOnScreenDebugMessage(22, 1.0f, FColor::White, FString::Printf(TEXT("%s: AI Detects No Enemies - Clear"), *ControlledPawn->GetName()));
        }
    }
}