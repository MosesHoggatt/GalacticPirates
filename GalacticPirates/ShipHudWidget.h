#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShipHudWidget.generated.h"

class STextBlock;
class SProgressBar;
class SWidget;
class SBorder;

UCLASS()
class GALACTICPIRATES_API UShipHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	TSharedPtr<STextBlock> PromptText;
	TSharedPtr<STextBlock> HealthLabel;
	TSharedPtr<STextBlock> RechargeLabel;
	TSharedPtr<STextBlock> MissileLabel;
	TSharedPtr<SProgressBar> HealthBar;
	TSharedPtr<SProgressBar> RechargeBar;
	TSharedPtr<SProgressBar> MissileBar;
	TSharedPtr<SWidget> Crosshair;
	TArray<TSharedPtr<SBorder>> CrosshairStrokes;

	FSlateFontInfo MakeFont(int32 Size, bool bBold) const;
	TSharedRef<SWidget> BuildCrosshair();
	void RefreshFromPawn();
};
