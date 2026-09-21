#include "GalacticPiratesHUD.h"
#include "GalacticPirates.h"
#include "GalacticPiratesCharacter.h"
#include "ShipPolish.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/Paths.h"
#include "Fonts/CompositeFont.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPDeath, Log, All);

namespace
{
	void GPWrapDeathTexture(UTexture2D* Texture)
	{
		if (!Texture)
		{
			return;
		}
		Texture->AddressX = TA_Wrap;
		Texture->AddressY = TA_Wrap;
		Texture->Filter = TF_Bilinear;
		Texture->MipGenSettings = TMGS_NoMipmaps;
	}

	FSlateFontInfo GPMakeDeathFont(int32 Size)
	{
		const FString Orbitron = FPaths::ProjectContentDir() / TEXT("Polish/Fonts/Orbitron.ttf");
		const FString Mono = FPaths::ProjectContentDir() / TEXT("Polish/Fonts/ShareTechMono-Regular.ttf");
		const FString FontPath = FPaths::FileExists(Orbitron) ? Orbitron : Mono;

		FSlateFontInfo Font;
		if (FPaths::FileExists(FontPath))
		{
			const TSharedRef<FCompositeFont> Composite = MakeShared<FCompositeFont>();
			Composite->DefaultTypeface.AppendFont(TEXT("Regular"), FontPath, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			Font = FSlateFontInfo(Composite, Size);
		}
		else
		{
			Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size);
		}
		Font.Size = Size;
		Font.OutlineSettings.OutlineSize = 3;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.02f, 0.12f, 0.28f, 0.9f);
		return Font;
	}
}

AGalacticPiratesHUD::AGalacticPiratesHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGalacticPiratesHUD::BeginPlay()
{
	Super::BeginPlay();
	LoadDeathTextures();
	UE_LOG(LogGPDeath, Warning, TEXT("[DeathFX] HUD BeginPlay hud=%s frames=%d scan=%s"),
		*GetName(),
		StaticFrames.Num(),
		ScanlineTexture ? TEXT("yes") : TEXT("NO"));
}

void AGalacticPiratesHUD::LoadDeathTextures()
{
	StaticFrames.Reset();
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const FString Name = FString::Printf(TEXT("T_TvStatic_%02d"), Index);
		UTexture2D* Frame = GPLoadPolishTexture(*Name);
		if (!Frame)
		{
			Frame = GPLoadPolishTexture(TEXT("T_TvStatic"));
		}
		if (Frame)
		{
			GPWrapDeathTexture(Frame);
			StaticFrames.Add(Frame);
		}
	}
	ScanlineTexture = GPLoadPolishTexture(TEXT("T_Scanlines"));
	GPWrapDeathTexture(ScanlineTexture);
}

void AGalacticPiratesHUD::BeginDeathPresentation()
{
	bDeathPresentationActive = true;
	bPlayedStaticSfx = false;
	DeathStartWallSeconds = FPlatformTime::Seconds();
	LastLoggedPhase = EGPDeathFxPhase::Idle;
	LastPeriodicLogAge = -1000.0f;
	LoadDeathTextures();
	SetLostTextVisible(false);
	LogDeathFx(TEXT("BEGIN"));
}

void AGalacticPiratesHUD::EndDeathPresentation()
{
	bDeathPresentationActive = false;
	bPlayedStaticSfx = false;
	SetLostTextVisible(false);
	UE_LOG(LogGPDeath, Warning, TEXT("[DeathFX] END hud=%s"), *GetName());
}

float AGalacticPiratesHUD::GetDeathAgeSeconds() const
{
	if (!bDeathPresentationActive)
	{
		return -1.0f;
	}
	return static_cast<float>(FPlatformTime::Seconds() - DeathStartWallSeconds);
}

void AGalacticPiratesHUD::LogDeathFx(const TCHAR* Reason) const
{
	const float Age = GetDeathAgeSeconds();
	const EGPDeathFxPhase Phase = GPDeathFxPhaseForAge(Age);
	UE_LOG(LogGPDeath, Warning,
		TEXT("[DeathFX] %s age=%.3f phase=%s text=%d static=%d sfx=%d frames=%d canvas=%dx%d"),
		Reason,
		Age,
		GPDeathFxPhaseName(Phase),
		GPDeathFxWantsText(Age) ? 1 : 0,
		GPDeathFxWantsStatic(Age) ? 1 : 0,
		bPlayedStaticSfx ? 1 : 0,
		StaticFrames.Num(),
		Canvas ? Canvas->SizeX : 0,
		Canvas ? Canvas->SizeY : 0);
}

void AGalacticPiratesHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!bDeathPresentationActive || !Canvas)
	{
		return;
	}

	const float Age = GetDeathAgeSeconds();
	const EGPDeathFxPhase Phase = GPDeathFxPhaseForAge(Age);

	if (Phase != LastLoggedPhase)
	{
		LastLoggedPhase = Phase;
		LogDeathFx(TEXT("PHASE"));
	}
	if (bVerboseDeathLog && (Age - LastPeriodicLogAge) >= 0.25f)
	{
		LastPeriodicLogAge = Age;
		LogDeathFx(TEXT("TICK"));
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(41701, 0.2f, FColor::Cyan,
			FString::Printf(TEXT("DeathFX age=%.2f %s text=%s static=%s"),
				Age,
				GPDeathFxPhaseName(Phase),
				GPDeathFxWantsText(Age) ? TEXT("ON") : TEXT("off"),
				GPDeathFxWantsStatic(Age) ? TEXT("ON") : TEXT("off")));
	}

	const float Width = Canvas->SizeX;
	const float Height = Canvas->SizeY;
	if (Width <= 1.0f || Height <= 1.0f)
	{
		return;
	}

	if (GPDeathFxWantsStatic(Age))
	{
		if (!bPlayedStaticSfx)
		{
			bPlayedStaticSfx = true;
			GPPlayPolishSound2D(this, TEXT("SFX_SignalLost"), 0.85f);
			LogDeathFx(TEXT("SFX+STATIC"));
		}
		DrawFullScreenStatic(Age, Width, Height);
	}

	SetLostTextVisible(GPDeathFxWantsText(Age));
}

void AGalacticPiratesHUD::DrawFullScreenStatic(float Age, float Width, float Height)
{
	FVector2D ViewSize(Width, Height);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewSize);
	}
	const float ScreenW = FMath::Max(Width, static_cast<float>(ViewSize.X));
	const float ScreenH = FMath::Max(Height, static_cast<float>(ViewSize.Y));

	const float StaticAlpha = FMath::Clamp((Age - GPDeathStaticAtSeconds) / 0.85f, 0.0f, 1.0f);
	DrawRect(FLinearColor(0.01f, 0.06f, 0.14f, StaticAlpha), -8.0f, -8.0f, ScreenW + 16.0f, ScreenH + 16.0f);

	const float AnimAge = Age * 0.2f;
	UTexture2D* FrameA = nullptr;
	UTexture2D* FrameB = nullptr;
	if (StaticFrames.Num() > 0)
	{
		const int32 FrameIndex = FMath::Abs(FMath::FloorToInt(AnimAge * 12.0f)) % StaticFrames.Num();
		const int32 AltIndex = (FrameIndex + 3) % StaticFrames.Num();
		FrameA = StaticFrames[FrameIndex];
		FrameB = StaticFrames[AltIndex];
	}

	const float TileU = FMath::Max(ScreenW / 256.0f, 4.0f);
	const float TileV = FMath::Max(ScreenH / 256.0f, 4.0f);
	const float JitterX = FMath::Sin(AnimAge * 73.0f) * 48.0f + FMath::Cos(AnimAge * 19.0f) * 22.0f;
	const float JitterY = FMath::Cos(AnimAge * 61.0f) * 40.0f + FMath::Sin(AnimAge * 27.0f) * 18.0f;
	const float DrawW = ScreenW * 2.2f;
	const float DrawH = ScreenH * 2.2f;
	const float BaseX = (ScreenW - DrawW) * 0.5f;
	const float BaseY = (ScreenH - DrawH) * 0.5f;

	if (FrameA)
	{
		DrawTexture(FrameA, -4.0f, -4.0f, ScreenW + 8.0f, ScreenH + 8.0f, 0.0f, 0.0f, TileU, TileV,
			FLinearColor(0.4f, 0.85f, 1.0f, StaticAlpha * 0.9f), BLEND_Translucent);
		DrawTexture(FrameA, BaseX + JitterX, BaseY + JitterY, DrawW, DrawH, 0.0f, 0.0f, TileU * 1.35f, TileV * 1.35f,
			FLinearColor(0.4f, 0.85f, 1.0f, StaticAlpha * 0.85f), BLEND_Translucent);
	}
	if (FrameB)
	{
		DrawTexture(FrameB, BaseX - JitterY * 0.7f, BaseY + JitterX * 0.6f, DrawW, DrawH, 0.25f, 0.15f, TileU * 1.35f, TileV * 1.35f,
			FLinearColor(0.15f, 0.55f, 1.0f, StaticAlpha * 0.45f), BLEND_Translucent);
	}
	if (!FrameA)
	{
		DrawRect(FLinearColor(0.08f, 0.28f, 0.55f, StaticAlpha * 0.95f), -8.0f, -8.0f, ScreenW + 16.0f, ScreenH + 16.0f);
	}

	if (ScanlineTexture)
	{
		const float ScrollV = FMath::Fmod(AnimAge * 6.56f, 1.0f);
		DrawTexture(ScanlineTexture, BaseX, BaseY - 120.0f, DrawW, DrawH + 240.0f, 0.0f, ScrollV, 1.0f, 12.0f,
			FLinearColor(0.45f, 0.95f, 1.0f, StaticAlpha * 0.55f), BLEND_Translucent);
	}
}

void AGalacticPiratesHUD::SetLostTextVisible(bool bVisible)
{
	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (!Viewport)
	{
		return;
	}

	if (bVisible && !LostTextHost.IsValid())
	{
		LostTextBlock = SNew(STextBlock)
			.Font(GPMakeDeathFont(96))
			.ColorAndOpacity(FLinearColor(0.55f, 0.95f, 1.0f, 1.0f))
			.ShadowOffset(FVector2D(3.0f, 3.0f))
			.ShadowColorAndOpacity(FLinearColor(0.0f, 0.08f, 0.2f, 0.9f))
			.Justification(ETextJustify::Center)
			.Text(FText::FromString(TEXT("SIGNAL LOST")));

		LostTextHost = SNew(SOverlay)
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				LostTextBlock.ToSharedRef()
			];
		Viewport->AddViewportWidgetContent(LostTextHost.ToSharedRef(), 50);
	}

	if (LostTextBlock.IsValid())
	{
		LostTextBlock->SetVisibility(bVisible ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	}
	if (LostTextHost.IsValid())
	{
		LostTextHost->SetVisibility(bVisible ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	}

	if (!bVisible && LostTextHost.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(LostTextHost.ToSharedRef());
		LostTextHost.Reset();
		LostTextBlock.Reset();
	}
}

void GPStartDeathPresentationTest(UWorld* World)
{
	if (!World)
	{
		UE_LOG(LogGPDeath, Error, TEXT("[DeathFX] gp.DeathPresentationTest has no world"));
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UE_LOG(LogGPDeath, Warning, TEXT("[DeathFX] TEST start world=%s pc=%s pawn=%s net=%d"),
		*GetNameSafe(World),
		*GetNameSafe(PC),
		*GetNameSafe(Pawn),
		static_cast<int32>(World->GetNetMode()));

	auto Kick = [World]()
	{
		APlayerController* LocalPC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* LocalPawn = LocalPC ? LocalPC->GetPawn() : nullptr;
		if (!LocalPC || !LocalPawn)
		{
			UE_LOG(LogGPDeath, Error, TEXT("[DeathFX] TEST abort: no local pawn yet"));
			return;
		}

		LocalPC->ClientSetHUD(AGalacticPiratesHUD::StaticClass());
		if (AGalacticPiratesHUD* HUD = Cast<AGalacticPiratesHUD>(LocalPC->GetHUD()))
		{
			HUD->SetVerboseDeathLog(true);
		}

		if (AGalacticPiratesCharacter* Crew = Cast<AGalacticPiratesCharacter>(LocalPawn))
		{
			UE_LOG(LogGPDeath, Warning, TEXT("[DeathFX] TEST DieInWreck crew=%s"), *Crew->GetName());
			Crew->DieInWreck(Crew->GetActorLocation() + Crew->GetActorForwardVector() * 250.0f);
		}
		else
		{
			UE_LOG(LogGPDeath, Warning, TEXT("[DeathFX] TEST pawn is not crew, calling HUD directly"));
			if (AGalacticPiratesHUD* HUD = Cast<AGalacticPiratesHUD>(LocalPC->GetHUD()))
			{
				HUD->BeginDeathPresentation();
			}
		}
	};

	FTimerHandle Delay;
	World->GetTimerManager().SetTimer(Delay, FTimerDelegate::CreateLambda(Kick), 2.0f, false);
	UE_LOG(LogGPDeath, Warning, TEXT("[DeathFX] TEST scheduled DieInWreck in 2.0s"));
}
