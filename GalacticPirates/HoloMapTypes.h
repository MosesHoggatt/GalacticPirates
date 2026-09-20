#pragma once

#include "CoreMinimal.h"
#include "HoloMapTypes.generated.h"

UENUM(BlueprintType)
enum class EHoloMapPoiKind : uint8
{
	Unknown UMETA(DisplayName = "Unknown"),
	OwnShip UMETA(DisplayName = "Own Ship"),
	EnemyShip UMETA(DisplayName = "Enemy Ship"),
	FriendlyShip UMETA(DisplayName = "Friendly Ship"),
	NeutralShip UMETA(DisplayName = "Neutral Ship"),
	Station UMETA(DisplayName = "Station"),
	Asteroid UMETA(DisplayName = "Asteroid"),
	Missile UMETA(DisplayName = "Missile")
};

UENUM(BlueprintType)
enum class EHoloMapPrimitive : uint8
{
	Cone UMETA(DisplayName = "Cone"),
	Sphere UMETA(DisplayName = "Sphere"),
	Cube UMETA(DisplayName = "Cube"),
	Cylinder UMETA(DisplayName = "Cylinder"),
	Plane UMETA(DisplayName = "Plane")
};

USTRUCT(BlueprintType)
struct GALACTICPIRATES_API FHoloMapTrackedPoi
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Holo Map")
	TWeakObjectPtr<AActor> Actor;

	UPROPERTY(BlueprintReadOnly, Category = "Holo Map")
	EHoloMapPoiKind Kind = EHoloMapPoiKind::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Holo Map")
	EHoloMapPrimitive Primitive = EHoloMapPrimitive::Cone;

	UPROPERTY(BlueprintReadOnly, Category = "Holo Map")
	FLinearColor Color = FLinearColor::Red;

	UPROPERTY(BlueprintReadOnly, Category = "Holo Map")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Holo Map")
	FVector TableRelative = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Holo Map")
	float DistanceCm = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Holo Map")
	FRotator RelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Holo Map")
	FVector MarkerScale = FVector::OneVector;
};
