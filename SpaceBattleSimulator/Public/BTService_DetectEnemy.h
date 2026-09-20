#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_DetectEnemy.generated.h"

UCLASS()
class SPACEBATTLESIMULATOR_API UBTService_DetectEnemy : public UBTService
{
    GENERATED_BODY()

public:
    UBTService_DetectEnemy();

    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector EnemyStarshipKey;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector IsEngagingKey;

    UPROPERTY(EditAnywhere, Category = "Detection")
    float DetectionRange = 5000.0f;
};