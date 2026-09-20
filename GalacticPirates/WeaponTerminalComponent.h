#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WeaponTerminalComponent.generated.h"

class AWalkableShip;
class AGalacticPiratesCharacter;
class UStaticMeshComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GALACTICPIRATES_API UWeaponTerminalComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UWeaponTerminalComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Terminal", meta = (ClampMin = "50.0"))
	float InteractRange = 280.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* TerminalMesh;

	UFUNCTION(BlueprintCallable, Category = "Weapon Terminal")
	bool TryInteract(AGalacticPiratesCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Weapon Terminal")
	bool IsCharacterInRange(const AGalacticPiratesCharacter* Character) const;

	UFUNCTION(BlueprintPure, Category = "Weapon Terminal")
	AWalkableShip* GetOwningShip() const { return OwningShip; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	AWalkableShip* OwningShip = nullptr;
};
