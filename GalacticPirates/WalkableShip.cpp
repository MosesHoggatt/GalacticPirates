#include "WalkableShip.h"
#include "ShipMovementComponent.h"
#include "HelmComponent.h"
#include "ShipPulseCannonComponent.h"
#include "WeaponTerminalComponent.h"
#include "ShipMissileSalvoComponent.h"
#include "MissileSalvoTerminalComponent.h"
#include "MinigunPodComponent.h"
#include "HolographicMapTableComponent.h"
#include "HoloMapPoiComponent.h"
#include "ShipOrbitAiComponent.h"
#include "ShipCrewAiComponent.h"
#include "ShipPulseBeamVisual.h"
#include "ShipWreckDebris.h"
#include "GalacticPiratesCharacter.h"
#include "ShipDebug.h"
#include "GalacticPirates.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/DamageEvents.h"
#include "Engine/EngineTypes.h"
#include "ShipPolish.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AWalkableShip::AWalkableShip()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;

	ShipRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ShipRoot"));
	RootComponent = ShipRoot;

	HullMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HullMesh"));
	HullMesh->SetupAttachment(ShipRoot);
	HullMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HullMesh->SetVisibility(true);

	InteriorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InteriorMesh"));
	InteriorMesh->SetupAttachment(ShipRoot);
	InteriorMesh->SetMobility(EComponentMobility::Movable);
	InteriorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	InteriorMesh->SetCollisionProfileName(TEXT("BlockAll"));

	SpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnPoint"));
	SpawnPoint->SetupAttachment(ShipRoot);
	SpawnPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));

	ShipMovement = CreateDefaultSubobject<UShipMovementComponent>(TEXT("ShipMovement"));

	Helm = CreateDefaultSubobject<UHelmComponent>(TEXT("Helm"));
	Helm->SetupAttachment(ShipRoot);

	PulseCannon = CreateDefaultSubobject<UShipPulseCannonComponent>(TEXT("PulseCannon"));
	PulseCannon->SetupAttachment(ShipRoot);
	PulseCannon->SetRelativeLocation(FVector(1600.0f, 0.0f, 180.0f));

	WeaponTerminal = CreateDefaultSubobject<UWeaponTerminalComponent>(TEXT("WeaponTerminal"));
	WeaponTerminal->SetupAttachment(ShipRoot);
	WeaponTerminal->SetRelativeLocation(FVector(-180.0f, 280.0f, 90.0f));

	MissileSalvo = CreateDefaultSubobject<UShipMissileSalvoComponent>(TEXT("MissileSalvo"));
	MissileSalvo->SetupAttachment(ShipRoot);
	MissileSalvo->SetRelativeLocation(FVector(1600.0f, -420.0f, 180.0f));
	MissileSalvo->SetRelativeRotation(FRotator(0.0f, -8.0f, 0.0f));

	MissileTerminal = CreateDefaultSubobject<UMissileSalvoTerminalComponent>(TEXT("MissileTerminal"));
	MissileTerminal->SetupAttachment(ShipRoot);
	MissileTerminal->SetRelativeLocation(FVector(-180.0f, -280.0f, 90.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BlockoutCube(TEXT("/Engine/BasicShapes/Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BlockoutSphere(TEXT("/Engine/BasicShapes/Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BlockoutCylinder(TEXT("/Engine/BasicShapes/Cylinder"));
	UStaticMesh* CubeMesh = BlockoutCube.Succeeded() ? BlockoutCube.Object : nullptr;
	UStaticMesh* SphereMesh = BlockoutSphere.Succeeded() ? BlockoutSphere.Object : nullptr;
	UStaticMesh* CylinderMesh = BlockoutCylinder.Succeeded() ? BlockoutCylinder.Object : nullptr;

	// Pod geometry hangs off ShipRoot, never InteriorMesh, so it does not inherit the deck's 15x8 scale.
	auto MakePodPiece = [this](UStaticMesh* Mesh, const TCHAR* Name, const FVector& Scale, const FVector& RelLocation, bool bWalkable)
	{
		UStaticMeshComponent* Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetupAttachment(ShipRoot);
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->SetRelativeLocation(RelLocation);
		Comp->SetRelativeScale3D(Scale);
		Comp->SetStaticMesh(Mesh);
		Comp->SetCastShadow(false);
		Comp->SetCanEverAffectNavigation(false);
		if (bWalkable)
		{
			Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Comp->SetCollisionObjectType(ECC_WorldDynamic);
			Comp->SetCollisionProfileName(TEXT("BlockAll"));
		}
		else
		{
			Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		return Comp;
	};

	PortPodBubble = MakePodPiece(SphereMesh, TEXT("PortPodBubble"), FVector(PodBubbleDiameter / 100.0f), FVector(PodDoorwayCenterX, -PodCenterY, PodBubbleCenterZ), false);
	PortPodDeck = MakePodPiece(CylinderMesh, TEXT("PortPodDeck"), FVector(2.6f, 2.6f, 0.1f), FVector(PodDoorwayCenterX, -PodCenterY, 5.0f), true);
	PortPodNeck = MakePodPiece(CubeMesh, TEXT("PortPodNeck"), FVector(PodDoorwayWidth / 100.0f, 2.4f, 0.1f), FVector(PodDoorwayCenterX, -(PodCenterY - 120.0f), 5.0f), true);
	PortDoorFillLower = MakePodPiece(CubeMesh, TEXT("PortDoorFillLower"), FVector::OneVector, FVector::ZeroVector, true);
	PortDoorFillUpper = MakePodPiece(CubeMesh, TEXT("PortDoorFillUpper"), FVector::OneVector, FVector::ZeroVector, true);

	StarboardPodBubble = MakePodPiece(SphereMesh, TEXT("StarboardPodBubble"), FVector(PodBubbleDiameter / 100.0f), FVector(PodDoorwayCenterX, PodCenterY, PodBubbleCenterZ), false);
	StarboardPodDeck = MakePodPiece(CylinderMesh, TEXT("StarboardPodDeck"), FVector(2.6f, 2.6f, 0.1f), FVector(PodDoorwayCenterX, PodCenterY, 5.0f), true);
	StarboardPodNeck = MakePodPiece(CubeMesh, TEXT("StarboardPodNeck"), FVector(PodDoorwayWidth / 100.0f, 2.4f, 0.1f), FVector(PodDoorwayCenterX, PodCenterY - 120.0f, 5.0f), true);
	StarboardDoorFillLower = MakePodPiece(CubeMesh, TEXT("StarboardDoorFillLower"), FVector::OneVector, FVector::ZeroVector, true);
	StarboardDoorFillUpper = MakePodPiece(CubeMesh, TEXT("StarboardDoorFillUpper"), FVector::OneVector, FVector::ZeroVector, true);

	PortMinigun = CreateDefaultSubobject<UMinigunPodComponent>(TEXT("PortMinigun"));
	PortMinigun->SetupAttachment(ShipRoot);
	PortMinigun->SetRelativeLocation(FVector(PodDoorwayCenterX, -(PodCenterY + 40.0f), 96.0f));
	PortMinigun->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	StarboardMinigun = CreateDefaultSubobject<UMinigunPodComponent>(TEXT("StarboardMinigun"));
	StarboardMinigun->SetupAttachment(ShipRoot);
	StarboardMinigun->SetRelativeLocation(FVector(PodDoorwayCenterX, PodCenterY + 40.0f, 96.0f));
	StarboardMinigun->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));

	MapTable = CreateDefaultSubobject<UHolographicMapTableComponent>(TEXT("MapTable"));
	MapTable->SetupAttachment(ShipRoot);
	MapTable->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

	HoloPoi = CreateDefaultSubobject<UHoloMapPoiComponent>(TEXT("HoloPoi"));
	HoloPoi->SetupAttachment(ShipRoot);
	HoloPoi->Kind = EHoloMapPoiKind::EnemyShip;
	HoloPoi->Primitive = EHoloMapPrimitive::Cone;
	HoloPoi->bOverridePrimitive = true;

	OrbitAI = CreateDefaultSubobject<UShipOrbitAiComponent>(TEXT("OrbitAI"));
	CrewAI = CreateDefaultSubobject<UShipCrewAiComponent>(TEXT("CrewAI"));

	CombatHull = CreateDefaultSubobject<UBoxComponent>(TEXT("CombatHull"));
	CombatHull->SetupAttachment(ShipRoot);
	CombatHull->SetBoxExtent(FVector(1100.0f, 800.0f, 400.0f));
	CombatHull->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	CombatHull->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CombatHull->SetCollisionObjectType(ECC_WorldDynamic);
	CombatHull->SetCollisionResponseToAllChannels(ECR_Block);
	CombatHull->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	CombatHull->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CombatHull->SetCanEverAffectNavigation(false);

	CurrentPilot = nullptr;
	LastReplicatedRotation = FQuat::Identity;
	CurrentHealth = MaxHealth;
	SetCanBeDamaged(true);
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(10.0f);
}

void AWalkableShip::BeginPlay()
{
	Super::BeginPlay();
	LastReplicatedRotation = GetActorQuat();
	if (HasAuthority())
	{
		CurrentHealth = MaxHealth;
		bWrecked = false;
	}

	if (InteriorMesh)
	{
		InteriorMesh->PrimaryComponentTick.TickGroup = TG_PrePhysics;
		if (ShipMovement)
		{
			InteriorMesh->PrimaryComponentTick.AddPrerequisite(ShipMovement, ShipMovement->PrimaryComponentTick);
		}
	}

	if (HasAuthority() && GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer
		&& GPDedicatedNetTestEnabled())
	{
		GPStartDedicatedNetTest(GetWorld());
	}

	CarveGunPodDoorways();
	GPApplyPolishVfxMaterial(PortPodBubble, TEXT("circle_05"), FLinearColor(0.35f, 0.62f, 0.9f, 0.35f));
	GPApplyPolishVfxMaterial(StarboardPodBubble, TEXT("circle_05"), FLinearColor(0.35f, 0.62f, 0.9f, 0.35f));
}

void AWalkableShip::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority() && ShipMovement)
	{
		ReplicatedLinearVelocity = ShipMovement->GetLinearVelocity();
		ReplicatedAngularVelocity = ShipMovement->GetAngularVelocity();
	}

	if (HasAuthority() && !bWrecked && CurrentHealth <= 0.0f)
	{
		Explode();
	}

	BroadcastRotationChange();
	GPTickDedicatedNetTest(GetWorld(), DeltaTime);
	GPTickShipDuelTest(GetWorld(), DeltaTime);
}

void AWalkableShip::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupAllPlayers();
	Super::EndPlay(EndPlayReason);
}

void AWalkableShip::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWalkableShip, CurrentPilot);
	DOREPLIFETIME(AWalkableShip, PlayersAboard);
	DOREPLIFETIME(AWalkableShip, ReplicatedLinearVelocity);
	DOREPLIFETIME(AWalkableShip, ReplicatedAngularVelocity);
	DOREPLIFETIME(AWalkableShip, CurrentHealth);
	DOREPLIFETIME(AWalkableShip, bWrecked);
}

AWalkableShip* AWalkableShip::FindPersistentShip(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AWalkableShip> It(World); It; ++It)
	{
		if (*It)
		{
			return *It;
		}
	}

	return nullptr;
}

void AWalkableShip::RegisterPlayer(AGalacticPiratesCharacter* Character)
{
	if (!HasAuthority() || !Character || bWrecked)
	{
		return;
	}

	if (!PlayersAboard.Contains(Character))
	{
		PlayersAboard.Add(Character);
	}
}

void AWalkableShip::UnregisterPlayer(AGalacticPiratesCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}

	if (CurrentPilot == Character)
	{
		ReleasePilot(Character);
	}

	if (PortMinigun && PortMinigun->GetGunner() == Character)
	{
		PortMinigun->ForceRelease();
	}
	if (StarboardMinigun && StarboardMinigun->GetGunner() == Character)
	{
		StarboardMinigun->ForceRelease();
	}

	PlayersAboard.Remove(Character);
}

bool AWalkableShip::HasHumanCrew() const
{
	if (CurrentPilot && !CurrentPilot->IsAiCrew())
	{
		return true;
	}

	for (AGalacticPiratesCharacter* Aboard : PlayersAboard)
	{
		if (Aboard && !Aboard->IsAiCrew())
		{
			return true;
		}
	}
	return false;
}

bool AWalkableShip::RequestPilotAssignment(AGalacticPiratesCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return false;
	}

	if (Character->IsManningMinigun())
	{
		return false;
	}

	if (CurrentPilot != nullptr)
	{
		return false;
	}

	if (!PlayersAboard.Contains(Character))
	{
		return false;
	}

	AGalacticPiratesCharacter* OldPilot = CurrentPilot;
	CurrentPilot = Character;
	OnRep_CurrentPilot(OldPilot);

	return true;
}

void AWalkableShip::ReleasePilot(AGalacticPiratesCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}

	if (CurrentPilot != Character)
	{
		return;
	}

	AGalacticPiratesCharacter* OldPilot = CurrentPilot;
	CurrentPilot = nullptr;
	OnRep_CurrentPilot(OldPilot);
}

void AWalkableShip::OnRep_CurrentPilot(AGalacticPiratesCharacter* OldPilot)
{
	if (HasAuthority())
	{
		if (OldPilot && OldPilot != CurrentPilot)
		{
			OldPilot->SetPiloting(false);
		}

		if (CurrentPilot)
		{
			CurrentPilot->SetPiloting(true);
		}
	}

	OnPilotChanged.Broadcast(CurrentPilot, OldPilot);

	if (Helm)
	{
		Helm->OnPilotChanged(CurrentPilot, OldPilot);
	}
}

void AWalkableShip::ApplyPilotInput(AGalacticPiratesCharacter* Pilot, const FVector& ThrustInput, const FVector& RotationInput)
{
	if (!HasAuthority() || bWrecked)
	{
		return;
	}

	if (Pilot != CurrentPilot)
	{
		return;
	}

	if (ShipMovement)
	{
		ShipMovement->SetThrustInput(ThrustInput);
		ShipMovement->SetRotationInput(RotationInput);
	}
}

void AWalkableShip::HandlePlayerDisconnected(AGalacticPiratesCharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}

	UnregisterPlayer(Character);
}

FTransform AWalkableShip::GetSpawnTransform() const
{
	if (SpawnPoint)
	{
		return SpawnPoint->GetComponentTransform();
	}
	return GetActorTransform();
}

FTransform AWalkableShip::GetSpawnTransformForSlot(int32 Slot) const
{
	FTransform SpawnTransform = GetSpawnTransform();
	SpawnTransform.AddToTranslation(GetActorRightVector() * static_cast<float>(FMath::Max(0, Slot)) * 90.0f);
	return SpawnTransform;
}

void AWalkableShip::CarveGunPodDoorways()
{
	const float DoorMinX = PodDoorwayCenterX - PodDoorwayWidth * 0.5f;
	const float DoorMaxX = PodDoorwayCenterX + PodDoorwayWidth * 0.5f;

	UStaticMeshComponent* Unused[] = { PortDoorFillLower, PortDoorFillUpper, StarboardDoorFillLower, StarboardDoorFillUpper };
	for (UStaticMeshComponent* Fill : Unused)
	{
		if (Fill)
		{
			Fill->SetVisibility(false);
			Fill->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	int32 CarvedWalls = 0;
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Wall : Meshes)
	{
		if (!Wall || Wall == PortDoorFillLower || Wall == PortDoorFillUpper
			|| Wall == StarboardDoorFillLower || Wall == StarboardDoorFillUpper)
		{
			continue;
		}

		const FVector Loc = Wall->GetRelativeLocation();
		const FVector Scale = Wall->GetRelativeScale3D();
		const bool bIsHullSideWall = FMath::Abs(Loc.Y) > 300.0f && FMath::Abs(Loc.Y) < 500.0f
			&& Scale.X >= 8.0f && Scale.Y <= 1.0f && Scale.Z >= 0.5f;
		if (!bIsHullSideWall)
		{
			continue;
		}

		const float HalfLength = Scale.X * 50.0f;
		const float WallMinX = Loc.X - HalfLength;
		const float WallMaxX = Loc.X + HalfLength;
		if (DoorMinX <= WallMinX || DoorMaxX >= WallMaxX)
		{
			continue;
		}

		const bool bPortSide = Loc.Y < 0.0f;
		const bool bUpperBand = Loc.Z > 150.0f;
		UStaticMeshComponent* Fill = bPortSide
			? (bUpperBand ? PortDoorFillUpper : PortDoorFillLower)
			: (bUpperBand ? StarboardDoorFillUpper : StarboardDoorFillLower);
		if (!Fill)
		{
			continue;
		}

		const float ForeLength = WallMaxX - DoorMaxX;
		Fill->SetStaticMesh(Wall->GetStaticMesh());
		Fill->SetMaterial(0, Wall->GetMaterial(0));
		Fill->SetRelativeLocation(FVector(DoorMaxX + ForeLength * 0.5f, Loc.Y, Loc.Z));
		Fill->SetRelativeScale3D(FVector(ForeLength / 100.0f, Scale.Y, Scale.Z));
		Fill->SetVisibility(true);
		Fill->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		const float AftLength = DoorMinX - WallMinX;
		Wall->SetRelativeLocation(FVector(WallMinX + AftLength * 0.5f, Loc.Y, Loc.Z));
		Wall->SetRelativeScale3D(FVector(AftLength / 100.0f, Scale.Y, Scale.Z));
		++CarvedWalls;
	}

	UE_LOG(LogGalacticPirates, Warning, TEXT("[GunPods] Carved %d hull wall sections for the port/starboard doorways"), CarvedWalls);
}

bool AWalkableShip::IsWalkableWorldLocation(const FVector& WorldLocation) const
{
	FBox Bounds = GetComponentsBoundingBox(true);
	if (!Bounds.IsValid)
	{
		return false;
	}

	return Bounds.ExpandBy(250.0f).IsInsideOrOn(WorldLocation);
}

bool AWalkableShip::HasDeckBelow(const FVector& WorldLocation, float TraceDistance) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector End = WorldLocation - GetShipUpVector() * TraceDistance;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WalkableShipDeckTrace), false);

	if (InteriorMesh)
	{
		FCollisionQueryParams CompParams(SCENE_QUERY_STAT(WalkableShipDeckTrace), false);
		if (InteriorMesh->LineTraceComponent(Hit, WorldLocation, End, CompParams))
		{
			return true;
		}
	}

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	if (World->LineTraceSingleByObjectType(Hit, WorldLocation, End, ObjParams, Params))
	{
		return Hit.GetActor() == this || (Hit.GetComponent() && Hit.GetComponent()->GetOwner() == this);
	}

	return false;
}

FVector AWalkableShip::GetPointVelocity(const FVector& WorldPoint) const
{
	const FVector Linear = HasAuthority() && ShipMovement
		? ShipMovement->GetLinearVelocity()
		: ReplicatedLinearVelocity;
	const FVector AngularDeg = HasAuthority() && ShipMovement
		? ShipMovement->GetAngularVelocity()
		: ReplicatedAngularVelocity;
	const FVector Omega = FMath::DegreesToRadians(AngularDeg);
	return Linear + FVector::CrossProduct(Omega, WorldPoint - GetActorLocation());
}

void AWalkableShip::BroadcastRotationChange()
{
	FQuat CurrentRotation = GetActorQuat();
	
	if (!CurrentRotation.Equals(LastReplicatedRotation, 0.001f))
	{
		LastReplicatedRotation = CurrentRotation;
		OnShipRotationChanged.Broadcast(CurrentRotation);
	}
}

void AWalkableShip::CleanupAllPlayers()
{
	if (!HasAuthority())
	{
		return;
	}

	if (CurrentPilot)
	{
		ReleasePilot(CurrentPilot);
	}

	TArray<AGalacticPiratesCharacter*> Snapshot = PlayersAboard;
	for (AGalacticPiratesCharacter* Character : Snapshot)
	{
		if (!Character)
		{
			continue;
		}

		const bool bAi = Character->IsAiCrew();
		Character->OnShipDestroyed();
		if (bAi)
		{
			Character->Destroy();
		}
	}

	PlayersAboard.Empty();
}

float AWalkableShip::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	AGalacticPiratesCharacter* InstigatorCharacter = EventInstigator ? Cast<AGalacticPiratesCharacter>(EventInstigator->GetPawn()) : nullptr;
	return ApplyShipDamage(DamageAmount, InstigatorCharacter, DamageCauser);
}

void AWalkableShip::SetHealth(float NewHealth)
{
	if (!HasAuthority() || bWrecked)
	{
		return;
	}

	CurrentHealth = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
	OnRep_CurrentHealth();
}

float AWalkableShip::ApplyShipDamage(float DamageAmount, AGalacticPiratesCharacter* InstigatorCharacter, AActor* DamageCauser)
{
	if (!HasAuthority() || bWrecked || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	const float Applied = FMath::Min(CurrentHealth, DamageAmount);
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - Applied);
	OnRep_CurrentHealth();
	OnShipDamaged.Broadcast(Applied, CurrentHealth);

	UE_LOG(LogGalacticPirates, Warning,
		TEXT("[ShipCombat] %s took %.1f damage from %s via %s remaining=%.1f"),
		*GetName(),
		Applied,
		*GetNameSafe(InstigatorCharacter),
		*GetNameSafe(DamageCauser),
		CurrentHealth);

	return Applied;
}

void AWalkableShip::Explode()
{
	if (!HasAuthority() || bWrecked)
	{
		return;
	}

	bWrecked = true;
	CurrentHealth = 0.0f;
	if (ShipMovement)
	{
		ShipMovement->SetThrustInput(FVector::ZeroVector);
		ShipMovement->SetRotationInput(FVector::ZeroVector);
		ShipMovement->SetComponentTickEnabled(false);
	}

	CleanupAllPlayers();
	OnRep_Wrecked();
	Multicast_Explode();
	OnShipExploded.Broadcast();
	SetLifeSpan(WreckLifetime);

	UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipCombat] %s exploded"), *GetName());
}

void AWalkableShip::OnRep_CurrentHealth()
{
	UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipCombat] Health replicated on %s health=%.1f/%.1f net=%d"),
		*GetName(),
		CurrentHealth,
		MaxHealth,
		static_cast<int32>(GetNetMode()));
}

void AWalkableShip::OnRep_Wrecked()
{
	if (!bWrecked)
	{
		return;
	}

	if (ShipMovement)
	{
		ShipMovement->SetComponentTickEnabled(false);
	}

	if (HullMesh)
	{
		HullMesh->SetVisibility(false, true);
	}
	if (InteriorMesh)
	{
		InteriorMesh->SetVisibility(false, true);
		InteriorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (CombatHull)
	{
		CombatHull->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AWalkableShip::Multicast_Explode_Implementation()
{
	UWorld* World = GetWorld();
	if (World)
	{
		DrawDebugSphere(World, GetActorLocation(), ExplosionVisualScale, 16, FColor::Orange, false, 2.5f, 0, 8.0f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AShipPulseBeamVisual* Burst = World->SpawnActor<AShipPulseBeamVisual>(AShipPulseBeamVisual::StaticClass(), GetActorLocation(), FRotator::ZeroRotator, Params))
		{
			Burst->InitializeExplosion(GetActorLocation(), ExplosionVisualScale, 1.8f, FLinearColor(1.0f, 0.32f, 0.06f, 1.0f));
		}

		if (GetNetMode() != NM_DedicatedServer)
		{
			if (AShipWreckDebris* Debris = World->SpawnActor<AShipWreckDebris>(AShipWreckDebris::StaticClass(), GetActorLocation(), GetActorRotation(), Params))
			{
				Debris->InitializeFromShip(this);
			}
			GPPlayPolishSound2D(this, TEXT("SFX_Explosion"), 1.15f);
			GPPlayPolishSound2D(this, TEXT("SFX_ExplosionBass"), 1.0f);
			GPPlayPolishSoundAt(this, TEXT("SFX_Explosion"), GetActorLocation(), 1.25f);
			GPPlayPolishSoundAt(this, TEXT("SFX_ExplosionBass"), GetActorLocation(), 1.1f);
			GPPlayExplosionCameraShake(World, GetActorLocation(), 400.0f, 9000.0f, 1.0f);
		}
	}

	TInlineComponentArray<UStaticMeshComponent*> Meshes(this);
	GetComponents(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh)
		{
			Mesh->SetVisibility(false, true);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	if (CombatHull)
	{
		CombatHull->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipCombat] Explosion visual on %s net=%d"),
		*GetName(),
		static_cast<int32>(GetNetMode()));
}

bool AWalkableShip::TryStationInteract(AGalacticPiratesCharacter* Character)
{
	if (!HasAuthority() || !Character || bWrecked)
	{
		return false;
	}

	if (Character->IsPiloting() && Helm)
	{
		return Helm->TryInteract(Character);
	}

	if (Character->IsManningMinigun())
	{
		if (UMinigunPodComponent* Pod = Character->GetOccupiedMinigun())
		{
			return Pod->TryInteract(Character);
		}
		return false;
	}

	const FVector CharLoc = Character->GetActorLocation();
	const bool bHelmInRange = Helm && FVector::Dist(CharLoc, Helm->GetComponentLocation()) <= Helm->InteractRange;
	const bool bCannonInRange = WeaponTerminal && WeaponTerminal->IsCharacterInRange(Character);
	const bool bMissileInRange = MissileTerminal && MissileTerminal->IsCharacterInRange(Character);
	const bool bPortGunInRange = PortMinigun && PortMinigun->IsCharacterInRange(Character);
	const bool bStbdGunInRange = StarboardMinigun && StarboardMinigun->IsCharacterInRange(Character);
	const float HelmDist = Helm ? FVector::Dist(CharLoc, Helm->GetComponentLocation()) : TNumericLimits<float>::Max();
	const float CannonDist = WeaponTerminal ? FVector::Dist(CharLoc, WeaponTerminal->GetComponentLocation()) : TNumericLimits<float>::Max();
	const float MissileDist = MissileTerminal ? FVector::Dist(CharLoc, MissileTerminal->GetComponentLocation()) : TNumericLimits<float>::Max();
	const float PortGunDist = PortMinigun ? FVector::Dist(CharLoc, PortMinigun->GetComponentLocation()) : TNumericLimits<float>::Max();
	const float StbdGunDist = StarboardMinigun ? FVector::Dist(CharLoc, StarboardMinigun->GetComponentLocation()) : TNumericLimits<float>::Max();

	float BestDist = TNumericLimits<float>::Max();
	int32 Best = 0;
	if (bHelmInRange && HelmDist < BestDist) { BestDist = HelmDist; Best = 1; }
	if (bCannonInRange && CannonDist < BestDist) { BestDist = CannonDist; Best = 2; }
	if (bMissileInRange && MissileDist < BestDist) { BestDist = MissileDist; Best = 3; }
	if (bPortGunInRange && PortGunDist < BestDist) { BestDist = PortGunDist; Best = 4; }
	if (bStbdGunInRange && StbdGunDist < BestDist) { BestDist = StbdGunDist; Best = 5; }

	if (Best == 1)
	{
		return Helm->TryInteract(Character);
	}
	if (Best == 2)
	{
		return WeaponTerminal->TryInteract(Character);
	}
	if (Best == 3)
	{
		return MissileTerminal->TryInteract(Character);
	}
	if (Best == 4)
	{
		return PortMinigun->TryInteract(Character);
	}
	if (Best == 5)
	{
		return StarboardMinigun->TryInteract(Character);
	}

	UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipStations] %s is not in range of helm, cannon, missile, or minigun"), *GetNameSafe(Character));
	return false;
}

FText AWalkableShip::GetInteractPrompt(AGalacticPiratesCharacter* Character) const
{
	if (!Character || bWrecked)
	{
		return FText::GetEmpty();
	}

	if (Character->IsPiloting())
	{
		return FText::FromString(TEXT("Press F  ·  Leave helm"));
	}

	if (Character->IsManningMinigun())
	{
		return FText::FromString(TEXT("Press F  ·  Leave minigun"));
	}

	const FVector CharLoc = Character->GetActorLocation();
	const bool bHelmInRange = Helm && FVector::Dist(CharLoc, Helm->GetComponentLocation()) <= Helm->InteractRange;
	const bool bCannonInRange = WeaponTerminal && WeaponTerminal->IsCharacterInRange(Character);
	const bool bMissileInRange = MissileTerminal && MissileTerminal->IsCharacterInRange(Character);
	const bool bPortGunInRange = PortMinigun && PortMinigun->IsCharacterInRange(Character);
	const bool bStbdGunInRange = StarboardMinigun && StarboardMinigun->IsCharacterInRange(Character);
	const float HelmDist = Helm ? FVector::Dist(CharLoc, Helm->GetComponentLocation()) : TNumericLimits<float>::Max();
	const float CannonDist = WeaponTerminal ? FVector::Dist(CharLoc, WeaponTerminal->GetComponentLocation()) : TNumericLimits<float>::Max();
	const float MissileDist = MissileTerminal ? FVector::Dist(CharLoc, MissileTerminal->GetComponentLocation()) : TNumericLimits<float>::Max();
	const float PortGunDist = PortMinigun ? FVector::Dist(CharLoc, PortMinigun->GetComponentLocation()) : TNumericLimits<float>::Max();
	const float StbdGunDist = StarboardMinigun ? FVector::Dist(CharLoc, StarboardMinigun->GetComponentLocation()) : TNumericLimits<float>::Max();

	float BestDist = TNumericLimits<float>::Max();
	int32 Best = 0;
	if (bHelmInRange && HelmDist < BestDist) { BestDist = HelmDist; Best = 1; }
	if (bCannonInRange && CannonDist < BestDist) { BestDist = CannonDist; Best = 2; }
	if (bMissileInRange && MissileDist < BestDist) { BestDist = MissileDist; Best = 3; }
	if (bPortGunInRange && PortGunDist < BestDist) { BestDist = PortGunDist; Best = 4; }
	if (bStbdGunInRange && StbdGunDist < BestDist) { BestDist = StbdGunDist; Best = 5; }

	if (Best == 1)
	{
		if (CurrentPilot && CurrentPilot != Character)
		{
			return FText::FromString(TEXT("Helm occupied"));
		}
		return FText::FromString(TEXT("Press F  ·  Take helm"));
	}

	if (Best == 2)
	{
		if (PulseCannon && !PulseCannon->CanFire())
		{
			return FText::FromString(FString::Printf(TEXT("Cannon recharging  ·  %.1fs"), PulseCannon->GetCooldownRemaining()));
		}
		return FText::FromString(TEXT("Press F  ·  Fire pulse cannon"));
	}

	if (Best == 3)
	{
		if (MissileSalvo && !MissileSalvo->CanFire())
		{
			return FText::FromString(FString::Printf(TEXT("Missiles recharging  ·  %.1fs"), MissileSalvo->GetCooldownRemaining()));
		}
		return FText::FromString(TEXT("Press F  ·  Fire missile salvo"));
	}

	if (Best == 4 || Best == 5)
	{
		UMinigunPodComponent* Pod = (Best == 4) ? PortMinigun : StarboardMinigun;
		if (Pod && Pod->IsOccupied() && Pod->GetGunner() != Character)
		{
			return FText::FromString(TEXT("Minigun occupied"));
		}
		return FText::FromString(TEXT("Press F  ·  Man minigun"));
	}

	return FText::GetEmpty();
}
