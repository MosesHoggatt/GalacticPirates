#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SignalLostWidget.generated.h"

class SImage;
class STextBlock;
class SBorder;

UCLASS()
class GALACTICPIRATES_API USignalLostWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USignalLostWidget(const FObjectInitializer& ObjectInitializer);

	static constexpr float TextAtSeconds = 4.0f;
	static constexpr float StaticAtSeconds = 6.0f;
	static constexpr float OverlayFadeSeconds = 0.85f;
	static constexpr float OverlayBlinkAtSeconds = 8.2f;

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void BeginSequence();
	void TickSequence(float AbsoluteAge);
	float GetElapsed() const { return Elapsed; }
	bool IsTextShowing() const { return LastTextAlpha > 0.01f; }
	bool IsStaticShowing() const { return LastStaticAlpha > 0.01f; }

private:
	TSharedPtr<SBorder> RootLayer;
	TSharedPtr<SImage> NoiseA;
	TSharedPtr<SImage> NoiseB;
	TSharedPtr<SImage> Scanlines;
	TSharedPtr<STextBlock> LostText;
	TSharedPtr<SBorder> Blackout;

	TArray<FSlateBrush> NoiseFrames;
	FSlateBrush ScanBrush;
	FSlateBrush HiddenBrush;
	int32 FrameIndex = 0;
	float FrameTimer = 0.0f;
	float Elapsed = 0.0f;
	float LastStaticAlpha = 0.0f;
	float LastTextAlpha = 0.0f;
	bool bPlayedStaticSfx = false;

	FSlateFontInfo MakeLostFont(int32 Size) const;
	void ApplyLayerOpacity(float StaticAlpha, float TextAlpha, float BlackAlpha);
	void AdvanceStaticAnimation(float Age, float DeltaTime, float TextAlpha);
};
