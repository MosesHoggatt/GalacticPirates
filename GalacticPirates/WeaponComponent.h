#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WeaponComponent.generated.h"

UCLASS(Abstract, ClassGroup = (Combat))
class GALACTICPIRATES_API UWeaponComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UWeaponComponent();

	virtual bool CanFireWeapon() const { return true; }
	virtual bool TryFireWeapon(APawn* InstigatorPawn) { return false; }
};
