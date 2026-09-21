#pragma once

#include "CoreMinimal.h"
#include "WeaponComponent.h"
#include "MinigunPodComponent.generated.h"

class AWalkableShip;
class AGalacticPiratesCharacter;
class AHeatseekingMissile;
class UStaticMeshComponent;
class UPointLightComponent;
class UOccupancyComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GALACTICPIRATES_API UMinigunPodComponent : public UWeaponComponent
{
	GENERATED_BODY()

public:
	UMinigunPodComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun", meta = (ClampMin = "50.0"))
	float InteractRange = 240.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun", meta = (ClampMin = "1.0"))
	float FireInterval = 0.07f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun", meta = (ClampMin = "100.0"))
	float TraceRange = 18000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun", meta = (ClampMin = "1.0"))
	float TraceRadius = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun", meta = (ClampMin = "0.0"))
	float ArmoredShipDamage = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun", meta = (ClampMin = "0.0"))
	float LightShipDamage = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun", meta = (ClampMin = "0.0"))
	float MissileDamage = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun")
	float YawMin = -110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun")
	float YawMax = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun")
	float PitchMin = -70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun")
	float PitchMax = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun")
	float AimSensitivity = 1.35f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> YawMount;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> PitchMount;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PodMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BarrelMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MuzzleFlashMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TracerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TracerRibbon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ImpactFlashMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> MuzzleLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> ImpactLight;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SparkMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun|Fx", meta = (ClampMin = "0.005"))
	float MuzzleFlashSeconds = 0.018f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun|Fx", meta = (ClampMin = "0.01"))
	float TracerSeconds = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun|Fx", meta = (ClampMin = "100.0"))
	float TracerVisibleLength = 2400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun|Fx")
	FVector GunSightOffset = FVector(-90.0f, 0.0f, 42.0f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UOccupancyComponent> Occupancy;

	UFUNCTION(BlueprintCallable, Category = "Minigun")
	bool TryInteract(AGalacticPiratesCharacter* Character);

	UFUNCTION(BlueprintPure, Category = "Minigun")
	bool IsFiring() const { return bFiring; }

	UFUNCTION(BlueprintCallable, Category = "Minigun")
	bool IsCharacterInRange(const AGalacticPiratesCharacter* Character) const;

	UFUNCTION(BlueprintPure, Category = "Minigun")
	bool IsOccupied() const;

	UFUNCTION(BlueprintPure, Category = "Minigun")
	AGalacticPiratesCharacter* GetGunner() const;

	UOccupancyComponent* GetOccupancy() const { return Occupancy; }

	/** Unmanned AI mounts (fighter nose gun) fire through the existing SetFiring/tick path. */
	bool bAllowFireWithoutGunner = false;

	void ForceRelease();
	/** Muzzle flash, tracer and report for one round; also the body of the fire multicast. */
	void PlayShotFx(const FVector& Start, const FVector& End, bool bHit);
	void UpdateShotFx(float DeltaTime);
	void AddAimInput(float YawDelta, float PitchDelta);
	float GetAimYaw() const { return AimYaw; }
	float GetAimPitch() const { return AimPitch; }
	void SetFiring(bool bNewFiring);
	virtual bool CanFireWeapon() const override;
	virtual bool TryFireWeapon(APawn* InstigatorPawn) override;
	void ApplyAim(float NewYaw, float NewPitch);
	void AimAtWorldLocation(const FVector& WorldLocation);
	void RestAim();
	void ApplyGunnerCamera();
	FVector GetMuzzleLocation() const;
	FVector GetMuzzleForward() const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_Gunner)
	TObjectPtr<AGalacticPiratesCharacter> Gunner;

	UPROPERTY(ReplicatedUsing = OnRep_Aim)
	float AimYaw = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Aim)
	float AimPitch = 0.0f;

	UFUNCTION()
	void OnRep_Gunner();

	UFUNCTION()
	void OnRep_Aim();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Tracer(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit);

private:
	UPROPERTY()
	TObjectPtr<AWalkableShip> OwningShip;

	bool bRigBuilt = false;
	bool bFiring = false;
	float FireTimer = 0.0f;
	float FlashTimer = 0.0f;
	float TracerTimer = 0.0f;
	int32 ShotsPlayed = 0;
	TArray<FVector> SparkVelocity;
	TArray<float> SparkLife;

	UFUNCTION()
	void HandleOccupancyChanged(APawn* NewOccupant, APawn* OldOccupant);

	void BuildRig();
	void Occupy(AGalacticPiratesCharacter* Character);
	void Vacate();
	void LockGunner(AGalacticPiratesCharacter* Character);
	void UnlockGunner(AGalacticPiratesCharacter* Character);
	void ApplyMountRotation();
	void FireTrace();
	float DamageForActor(AActor* HitActor) const;
	void PlaceTracer(const FVector& Start, const FVector& End);
	void SpawnImpactSparks(const FVector& ImpactPoint, const FVector& IncomingDir);
	void TickSparks(float DeltaTime);
	void HideMuzzleFlash();
};
