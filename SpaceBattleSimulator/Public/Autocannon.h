#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Autocannon.generated.h"

USTRUCT()
struct FParticleInstance
{
	GENERATED_BODY()

	UPROPERTY()
	UParticleSystemComponent* Component;

	UPROPERTY()
	float RemainingLifetime;

	FParticleInstance() : Component(nullptr), RemainingLifetime(0.0f) {}
	FParticleInstance(UParticleSystemComponent* InComponent, float InLifetime)
		: Component(InComponent), RemainingLifetime(InLifetime) {
	}
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UAutocannon : public USceneComponent
{
	GENERATED_BODY()

public:
	UAutocannon();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StartFiring();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StopFiring();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void Fire();

	UFUNCTION(BlueprintCallable, Category = "Debug")
	void ToggleDebugArrow(bool ShowDebugArrow);

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void SetTargetActor(AActor* NewTarget);

	UFUNCTION()
	UParticleSystemComponent* GetPooledParticleComponent(TArray<UParticleSystemComponent*>& Pool, UParticleSystem* Template);

	UFUNCTION()
	void ActivateParticleComponent(UParticleSystemComponent* Component, float Duration, bool IsMuzzleFlash = false);

	UPROPERTY(Transient)
	TArray<UParticleSystemComponent*> ImpactEffectPool;

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.01", ClampMax = "10.0"))
	float FireRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float ProjectileSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	UMaterialInterface* ProjectileMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0"))
	float DamageAmount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float FirstFireDelay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float ProjectileScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	UParticleSystem* MuzzleFlashFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	USoundCue* FiringSFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	FRotator MuzzleFlashRotationOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	FVector MuzzleFlashOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float MuzzleFlashScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	UParticleSystem* ImpactFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects", meta = (ClampMin = "1", ClampMax = "50"))
	int32 ParticlePoolSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool EnableDebugDraws;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool EnableDebugLogs;

protected:
	float CurrentFireCooldown;
	bool IsFiring;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug", meta = (AllowPrivateAccess = "true"))
	class UArrowComponent* DebugArrow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Targeting", meta = (AllowPrivateAccess = "true"))
	AActor* TargetActor;

	UPROPERTY(Transient)
	TArray<UParticleSystemComponent*> MuzzleFlashPool;

	UPROPERTY(Transient)
	TArray<FParticleInstance> ActiveParticles;

	void InitializeParticlePool();
};