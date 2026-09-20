#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GalacticPiratesHUD.generated.h"

class UTexture2D;

enum class EGPDeathFxPhase : uint8
{
	Idle,
	Ragdoll,
	Text,
	Static
};

inline constexpr float GPDeathTextAtSeconds = 4.0f;
inline constexpr float GPDeathStaticAtSeconds = 6.0f;

inline EGPDeathFxPhase GPDeathFxPhaseForAge(float AgeSeconds)
{
	if (AgeSeconds < 0.0f)
	{
		return EGPDeathFxPhase::Idle;
	}
	if (AgeSeconds < GPDeathTextAtSeconds)
	{
		return EGPDeathFxPhase::Ragdoll;
	}
	if (AgeSeconds < GPDeathStaticAtSeconds)
	{
		return EGPDeathFxPhase::Text;
	}
	return EGPDeathFxPhase::Static;
}

inline const TCHAR* GPDeathFxPhaseName(EGPDeathFxPhase Phase)
{
	switch (Phase)
	{
	case EGPDeathFxPhase::Ragdoll: return TEXT("Ragdoll");
	case EGPDeathFxPhase::Text: return TEXT("Text");
	case EGPDeathFxPhase::Static: return TEXT("Static");
	default: return TEXT("Idle");
	}
}

inline bool GPDeathFxWantsText(float AgeSeconds)
{
	return AgeSeconds >= GPDeathTextAtSeconds;
}

inline bool GPDeathFxWantsStatic(float AgeSeconds)
{
	return AgeSeconds >= GPDeathStaticAtSeconds;
}

UCLASS()
class GALACTICPIRATES_API AGalacticPiratesHUD : public AHUD
{
	GENERATED_BODY()

public:
	AGalacticPiratesHUD();

	void BeginDeathPresentation();
	void EndDeathPresentation();
	float GetDeathAgeSeconds() const;
	bool IsDeathPresentationActive() const { return bDeathPresentationActive; }
	EGPDeathFxPhase GetDeathPhase() const { return GPDeathFxPhaseForAge(GetDeathAgeSeconds()); }
	void SetVerboseDeathLog(bool bVerbose) { bVerboseDeathLog = bVerbose; }

protected:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

private:
	bool bDeathPresentationActive = false;
	bool bPlayedStaticSfx = false;
	bool bVerboseDeathLog = true;
	double DeathStartWallSeconds = 0.0;
	EGPDeathFxPhase LastLoggedPhase = EGPDeathFxPhase::Idle;
	float LastPeriodicLogAge = -1000.0f;

	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> StaticFrames;

	UPROPERTY()
	TObjectPtr<UTexture2D> ScanlineTexture;

	TSharedPtr<class SWidget> LostTextHost;
	TSharedPtr<class STextBlock> LostTextBlock;

	void LoadDeathTextures();
	void LogDeathFx(const TCHAR* Reason) const;
	void SetLostTextVisible(bool bVisible);
	void DrawFullScreenStatic(float Age, float Width, float Height);
};

void GPStartDeathPresentationTest(UWorld* World);
