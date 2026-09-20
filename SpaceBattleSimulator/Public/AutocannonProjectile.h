#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "AutocannonProjectile.generated.h"

UCLASS()
class AAutocannonProjectile : public AActor
{
	GENERATED_BODY()

public:
	AAutocannonProjectile();

	void Initialize(float Speed, FVector InheritedVelocity = FVector::ZeroVector, float Damage = 0.0f, class UParticleSystem* ImpactEffect = nullptr);
	void SetMaterial(UMaterialInterface* Material);
	void SetProjectileScale(float Scale);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	float DamageAmount;

	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	class UParticleSystem* ImpactFX;
};