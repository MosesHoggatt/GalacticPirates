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
class UHullHealthComponent;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHullHealthComponent> HullHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	float Damage = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	float FuseSeconds = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	float HomingAcceleration = 16335.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	float RetargetInterval = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile")
	float HeatSeekMinDot = 0.15f;

	UFUNCTION(BlueprintCallable, Category = "Missile")
	void InitializeMissile(AActor* InSource, AActor* InTarget, APawn* InInstigator, float InDamage, float Speed, const FVector& InheritedVelocity = FVector::ZeroVector);

	UFUNCTION(BlueprintPure, Category = "Missile")
	AWalkableShip* GetLockedTarget() const;

	UFUNCTION(BlueprintPure, Category = "Missile")
	AActor* GetLockedTargetActor() const { return LockedTarget; }

	UFUNCTION(BlueprintPure, Category = "Missile")
	AWalkableShip* GetSourceShip() const;

	UFUNCTION(BlueprintPure, Category = "Missile")
	AActor* GetSourceActor() const { return SourceActor; }

	UFUNCTION(BlueprintCallable, Category = "Missile")
	bool ApplyMinigunHit(float InDamage, APawn* InInstigator);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Missile", meta = (ClampMin = "1.0"))
	float MissileHealth = 18.0f;

	AActor* FindHottestTarget() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	UPROPERTY(Replicated)
	TObjectPtr<AActor> SourceActor;

	UPROPERTY(ReplicatedUsing = OnRep_LockedTarget)
	TObjectPtr<AActor> LockedTarget;

	UFUNCTION()
	void OnRep_LockedTarget();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_DetonateFx(FVector_NetQuantize Location, bool bShipHit);

	UFUNCTION()
	void HandleHullDestroyed();

private:
	UPROPERTY()
	TObjectPtr<APawn> InstigatorPawn;

	bool bDetonated = false;
	float RetargetTimer = 0.0f;
	float FuseElapsed = 0.0f;

	void AcquireOrRefreshTarget();
	void ApplyHoming();
	void Detonate(AActor* HitActor);
	void PlayDetonationFx(const FVector& Location, bool bShipHit);
	void IgnoreSourceCollision();
};
