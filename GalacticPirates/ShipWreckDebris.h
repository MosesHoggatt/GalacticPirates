#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShipWreckDebris.generated.h"

class AWalkableShip;
class UPrimitiveComponent;
class UStaticMesh;
class UMaterialInterface;

UCLASS()
class GALACTICPIRATES_API AShipWreckDebris : public AActor
{
	GENERATED_BODY()

public:
	AShipWreckDebris();

	UFUNCTION(BlueprintCallable, Category = "Ship|Combat")
	void InitializeFromShip(AWalkableShip* Ship);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* DebrisRoot;

private:
	UPROPERTY()
	TArray<TObjectPtr<UPrimitiveComponent>> PhysicsChunks;

	FVector Epicenter = FVector::ZeroVector;

	void ApplyEmissive(UPrimitiveComponent* Mesh, const FLinearColor& Color) const;
	UPrimitiveComponent* SpawnPhysicsCube(const FTransform& Transform, UStaticMesh* CubeMesh, const FLinearColor& Color);

	UFUNCTION()
	void KickFragments();
};
