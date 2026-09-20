#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_EngageEnemy.generated.h"

USTRUCT()
struct FBTEngageEnemyMemory
{
    GENERATED_BODY()

    bool IsInitialized = false;
    int32 TickCounter = 0;
    TWeakObjectPtr<AActor> PersistentEnemyStarship = nullptr;
    float LastTargetUpdateTime = 0.0f;
    float TargetLostTimeout = 10.0f; // Seconds before failing task if no new target
};

UCLASS()
class SPACEBATTLESIMULATOR_API UBTTask_EngageEnemy : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_EngageEnemy();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTEngageEnemyMemory); }

protected:
    virtual void ExecuteMovementLogic(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds);

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector EnemyStarshipKey;

    UPROPERTY(EditAnywhere, Category = "Engagement")
    float OptimalEngagementDistance = 1000.0f;

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bEnableDebugVisuals = true;
};