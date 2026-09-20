#include "ShipMissileSalvoComponent.h"
#include "WalkableShip.h"
#include "MissileSalvoTerminalComponent.h"
#include "HeatseekingMissile.h"
#include "HolographicMapTableComponent.h"
#include "GalacticPiratesCharacter.h"
#include "ShipPolish.h"
#include "GalacticPirates.h"
#include "SpaceCraft.h"
#include "HullHealthComponent.h"
#include "ShipMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"

UShipMissileSalvoComponent::UShipMissileSalvoComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	LaunchSpeed = 5760.0f;
}

void UShipMissileSalvoComponent::BeginPlay()
{
	Super::BeginPlay();
	OwningCraft = GetOwner();
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
	if (!OwningCraft || GPIsCraftWrecked(OwningCraft))
	{
		return false;
	}
	return CooldownRemaining <= KINDA_SMALL_NUMBER && PendingLaunches <= 0;
}

bool UShipMissileSalvoComponent::CanFireWeapon() const
{
	return CanFire();
}

bool UShipMissileSalvoComponent::TryFireWeapon(APawn* InstigatorPawn)
{
	return Fire(Cast<AGalacticPiratesCharacter>(InstigatorPawn));
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

AActor* UShipMissileSalvoComponent::ResolveSalvoTarget() const
{
	AWalkableShip* OwningShip = Cast<AWalkableShip>(OwningCraft);
	if (OwningShip && OwningShip->MapTable)
	{
		if (AActor* Mapped = OwningShip->MapTable->GetBestAutoTarget())
		{
			return Mapped;
		}
	}

	if (!GetWorld())
	{
		return nullptr;
	}

	AActor* Best = nullptr;
	float BestScore = 0.0f;
	const FVector Origin = GetMuzzleWorldLocation();
	const FVector Forward = GetComponentQuat().GetForwardVector();
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Pawn = *It;
		ISpaceCraft* Craft = GPAsSpaceCraft(Pawn);
		if (!Pawn || Pawn == OwningCraft || !Craft || Craft->IsCraftWrecked())
		{
			continue;
		}
		const float Health = Craft->GetHullHealth() ? Craft->GetHullHealth()->GetHealth() : 0.0f;
		const float Heat = Health + 200.0f;
		const float Score = GPComputeMissileHeatScore(Origin, Forward, Pawn->GetActorLocation(), Heat, 0.15f);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Pawn;
		}
	}
	return Best;
}

bool UShipMissileSalvoComponent::Fire(AGalacticPiratesCharacter* Operator)
{
	return FireInternal(Operator, true, nullptr);
}

bool UShipMissileSalvoComponent::FireIgnoringTerminalRange(AGalacticPiratesCharacter* Operator)
{
	return FireInternal(Operator, false, nullptr);
}

bool UShipMissileSalvoComponent::FireAt(AActor* Target, APawn* InstigatorPawn)
{
	return FireInternal(InstigatorPawn, false, Target);
}

bool UShipMissileSalvoComponent::FireInternal(APawn* Operator, bool bRequireTerminalRange, AActor* ForcedTarget)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld())
	{
		return false;
	}

	OwningCraft = GetOwner();
	if (!CanFire())
	{
		if (GPCombatLogEnabled())
		{
			UE_LOG(LogGalacticPirates, Log, TEXT("[MissileSalvo] Fire denied on %s cooldown=%.2f pending=%d"),
				*GetNameSafe(OwningCraft), CooldownRemaining, PendingLaunches);
		}
		return false;
	}

	AWalkableShip* OwningShip = Cast<AWalkableShip>(OwningCraft);
	if (AGalacticPiratesCharacter* Character = Cast<AGalacticPiratesCharacter>(Operator))
	{
		if (OwningShip && Character->GetBoardedShip() != OwningShip)
		{
			return false;
		}
		if (bRequireTerminalRange && OwningShip && OwningShip->MissileTerminal && !OwningShip->MissileTerminal->IsCharacterInRange(Character))
		{
			if (GPCombatLogEnabled())
			{
				UE_LOG(LogGalacticPirates, Log, TEXT("[MissileSalvo] Operator %s is not at the missile terminal"),
					*GetNameSafe(Character));
			}
			return false;
		}
	}

	PendingTarget = ForcedTarget ? ForcedTarget : ResolveSalvoTarget();
	PendingOperator = Operator;
	PendingLaunches = FMath::Max(1, MissilesPerSalvo);
	StaggerTimer = 0.0f;
	SalvosFired++;
	CooldownRemaining = RechargeTime;
	MissilesInFlight += PendingLaunches;

	if (GPCombatLogEnabled())
	{
		UE_LOG(LogGalacticPirates, Log, TEXT("[MissileSalvo] Salvo start from %s by %s count=%d target=%s"),
			*GetNameSafe(OwningCraft),
			*GetNameSafe(Operator),
			PendingLaunches,
			*GetNameSafe(PendingTarget.Get()));
	}

	return true;
}

void UShipMissileSalvoComponent::LaunchOneMissile(int32 IndexInSalvo, AActor* Target, APawn* Operator)
{
	UWorld* World = GetWorld();
	if (!World || !OwningCraft)
	{
		return;
	}

	const float YawOffset = (IndexInSalvo - (MissilesPerSalvo - 1) * 0.5f) * SpreadYawDegrees;
	FRotator LaunchRot = GetComponentRotation() + FRotator(4.0f, YawOffset, 0.0f);
	FVector LaunchLoc = GetMuzzleWorldLocation() + GetComponentQuat().GetRightVector() * (YawOffset * 8.0f);
	if (bLaunchAlongOwnerForward)
	{
		LaunchRot = OwningCraft->GetActorRotation();
		LaunchLoc = OwningCraft->GetActorTransform().TransformPosition(MuzzleOffset);
	}

	FVector Inherited = FVector::ZeroVector;
	if (bInheritOwnerVelocity)
	{
		if (ISpaceCraft* Craft = GPAsSpaceCraft(OwningCraft))
		{
			Inherited = Craft->GetCraftVelocity();
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = OwningCraft;
	Params.Instigator = Operator;
	AHeatseekingMissile* Missile = World->SpawnActor<AHeatseekingMissile>(LaunchLoc, LaunchRot, Params);
	if (!Missile)
	{
		MissilesInFlight = FMath::Max(0, MissilesInFlight - 1);
		return;
	}

	Missile->InitializeMissile(OwningCraft, Target, Operator, MissileDamage, LaunchSpeed, Inherited);
	if (GetNetMode() != NM_DedicatedServer)
	{
		GPPlayPolishSoundAt(this, TEXT("SFX_PulseFire"), LaunchLoc, 0.55f);
	}

	if (GPCombatLogEnabled())
	{
		UE_LOG(LogGalacticPirates, Log, TEXT("[MissileSalvo] Launched %d/%d loc=%s tgt=%s"),
			IndexInSalvo + 1,
			MissilesPerSalvo,
			*LaunchLoc.ToCompactString(),
			*GetNameSafe(Target));
	}
}
