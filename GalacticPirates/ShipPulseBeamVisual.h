#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShipPulseBeamVisual.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class USceneComponent;
class AWalkableShip;
class APawn;

UCLASS()
class GALACTICPIRATES_API AShipPulseBeamVisual : public AActor
{
	GENERATED_BODY()

public:
	AShipPulseBeamVisual();

	UFUNCTION(BlueprintCallable, Category = "Ship|Weapon")
	void InitializeBeam(const FVector& Start, const FVector& End, float Thickness, float Duration, const FLinearColor& Color, float DamageRadius = 0.0f, float DamagePerSecond = 0.0f, AWalkableShip* SourceShip = nullptr, APawn* InstigatorPawn = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Ship|Weapon")
	void InitializeExplosion(const FVector& Location, float Scale, float Duration, const FLinearColor& Color);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* BeamRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* VisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* GlowMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* SheathMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> CoreLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> ImpactLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> MidLight;

private:
	FVector BeamStart = FVector::ZeroVector;
	FVector BeamEnd = FVector::ZeroVector;
	FLinearColor BeamColor = FLinearColor::White;
	float BeamThickness = 80.0f;
	float BeamDuration = 1.5f;
	float BeamElapsed = 0.0f;
	float BeamLength = 1.0f;
	float DamageRadius = 0.0f;
	float DamagePerSecond = 0.0f;
	bool bIsBeam = false;

	UPROPERTY()
	TObjectPtr<AWalkableShip> SourceShip;

	UPROPERTY()
	TObjectPtr<APawn> InstigatorPawn;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> MuzzleFlare;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ImpactBurst;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SparkMeshes;

	float ComputeIntensity() const;
	void ApplyVisualIntensity(float Intensity);
	void ApplyOverlapDamage(float DeltaTime, float Intensity);
};
