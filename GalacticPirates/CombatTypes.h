#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.generated.h"

UENUM(BlueprintType)
enum class EShipArmorClass : uint8
{
	Unarmored UMETA(DisplayName = "Unarmored"),
	Light UMETA(DisplayName = "Light"),
	Armored UMETA(DisplayName = "Armored")
};
