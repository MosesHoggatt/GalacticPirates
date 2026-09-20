#include "ShipCrewAiComponent.h"
#include "WalkableShip.h"
#include "GalacticPiratesCharacter.h"
#include "HelmComponent.h"
#include "MinigunPodComponent.h"
#include "ShipMissileSalvoComponent.h"
#include "MissileSalvoTerminalComponent.h"
#include "HoloMapPoiComponent.h"
#include "HoloMapTypes.h"
#include "GalacticPirates.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
		TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
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
		Pilot->SetActorLocation(OwningShip->Helm->GetComponentLocation());
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
		OwningShip->PortMinigun->TryInteract(PortGunner);
	}

	if (StarboardGunner && OwningShip->StarboardMinigun)
	{
		StarboardGunner->SetActorLocation(OwningShip->StarboardMinigun->GetComponentLocation());
		OwningShip->StarboardMinigun->TryInteract(StarboardGunner);
	}
}

AWalkableShip* UShipCrewAiComponent::FindAttackTarget() const
{
	if (!OwningShip)
	{
		return nullptr;
	}

	AWalkableShip* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<AWalkableShip> It(GetWorld()); It; ++It)
	{
		AWalkableShip* Candidate = *It;
		if (!Candidate || Candidate == OwningShip || Candidate->IsWrecked() || !Candidate->HasHumanCrew())
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

void UShipCrewAiComponent::DriveMinigun(UMinigunPodComponent* Pod, AWalkableShip* Target)
{
	if (!Pod || !Pod->GetGunner() || !Target)
	{
		if (Pod)
		{
			Pod->SetFiring(false);
		}
		return;
	}

	const FVector AimPoint = Target->GetActorLocation();
	Pod->AimAtWorldLocation(AimPoint);
	const FVector ToTarget = (AimPoint - Pod->GetMuzzleLocation()).GetSafeNormal();
	const float Dist = FVector::Dist(Pod->GetMuzzleLocation(), AimPoint);
	const bool bOnTarget = Dist < Pod->TraceRange
		&& FVector::DotProduct(Pod->GetMuzzleForward(), ToTarget) >= GunEngageDot;
	Pod->SetFiring(bOnTarget);
}

void UShipCrewAiComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!OwningShip || !OwningShip->HasAuthority() || OwningShip->IsWrecked() || !bCrewSpawned)
	{
		return;
	}

	AWalkableShip* Target = FindAttackTarget();
	DriveMinigun(OwningShip->PortMinigun, Target);
	DriveMinigun(OwningShip->StarboardMinigun, Target);

	SalvoTimer -= DeltaTime;
	if (SalvoTimer <= 0.0f)
	{
		SalvoTimer = SalvoInterval;
		if (Target && OwningShip->MissileSalvo && OwningShip->MissileSalvo->CanFire())
		{
			OwningShip->MissileSalvo->FireIgnoringTerminalRange(SalvoOperator);
		}
	}
}

void UShipCrewAiComponent::DestroyCrew()
{
	AGalacticPiratesCharacter* Members[] = { Pilot, SalvoOperator, PortGunner, StarboardGunner };
	for (AGalacticPiratesCharacter* Member : Members)
	{
		if (IsValid(Member))
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
