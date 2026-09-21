// Copyright Epic Games, Inc. All Rights Reserved.


#include "GalacticPiratesPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "GalacticPiratesCameraManager.h"
#include "GalacticPiratesHUD.h"
#include "GalacticPiratesCharacter.h"
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
			ShipHud->SetVisibility(ESlateVisibility::HitTestInvisible);
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

void AGalacticPiratesPlayerController::BeginCrewDeathPresentation()
{
	UE_LOG(LogGalacticPirates, Warning, TEXT("[DeathFX] PC BeginCrewDeathPresentation local=%d net=%d hud=%s"),
		IsLocalPlayerController() ? 1 : 0,
		static_cast<int32>(GetNetMode()),
		*GetNameSafe(MyHUD));

	if (!IsLocalPlayerController() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	AGalacticPiratesCharacter* Crew = Cast<AGalacticPiratesCharacter>(GetPawn());
	if (!Crew || !Crew->IsDead() || !Crew->IsLocallyControlled())
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[DeathFX] PC BeginCrewDeathPresentation ignored: pawn is not the local dead crew (%s)"),
			*GetNameSafe(GetPawn()));
		return;
	}

	if (ShipHud)
	{
		ShipHud->SetVisibility(ESlateVisibility::Collapsed);
	}

	ClientSetHUD(AGalacticPiratesHUD::StaticClass());
	AGalacticPiratesHUD* DeathHud = Cast<AGalacticPiratesHUD>(GetHUD());
	if (!DeathHud)
	{
		DeathHud = Cast<AGalacticPiratesHUD>(MyHUD);
	}
	if (DeathHud)
	{
		DeathHud->BeginDeathPresentation();
	}
	else
	{
		UE_LOG(LogGalacticPirates, Error, TEXT("[DeathFX] PC failed to spawn GalacticPiratesHUD, got %s"),
			*GetNameSafe(GetHUD()));
	}
}

void AGalacticPiratesPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);
	AGalacticPiratesCharacter* Crew = Cast<AGalacticPiratesCharacter>(InPawn);
	if (InPawn && (!Crew || !Crew->IsDead()))
	{
		if (AGalacticPiratesHUD* DeathHud = Cast<AGalacticPiratesHUD>(GetHUD()))
		{
			DeathHud->EndDeathPresentation();
		}
	}
}
