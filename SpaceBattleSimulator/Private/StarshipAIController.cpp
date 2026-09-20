#include "StarshipAIController.h"
#include "StarshipMovementComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

AStarshipAIController::AStarshipAIController()
{
    BlackboardComponent = CreateDefaultSubobject<UBlackboardComponent>("BlackboardComponent");
    BehaviorTreeComponent = CreateDefaultSubobject<UBehaviorTreeComponent>("BehaviorTreeComponent");

    AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>("AIPerceptionComponent");
    if (AIPerceptionComponent)
    {
        UAISenseConfig_Sight* SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>("SightConfig");
        SightConfig->SightRadius = SightRadius;
        SightConfig->LoseSightRadius = SightRadius + 500.0f;
        SightConfig->PeripheralVisionAngleDegrees = SightAngle;
        SightConfig->DetectionByAffiliation.bDetectEnemies = true;
        SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
        SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
        SightConfig->SetMaxAge(0.1f);
        AIPerceptionComponent->ConfigureSense(*SightConfig);
        AIPerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());
    }
}

void AStarshipAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (!InPawn)
    {
        FString ControllerType = IsA<AStarshipAIController>() ? TEXT("AI") : TEXT("Player");
        UE_LOG(LogTemp, Error, TEXT("%s StarshipAIController: No pawn provided"), *ControllerType);
        if (bEnableDebugVisuals && GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("StarshipAIController: No pawn provided"));
        }
        return;
    }

    FString ControllerType = IsA<AStarshipAIController>() ? TEXT("AI") : TEXT("Player");
    UE_LOG(LogTemp, Log, TEXT("%s StarshipAIController: Possessing pawn %s"), *ControllerType, *InPawn->GetName());
    if (bEnableDebugVisuals)
    {
        DrawDebugString(GetWorld(), InPawn->GetActorLocation() + FVector(0, 0, 100), FString::Printf(TEXT("Possessing %s"), *InPawn->GetName()), nullptr, FColor::Green, 5.0f);
    }

    StarshipMovementComponent = InPawn->FindComponentByClass<UStarshipMovementComponent>();
    if (!StarshipMovementComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s StarshipAIController: No StarshipMovementComponent on %s, retrying next tick"), *ControllerType, *InPawn->GetName());
        if (bEnableDebugVisuals && GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, FString::Printf(TEXT("No StarshipMovementComponent on %s"), *InPawn->GetName()));
        }
        GetWorld()->GetTimerManager().SetTimerForNextTick(this, &AStarshipAIController::RetryFindMovementComponent);
    }

    UE_LOG(LogTemp, Log, TEXT("%s MovementComponent: %s %s"), *ControllerType, *FString(StarshipMovementComponent->GetFullName()),
        StarshipMovementComponent ? TEXT("Found") : TEXT("Not Found"));

    if (AIPerceptionComponent)
    {
        AIPerceptionComponent->SetActive(true);
        AIPerceptionComponent->OnPerceptionUpdated.AddDynamic(this, &AStarshipAIController::OnPerceptionUpdated);
        UE_LOG(LogTemp, Log, TEXT("%s StarshipAIController: Configured AIPerceptionComponent with SightRadius: %.2f, SightAngle: %.2f"), *ControllerType, SightRadius, SightAngle);
        if (bEnableDebugVisuals)
        {
            DrawDebugString(GetWorld(), InPawn->GetActorLocation() + FVector(0, 0, 150), FString::Printf(TEXT("Perception: Radius %.2f, Angle %.2f"), SightRadius, SightAngle), nullptr, FColor::Cyan, 5.0f);
        }
    }

    if (BlackboardAsset && BlackboardComponent)
    {
        BlackboardComponent->InitializeBlackboard(*BlackboardAsset);
        UE_LOG(LogTemp, Log, TEXT("%s StarshipAIController: Blackboard initialized for %s"), *ControllerType, *InPawn->GetName());
        if (bEnableDebugVisuals)
        {
            DrawDebugString(GetWorld(), InPawn->GetActorLocation() + FVector(0, 0, 200), TEXT("Blackboard Initialized"), nullptr, FColor::White, 5.0f);
        }
    }

    if (BehaviorTreeAsset && BehaviorTreeComponent)
    {
        BehaviorTreeComponent->StartTree(*BehaviorTreeAsset);
        UE_LOG(LogTemp, Log, TEXT("%s StarshipAIController: Behavior Tree started for %s"), *ControllerType, *InPawn->GetName());
        if (bEnableDebugVisuals)
        {
            DrawDebugString(GetWorld(), InPawn->GetActorLocation() + FVector(0, 0, 250), TEXT("Behavior Tree Started"), nullptr, FColor::Green, 5.0f);
        }
    }
}

void AStarshipAIController::OnUnPossess()
{
    Super::OnUnPossess();

    if (AIPerceptionComponent)
    {
        AIPerceptionComponent->OnPerceptionUpdated.RemoveDynamic(this, &AStarshipAIController::OnPerceptionUpdated);
        AIPerceptionComponent->SetActive(false);
        FString PawnName = GetPawn() ? GetPawn()->GetName() : TEXT("UnknownPawn");
        FString ControllerType = IsA<AStarshipAIController>() ? TEXT("AI") : TEXT("Player");
        UE_LOG(LogTemp, Log, TEXT("%s StarshipAIController: Disabled AIPerceptionComponent for %s"), *ControllerType, *PawnName);
        if (bEnableDebugVisuals && GetPawn() && GetWorld())
        {
            DrawDebugString(GetWorld(), GetPawn()->GetActorLocation() + FVector(0, 0, 100), FString::Printf(TEXT("Perception Disabled for %s"), *PawnName), nullptr, FColor::Red, 5.0f);
        }
    }

    AIPerceptionComponent = nullptr;
}

void AStarshipAIController::RetryFindMovementComponent()
{
    APawn* PossessedPawn = GetPawn();
    FString ControllerType = IsA<AStarshipAIController>() ? TEXT("AI") : TEXT("Player");
    if (!PossessedPawn)
    {
        UE_LOG(LogTemp, Error, TEXT("%s StarshipAIController: No pawn available for retry"), *ControllerType);
        if (bEnableDebugVisuals && GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("StarshipAIController: No pawn for retry"));
        }
        return;
    }

    StarshipMovementComponent = PossessedPawn->FindComponentByClass<UStarshipMovementComponent>();
    if (!StarshipMovementComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("%s StarshipAIController: Still no StarshipMovementComponent on %s"), *ControllerType, *PossessedPawn->GetName());
        if (bEnableDebugVisuals)
        {
            DrawDebugString(GetWorld(), PossessedPawn->GetActorLocation() + FVector(0, 0, 100), FString::Printf(TEXT("No MovementComponent on %s"), *PossessedPawn->GetName()), nullptr, FColor::Red, 5.0f);
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("%s StarshipAIController: Found StarshipMovementComponent on %s after retry"), *ControllerType, *PossessedPawn->GetName());
        if (bEnableDebugVisuals)
        {
            DrawDebugString(GetWorld(), PossessedPawn->GetActorLocation() + FVector(0, 0, 100), FString::Printf(TEXT("Found MovementComponent on %s"), *PossessedPawn->GetName()), nullptr, FColor::Green, 5.0f);
        }
    }
}

void AStarshipAIController::OnPerceptionUpdated(const TArray<AActor*>& DetectedActors)
{
    if (!BlackboardComponent) return;

    APawn* ControlledPawn = GetPawn();
    FString PawnName = ControlledPawn ? ControlledPawn->GetName() : TEXT("UnknownPawn");
    FString ControllerType = IsA<AStarshipAIController>() ? TEXT("AI") : TEXT("Player");

    AActor* ClosestEnemy = nullptr;
    float MinDistance = MAX_flt;

    for (AActor* Actor : DetectedActors)
    {
        if (!Actor || Actor == ControlledPawn) continue;

        if (Actor->IsA(APawn::StaticClass()))
        {
            float Distance = FVector::Distance(ControlledPawn->GetActorLocation(), Actor->GetActorLocation());
            UE_LOG(LogTemp, Log, TEXT("%s Detected pawn: %s, Distance: %.2f"), *ControllerType, *Actor->GetName(), Distance);
            if (bEnableDebugVisuals)
            {
                DrawDebugSphere(GetWorld(), Actor->GetActorLocation(), 50.0f, 12, FColor::Yellow, false, 0.0f);
                DrawDebugString(GetWorld(), Actor->GetActorLocation() + FVector(0, 0, 50), FString::Printf(TEXT("%s: %.2f"), *Actor->GetName(), Distance), nullptr, FColor::Yellow, 0.0f);
            }
            if (Distance < MinDistance)
            {
                MinDistance = Distance;
                ClosestEnemy = Actor;
            }
        }
    }

    bool IsEngaging = ClosestEnemy != nullptr;
    BlackboardComponent->SetValueAsObject(FName("EnemyStarship"), ClosestEnemy);
    BlackboardComponent->SetValueAsBool(FName("IsEngaging"), IsEngaging);

    if (bEnableDebugVisuals)
    {
        if (IsEngaging)
        {
            FVector AIPosition = ControlledPawn->GetActorLocation();
            FVector TargetPosition = ClosestEnemy->GetActorLocation();
            DrawDebugLine(GetWorld(), AIPosition, TargetPosition, FColor::Green, false, 0.0f, 0, 2.0f); // Thicker line
            DrawDebugSphere(GetWorld(), TargetPosition, 60.0f, 12, FColor::Red, false, 0.0f); // Target marker
            DrawDebugBox(GetWorld(), TargetPosition, FVector(100.0f), FColor::Red, false, 0.0f); // Bounding box
            DrawDebugString(GetWorld(), TargetPosition + FVector(0, 0, 100), FString::Printf(TEXT("Target: %s (%.2f)"), *ClosestEnemy->GetName(), MinDistance), nullptr, FColor::Red, 0.0f);
            DrawDebugString(GetWorld(), AIPosition + FVector(0, 0, 200), FString::Printf(TEXT("Engaging %s"), *ClosestEnemy->GetName()), nullptr, FColor::Magenta, 0.0f);
        }
        else
        {
            DrawDebugString(GetWorld(), ControlledPawn->GetActorLocation() + FVector(0, 0, 200), TEXT("No Enemies"), nullptr, FColor::White, 0.0f);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("%s OnPerceptionUpdated: EnemyStarship set to %s, IsEngaging: %d"),
        *ControllerType, ClosestEnemy ? *ClosestEnemy->GetName() : TEXT("None"), IsEngaging);
}

void AStarshipAIController::Tick(float DeltaTimeSeconds)
{
    Super::Tick(DeltaTimeSeconds);

    if (bEnableDebugVisuals)
    {
        VisualizeSightCone();
    }

    if (bEnableDebugVisuals)
    {
        TArray<AActor*> DetectedPawns;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), APawn::StaticClass(), DetectedPawns);
        FString ControllerType = IsA<AStarshipAIController>() ? TEXT("AI") : TEXT("Player");
        for (AActor* DetectedPawn : DetectedPawns)
        {
            if (DetectedPawn != GetPawn())
            {
                UE_LOG(LogTemp, Log, TEXT("%s Found pawn in world: %s"), *ControllerType, *DetectedPawn->GetName());
                DrawDebugSphere(GetWorld(), DetectedPawn->GetActorLocation(), 50.0f, 12, FColor::Blue, false, 0.0f);
                DrawDebugString(GetWorld(), DetectedPawn->GetActorLocation() + FVector(0, 0, 50), *DetectedPawn->GetName(), nullptr, FColor::Blue, 0.0f);
                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(11, 0.0f, FColor::Blue, FString::Printf(TEXT("%s: Pawn in world %s"), *GetPawn()->GetName(), *DetectedPawn->GetName()));
                }
            }
        }
    }
}

void AStarshipAIController::VisualizeSightCone()
{
    if (!GetPawn() || !AIPerceptionComponent) return;

    FVector StartLocation = GetPawn()->GetActorLocation();
    FVector ForwardVector = GetPawn()->GetActorForwardVector();

    DrawDebugLine(GetWorld(), StartLocation, StartLocation + ForwardVector.RotateAngleAxis(-SightAngle / 2.0f, FVector(0, 0, 1)) * SightRadius, FColor::Cyan, false, 0.0f);
    DrawDebugLine(GetWorld(), StartLocation, StartLocation + ForwardVector.RotateAngleAxis(SightAngle / 2.0f, FVector(0, 0, 1)) * SightRadius, FColor::Cyan, false, 0.0f);
    DrawDebugSphere(GetWorld(), StartLocation, SightRadius, 32, FColor::Cyan, false, 0.0f, 0, 1.0f);
    if (bEnableDebugVisuals)
    {
        DrawDebugString(GetWorld(), StartLocation + FVector(0, 0, 300), FString::Printf(TEXT("Sight: %.2f, %.2f"), SightRadius, SightAngle), nullptr, FColor::Cyan, 0.0f);
    }
}