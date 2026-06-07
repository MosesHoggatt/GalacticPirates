// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "GalacticPiratesCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UQuatCamera;
class AWalkableShip;
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

	/** Ship blueprint to spawn when boarding */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	AWalkableShip* SpawnShip;

	/** The ship this character is currently boarded on */
	UPROPERTY(ReplicatedUsing = OnRep_BoardedShip, BlueprintReadOnly, Category = "Ship")
	AWalkableShip* BoardedShip;

	/** Is the character currently piloting a ship? */
	UPROPERTY(ReplicatedUsing = OnRep_IsPiloting, BlueprintReadOnly, Category = "Ship")
	bool bIsPiloting = false;

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

	/** Returns if the character is currently piloting a ship */
	UFUNCTION(BlueprintCallable, Category = "Ship")
	bool IsPiloting() const { return bIsPiloting; }

	/** Set whether the character is piloting a ship */
	UFUNCTION(BlueprintCallable, Category = "Ship")
	void SetPiloting(bool bNewPiloting);

	/** Called when the boarded ship is destroyed */
	void OnShipDestroyed();

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
	virtual void Tick(float DeltaTime) override;
	virtual void Destroyed() override;

	/** Called when the boarded ship is replicated */
	UFUNCTION()
	void OnRep_BoardedShip();

	/** Called when the piloting status is replicated */
	UFUNCTION()
	void OnRep_IsPiloting();

	/** Called when the ship's rotation is changed */
	UFUNCTION()
	void OnShipRotationChanged(const FQuat& NewRotation);

	/** Sends the pilot input to the server */
	UFUNCTION(Server, Unreliable, WithValidation)
	void Server_SendPilotInput(FVector ThrustInput, FVector RotationInput);

	/** Requests the server to toggle helm interaction */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestHelmInteraction();

private:
	FVector AccumulatedThrustInput;
	FVector AccumulatedRotationInput;
	float TimeSinceLastPilotInputSend;

	/** Updates the camera's up direction based on the ship's orientation */
	void UpdateCameraUpDirection();

	/** Maintains an upright orientation for the character when on a ship */
	void MaintainUprightOrientation();

	/** Sends the accumulated pilot input to the server */
	void SendAccumulatedPilotInput();

	/** Sets up the character's movement base when boarded on a ship */
	void SetupMovementBaseOnShip();

	/** Clears the movement base, reverting to default behavior */
	void ClearMovementBase();

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns quaternion camera component **/
	UQuatCamera* GetQuatCameraComponent() const { return QuatCameraComponent; }

};

