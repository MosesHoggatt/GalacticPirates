#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "OccupancyComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOccupancyChanged, APawn*, NewOccupant, APawn*, OldOccupant);

UCLASS(ClassGroup = (Craft), meta = (BlueprintSpawnableComponent))
class GALACTICPIRATES_API UOccupancyComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UOccupancyComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occupancy", meta = (ClampMin = "50.0"))
	float InteractRange = 350.0f;

	UPROPERTY(BlueprintAssignable, Category = "Occupancy")
	FOnOccupancyChanged OnOccupancyChanged;

	UFUNCTION(BlueprintPure, Category = "Occupancy")
	bool IsOccupied() const { return Occupant != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Occupancy")
	APawn* GetOccupant() const { return Occupant; }

	UFUNCTION(BlueprintPure, Category = "Occupancy")
	bool IsOccupant(const APawn* Pawn) const { return Occupant == Pawn; }

	UFUNCTION(BlueprintPure, Category = "Occupancy")
	bool IsInRange(const APawn* Pawn) const;

	/** Seat or vacate. Never gated by affiliation. */
	UFUNCTION(BlueprintCallable, Category = "Occupancy")
	bool TryOccupy(APawn* Pawn);

	UFUNCTION(BlueprintCallable, Category = "Occupancy")
	void ForceRelease();

	/** Authority-only assign used when helm/pilot state is the source of truth. */
	void SetOccupant(APawn* NewOccupant);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Occupant)
	TObjectPtr<APawn> Occupant;

	UFUNCTION()
	void OnRep_Occupant(APawn* OldOccupant);
};
