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

UENUM(BlueprintType)
enum class ESpaceDamageKind : uint8
{
	Generic UMETA(DisplayName = "Generic"),
	Ballistic UMETA(DisplayName = "Ballistic"),
	Energy UMETA(DisplayName = "Energy"),
	Missile UMETA(DisplayName = "Missile")
};

USTRUCT(BlueprintType)
struct GALACTICPIRATES_API FSpaceDamageEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float Amount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	ESpaceDamageKind Kind = ESpaceDamageKind::Generic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	TWeakObjectPtr<APawn> InstigatorPawn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	TWeakObjectPtr<AActor> Causer;
};

/** Minigun armored vs light: 6 hull from a 40-damage ballistic round. */
inline float GPArmorDamageScale(EShipArmorClass Armor, ESpaceDamageKind Kind)
{
	if (Kind == ESpaceDamageKind::Ballistic && Armor == EShipArmorClass::Armored)
	{
		return 6.0f / 40.0f;
	}
	return 1.0f;
}
