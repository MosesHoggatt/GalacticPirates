#include "ShipMissileSalvoComponent.h"
#include "WalkableShip.h"
#include "MissileSalvoTerminalComponent.h"
#include "HeatseekingMissile.h"
#include "HolographicMapTableComponent.h"
#include "GalacticPiratesCharacter.h"
#include "ShipPolish.h"
#include "GalacticPirates.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"

UShipMissileSalvoComponent::UShipMissileSalvoComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UShipMissileSalvoComponent::BeginPlay()
{
	Super::BeginPlay();
	OwningShip = Cast<AWalkableShip>(GetOwner());
}

void UShipMissileSalvoComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UShipMissileSalvoComponent, CooldownRemaining);
	DOREPLIFETIME(UShipMissileSalvoComponent, SalvosFired);
	DOREPLIFETIME(UShipMissileSalvoComponent, MissilesInFlight);
}

void UShipMissileSalvoComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (CooldownRemaining > 0.0f)
	{
		CooldownRemaining = FMath::Max(0.0f, CooldownRemaining - DeltaTime);
	}

	AdvanceSalvo(DeltaTime);
}

void UShipMissileSalvoComponent::AdvanceSalvo(float DeltaTime)
{
	if (PendingLaunches <= 0)
	{
		return;
	}

	StaggerTimer -= DeltaTime;
	if (StaggerTimer > 0.0f)
	{
		return;
	}

	const int32 Index = MissilesPerSalvo - PendingLaunches;
	LaunchOneMissile(Index, PendingTarget.Get(), PendingOperator.Get());
	PendingLaunches--;
	StaggerTimer = StaggerSeconds;
	if (PendingLaunches <= 0)
	{
		PendingTarget = nullptr;
		PendingOperator = nullptr;
	}
}

bool UShipMissileSalvoComponent::CanFire() const
{
	if (!OwningShip || OwningShip->IsWrecked())
	{
		return false;
	}
	return CooldownRemaining <= KINDA_SMALL_NUMBER && PendingLaunches <= 0;
}

float UShipMissileSalvoComponent::GetRechargeAlpha() const
{
	if (RechargeTime <= KINDA_SMALL_NUMBER)
	{
		return 1.0f;
	}
	return 1.0f - FMath::Clamp(CooldownRemaining / RechargeTime, 0.0f, 1.0f);
}

FVector UShipMissileSalvoComponent::GetMuzzleWorldLocation() const
{
	return GetComponentTransform().TransformPosition(MuzzleOffset);
}

AWalkableShip* UShipMissileSalvoComponent::ResolveSalvoTarget() const
{
	if (OwningShip && OwningShip->MapTable)
	{
		if (AWalkableShip* Mapped = Cast<AWalkableShip>(OwningShip->MapTable->GetBestAutoTarget()))
		{
			return Mapped;
		}
	}

	if (!GetWorld())
	{
		return nullptr;
	}

	AWalkableShip* Best = nullptr;
	float BestScore = 0.0f;
	const FVector Origin = GetMuzzleWorldLocation();
	const FVector Forward = GetComponentQuat().GetForwardVector();
	for (TActorIterator<AWalkableShip> It(GetWorld()); It; ++It)
	{
		AWalkableShip* Ship = *It;
		if (!Ship || Ship == OwningShip || Ship->IsWrecked())
		{
			continue;
		}
		const float Heat = Ship->GetHealth() + 200.0f;
		const float Score = GPComputeMissileHeatScore(Origin, Forward, Ship->GetActorLocation(), Heat, 0.15f);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Ship;
		}
	}
	return Best;
}

bool UShipMissileSalvoComponent::Fire(AGalacticPiratesCharacter* Operator)
{
	return FireInternal(Operator, true);
}

bool UShipMissileSalvoComponent::FireIgnoringTerminalRange(AGalacticPiratesCharacter* Operator)
{
	return FireInternal(Operator, false);
}

bool UShipMissileSalvoComponent::FireInternal(AGalacticPiratesCharacter* Operator, bool bRequireTerminalRange)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld())
	{
		return false;
	}

	OwningShip = Cast<AWalkableShip>(GetOwner());
	if (!CanFire())
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[MissileSalvo] Fire denied on %s cooldown=%.2f pending=%d"),
			*GetNameSafe(OwningShip), CooldownRemaining, PendingLaunches);
		return false;
	}

	if (Operator)
	{
		if (Operator->GetBoardedShip() != OwningShip)
		{
			return false;
		}
		if (bRequireTerminalRange && OwningShip->MissileTerminal && !OwningShip->MissileTerminal->IsCharacterInRange(Operator))
		{
			UE_LOG(LogGalacticPirates, Warning, TEXT("[MissileSalvo] Operator %s is not at the missile terminal"),
				*GetNameSafe(Operator));
			return false;
		}
	}

	PendingTarget = ResolveSalvoTarget();
	PendingOperator = Operator;
	PendingLaunches = FMath::Max(1, MissilesPerSalvo);
	StaggerTimer = 0.0f;
	SalvosFired++;
	CooldownRemaining = RechargeTime;
	MissilesInFlight += PendingLaunches;

	UE_LOG(LogGalacticPirates, Warning, TEXT("[MissileSalvo] Salvo start from %s by %s count=%d target=%s"),
		*GetNameSafe(OwningShip),
		*GetNameSafe(Operator),
		PendingLaunches,
		*GetNameSafe(PendingTarget.Get()));

	return true;
}

void UShipMissileSalvoComponent::LaunchOneMissile(int32 IndexInSalvo, AWalkableShip* Target, AGalacticPiratesCharacter* Operator)
{
	UWorld* World = GetWorld();
	if (!World || !OwningShip)
	{
		return;
	}

	const float YawOffset = (IndexInSalvo - (MissilesPerSalvo - 1) * 0.5f) * SpreadYawDegrees;
	const FRotator LaunchRot = GetComponentRotation() + FRotator(4.0f, YawOffset, 0.0f);
	const FVector LaunchLoc = GetMuzzleWorldLocation() + GetComponentQuat().GetRightVector() * (YawOffset * 8.0f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = OwningShip;
	Params.Instigator = Operator;
	AHeatseekingMissile* Missile = World->SpawnActor<AHeatseekingMissile>(LaunchLoc, LaunchRot, Params);
	if (!Missile)
	{
		MissilesInFlight = FMath::Max(0, MissilesInFlight - 1);
		return;
	}

	Missile->InitializeMissile(OwningShip, Target, Operator, MissileDamage, LaunchSpeed);
	if (GetNetMode() != NM_DedicatedServer)
	{
		GPPlayPolishSoundAt(this, TEXT("SFX_PulseFire"), LaunchLoc, 0.55f);
	}

	UE_LOG(LogGalacticPirates, Warning, TEXT("[MissileSalvo] Launched %d/%d loc=%s tgt=%s"),
		IndexInSalvo + 1,
		MissilesPerSalvo,
		*LaunchLoc.ToCompactString(),
		*GetNameSafe(Target));
}
