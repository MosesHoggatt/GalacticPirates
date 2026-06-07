#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "WalkableShip.generated.h"

class UShipMovementComponent;
class UHelmComponent;
class AGalacticPiratesCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShipRotationChanged, const FQuat&, NewRotation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPilotChanged, AGalacticPiratesCharacter*, NewPilot, AGalacticPiratesCharacter*, OldPilot);

UCLASS()
class GALACTICPIRATES_API AWalkableShip : public APawn
{
	GENERATED_BODY()

public:
	AWalkableShip();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* ShipRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* HullMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* InteriorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UShipMovementComponent* ShipMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UHelmComponent* Helm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SpawnPoint;

	UPROPERTY(BlueprintAssignable, Category = "Ship Events")
	FOnShipRotationChanged OnShipRotationChanged;

	UPROPERTY(BlueprintAssignable, Category = "Ship Events")
	FOnPilotChanged OnPilotChanged;

	UFUNCTION(BlueprintCallable, Category = "Ship")
	void RegisterPlayer(AGalacticPiratesCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Ship")
	void UnregisterPlayer(AGalacticPiratesCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Ship")
	bool RequestPilotAssignment(AGalacticPiratesCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Ship")
	void ReleasePilot(AGalacticPiratesCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Ship")
	AGalacticPiratesCharacter* GetCurrentPilot() const { return CurrentPilot; }

	UFUNCTION(BlueprintCallable, Category = "Ship")
	bool IsPilot(AGalacticPiratesCharacter* Character) const { return CurrentPilot == Character; }

	UFUNCTION(BlueprintCallable, Category = "Ship")
	const TArray<AGalacticPiratesCharacter*>& GetPlayersAboard() const { return PlayersAboard; }

	UFUNCTION(BlueprintCallable, Category = "Ship")
	UStaticMeshComponent* GetInteriorMesh() const { return InteriorMesh; }

	UFUNCTION(BlueprintCallable, Category = "Ship")
	FVector GetShipUpVector() const { return GetActorUpVector(); }

	UFUNCTION(BlueprintCallable, Category = "Ship")
	FTransform GetSpawnTransform() const;

	void ApplyPilotInput(AGalacticPiratesCharacter* Pilot, const FVector& ThrustInput, const FVector& RotationInput);
	void HandlePlayerDisconnected(AGalacticPiratesCharacter* Character);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentPilot)
	AGalacticPiratesCharacter* CurrentPilot;

	UPROPERTY(Replicated)
	TArray<AGalacticPiratesCharacter*> PlayersAboard;

	UFUNCTION()
	void OnRep_CurrentPilot(AGalacticPiratesCharacter* OldPilot);

private:
	FQuat LastReplicatedRotation;
	void BroadcastRotationChange();
	void CleanupAllPlayers();
};
