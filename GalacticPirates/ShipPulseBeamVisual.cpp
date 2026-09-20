#include "ShipPulseBeamVisual.h"
#include "ShipPolish.h"
#include "WalkableShip.h"
#include "GalacticPiratesCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "CollisionQueryParams.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

static void GPTintMesh(UStaticMeshComponent* Mesh, const TCHAR* TextureName, const FLinearColor& Color)
{
	GPApplyPolishVfxMaterial(Mesh, TextureName, Color);
}

AShipPulseBeamVisual::AShipPulseBeamVisual()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = false;

	BeamRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BeamRoot"));
	RootComponent = BeamRoot;
	BeamRoot->SetMobility(EComponentMobility::Movable);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(BeamRoot);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetCastShadow(false);
	VisualMesh->SetMobility(EComponentMobility::Movable);

	GlowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlowMesh"));
	GlowMesh->SetupAttachment(VisualMesh);
	GlowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GlowMesh->SetCastShadow(false);
	GlowMesh->SetMobility(EComponentMobility::Movable);

	SheathMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SheathMesh"));
	SheathMesh->SetupAttachment(VisualMesh);
	SheathMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SheathMesh->SetCastShadow(false);
	SheathMesh->SetMobility(EComponentMobility::Movable);

	CoreLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CoreLight"));
	CoreLight->SetupAttachment(BeamRoot);
	CoreLight->SetIntensity(120000.0f);
	CoreLight->SetAttenuationRadius(2800.0f);
	CoreLight->SetCastShadows(false);
	CoreLight->bUseInverseSquaredFalloff = false;

	ImpactLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ImpactLight"));
	ImpactLight->SetupAttachment(BeamRoot);
	ImpactLight->SetIntensity(90000.0f);
	ImpactLight->SetAttenuationRadius(2400.0f);
	ImpactLight->SetCastShadows(false);
	ImpactLight->bUseInverseSquaredFalloff = false;

	MidLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MidLight"));
	MidLight->SetupAttachment(BeamRoot);
	MidLight->SetIntensity(70000.0f);
	MidLight->SetAttenuationRadius(2600.0f);
	MidLight->SetCastShadows(false);
	MidLight->bUseInverseSquaredFalloff = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(CylinderMesh.Object);
		GlowMesh->SetStaticMesh(CylinderMesh.Object);
		SheathMesh->SetStaticMesh(CylinderMesh.Object);
	}
}

void AShipPulseBeamVisual::BeginPlay()
{
	Super::BeginPlay();
}

float AShipPulseBeamVisual::ComputeIntensity() const
{
	if (BeamDuration <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float FadeStart = BeamDuration * 0.72f;
	if (BeamElapsed <= 0.0f)
	{
		return 0.0f;
	}

	if (BeamElapsed < FadeStart)
	{
		const float T = FMath::Clamp(BeamElapsed / FadeStart, 0.0f, 1.0f);
		return FMath::InterpEaseIn(0.0f, 1.0f, T, 2.0f);
	}

	const float FadeT = FMath::Clamp((BeamElapsed - FadeStart) / FMath::Max(BeamDuration - FadeStart, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	return FMath::InterpEaseOut(1.0f, 0.0f, FadeT, 2.8f);
}

void AShipPulseBeamVisual::ApplyVisualIntensity(float Intensity)
{
	const float Clamped = FMath::Clamp(Intensity, 0.0f, 1.0f);
	const float Radial = (BeamThickness / 50.0f) * FMath::Lerp(0.22f, 1.7f, Clamped);
	if (VisualMesh)
	{
		VisualMesh->SetRelativeScale3D(FVector(Radial * 0.55f, Radial * 0.55f, BeamLength / 100.0f));
	}

	if (GlowMesh)
	{
		GlowMesh->SetRelativeScale3D(FVector(FMath::Lerp(1.4f, 2.2f, Clamped), FMath::Lerp(1.4f, 2.2f, Clamped), 1.0f));
	}
	if (SheathMesh)
	{
		SheathMesh->SetRelativeScale3D(FVector(FMath::Lerp(2.2f, 3.4f, Clamped), FMath::Lerp(2.2f, 3.4f, Clamped), 1.0f));
	}

	const float FlareScale = FMath::Lerp(0.4f, 1.8f, Clamped) * (BeamThickness / 50.0f);
	if (MuzzleFlare)
	{
		MuzzleFlare->SetRelativeScale3D(FVector(FlareScale));
	}
	if (ImpactBurst)
	{
		ImpactBurst->SetRelativeScale3D(FVector(FlareScale * 1.35f));
	}

	FLinearColor Hot = BeamColor * FMath::Lerp(0.25f, 2.4f, Clamped);
	Hot.A = 1.0f;
	GPTintMesh(VisualMesh, TEXT("muzzle_01"), Hot);
	GPTintMesh(GlowMesh, TEXT("magic_05"), Hot * FLinearColor(1.15f, 1.25f, 1.55f, 1.0f));
	GPTintMesh(SheathMesh, TEXT("flare_01"), Hot * FLinearColor(0.55f, 0.75f, 1.2f, 1.0f));
	GPTintMesh(MuzzleFlare, TEXT("muzzle_05"), Hot);
	GPTintMesh(ImpactBurst, TEXT("twirl_02"), FLinearColor(1.0f, 0.62f, 0.18f, 1.0f) * FMath::Lerp(0.25f, 2.2f, Clamped));

	const TCHAR* SparkNames[] = { TEXT("star_01"), TEXT("circle_05"), TEXT("light_01") };
	for (int32 Index = 0; Index < SparkMeshes.Num(); ++Index)
	{
		const float SparkScale = FMath::Lerp(0.8f, 2.4f, Clamped) * (BeamThickness / 70.0f);
		if (SparkMeshes[Index])
		{
			SparkMeshes[Index]->SetRelativeScale3D(FVector(SparkScale));
			GPTintMesh(SparkMeshes[Index], SparkNames[Index % 3], Hot);
		}
	}

	if (CoreLight)
	{
		CoreLight->SetLightColor(BeamColor);
		CoreLight->SetIntensity(140000.0f * Clamped);
		CoreLight->SetRelativeLocation(FVector(0.0f, 0.0f, -BeamLength * 0.5f));
	}

	if (ImpactLight)
	{
		ImpactLight->SetLightColor(FLinearColor(1.0f, 0.45f, 0.12f));
		ImpactLight->SetIntensity(110000.0f * Clamped);
		ImpactLight->SetRelativeLocation(FVector(0.0f, 0.0f, BeamLength * 0.5f));
	}

	if (MidLight)
	{
		MidLight->SetLightColor(BeamColor * 1.2f);
		MidLight->SetIntensity(90000.0f * Clamped);
		MidLight->SetRelativeLocation(FVector::ZeroVector);
	}
}

void AShipPulseBeamVisual::ApplyOverlapDamage(float DeltaTime, float Intensity)
{
	if (!SourceShip || !SourceShip->HasAuthority() || !GetWorld() || DamagePerSecond <= 0.0f || DamageRadius <= 0.0f)
	{
		return;
	}

	if (Intensity < 0.02f || DeltaTime <= 0.0f)
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShipPulseBeamOverlap), false, SourceShip);
	Params.AddIgnoredActor(this);
	for (AGalacticPiratesCharacter* Aboard : SourceShip->GetPlayersAboard())
	{
		Params.AddIgnoredActor(Aboard);
	}

	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByChannel(
		Hits,
		BeamStart,
		BeamEnd,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(DamageRadius),
		Params);

	TSet<AActor*> DamagedThisTick;
	AController* InstigatorController = InstigatorPawn ? InstigatorPawn->GetController() : nullptr;
	const float Amount = DamagePerSecond * DeltaTime;

	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor || HitActor == SourceShip || DamagedThisTick.Contains(HitActor))
		{
			continue;
		}

		if (AWalkableShip* HitShip = Cast<AWalkableShip>(HitActor))
		{
			if (HitShip->IsWrecked())
			{
				continue;
			}

			DamagedThisTick.Add(HitActor);
			HitShip->ApplyShipDamage(Amount, Cast<AGalacticPiratesCharacter>(InstigatorPawn), SourceShip);
			continue;
		}

		if (AGalacticPiratesCharacter* HitCharacter = Cast<AGalacticPiratesCharacter>(HitActor))
		{
			if (HitCharacter->GetBoardedShip() == SourceShip)
			{
				continue;
			}
		}

		DamagedThisTick.Add(HitActor);
		UGameplayStatics::ApplyDamage(HitActor, Amount, InstigatorController, SourceShip, UDamageType::StaticClass());
	}
}

void AShipPulseBeamVisual::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsBeam)
	{
		return;
	}

	BeamElapsed += DeltaTime;
	const float Intensity = ComputeIntensity();
	ApplyVisualIntensity(Intensity);
	ApplyOverlapDamage(DeltaTime, Intensity);

	if (BeamElapsed >= BeamDuration)
	{
		Destroy();
	}
}

void AShipPulseBeamVisual::InitializeBeam(const FVector& Start, const FVector& End, float Thickness, float Duration, const FLinearColor& Color, float InDamageRadius, float InDamagePerSecond, AWalkableShip* InSourceShip, APawn* InInstigatorPawn)
{
	bIsBeam = true;
	BeamStart = Start;
	BeamEnd = End;
	BeamThickness = Thickness;
	BeamDuration = FMath::Max(0.05f, Duration);
	BeamColor = Color;
	DamageRadius = InDamageRadius;
	DamagePerSecond = InDamagePerSecond;
	SourceShip = InSourceShip;
	InstigatorPawn = InInstigatorPawn;
	BeamElapsed = 0.0f;

	const FVector Delta = End - Start;
	BeamLength = FMath::Max(Delta.Size(), 1.0f);
	const FVector Dir = Delta.GetSafeNormal();

	SetActorLocation((Start + End) * 0.5f);
	SetActorRotation(FRotationMatrix::MakeFromZ(Dir).Rotator());
	SetActorScale3D(FVector::OneVector);

	if (UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
	{
		if (!MuzzleFlare)
		{
			MuzzleFlare = NewObject<UStaticMeshComponent>(this);
			MuzzleFlare->SetupAttachment(BeamRoot);
			MuzzleFlare->SetStaticMesh(SphereMesh);
			MuzzleFlare->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MuzzleFlare->SetCastShadow(false);
			MuzzleFlare->RegisterComponent();
		}

		if (!ImpactBurst)
		{
			ImpactBurst = NewObject<UStaticMeshComponent>(this);
			ImpactBurst->SetupAttachment(BeamRoot);
			ImpactBurst->SetStaticMesh(SphereMesh);
			ImpactBurst->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ImpactBurst->SetCastShadow(false);
			ImpactBurst->RegisterComponent();
		}
	}

	if (MuzzleFlare)
	{
		MuzzleFlare->SetRelativeLocation(FVector(0.0f, 0.0f, -BeamLength * 0.5f));
	}
	if (ImpactBurst)
	{
		ImpactBurst->SetRelativeLocation(FVector(0.0f, 0.0f, BeamLength * 0.5f));
	}

	if (SparkMeshes.Num() == 0)
	{
		if (UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
		{
			for (int32 Index = 0; Index < 7; ++Index)
			{
				UStaticMeshComponent* Spark = NewObject<UStaticMeshComponent>(this);
				Spark->SetupAttachment(BeamRoot);
				Spark->SetStaticMesh(PlaneMesh);
				Spark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Spark->SetCastShadow(false);
				Spark->SetRelativeRotation(FRotator(90.0f, 0.0f, 45.0f * Index));
				Spark->RegisterComponent();
				SparkMeshes.Add(Spark);
			}
		}
	}

	for (int32 Index = 0; Index < SparkMeshes.Num(); ++Index)
	{
		const float Alpha = (SparkMeshes.Num() == 1) ? 0.5f : static_cast<float>(Index) / static_cast<float>(SparkMeshes.Num() - 1);
		SparkMeshes[Index]->SetRelativeLocation(FVector(0.0f, 0.0f, FMath::Lerp(-BeamLength * 0.45f, BeamLength * 0.45f, Alpha)));
	}

	ApplyVisualIntensity(0.0f);
	SetActorTickEnabled(true);
	SetLifeSpan(BeamDuration + 0.05f);
}

void AShipPulseBeamVisual::InitializeExplosion(const FVector& Location, float Scale, float Duration, const FLinearColor& Color)
{
	bIsBeam = false;
	SetActorTickEnabled(false);

	if (UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
	{
		VisualMesh->SetStaticMesh(SphereMesh);
		if (GlowMesh)
		{
			GlowMesh->SetStaticMesh(SphereMesh);
			GlowMesh->SetRelativeScale3D(FVector(1.35f));
		}
	}

	SetActorLocation(Location);
	SetActorScale3D(FVector::OneVector);
	if (VisualMesh)
	{
		VisualMesh->SetRelativeScale3D(FVector(Scale / 50.0f));
	}

	GPTintMesh(VisualMesh, TEXT("VFX_Fireball"), Color);
	GPTintMesh(GlowMesh, TEXT("VFX_Blast"), FLinearColor(1.0f, 0.45f, 0.08f, 1.0f));
	if (SheathMesh)
	{
		GPTintMesh(SheathMesh, TEXT("VFX_Ember"), FLinearColor(1.0f, 0.35f, 0.05f, 1.0f));
		SheathMesh->SetRelativeScale3D(FVector(2.1f));
	}

	if (CoreLight)
	{
		CoreLight->SetLightColor(Color);
		CoreLight->SetAttenuationRadius(Scale * 4.0f);
		CoreLight->SetIntensity(180000.0f);
		CoreLight->SetRelativeLocation(FVector::ZeroVector);
	}

	if (ImpactLight)
	{
		ImpactLight->SetLightColor(FLinearColor(1.0f, 0.4f, 0.05f));
		ImpactLight->SetAttenuationRadius(Scale * 6.0f);
		ImpactLight->SetIntensity(140000.0f);
		ImpactLight->SetRelativeLocation(FVector(0.0f, 0.0f, Scale * 0.15f));
	}

	SetLifeSpan(FMath::Max(0.05f, Duration));
}
