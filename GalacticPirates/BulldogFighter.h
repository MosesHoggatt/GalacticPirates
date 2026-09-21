#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "SpaceCraft.h"
#include "BulldogFighter.generated.h"

class UShipMovementComponent;
class UHoloMapPoiComponent;
class UBoxComponent;
class AWalkableShip;
class AHeatseekingMissile;
class UHullHealthComponent;
class UWeaponHardpointComponent;
class UOccupancyComponent;
class UShipMissileSalvoComponent;
class UMinigunMuzzleComponent;

UENUM()
enum class EBulldogStrafePhase : uint8
{
	Outbound,
	TurnIn,
	Strafe
};

UCLASS()
class GALACTICPIRATES_API ABulldogFighter : public APawn, public ISpaceCraft
{
	GENERATED_BODY()

public:
	ABulldogFighter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HullMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UShipMovementComponent> ShipMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHullHealthComponent> HullHealth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWeaponHardpointComponent> MissileHardpoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UShipMissileSalvoComponent> MissileSalvo;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWeaponHardpointComponent> GunHardpoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMinigunMuzzleComponent> NoseGun;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHoloMapPoiComponent> HoloPoi;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UOccupancyComponent> CockpitOccupancy;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Fighter|Affiliation")
	FName AffiliationId;

	/** FOF parent only. Does not restrict who may occupy this fighter. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Fighter|Affiliation")
	TObjectPtr<AActor> HomeCraft;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Fighter")
	bool bWrecked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fighter")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fighter|Attack", meta = (ClampMin = "200.0"))
	float StrafeLateral = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fighter|Attack")
	float StrafeHeight = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fighter|Attack", meta = (ClampMin = "0.1"))
	float FireConeDot = 0.88f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fighter|Attack", meta = (ClampMin = "0.1"))
	float MissileAimDot = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fighter|Attack", meta = (ClampMin = "50.0"))
	float MinFireDistance = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fighter|Attack", meta = (ClampMin = "400.0"))
	float MaxFireDistance = 4500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fighter|Attack", meta = (ClampMin = "200.0"))
	float MissileEngageDistance = 2400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fighter|Missile", meta = (ClampMin = "100.0"))
	float MissileLaunchSpeed = 5760.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fighter|Missile")
	float MissileDamage = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fighter|Missile")
	FVector MuzzleOffset = FVector(420.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fighter|Missile", meta = (ClampMin = "0.0"))
	float FireCooldown = 1.0f;

	UFUNCTION(BlueprintCallable, Category = "Fighter")
	AActor* FindAttackTarget() const;

	UFUNCTION(BlueprintCallable, Category = "Fighter")
	bool CanFireMissile() const;

	UFUNCTION(BlueprintCallable, Category = "Fighter")
	bool FireSeekingMissile(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "Fighter")
	void ApplyPilotInput(APawn* Pilot, const FVector& ThrustInput, const FVector& RotationInput);

	UFUNCTION(BlueprintCallable, Category = "Fighter")
	bool TryCockpitInteract(APawn* Pawn);

	UFUNCTION(BlueprintCallable, Category = "Fighter")
	void RegisterHomeCraft(AActor* InHomeCraft);

	UFUNCTION(BlueprintCallable, Category = "Fighter")
	void DockToHull(AWalkableShip* Host);

	UFUNCTION(BlueprintCallable, Category = "Fighter")
	void UndockFromHull();

	UFUNCTION(BlueprintPure, Category = "Fighter")
	bool IsHullDocked() const { return bHullDocked; }

	UFUNCTION(BlueprintPure, Category = "Fighter")
	AHeatseekingMissile* GetActiveMissile() const { return ActiveMissile.Get(); }

	UFUNCTION(BlueprintPure, Category = "Fighter")
	EBulldogStrafePhase GetStrafePhase() const { return Phase; }

	virtual UShipMovementComponent* GetSpaceMovement() const override;
	virtual UHullHealthComponent* GetHullHealth() const override;
	virtual bool IsCraftWrecked() const override;
	virtual FVector GetCraftVelocity() const override;
	virtual USceneComponent* GetHomingSceneComponent() const override;
	virtual bool HasHumanOccupant() const override;
	virtual FName GetAffiliationId() const override;
	virtual AActor* GetHomeCraft() const override;
	virtual UOccupancyComponent* GetPilotOccupancy() const override;
	virtual void NotifyCraftWrecked() override;
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;

	static ABulldogFighter* SpawnNearShip(UWorld* World, AWalkableShip* TargetShip);
	static ABulldogFighter* SpawnDockedOnShip(UWorld* World, AWalkableShip* TargetShip);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	EBulldogStrafePhase Phase = EBulldogStrafePhase::Outbound;
	float StrafeSide = 1.0f;
	float FireCooldownRemaining = 0.0f;
	float AiLogTimer = 0.0f;
	FVector RunAimPoint = FVector::ZeroVector;
	FVector RunAxis = FVector::ForwardVector;
	FVector RunOffset = FVector::ZeroVector;
	TWeakObjectPtr<AHeatseekingMissile> ActiveMissile;
	TWeakObjectPtr<AActor> CachedTarget;

	UPROPERTY(Replicated)
	bool bHullDocked = false;

	void UpdateNoseGun(AActor* Target, float Dist, float AimDot);

	void TickStrafeAi(float DeltaTime);
	void SteerToward(const FVector& WorldPoint, const FVector& LookPoint, float ThrottleBoost, bool bBrake);
	void PickNewOutbound(AActor* Target);
	float GetMapRangeCm() const;
	FVector GetInheritedLaunchVelocity() const;
	bool IsPlayerOccupied() const;

	UFUNCTION()
	void HandleHullDestroyed();

	UFUNCTION()
	void HandleCockpitOccupancy(APawn* NewOccupant, APawn* OldOccupant);
};
