#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "HoloMapTypes.h"
#include "HoloMapPoiComponent.generated.h"

class UStaticMesh;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GALACTICPIRATES_API UHoloMapPoiComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UHoloMapPoiComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Holo Map")
	EHoloMapPoiKind Kind = EHoloMapPoiKind::EnemyShip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Holo Map")
	EHoloMapPrimitive Primitive = EHoloMapPrimitive::Cube;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Holo Map")
	bool bOverridePrimitive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Holo Map")
	FLinearColor MarkerColor = FLinearColor(1.0f, 0.12f, 0.08f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Holo Map")
	bool bOverrideColor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Holo Map")
	FVector MarkerScale = FVector(0.10f, 0.10f, 0.10f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Holo Map")
	TObjectPtr<UStaticMesh> OverrideMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Holo Map")
	bool bVisibleOnMaps = true;

	UFUNCTION(BlueprintPure, Category = "Holo Map")
	FLinearColor GetResolvedColor() const;

	UFUNCTION(BlueprintPure, Category = "Holo Map")
	EHoloMapPrimitive GetResolvedPrimitive() const;

	UFUNCTION(BlueprintPure, Category = "Holo Map")
	UStaticMesh* ResolveMarkerMesh() const;

	UFUNCTION(BlueprintPure, Category = "Holo Map")
	bool IsHostileTo(const AActor* Viewer) const;
};

UStaticMesh* GPLoadHoloPrimitiveMesh(EHoloMapPrimitive Primitive);
