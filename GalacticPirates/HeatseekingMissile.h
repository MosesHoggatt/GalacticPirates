#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeatseekingMissile.generated.h"

class UStaticMeshComponent;
class UProjectileMovementComponent;
class UPointLightComponent;
class AWalkableShip;
class APawn;
class UHoloMapPoiComponent;

float GPComputeMissileHeatScore(const FVector& Origin, const FVector& Forward, const FVector& TargetLocation, float TargetHeat, float MinDot);

UCLASS()
class GALACTICPIRATES_API AHeatseekingMissile : public AActor
{
	GENERATED_BODY()

public:
	AHeatseekingMissile();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MissileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> ExhaustLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHoloMapPoiComponent> HoloPoi;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	float Damage = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	float FuseSeconds = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	float HomingAcceleration = 9000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	float RetargetInterval = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	float HeatSeekMinDot = 0.15f;

	UFUNCTION(BlueprintCallable, Category = "Missile")
	void InitializeMissile(AWalkableShip* InSourceShip, AWalkableShip* InTarget, APawn* InInstigator, float InDamage, float Speed);

	UFUNCTION(BlueprintPure, Category = "Missile")
	AWalkableShip* GetLockedTarget() const { return LockedTarget; }

	UFUNCTION(BlueprintPure, Category = "Missile")
	AWalkableShip* GetSourceShip() const { return SourceShip; }

	UFUNCTION(BlueprintCallable, Category = "Missile")
	bool ApplyMinigunHit(float InDamage, APawn* InInstigator);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile", meta = (ClampMin = "1.0"))
	float MissileHealth = 18.0f;

	AWalkableShip* FindHottestTarget() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	UPROPERTY(Replicated)
	TObjectPtr<AWalkableShip> SourceShip;

	UPROPERTY(ReplicatedUsing = OnRep_LockedTarget)
	TObjectPtr<AWalkableShip> LockedTarget;

	UFUNCTION()
	void OnRep_LockedTarget();

private:
	UPROPERTY()
	TObjectPtr<APawn> InstigatorPawn;

	bool bDetonated = false;
	float RetargetTimer = 0.0f;
	float RemainingHealth = 18.0f;

	void AcquireOrRefreshTarget();
	void ApplyHoming();
	void Detonate(AWalkableShip* HitShip);
	void IgnoreSourceCollision();
};
