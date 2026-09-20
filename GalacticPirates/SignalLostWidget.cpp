#include "SignalLostWidget.h"
#include "ShipPolish.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"

USignalLostWidget::USignalLostWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FSlateFontInfo USignalLostWidget::MakeLostFont(int32 Size) const
{
	const FString Orbitron = FPaths::ProjectContentDir() / TEXT("Polish/Fonts/Orbitron.ttf");
	const FString Mono = FPaths::ProjectContentDir() / TEXT("Polish/Fonts/ShareTechMono-Regular.ttf");
	FSlateFontInfo Font;
	if (FPaths::FileExists(Orbitron))
	{
		Font = FSlateFontInfo(Orbitron, Size);
	}
	else if (FPaths::FileExists(Mono))
	{
		Font = FSlateFontInfo(Mono, Size);
	}
	else
	{
		Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size);
	}
	Font.Size = Size;
	Font.OutlineSettings.OutlineSize = 2;
	Font.OutlineSettings.OutlineColor = FLinearColor(0.02f, 0.12f, 0.28f, 0.85f);
	return Font;
}

TSharedRef<SWidget> USignalLostWidget::RebuildWidget()
{
	NoiseFrames.Reset();
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const FString Name = FString::Printf(TEXT("T_TvStatic_%02d"), Index);
		UTexture2D* NoiseTex = GPLoadPolishTexture(*Name);
		if (!NoiseTex)
		{
			NoiseTex = GPLoadPolishTexture(TEXT("T_TvStatic"));
		}

		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.Tiling = ESlateBrushTileType::Both;
		Brush.ImageSize = FVector2D(256.0f, 256.0f);
		Brush.TintColor = FSlateColor(FLinearColor(0.35f, 0.82f, 1.0f, 0.0f));
		if (NoiseTex)
		{
			Brush.SetResourceObject(NoiseTex);
		}
		else
		{
			Brush = *FCoreStyle::Get().GetBrush("WhiteBrush");
			Brush.TintColor = FSlateColor(FLinearColor(0.08f, 0.28f, 0.55f, 0.0f));
		}
		NoiseFrames.Add(Brush);
	}

	UTexture2D* ScanTex = GPLoadPolishTexture(TEXT("T_Scanlines"));
	ScanBrush = FSlateBrush();
	ScanBrush.DrawAs = ESlateBrushDrawType::Image;
	ScanBrush.Tiling = ESlateBrushTileType::Both;
	ScanBrush.ImageSize = FVector2D(8.0f, 16.0f);
	ScanBrush.TintColor = FSlateColor(FLinearColor(0.4f, 0.9f, 1.0f, 0.0f));
	if (ScanTex)
	{
		ScanBrush.SetResourceObject(ScanTex);
	}

	HiddenBrush = FSlateBrush();
	HiddenBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	HiddenBrush.TintColor = FSlateColor(FLinearColor::Transparent);

	NoiseA = SNew(SImage).Image(&HiddenBrush).ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f)).Visibility(EVisibility::Collapsed);
	NoiseB = SNew(SImage).Image(&HiddenBrush).ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f)).Visibility(EVisibility::Collapsed);
	Scanlines = SNew(SImage).Image(&HiddenBrush).ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f)).Visibility(EVisibility::Collapsed);

	LostText = SNew(STextBlock)
		.Font(MakeLostFont(72))
		.ColorAndOpacity(FLinearColor(0.45f, 0.92f, 1.0f, 1.0f))
		.Visibility(EVisibility::HitTestInvisible)
		.ShadowOffset(FVector2D(3.0f, 3.0f))
		.ShadowColorAndOpacity(FLinearColor(0.0f, 0.08f, 0.2f, 0.9f))
		.Justification(ETextJustify::Center)
		.Text(FText::FromString(TEXT("SIGNAL LOST")));

	Blackout = SNew(SBorder)
		.BorderImage(&HiddenBrush)
		.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.02f, 0.0f))
		.Visibility(EVisibility::Collapsed);

	RootLayer = SNew(SBorder)
		.BorderImage(&HiddenBrush)
		.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f))
		.Padding(0.0f)
		.Visibility(EVisibility::HitTestInvisible)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SScaleBox).Stretch(EStretch::ScaleToFill)
				[ NoiseA.ToSharedRef() ]
			]
			+ SOverlay::Slot()
			[
				SNew(SScaleBox).Stretch(EStretch::ScaleToFill)
				[ NoiseB.ToSharedRef() ]
			]
			+ SOverlay::Slot()
			[
				SNew(SScaleBox).Stretch(EStretch::ScaleToFill)
				[ Scanlines.ToSharedRef() ]
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				LostText.ToSharedRef()
			]
			+ SOverlay::Slot()
			[
				Blackout.ToSharedRef()
			]
		];

	return RootLayer.ToSharedRef();
}

void USignalLostWidget::BeginSequence()
{
	Elapsed = 0.0f;
	LastStaticAlpha = 0.0f;
	LastTextAlpha = 0.0f;
	bPlayedStaticSfx = false;
	FrameIndex = 0;
	FrameTimer = 0.0f;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	ApplyLayerOpacity(0.0f, 1.0f, 0.0f);
}

void USignalLostWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USignalLostWidget::ApplyLayerOpacity(float StaticAlpha, float TextAlpha, float BlackAlpha)
{
	LastStaticAlpha = StaticAlpha;
	LastTextAlpha = TextAlpha;

	if (RootLayer.IsValid())
	{
		RootLayer->SetVisibility(EVisibility::HitTestInvisible);
		RootLayer->SetBorderImage(&HiddenBrush);
		RootLayer->SetBorderBackgroundColor(FLinearColor(0.01f, 0.06f, 0.14f, StaticAlpha * 0.4f));
	}

	const EVisibility StaticVis = StaticAlpha > 0.01f ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
	if (NoiseA.IsValid())
	{
		if (StaticAlpha > 0.01f && NoiseFrames.IsValidIndex(FrameIndex))
		{
			NoiseFrames[FrameIndex].TintColor = FSlateColor(FLinearColor(0.35f, 0.82f, 1.0f, StaticAlpha));
			NoiseA->SetImage(&NoiseFrames[FrameIndex]);
		}
		else
		{
			NoiseA->SetImage(&HiddenBrush);
		}
		NoiseA->SetColorAndOpacity(FLinearColor(0.4f, 0.85f, 1.0f, StaticAlpha * 0.85f));
		NoiseA->SetVisibility(StaticVis);
	}
	if (NoiseB.IsValid())
	{
		const int32 Alt = NoiseFrames.Num() > 0 ? (FrameIndex + 3) % NoiseFrames.Num() : 0;
		if (StaticAlpha > 0.01f && NoiseFrames.IsValidIndex(Alt))
		{
			NoiseFrames[Alt].TintColor = FSlateColor(FLinearColor(0.2f, 0.6f, 1.0f, StaticAlpha * 0.55f));
			NoiseB->SetImage(&NoiseFrames[Alt]);
		}
		else
		{
			NoiseB->SetImage(&HiddenBrush);
		}
		NoiseB->SetColorAndOpacity(FLinearColor(0.15f, 0.55f, 1.0f, StaticAlpha * 0.45f));
		NoiseB->SetVisibility(StaticVis);
	}
	if (Scanlines.IsValid())
	{
		if (StaticAlpha > 0.01f)
		{
			ScanBrush.TintColor = FSlateColor(FLinearColor(0.4f, 0.9f, 1.0f, StaticAlpha));
			Scanlines->SetImage(&ScanBrush);
		}
		else
		{
			Scanlines->SetImage(&HiddenBrush);
		}
		Scanlines->SetColorAndOpacity(FLinearColor(0.45f, 0.95f, 1.0f, StaticAlpha * 0.55f));
		Scanlines->SetVisibility(StaticVis);
	}
	if (LostText.IsValid())
	{
		LostText->SetColorAndOpacity(FLinearColor(0.55f, 0.95f, 1.0f, TextAlpha));
		LostText->SetVisibility(TextAlpha > 0.01f ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	}
	if (Blackout.IsValid())
	{
		if (BlackAlpha > 0.01f)
		{
			Blackout->SetBorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"));
			Blackout->SetBorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.02f, BlackAlpha));
			Blackout->SetVisibility(EVisibility::HitTestInvisible);
		}
		else
		{
			Blackout->SetBorderImage(&HiddenBrush);
			Blackout->SetBorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.02f, 0.0f));
			Blackout->SetVisibility(EVisibility::Collapsed);
		}
	}
}

void USignalLostWidget::AdvanceStaticAnimation(float Age, float DeltaTime, float TextAlpha)
{
	FrameTimer += DeltaTime;
	if (FrameTimer >= (1.0f / 24.0f) && NoiseFrames.Num() > 0)
	{
		FrameTimer = 0.0f;
		FrameIndex = (FrameIndex + 1) % NoiseFrames.Num();
		const int32 Alt = (FrameIndex + 3) % NoiseFrames.Num();
		if (NoiseA.IsValid())
		{
			NoiseA->SetImage(&NoiseFrames[FrameIndex]);
		}
		if (NoiseB.IsValid())
		{
			NoiseB->SetImage(&NoiseFrames[Alt]);
		}
	}

	const float JitterX = FMath::Sin(Age * 73.0f) * 48.0f + FMath::Cos(Age * 19.0f) * 22.0f;
	const float JitterY = FMath::Cos(Age * 61.0f) * 40.0f + FMath::Sin(Age * 27.0f) * 18.0f;
	if (NoiseA.IsValid())
	{
		NoiseA->SetRenderTransform(FSlateRenderTransform(FVector2D(JitterX, JitterY)));
		NoiseA->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}
	if (NoiseB.IsValid())
	{
		NoiseB->SetRenderTransform(FSlateRenderTransform(FVector2D(-JitterY * 0.7f, JitterX * 0.6f)));
	}
	if (Scanlines.IsValid())
	{
		const float Scroll = FMath::Fmod(Age * 420.0f, 64.0f);
		Scanlines->SetRenderTransform(FSlateRenderTransform(FVector2D(0.0f, Scroll)));
	}

	if (LostText.IsValid() && TextAlpha > 0.05f)
	{
		const float Shake = (FMath::Frac(Age * 18.0f) > 0.86f) ? 6.0f : 0.0f;
		LostText->SetRenderTransform(FSlateRenderTransform(FVector2D(Shake * FMath::Sin(Age * 90.0f), Shake * 0.4f)));
	}
}

void USignalLostWidget::TickSequence(float AbsoluteAge)
{
	Elapsed = AbsoluteAge;
	float StaticAlpha = 0.0f;
	float TextAlpha = 0.0f;
	float BlackAlpha = 0.0f;

	if (Elapsed >= TextAtSeconds)
	{
		TextAlpha = 1.0f;
	}

	if (Elapsed >= StaticAtSeconds)
	{
		if (!bPlayedStaticSfx)
		{
			bPlayedStaticSfx = true;
			GPPlayPolishSound2D(this, TEXT("SFX_SignalLost"), 0.85f);
		}

		StaticAlpha = FMath::Clamp((Elapsed - StaticAtSeconds) / OverlayFadeSeconds, 0.0f, 1.0f);
		AdvanceStaticAnimation(Elapsed, 0.016f, TextAlpha);

		if (Elapsed >= OverlayBlinkAtSeconds)
		{
			const int32 Cycle = FMath::FloorToInt((Elapsed - OverlayBlinkAtSeconds) / 0.12f);
			if (Cycle >= 8)
			{
				TextAlpha = 0.0f;
				StaticAlpha = 0.0f;
				BlackAlpha = 1.0f;
			}
			else if ((Cycle % 2) == 1)
			{
				TextAlpha = 0.0f;
				StaticAlpha *= 0.2f;
			}
		}
	}

	ApplyLayerOpacity(StaticAlpha, TextAlpha, BlackAlpha);
}

void USignalLostWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
}
