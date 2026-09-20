// Copyright Epic Games, Inc. All Rights Reserved.


#include "GalacticPiratesPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "GalacticPiratesCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "ShipHudWidget.h"
#include "GalacticPirates.h"
#include "Widgets/Input/SVirtualJoystick.h"

AGalacticPiratesPlayerController::AGalacticPiratesPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = AGalacticPiratesCameraManager::StaticClass();
}

void AGalacticPiratesPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		ShipHud = CreateWidget<UShipHudWidget>(this);
		if (ShipHud)
		{
			ShipHud->AddToViewport(20);
		}
	}

	if (ShouldUseTouchControls())
	{
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			MobileControlsWidget->AddToPlayerScreen(0);
		}
		else
		{
			UE_LOG(LogGalacticPirates, Error, TEXT("Could not spawn mobile controls widget."));
		}
	}
}

void AGalacticPiratesPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}

bool AGalacticPiratesPlayerController::ShouldUseTouchControls() const
{
	if (!IsLocalPlayerController() || GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
