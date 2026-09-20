#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "HoloMapTypes.h"
#include "HolographicMapTableComponent.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UHoloMapPoiComponent;
class AWalkableShip;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GALACTICPIRATES_API UHolographicMapTableComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UHolographicMapTableComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Holo Map", meta = (ClampMin = "500.0"))
	float ScanRangeCm = 12500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Holo Map", meta = (ClampMin = "20.0"))
	float VolumeRadiusCm = 96.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Holo Map")
	float VolumeCenterZ = 210.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Holo Map", meta = (ClampMin = "0.02"))
	float RefreshInterval = 0.05f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TableMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HoloVolumeMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> OwnShipMarker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> HoloLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> EquatorRing;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeridianRing;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TransverseRing;

	UFUNCTION(BlueprintPure, Category = "Holo Map")
	const TArray<FHoloMapTrackedPoi>& GetTrackedPois() const { return TrackedPois; }

	UFUNCTION(BlueprintPure, Category = "Holo Map")
	AActor* GetBestAutoTarget() const;

	UFUNCTION(BlueprintPure, Category = "Holo Map")
	AWalkableShip* GetOwningShip() const { return OwningShip; }

	void RebuildTrackedPois();
	UStaticMeshComponent* FindDisplayedMarker(const AActor* Actor) const;

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY()
	TObjectPtr<AWalkableShip> OwningShip;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> MarkerPool;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> AltitudeLinePool;

	UPROPERTY()
	TArray<FHoloMapTrackedPoi> TrackedPois;

	struct FHoloMarkerInterp
	{
		TWeakObjectPtr<AActor> Actor;
		int32 PoolIndex = INDEX_NONE;
		FVector FromLoc = FVector::ZeroVector;
		FVector ToLoc = FVector::ZeroVector;
		FVector DisplayedLoc = FVector::ZeroVector;
		FQuat FromRot = FQuat::Identity;
		FQuat ToRot = FQuat::Identity;
		FQuat DisplayedRot = FQuat::Identity;
	};

	TArray<FHoloMarkerInterp> MarkerInterps;

	float RefreshTimer = 0.0f;

	UStaticMesh* LoadPrimitiveMesh(EHoloMapPrimitive Primitive) const;
	UStaticMeshComponent* GetOrCreatePooledMesh(TArray<TObjectPtr<UStaticMeshComponent>>& Pool, int32 Index);
	void BuildRig();
	void HideStaleVolumeCones();
	FVector IconScaleForPrimitive(EHoloMapPrimitive Primitive) const;
	FRotator MarkerHeading(EHoloMapPrimitive Primitive, const FVector& RelForward) const;

	bool bRigBuilt = false;
	void TintMarker(UStaticMeshComponent* Mesh, const FLinearColor& Color) const;
	FVector WorldOffsetToVolume(const FVector& ShipLocalOffset) const;
	FVector GetActorSizeCm(const AActor* Actor) const;
	FVector WorldSizeToMarkerScale(const FVector& WorldSizeCm) const;
	void HideCylinderMesh(UStaticMeshComponent* Mesh);
	void ResolveOwningShip();
	void PlaceInCabin();
	void BindVisualToTable(USceneComponent* Child, const FVector& RelLoc, const FRotator& RelRot, const FVector& RelScale);
	void CaptureMarkerInterpTargets();
	void InterpolateMarkers();
	void RefreshMarkers();
	void SetVisualsVisible(bool bShowVisuals);
	bool ShouldDrawVisuals() const;
	void BindPooledMesh(UStaticMeshComponent* Mesh);
};
