#include "ShipCrewAiComponent.h"
#include "WalkableShip.h"
#include "GalacticPiratesCharacter.h"
#include "HelmComponent.h"
#include "MinigunPodComponent.h"
#include "ShipMissileSalvoComponent.h"
#include "MissileSalvoTerminalComponent.h"
#include "HoloMapPoiComponent.h"
#include "HoloMapTypes.h"
#include "SpaceCraft.h"
#include "GalacticPirates.h"
#include "OccupancyComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

UShipCrewAiComponent::UShipCrewAiComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UShipCrewAiComponent::BeginPlay()
{
	Super::BeginPlay();
	OwningShip = Cast<AWalkableShip>(GetOwner());
	if (!OwningShip || !OwningShip->HasAuthority() || !bEnabled)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &UShipCrewAiComponent::SpawnCrew);
	}
}

void UShipCrewAiComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyCrew();
	Super::EndPlay(EndPlayReason);
}

int32 UShipCrewAiComponent::GetCrewCount() const
{
	int32 Count = 0;
	if (IsValid(Pilot)) { ++Count; }
	if (IsValid(SalvoOperator)) { ++Count; }
	if (IsValid(PortGunner)) { ++Count; }
	if (IsValid(StarboardGunner)) { ++Count; }
	return Count;
}

bool UShipCrewAiComponent::ShouldCrewThisShip() const
{
	if (!OwningShip || OwningShip->IsWrecked() || !OwningShip->HasAuthority())
	{
		return false;
	}
	if (OwningShip->HasHumanCrew())
	{
		return false;
	}
	if (OwningShip->HoloPoi && OwningShip->HoloPoi->Kind == EHoloMapPoiKind::OwnShip)
	{
		return false;
	}
	if (OwningShip->HoloPoi && OwningShip->HoloPoi->Kind == EHoloMapPoiKind::FriendlyShip)
	{
		return false;
	}
	return true;
}

void UShipCrewAiComponent::SpawnCrew()
{
	if (bCrewSpawned || !ShouldCrewThisShip())
	{
		return;
	}

	bCrewSpawned = true;
	Pilot = SpawnCrewMember(TEXT("AiPilot"));
	SalvoOperator = SpawnCrewMember(TEXT("AiSalvo"));
	PortGunner = SpawnCrewMember(TEXT("AiPortGun"));
	StarboardGunner = SpawnCrewMember(TEXT("AiStarboardGun"));
	SeatCrew();
	UE_LOG(LogGalacticPirates, Warning, TEXT("[CrewAI] Spawned %d crew on %s"), GetCrewCount(), *GetNameSafe(OwningShip));
}

AGalacticPiratesCharacter* UShipCrewAiComponent::SpawnCrewMember(const TCHAR* Name)
{
	UWorld* World = GetWorld();
	if (!World || !OwningShip)
	{
		return nullptr;
	}

	UClass* CharacterClass = LoadClass<AGalacticPiratesCharacter>(nullptr,
		TEXT("/Game/_Template/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
	if (!CharacterClass)
	{
		UE_LOG(LogGalacticPirates, Error, TEXT("[CrewAI] Missing BP_FirstPersonCharacter"));
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.Name = MakeUniqueObjectName(World, CharacterClass, Name);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = OwningShip;
	AGalacticPiratesCharacter* Crew = World->SpawnActor<AGalacticPiratesCharacter>(
		CharacterClass, OwningShip->GetSpawnTransform(), Params);
	if (!Crew)
	{
		return nullptr;
	}

	Crew->AutoPossessAI = EAutoPossessAI::Disabled;
	Crew->SetAiCrew(true);
	if (AController* Existing = Crew->GetController())
	{
		Existing->UnPossess();
		Existing->Destroy();
	}
	Crew->BoardShip(OwningShip);
	if (UCharacterMovementComponent* Move = Crew->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
	}
	return Crew;
}

void UShipCrewAiComponent::SeatCrew()
{
	if (!OwningShip)
	{
		return;
	}

	if (Pilot && OwningShip->Helm)
	{
		Pilot->SetActorLocation(OwningShip->HelmOccupancy ? OwningShip->HelmOccupancy->GetComponentLocation() : OwningShip->Helm->GetComponentLocation());
		OwningShip->Helm->TryInteract(Pilot);
	}

	if (SalvoOperator && OwningShip->MissileTerminal)
	{
		SalvoOperator->SetActorLocation(OwningShip->MissileTerminal->GetComponentLocation()
			+ OwningShip->GetActorForwardVector() * -40.0f);
	}

	if (PortGunner && OwningShip->PortMinigun)
	{
		PortGunner->SetActorLocation(OwningShip->PortMinigun->GetComponentLocation());
	}

	if (StarboardGunner && OwningShip->StarboardMinigun)
	{
		StarboardGunner->SetActorLocation(OwningShip->StarboardMinigun->GetComponentLocation());
	}
}

AActor* UShipCrewAiComponent::FindAttackTarget() const
{
	if (!OwningShip)
	{
		return nullptr;
	}

	AActor* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Candidate = *It;
		ISpaceCraft* Craft = GPAsSpaceCraft(Candidate);
		if (!Candidate || Candidate == OwningShip || !Craft || Craft->IsCraftWrecked() || !Craft->HasHumanOccupant())
		{
			continue;
		}
		if (!GPAreHostile(OwningShip, Candidate))
		{
			continue;
		}

		const float Dist = FVector::Dist(OwningShip->GetActorLocation(), Candidate->GetActorLocation());
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = Candidate;
		}
	}
	return Best;
}

bool UShipCrewAiComponent::CanEngageWithMinigun(const UMinigunPodComponent* Pod, AActor* Target) const
{
	if (!Pod || !Target)
	{
		return false;
	}

	const FVector AimPoint = GPAsSpaceCraft(Target) && GPAsSpaceCraft(Target)->GetHomingSceneComponent()
		? GPAsSpaceCraft(Target)->GetHomingSceneComponent()->GetComponentLocation()
		: Target->GetActorLocation();
	const FVector Muzzle = Pod->GetMuzzleLocation();
	const FVector WorldDir = (AimPoint - Muzzle).GetSafeNormal();
	if (WorldDir.IsNearlyZero())
	{
		return false;
	}

	const FVector LocalDir = Pod->GetComponentTransform().InverseTransformVectorNoScale(WorldDir).GetSafeNormal();
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(LocalDir.Y, LocalDir.X));
	const float Horizontal = FMath::Sqrt(LocalDir.X * LocalDir.X + LocalDir.Y * LocalDir.Y);
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(LocalDir.Z, Horizontal));
	if (Yaw < Pod->YawMin || Yaw > Pod->YawMax || Pitch < Pod->PitchMin || Pitch > Pod->PitchMax)
	{
		return false;
	}

	const float Dist = FVector::Dist(Muzzle, AimPoint);
	return Dist < Pod->TraceRange && HasGunLineOfSight(Pod, Target);
}

bool UShipCrewAiComponent::UpdateMinigun(UMinigunPodComponent* Pod, AActor* Target, bool bAllowFire)
{
	if (!Pod)
	{
		return false;
	}

	if (!bAllowFire || !Pod->GetGunner() || !Target)
	{
		Pod->SetFiring(false);
		return false;
	}

	const FVector AimPoint = GPAsSpaceCraft(Target) && GPAsSpaceCraft(Target)->GetHomingSceneComponent()
		? GPAsSpaceCraft(Target)->GetHomingSceneComponent()->GetComponentLocation()
		: Target->GetActorLocation();
	Pod->AimAtWorldLocation(AimPoint);
	const bool bCanFire = CanEngageWithMinigun(Pod, Target);
	Pod->SetFiring(bCanFire);
	return bCanFire;
}

void UShipCrewAiComponent::SetGunManned(UMinigunPodComponent* Pod, AGalacticPiratesCharacter* Crew, bool bManned)
{
	if (!Pod || !Crew || Crew->IsDead())
	{
		return;
	}

	if (bManned)
	{
		if (Pod->GetGunner() == Crew)
		{
			return;
		}
		if (Pod->GetGunner())
		{
			return;
		}
		Crew->SetActorLocation(Pod->GetComponentLocation());
		Pod->TryInteract(Crew);
	}
	else if (Pod->GetGunner() == Crew)
	{
		Pod->ForceRelease();
	}
}

void UShipCrewAiComponent::SilenceWeapons()
{
	if (!OwningShip)
	{
		return;
	}
	SetGunManned(OwningShip->PortMinigun, PortGunner, false);
	SetGunManned(OwningShip->StarboardMinigun, StarboardGunner, false);
	if (OwningShip->PortMinigun)
	{
		OwningShip->PortMinigun->SetFiring(false);
	}
	if (OwningShip->StarboardMinigun)
	{
		OwningShip->StarboardMinigun->SetFiring(false);
	}
}

void UShipCrewAiComponent::FinishWeaponUse()
{
	SilenceWeapons();
	LastWeapon = ActiveWeapon;
	ActiveWeapon = EAiWeapon::None;
	WeaponHoldTimer = 0.0f;
	WeaponDelayTimer = InterWeaponDelay;
}

UShipCrewAiComponent::EAiWeapon UShipCrewAiComponent::ChooseWeapon(AActor* Target) const
{
	if (!OwningShip || !Target)
	{
		return EAiWeapon::None;
	}

	TArray<EAiWeapon, TInlineAllocator<3>> Candidates;
	if (CanEngageWithMinigun(OwningShip->PortMinigun, Target))
	{
		Candidates.Add(EAiWeapon::PortMinigun);
	}
	if (CanEngageWithMinigun(OwningShip->StarboardMinigun, Target))
	{
		Candidates.Add(EAiWeapon::StarboardMinigun);
	}
	if (OwningShip->MissileSalvo && OwningShip->MissileSalvo->CanFire() && SalvoOperator)
	{
		Candidates.Add(EAiWeapon::Missiles);
	}

	if (Candidates.Num() == 0)
	{
		return EAiWeapon::None;
	}

	TArray<EAiWeapon, TInlineAllocator<3>> Fresh = Candidates;
	Fresh.RemoveAll([this](EAiWeapon Weapon) { return Weapon == LastWeapon; });
	const TArray<EAiWeapon, TInlineAllocator<3>>& Pool = Fresh.Num() > 0 ? Fresh : Candidates;
	return Pool[FMath::RandRange(0, Pool.Num() - 1)];
}

bool UShipCrewAiComponent::HasGunLineOfSight(const UMinigunPodComponent* Pod, AActor* Target) const
{
	UWorld* World = GetWorld();
	if (!World || !Pod || !Target || !OwningShip)
	{
		return false;
	}

	const FVector Start = Pod->GetMuzzleLocation();
	const FVector End = GPAsSpaceCraft(Target) && GPAsSpaceCraft(Target)->GetHomingSceneComponent()
		? GPAsSpaceCraft(Target)->GetHomingSceneComponent()->GetComponentLocation()
		: Target->GetActorLocation();
	if (Start.Equals(End, 1.0f))
	{
		return true;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AiGunLos), false, OwningShip);
	Params.AddIgnoredActor(OwningShip);
	for (AGalacticPiratesCharacter* Aboard : OwningShip->GetPlayersAboard())
	{
		if (Aboard)
		{
			Params.AddIgnoredActor(Aboard);
		}
	}

	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	if (!bHit)
	{
		return true;
	}

	AActor* HitActor = Hit.GetActor();
	if (!HitActor && Hit.GetComponent())
	{
		HitActor = Hit.GetComponent()->GetOwner();
	}
	if (!HitActor)
	{
		return false;
	}

	if (HitActor == Target || HitActor->GetOwner() == Target)
	{
		return true;
	}

	if (GPAsSpaceCraft(HitActor))
	{
		return HitActor == Target;
	}

	if (AGalacticPiratesCharacter* HitCrew = Cast<AGalacticPiratesCharacter>(HitActor))
	{
		return HitCrew->GetBoardedShip() == Target;
	}

	return false;
}

void UShipCrewAiComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!OwningShip || !OwningShip->HasAuthority() || OwningShip->IsWrecked() || !bCrewSpawned)
	{
		return;
	}

	AActor* Target = FindAttackTarget();
	if (!Target)
	{
		SilenceWeapons();
		ActiveWeapon = EAiWeapon::None;
		WeaponHoldTimer = 0.0f;
		return;
	}

	SetGunManned(OwningShip->PortMinigun, PortGunner, ActiveWeapon == EAiWeapon::PortMinigun);
	SetGunManned(OwningShip->StarboardMinigun, StarboardGunner, ActiveWeapon == EAiWeapon::StarboardMinigun);
	UpdateMinigun(OwningShip->PortMinigun, Target, ActiveWeapon == EAiWeapon::PortMinigun);
	UpdateMinigun(OwningShip->StarboardMinigun, Target, ActiveWeapon == EAiWeapon::StarboardMinigun);

	if (WeaponDelayTimer > 0.0f)
	{
		WeaponDelayTimer -= DeltaTime;
		return;
	}

	if (ActiveWeapon == EAiWeapon::None)
	{
		ActiveWeapon = ChooseWeapon(Target);
		if (ActiveWeapon == EAiWeapon::None)
		{
			SilenceWeapons();
			return;
		}

		WeaponHoldTimer = (ActiveWeapon == EAiWeapon::Missiles) ? 0.05f : GunBurstSeconds;
	}

	bool bFinished = false;
	switch (ActiveWeapon)
	{
	case EAiWeapon::PortMinigun:
		SetGunManned(OwningShip->PortMinigun, PortGunner, true);
		SetGunManned(OwningShip->StarboardMinigun, StarboardGunner, false);
		bFinished = !UpdateMinigun(OwningShip->PortMinigun, Target, true);
		break;
	case EAiWeapon::StarboardMinigun:
		SetGunManned(OwningShip->StarboardMinigun, StarboardGunner, true);
		SetGunManned(OwningShip->PortMinigun, PortGunner, false);
		bFinished = !UpdateMinigun(OwningShip->StarboardMinigun, Target, true);
		break;
	case EAiWeapon::Missiles:
		if (OwningShip->MissileSalvo && OwningShip->MissileSalvo->CanFire())
		{
			OwningShip->MissileSalvo->FireIgnoringTerminalRange(SalvoOperator);
		}
		bFinished = true;
		break;
	default:
		bFinished = true;
		break;
	}

	WeaponHoldTimer -= DeltaTime;
	if (bFinished || WeaponHoldTimer <= 0.0f)
	{
		FinishWeaponUse();
	}
}

void UShipCrewAiComponent::DestroyCrew()
{
	AGalacticPiratesCharacter* Members[] = { Pilot, SalvoOperator, PortGunner, StarboardGunner };
	for (AGalacticPiratesCharacter* Member : Members)
	{
		if (IsValid(Member) && !Member->IsDead())
		{
			Member->Destroy();
		}
	}
	Pilot = nullptr;
	SalvoOperator = nullptr;
	PortGunner = nullptr;
	StarboardGunner = nullptr;
	bCrewSpawned = false;
}
