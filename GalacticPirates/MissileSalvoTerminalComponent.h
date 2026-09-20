#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "MissileSalvoTerminalComponent.generated.h"

class AWalkableShip;
class AGalacticPiratesCharacter;
class UStaticMeshComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GALACTICPIRATES_API UMissileSalvoTerminalComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UMissileSalvoTerminalComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile Terminal", meta = (ClampMin = "50.0"))
	float InteractRange = 280.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TerminalMesh;

	UFUNCTION(BlueprintCallable, Category = "Missile Terminal")
	bool TryInteract(AGalacticPiratesCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Missile Terminal")
	bool IsCharacterInRange(const AGalacticPiratesCharacter* Character) const;

	UFUNCTION(BlueprintPure, Category = "Missile Terminal")
	AWalkableShip* GetOwningShip() const { return OwningShip; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<AWalkableShip> OwningShip;
};
