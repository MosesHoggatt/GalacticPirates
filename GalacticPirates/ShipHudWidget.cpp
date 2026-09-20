#include "ShipHudWidget.h"
#include "GalacticPiratesCharacter.h"
#include "WalkableShip.h"
#include "HullHealthComponent.h"
#include "SpaceCraft.h"
#include "HelmComponent.h"
#include "WeaponTerminalComponent.h"
#include "ShipPulseCannonComponent.h"
#include "ShipMissileSalvoComponent.h"
#include "MinigunPodComponent.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Styling/CoreStyle.h"
#include "Misc/Paths.h"

FSlateFontInfo UShipHudWidget::MakeFont(int32 Size, bool bBold) const
{
	static FSlateFontInfo CachedRegular;
	static FSlateFontInfo CachedBold;
	static bool bCached = false;
	if (!bCached)
	{
		const FString KenneyPath = FPaths::ProjectContentDir() / TEXT("Polish/Fonts/KenneyFutureNarrow.ttf");
		if (FPaths::FileExists(KenneyPath))
		{
			CachedRegular = FSlateFontInfo(KenneyPath, 16);
			CachedBold = FSlateFontInfo(KenneyPath, 16);
		}
		else
		{
			CachedRegular = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16);
			CachedBold = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16);
		}
		bCached = true;
	}

	FSlateFontInfo Font = bBold ? CachedBold : CachedRegular;
	Font.Size = Size;
	return Font;
}

TSharedRef<SWidget> UShipHudWidget::BuildCrosshair()
{
	CrosshairStrokes.Reset();

	auto MakeStroke = [this](float Width, float Height, const FLinearColor& Color)
	{
		TSharedRef<SBorder> Stroke = SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(Color)
			.Visibility(EVisibility::HitTestInvisible);
		CrosshairStrokes.Add(Stroke);

		return SNew(SBox)
			.WidthOverride(Width)
			.HeightOverride(Height)
			[
				Stroke
			];
	};

	const FLinearColor Fill(0.95f, 0.98f, 1.0f, 1.0f);
	return SNew(SBox)
		.WidthOverride(96.0f)
		.HeightOverride(96.0f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0.0f, 6.0f, 0.0f, 0.0f))
			[ MakeStroke(4.0f, 28.0f, Fill) ]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
			[ MakeStroke(4.0f, 28.0f, Fill) ]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
			[ MakeStroke(28.0f, 4.0f, Fill) ]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
			[ MakeStroke(28.0f, 4.0f, Fill) ]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[ MakeStroke(6.0f, 6.0f, Fill) ]
		];
}

TSharedRef<SWidget> UShipHudWidget::RebuildWidget()
{
	PromptText = SNew(STextBlock)
		.Font(MakeFont(28, true))
		.ColorAndOpacity(FLinearColor(0.45f, 0.92f, 1.0f, 1.0f))
		.ShadowOffset(FVector2D(2.0f, 2.0f))
		.Justification(ETextJustify::Center);

	HealthLabel = SNew(STextBlock)
		.Font(MakeFont(16, true))
		.ColorAndOpacity(FLinearColor(0.85f, 0.95f, 1.0f, 0.9f))
		.Text(FText::FromString(TEXT("HULL")));

	RechargeLabel = SNew(STextBlock)
		.Font(MakeFont(16, true))
		.ColorAndOpacity(FLinearColor(0.7f, 0.9f, 1.0f, 0.9f))
		.Text(FText::FromString(TEXT("CANNON")));

	MissileLabel = SNew(STextBlock)
		.Font(MakeFont(16, true))
		.ColorAndOpacity(FLinearColor(1.0f, 0.55f, 0.25f, 0.9f))
		.Text(FText::FromString(TEXT("MISSILES")));

	HealthBar = SNew(SProgressBar)
		.Percent(1.0f)
		.FillColorAndOpacity(FSlateColor(FLinearColor(0.15f, 0.85f, 0.45f, 0.95f)));

	RechargeBar = SNew(SProgressBar)
		.Percent(1.0f)
		.FillColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.75f, 1.0f, 0.95f)));

	MissileBar = SNew(SProgressBar)
		.Percent(1.0f)
		.FillColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.4f, 0.12f, 0.95f)));

	Crosshair = BuildCrosshair();
	Crosshair->SetVisibility(EVisibility::Hidden);

	return SNew(SSafeZone)
		.Visibility(EVisibility::HitTestInvisible)
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			Crosshair.ToSharedRef()
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(FMargin(36.0f, 36.0f))
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor(0.02f, 0.05f, 0.08f, 0.62f))
			.Padding(FMargin(16.0f, 12.0f))
			.Visibility(EVisibility::HitTestInvisible)
			[
				SNew(SBox)
				.WidthOverride(360.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[ HealthLabel.ToSharedRef() ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 10)
					[
						SNew(SBox).HeightOverride(16.0f)
						[ HealthBar.ToSharedRef() ]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[ RechargeLabel.ToSharedRef() ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 10)
					[
						SNew(SBox).HeightOverride(16.0f)
						[ RechargeBar.ToSharedRef() ]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[ MissileLabel.ToSharedRef() ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
					[
						SNew(SBox).HeightOverride(16.0f)
						[ MissileBar.ToSharedRef() ]
					]
				]
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 92.0f))
		[
			PromptText.ToSharedRef()
		]
	];
}

void UShipHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UShipHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshFromPawn();
}

void UShipHudWidget::RefreshFromPawn()
{
	AGalacticPiratesCharacter* Character = GetOwningPlayerPawn() ? Cast<AGalacticPiratesCharacter>(GetOwningPlayerPawn()) : nullptr;
	AWalkableShip* Ship = Character ? Character->GetBoardedShip() : nullptr;
	ISpaceCraft* OccupiedCraft = Character ? GPAsSpaceCraft(Character->GetOccupiedVehicle()) : nullptr;
	UHullHealthComponent* Hull = nullptr;
	if (OccupiedCraft)
	{
		Hull = OccupiedCraft->GetHullHealth();
	}
	else if (Ship)
	{
		Hull = Ship->GetHullHealth();
	}

	if (!PromptText.IsValid() || !HealthBar.IsValid() || !RechargeBar.IsValid() || !MissileBar.IsValid())
	{
		return;
	}

	UMinigunPodComponent* Minigun = Character ? Character->GetOccupiedMinigun() : nullptr;
	if (Crosshair.IsValid())
	{
		const bool bShowCrosshair = Minigun != nullptr;
		Crosshair->SetVisibility(bShowCrosshair ? EVisibility::HitTestInvisible : EVisibility::Hidden);

		const FLinearColor StrokeColor = (Minigun && Minigun->IsFiring())
			? FLinearColor(1.0f, 0.78f, 0.18f, 1.0f)
			: FLinearColor(0.95f, 0.98f, 1.0f, 1.0f);
		for (const TSharedPtr<SBorder>& Stroke : CrosshairStrokes)
		{
			if (Stroke.IsValid())
			{
				Stroke->SetBorderBackgroundColor(StrokeColor);
			}
		}
	}

	const bool bHasCraft = OccupiedCraft || Ship;
	const bool bDestroyed = OccupiedCraft
		? OccupiedCraft->IsCraftWrecked()
		: (Ship && Ship->IsWrecked());
	if (!Character || !bHasCraft || bDestroyed)
	{
		PromptText->SetText(bDestroyed
			? FText::FromString(TEXT("SHIP DESTROYED"))
			: FText::GetEmpty());
		HealthBar->SetPercent(0.0f);
		RechargeBar->SetPercent(0.0f);
		MissileBar->SetPercent(0.0f);
		if (HealthLabel.IsValid())
		{
			HealthLabel->SetText(FText::FromString(TEXT("HULL")));
		}
		if (RechargeLabel.IsValid())
		{
			RechargeLabel->SetText(FText::FromString(TEXT("CANNON")));
		}
		if (MissileLabel.IsValid())
		{
			MissileLabel->SetText(FText::FromString(TEXT("MISSILES")));
		}
		return;
	}

	if (Hull)
	{
		HealthBar->SetPercent(Hull->GetHealthPercent());
		if (HealthLabel.IsValid())
		{
			HealthLabel->SetText(FText::FromString(FString::Printf(TEXT("HULL  %.0f / %.0f"), Hull->GetHealth(), Hull->GetMaxHealth())));
		}
	}
	else if (Ship)
	{
		HealthBar->SetPercent(Ship->GetHealthPercent());
		if (HealthLabel.IsValid())
		{
			HealthLabel->SetText(FText::FromString(FString::Printf(TEXT("HULL  %.0f / %.0f"), Ship->GetHealth(), Ship->GetMaxHealth())));
		}
	}

	float Recharge = 1.0f;
	FString CannonLine = TEXT("CANNON  READY");
	if (Ship && Ship->PulseCannon)
	{
		Recharge = Ship->PulseCannon->GetRechargeAlpha();
		if (!Ship->PulseCannon->CanFire())
		{
			CannonLine = FString::Printf(TEXT("CANNON  RECHARGE  %.1fs"), Ship->PulseCannon->GetCooldownRemaining());
		}
	}
	RechargeBar->SetPercent(Recharge);
	if (RechargeLabel.IsValid())
	{
		RechargeLabel->SetText(FText::FromString(CannonLine));
	}

	float MissileRecharge = 1.0f;
	FString MissileLine = TEXT("MISSILES  READY");
	if (Ship && Ship->MissileSalvo)
	{
		MissileRecharge = Ship->MissileSalvo->GetRechargeAlpha();
		if (!Ship->MissileSalvo->CanFire())
		{
			MissileLine = FString::Printf(TEXT("MISSILES  RECHARGE  %.1fs"), Ship->MissileSalvo->GetCooldownRemaining());
		}
	}
	MissileBar->SetPercent(MissileRecharge);
	if (MissileLabel.IsValid())
	{
		MissileLabel->SetText(FText::FromString(MissileLine));
	}

	PromptText->SetText(Ship ? Ship->GetInteractPrompt(Character) : FText::GetEmpty());
}
