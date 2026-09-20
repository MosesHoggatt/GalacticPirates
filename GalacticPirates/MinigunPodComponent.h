#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "MinigunPodComponent.generated.h"

class AWalkableShip;
class AGalacticPiratesCharacter;
class AHeatseekingMissile;
class UStaticMeshComponent;
class UPointLightComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GALACTICPIRATES_API UMinigunPodComponent : public USceneComponent
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
	TObjectPtr<UStaticMeshComponent> ImpactFlashMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> MuzzleLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigun|Fx", meta = (ClampMin = "0.01"))
	float MuzzleFlashSeconds = 0.05f;

	UFUNCTION(BlueprintCallable, Category = "Minigun")
	bool TryInteract(AGalacticPiratesCharacter* Character);

	UFUNCTION(BlueprintPure, Category = "Minigun")
	bool IsFiring() const { return bFiring; }

	UFUNCTION(BlueprintCallable, Category = "Minigun")
	bool IsCharacterInRange(const AGalacticPiratesCharacter* Character) const;

	UFUNCTION(BlueprintPure, Category = "Minigun")
	bool IsOccupied() const { return Gunner != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Minigun")
	AGalacticPiratesCharacter* GetGunner() const { return Gunner; }

	void ForceRelease();
	/** Muzzle flash, tracer and report for one round; also the body of the fire multicast. */
	void PlayShotFx(const FVector& Start, const FVector& End, bool bHit);
	void UpdateShotFx(float DeltaTime);
	void AddAimInput(float YawDelta, float PitchDelta);
	void SetFiring(bool bNewFiring);
	void ApplyAim(float NewYaw, float NewPitch);
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
	int32 ShotsPlayed = 0;

	void BuildRig();
	void Occupy(AGalacticPiratesCharacter* Character);
	void Vacate();
	void LockGunner(AGalacticPiratesCharacter* Character);
	void UnlockGunner(AGalacticPiratesCharacter* Character);
	void ApplyMountRotation();
	void FireTrace();
	float DamageForActor(AActor* HitActor) const;
};
