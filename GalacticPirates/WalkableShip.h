#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CombatTypes.h"
#include "SpaceCraft.h"
#include "Engine/NetSerialization.h"
#include "WalkableShip.generated.h"

class UShipMovementComponent;
class UHelmComponent;
class UShipPulseCannonComponent;
class UWeaponTerminalComponent;
class UShipMissileSalvoComponent;
class UMissileSalvoTerminalComponent;
class UMinigunPodComponent;
class UHolographicMapTableComponent;
class UHoloMapPoiComponent;
class UShipOrbitAiComponent;
class UShipCrewAiComponent;
class UBoxComponent;
class AGalacticPiratesCharacter;
class UWorld;
class UPointLightComponent;
class UHullHealthComponent;
class UWeaponHardpointComponent;
class UOccupancyComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShipRotationChanged, const FQuat&, NewRotation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPilotChanged, AGalacticPiratesCharacter*, NewPilot, AGalacticPiratesCharacter*, OldPilot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShipDamaged, float, DamageAmount, float, RemainingHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShipExploded);

UCLASS()
class GALACTICPIRATES_API AWalkableShip : public APawn, public ISpaceCraft
{
	GENERATED_BODY()

public:
	AWalkableShip();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* ShipRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* HullMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* InteriorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UShipMovementComponent* ShipMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UHullHealthComponent* HullHealth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UWeaponHardpointComponent* PulseHardpoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UWeaponHardpointComponent* MissileHardpoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UWeaponHardpointComponent* PortGunHardpoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UWeaponHardpointComponent* StarboardGunHardpoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UHelmComponent* Helm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UOccupancyComponent* HelmOccupancy;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UOccupancyComponent* PortGunOccupancy;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UOccupancyComponent* StarboardGunOccupancy;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UShipPulseCannonComponent* PulseCannon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UWeaponTerminalComponent* WeaponTerminal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UShipMissileSalvoComponent* MissileSalvo;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UMissileSalvoTerminalComponent* MissileTerminal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UMinigunPodComponent* PortMinigun;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UMinigunPodComponent* StarboardMinigun;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|GunPods")
	UStaticMeshComponent* PortPodBubble;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|GunPods")
	UStaticMeshComponent* PortPodDeck;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|GunPods")
	UStaticMeshComponent* PortPodNeck;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|GunPods")
	UStaticMeshComponent* PortDoorFillLower;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|GunPods")
	UStaticMeshComponent* PortDoorFillUpper;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|GunPods")
	UStaticMeshComponent* StarboardPodBubble;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|GunPods")
	UStaticMeshComponent* StarboardPodDeck;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|GunPods")
	UStaticMeshComponent* StarboardPodNeck;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|GunPods")
	UStaticMeshComponent* StarboardDoorFillLower;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|GunPods")
	UStaticMeshComponent* StarboardDoorFillUpper;

	/** Gun pod blockout dimensions, in ship-local centimetres. */
	static constexpr float PodCenterY = 580.0f;
	static constexpr float PodBubbleDiameter = 400.0f;
	static constexpr float PodBubbleCenterZ = 120.0f;
	static constexpr float PodDoorwayCenterX = -100.0f;
	static constexpr float PodDoorwayWidth = 300.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UHolographicMapTableComponent* MapTable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UHoloMapPoiComponent* HoloPoi;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UShipOrbitAiComponent* OrbitAI;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UShipCrewAiComponent* CrewAI;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* CombatHull;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Alarm")
	UPointLightComponent* AlarmLightFore;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Alarm")
	UPointLightComponent* AlarmLightMid;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Alarm")
	UPointLightComponent* AlarmLightAft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Alarm")
	UPointLightComponent* AlarmLightPort;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Alarm")
	UPointLightComponent* AlarmLightStarboard;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SpawnPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Combat")
	EShipArmorClass ArmorClass = EShipArmorClass::Armored;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Combat", meta = (ClampMin = "1.0"))
	float MaxHealth = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Combat", meta = (ClampMin = "0.05"))
	float ExplosionVisualScale = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Combat", meta = (ClampMin = "0.1"))
	float WreckLifetime = 8.0f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Ship|Affiliation")
	FName AffiliationId;

	UPROPERTY(BlueprintAssignable, Category = "Ship Events")
	FOnShipRotationChanged OnShipRotationChanged;

	UPROPERTY(BlueprintAssignable, Category = "Ship Events")
	FOnPilotChanged OnPilotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Ship Events")
	FOnShipDamaged OnShipDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Ship Events")
	FOnShipExploded OnShipExploded;

	UFUNCTION(BlueprintCallable, Category = "Ship")
	void RegisterPlayer(AGalacticPiratesCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Ship")
	void UnregisterPlayer(AGalacticPiratesCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Ship")
	bool RequestPilotAssignment(AGalacticPiratesCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Ship")
	void ReleasePilot(AGalacticPiratesCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Ship")
	AGalacticPiratesCharacter* GetCurrentPilot() const { return CurrentPilot; }

	UFUNCTION(BlueprintCallable, Category = "Ship")
	bool IsPilot(AGalacticPiratesCharacter* Character) const { return CurrentPilot == Character; }

	UFUNCTION(BlueprintCallable, Category = "Ship")
	const TArray<AGalacticPiratesCharacter*>& GetPlayersAboard() const { return PlayersAboard; }

	UFUNCTION(BlueprintPure, Category = "Ship")
	bool HasHumanCrew() const;

	UFUNCTION(BlueprintCallable, Category = "Ship")
	UStaticMeshComponent* GetInteriorMesh() const { return InteriorMesh; }

	UFUNCTION(BlueprintCallable, Category = "Ship")
	FVector GetShipUpVector() const { return GetActorUpVector(); }

	UFUNCTION(BlueprintCallable, Category = "Ship")
	FTransform GetSpawnTransform() const;

	UFUNCTION(BlueprintCallable, Category = "Ship")
	FTransform GetSpawnTransformForSlot(int32 Slot) const;

	static AWalkableShip* FindPersistentShip(UWorld* World);

	UFUNCTION(BlueprintCallable, Category = "Ship")
	bool IsWalkableWorldLocation(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintCallable, Category = "Ship")
	bool HasDeckBelow(const FVector& WorldLocation, float TraceDistance = 400.0f) const;

	UFUNCTION(BlueprintCallable, Category = "Ship")
	FVector GetPointVelocity(const FVector& WorldPoint) const;

	void ApplyPilotInput(AGalacticPiratesCharacter* Pilot, const FVector& ThrustInput, const FVector& RotationInput);
	void HandlePlayerDisconnected(AGalacticPiratesCharacter* Character);

	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;

	UFUNCTION(BlueprintCallable, Category = "Ship|Combat")
	float ApplyShipDamage(float DamageAmount, AGalacticPiratesCharacter* InstigatorCharacter, AActor* DamageCauser);

	UFUNCTION(BlueprintCallable, Category = "Ship|Combat")
	void Explode();

	UFUNCTION(BlueprintPure, Category = "Ship|Combat")
	bool IsWrecked() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Combat")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Combat")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Combat")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintCallable, Category = "Ship|Combat")
	void SetHealth(float NewHealth);

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

	UFUNCTION(BlueprintCallable, Category = "Ship")
	bool TryStationInteract(AGalacticPiratesCharacter* Character);

	UFUNCTION(BlueprintPure, Category = "Ship")
	FText GetInteractPrompt(AGalacticPiratesCharacter* Character) const;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentPilot)
	AGalacticPiratesCharacter* CurrentPilot;

	UPROPERTY()
	TArray<AGalacticPiratesCharacter*> PlayersAboard;

	UPROPERTY(Replicated)
	FVector_NetQuantize10 ReplicatedLinearVelocity;

	UPROPERTY(Replicated)
	FVector_NetQuantize10 ReplicatedAngularVelocity;

	UPROPERTY(ReplicatedUsing = OnRep_Wrecked, BlueprintReadOnly, Category = "Ship|Combat")
	bool bWrecked = false;

	UFUNCTION()
	void OnRep_CurrentPilot(AGalacticPiratesCharacter* OldPilot);

	UFUNCTION()
	void OnRep_Wrecked();

	UFUNCTION()
	void HandleHelmOccupancy(APawn* NewOccupant, APawn* OldOccupant);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Explode();

private:
	FQuat LastReplicatedRotation;
	void BroadcastRotationChange();
	void CleanupAllPlayers();

	/** Splits the hull side walls so each gun pod has a walk-through doorway. */
	void CarveGunPodDoorways();
	void TriggerInteriorAlarm();
	void TickInteriorAlarm(float DeltaTime);
	void ApplyAlarmLightFlash(float IntensityScale);
	bool ShouldPlayInteriorAlarmAudio() const;
	void SyncHullFromAuthoring();

	UFUNCTION()
	void HandleHullHealthChanged(float InCurrentHealth, float InMaxHealth);

	UFUNCTION()
	void HandleHullDestroyed();

	float LastNotifiedHealth = 1200.0f;
	float InteriorAlarmTime = 0.0f;
	float InteriorSirenTimer = 0.0f;
};
