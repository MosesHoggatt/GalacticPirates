#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_UpdatePatrolState.generated.h"

UCLASS()
class SPACEBATTLESIMULATOR_API UBTService_UpdatePatrolState : public UBTService
{
    GENERATED_BODY()

public:
    UBTService_UpdatePatrolState();

    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector PatrolStateKey;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector TargetLocationKey;

    UPROPERTY(EditAnywhere, Category = "Patrol")
    float PatrolRadius = 5000.0f;

    UPROPERTY(EditAnywhere, Category = "Patrol")
    float IdleDuration = 3.0f;

private:
    float TimeInCurrentState = 0.0f;
};