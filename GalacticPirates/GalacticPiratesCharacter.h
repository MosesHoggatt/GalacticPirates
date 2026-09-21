// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputCoreTypes.h"
#include "Logging/LogMacros.h"
#include "GalacticPiratesCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UQuatCamera;
class AWalkableShip;
class AActor;
class UMinigunPodComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class AGalacticPiratesCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UQuatCamera* QuatCameraComponent;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;

	/** Gamepad look multiplier. 3 = 200% faster than the previous 1x stick look. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.1"))
	float GamepadLookSensitivity = 3.0f;

	/** Ship Thrust Input Action */
	UPROPERTY(EditAnywhere, Category = "Input|Ship")
	UInputAction* ShipThrustAction;

	/** Ship Vertical Input Action */
	UPROPERTY(EditAnywhere, Category = "Input|Ship")
	UInputAction* ShipVerticalAction;

	/** Ship Rotation Input Action */
	UPROPERTY(EditAnywhere, Category = "Input|Ship")
	UInputAction* ShipRotationAction;

	/** Ship Roll Input Action */
	UPROPERTY(EditAnywhere, Category = "Input|Ship")
	UInputAction* ShipRollAction;

	/** Ship Interact Input Action */
	UPROPERTY(EditAnywhere, Category = "Input|Ship")
	UInputAction* ShipInteractAction;

	/** Hold to fire the occupied minigun */
	UPROPERTY(EditAnywhere, Category = "Input|Ship")
	UInputAction* MinigunFireAction;

	/** Ship blueprint to spawn when boarding */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	AWalkableShip* SpawnShip;

	/** The ship this character is currently boarded on */
	UPROPERTY(ReplicatedUsing = OnRep_BoardedShip, BlueprintReadOnly, Category = "Ship")
	AWalkableShip* BoardedShip;

	/** Is the character currently piloting a ship? */
	UPROPERTY(ReplicatedUsing = OnRep_IsPiloting, BlueprintReadOnly, Category = "Ship")
	bool bIsPiloting = false;

	UPROPERTY(ReplicatedUsing = OnRep_OccupiedMinigun, BlueprintReadOnly, Category = "Ship")
	TObjectPtr<UMinigunPodComponent> OccupiedMinigun;

	UPROPERTY(ReplicatedUsing = OnRep_OccupiedVehicle, BlueprintReadOnly, Category = "Ship")
	TObjectPtr<AActor> OccupiedVehicle;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Ship")
	bool bIsAiCrew = false;

	UPROPERTY(ReplicatedUsing = OnRep_Dead, BlueprintReadOnly, Category = "Ship")
	bool bDead = false;

	/** Rate at which pilot input is sent over the network */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship", meta = (ClampMin = "10.0", ClampMax = "60.0"))
	float PilotInputSendRate = 30.0f;

public:
	AGalacticPiratesCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Boards the specified ship */
	UFUNCTION(BlueprintCallable, Category = "Ship")
	void BoardShip(AWalkableShip* Ship);

	/** Leaves the current ship */
	UFUNCTION(BlueprintCallable, Category = "Ship")
	void LeaveShip();

	/** Returns the ship this character is currently boarded on */
	UFUNCTION(BlueprintCallable, Category = "Ship")
	AWalkableShip* GetBoardedShip() const { return BoardedShip; }

	UFUNCTION(BlueprintPure, Category = "Ship")
	AActor* GetOccupiedVehicle() const { return OccupiedVehicle; }

	void SetOccupiedVehicle(AActor* Vehicle);

	/** Returns if the character is currently piloting a ship */
	UFUNCTION(BlueprintCallable, Category = "Ship")
	bool IsAiCrew() const { return bIsAiCrew; }

	void SetAiCrew(bool bNewAiCrew);

	UFUNCTION(BlueprintCallable, Category = "Ship")
	bool IsPiloting() const { return bIsPiloting; }

	uint32 GetLastAcceptedPilotInputSeq() const { return LastAcceptedPilotInputSeq; }
	void AuthorityReceiveSequencedPilotInput(FVector ThrustInput, FVector RotationInput, uint32 InputSeq);
	uint32 GetLastAcceptedMinigunAimSeq() const { return LastAcceptedMinigunAimSeq; }
	void AuthorityReceiveSequencedMinigunAim(float YawDelta, float PitchDelta, uint32 InputSeq);
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;

	UFUNCTION(BlueprintCallable, Category = "Ship")
	bool IsManningMinigun() const { return OccupiedMinigun != nullptr; }

	UFUNCTION(BlueprintCallable, Category = "Ship")
	UMinigunPodComponent* GetOccupiedMinigun() const { return OccupiedMinigun; }

	void SetManningMinigun(UMinigunPodComponent* Pod);

	/** Set whether the character is piloting a ship */
	UFUNCTION(BlueprintCallable, Category = "Ship")
	void SetPiloting(bool bNewPiloting);

	/** Replace leftover held ship commands with the keys/sticks held right now. */
	void FlushHeldShipInputsOnTakeHelm();

	/** C++ control path used by tests and as a fallback when Blueprint IMCs are empty. */
	void SimulateControlKey(const FKey& Key, bool bPressed);
	void ClearSimulatedControlKeys();
	void PollAndSendVehicleInput(bool bForceSend = true);
	void SimulateMouseSteer(const FVector2D& Delta);

	/** Called when the boarded ship is destroyed */
	void OnShipDestroyed();

	UFUNCTION(BlueprintPure, Category = "Ship")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintCallable, Category = "Ship|Combat")
	void DieInWreck(const FVector& Epicenter);

	UInputAction* GetMoveAction() const { return MoveAction; }
	UInputAction* GetLookAction() const { return LookAction; }
	UInputAction* GetMouseLookAction() const { return MouseLookAction; }
	UInputAction* GetShipThrustAction() const { return ShipThrustAction; }
	UInputAction* GetShipVerticalAction() const { return ShipVerticalAction; }
	UInputAction* GetShipRotationAction() const { return ShipRotationAction; }
	UInputAction* GetShipRollAction() const { return ShipRollAction; }
	UInputAction* GetShipInteractAction() const { return ShipInteractAction; }

	int32 GetDebugMoveInputCount() const { return DebugMoveInputCount; }
	int32 GetDebugLookInputCount() const { return DebugLookInputCount; }
	FVector2D GetDebugLastMoveInput() const { return DebugLastMoveInput; }
	FVector2D GetDebugLastLookInput() const { return DebugLastLookInput; }

	void RestoreWalkingOnShip();
	void RestoreWalkCamera();
	void UpdateDeathCamera(float DeltaTime);

	void DumpShipDebugSnapshot(const TCHAR* Reason) const;
	void StartAutomatedShipPlaytest();
	void ApplyLookForTest(float Yaw, float Pitch);
	void ApplyJumpForTest();

protected:

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Called from Input Actions for ship thrust input */
	void ShipThrustInput(const FInputActionValue& Value);

	/** Called from Input Actions for ship vertical input */
	void ShipVerticalInput(const FInputActionValue& Value);

	/** Called from Input Actions for ship rotation input */
	void ShipRotationInput(const FInputActionValue& Value);

	/** Called from Input Actions for ship roll input */
	void ShipRollInput(const FInputActionValue& Value);

	/** Called from Input Actions for ship interaction input */
	void ShipInteractInput(const FInputActionValue& Value);
	void MinigunFireInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	virtual void BeginPlay() override;
	virtual void NotifyControllerChanged() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Destroyed() override;

	/** Called when the boarded ship is replicated */
	UFUNCTION()
	void OnRep_BoardedShip();

	/** Called when the piloting status is replicated */
	UFUNCTION()
	void OnRep_IsPiloting();

	UFUNCTION()
	void OnRep_OccupiedMinigun();

	UFUNCTION()
	void OnRep_OccupiedVehicle();

	UFUNCTION()
	void OnRep_Dead();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_WreckRagdoll(FVector Epicenter);

	void ApplyWreckRagdoll(const FVector& Epicenter);
	void BeginLocalDeathPresentation();

	/** Called when the ship's rotation is changed */
	UFUNCTION()
	void OnShipRotationChanged(const FQuat& NewRotation);

	/** Sends the pilot input to the server */
	UFUNCTION(Server, Unreliable, WithValidation)
	void Server_SendPilotInput(FVector ThrustInput, FVector RotationInput, uint32 InputSeq);

	/** Requests the server to toggle helm interaction */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestHelmInteraction();

	UFUNCTION(Server, Unreliable, WithValidation)
	void Server_SendMinigunAim(float YawDelta, float PitchDelta, uint32 InputSeq);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SetMinigunFiring(bool bNewFiring);

private:
	FVector AccumulatedThrustInput;
	FVector AccumulatedRotationInput;
	float TimeSinceLastPilotInputSend;
	FVector2D HelmMouseSteer = FVector2D::ZeroVector;
	bool bHelmMouseSteerThisFrame = false;
	uint32 NextPilotInputSeq = 1;
	uint32 LastAcceptedPilotInputSeq = 0;
	uint32 NextMinigunAimSeq = 1;
	uint32 LastAcceptedMinigunAimSeq = 0;

	void MouseLookInput(const FInputActionValue& Value);
	void ApplyHelmMouseSteer(const FVector2D& MouseDelta);

	UPROPERTY(Transient)
	UInputMappingContext* RuntimeShipAccessIMC = nullptr;

	UPROPERTY(Transient)
	UInputMappingContext* RuntimeMinigunFireIMC = nullptr;

	UPROPERTY(Transient)
	UInputMappingContext* RuntimeLocomotionIMC = nullptr;

	UPROPERTY(Transient)
	UInputMappingContext* RuntimeVehicleControlIMC = nullptr;

	/** Adds helm interact plus walk/look mappings if the possessed controller does not provide them. */
	void SetupShipAccessInputContext();

	void MapRuntimeLocomotionKeys();
	void ApplyVehicleControlMapping(bool bEnable);
	void EnsureNativeShipInputActions();
	void BindNativeVehicleInput();
	void PollHeldWalkKeys();
	void PollHeldVehicleKeys();
	bool IsControlKeyDown(const FKey& Key) const;

	TSet<FKey> SimulatedHeldKeys;
	bool bNativeVehicleInputBound = false;

	void TickAutomatedShipPlaytest(float DeltaTime);
	void InjectPlaytestKey(const FKey& Key, EInputEvent Event, float Delta = 1.0f);
	void InjectPlaytestAxis(const FKey& Key, FVector Delta);

	int32 DebugMoveInputCount = 0;
	int32 DebugLookInputCount = 0;
	FVector2D DebugLastMoveInput = FVector2D::ZeroVector;
	FVector2D DebugLastLookInput = FVector2D::ZeroVector;

	int32 ShipPlaytestPhase = -1;
	float ShipPlaytestTime = 0.0f;
	FVector ShipPlaytestStartLocation = FVector::ZeroVector;
	float ShipPlaytestStartYaw = 0.0f;
	FRotator ShipPlaytestShipStartRotation = FRotator::ZeroRotator;
	int32 ShipPlaytestMoveCountAtMark = 0;
	int32 ShipPlaytestLookCountAtMark = 0;
	bool bShipPlaytestLookDirectPassed = false;
	bool bShipPlaytestMoveDirectPassed = false;
	bool bShipPlaytestWasdMappedPassed = false;
	bool bShipPlaytestLookMappedPassed = false;
	bool bShipPlaytestHelmFollowPassed = false;

	/** Updates the camera's up direction based on the ship's orientation */
	void UpdateCameraUpDirection();

	/** Maintains an upright orientation for the character when on a ship */
	void MaintainUprightOrientation();

	/** Sends the accumulated pilot input to the server */
	void SendAccumulatedPilotInput(bool bForceSend = false);

	/** Sets up the character's movement base when boarded on a ship */
	void SetupMovementBaseOnShip();

	/** Clears the movement base, reverting to default behavior */
	void ClearMovementBase();

	/** Keeps gravity, base, and deck contact valid while boarded. */
	void TickBoardedWalkPhysics(float DeltaTime);

	FTransform LastGoodShipRelative = FTransform::Identity;
	bool bHasLastGoodShipRelative = false;
	FTransform LastShipWorldTM = FTransform::Identity;
	bool bHasLastShipWorldTM = false;
	float TimeOffShipDeck = 0.0f;

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns quaternion camera component **/
	UQuatCamera* GetQuatCameraComponent() const { return QuatCameraComponent; }

};

