#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "HelmComponent.generated.h"

class AWalkableShip;
class AGalacticPiratesCharacter;
class UInputMappingContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHelmOccupancyChanged, bool, bOccupied);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GALACTICPIRATES_API UHelmComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UHelmComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helm", meta = (ClampMin = "50.0"))
	float InteractRange = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helm")
	FVector PilotRelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helm")
	FRotator PilotRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helm")
	bool bLockPilotPosition = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helm|Input")
	UInputMappingContext* ShipControlIMC = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helm|Input")
	int32 IMCPriority = 1;

	UPROPERTY(BlueprintAssignable, Category = "Helm Events")
	FOnHelmOccupancyChanged OnHelmOccupancyChanged;

	UFUNCTION(BlueprintCallable, Category = "Helm")
	bool IsOccupied() const;

	UFUNCTION(BlueprintCallable, Category = "Helm")
	bool TryInteract(AGalacticPiratesCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Helm")
	void ForceRelease();

	UFUNCTION(BlueprintCallable, Category = "Helm")
	AGalacticPiratesCharacter* GetCurrentPilot() const;

	void OnPilotChanged(AGalacticPiratesCharacter* NewPilot, AGalacticPiratesCharacter* OldPilot);
	void HandlePilotDisconnected(AGalacticPiratesCharacter* DisconnectedPilot);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY()
	bool bIsOccupied = false;

	UPROPERTY()
	AWalkableShip* OwningShip = nullptr;

	UPROPERTY()
	AGalacticPiratesCharacter* CurrentPilotRef = nullptr;

	UPROPERTY(Transient)
	UInputMappingContext* RuntimeShipControlIMC = nullptr;

	UInputMappingContext* GetOrCreateShipControlContext(AGalacticPiratesCharacter* Pilot);
	AWalkableShip* ResolveOwningShip();
	void AddInputContextToPilot(AGalacticPiratesCharacter* Pilot);
	void RemoveInputContextFromPilot(AGalacticPiratesCharacter* Pilot);
	void LockPilotToHelm(AGalacticPiratesCharacter* Pilot);
	void UnlockPilotFromHelm(AGalacticPiratesCharacter* Pilot);
};
