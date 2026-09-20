#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "StarshipAIController.generated.h"

UCLASS()
class SPACEBATTLESIMULATOR_API AStarshipAIController : public AAIController
{
    GENERATED_BODY()

public:
    AStarshipAIController();

    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void Tick(float DeltaTimeSeconds) override;

    UFUNCTION()
    void OnPerceptionUpdated(const TArray<AActor*>& DetectedActors);

    void RetryFindMovementComponent();

protected:
    UPROPERTY(EditAnywhere, Category = "AI")
    class UBlackboardData* BlackboardAsset;

    UPROPERTY(EditAnywhere, Category = "AI")
    class UBehaviorTree* BehaviorTreeAsset;

    UPROPERTY(EditAnywhere, Category = "Perception")
    float SightRadius = 50000.0f;

    UPROPERTY(EditAnywhere, Category = "Perception")
    float SightAngle = 90.0f;

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bEnableDebugVisuals = true;

private:
    UPROPERTY()
    class UBlackboardComponent* BlackboardComponent;

    UPROPERTY()
    class UBehaviorTreeComponent* BehaviorTreeComponent;

    UPROPERTY()
    class UStarshipMovementComponent* StarshipMovementComponent;

    UPROPERTY()
    class UAIPerceptionComponent* AIPerceptionComponent;

    void VisualizeSightCone();
};