#include "HeatseekingMissile.h"
#include "WalkableShip.h"
#include "ShipMissileSalvoComponent.h"
#include "HoloMapPoiComponent.h"
#include "HoloMapTypes.h"
#include "MinigunPodComponent.h"
#include "CombatTypes.h"
#include "ShipCrewAiComponent.h"
#include "HelmComponent.h"
#include "GalacticPiratesCharacter.h"
#include "QuatCamera.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPMissileHeatScorePrefersCloser, "GalacticPirates.Missiles.HeatScorePrefersCloser", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPMissileHeatScorePrefersCloser::RunTest(const FString& Parameters)
{
	const FVector Origin = FVector::ZeroVector;
	const FVector Forward = FVector::ForwardVector;
	const float Near = GPComputeMissileHeatScore(Origin, Forward, FVector(1000.0f, 0.0f, 0.0f), 1000.0f, 0.1f);
	const float Far = GPComputeMissileHeatScore(Origin, Forward, FVector(5000.0f, 0.0f, 0.0f), 1000.0f, 0.1f);
	const float Behind = GPComputeMissileHeatScore(Origin, Forward, FVector(-1000.0f, 0.0f, 0.0f), 8000.0f, 0.15f);
	const float Side = GPComputeMissileHeatScore(Origin, Forward, FVector(0.0f, 4000.0f, 0.0f), 8000.0f, 0.15f);

	TestTrue(TEXT("closer signature outscores farther"), Near > Far);
	TestTrue(TEXT("rear aspect is not a valid lock"), Behind < 0.0f);
	TestTrue(TEXT("abeam aspect is not a valid lock"), Side < 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPMissileSalvoSeeksAndDamages, "GalacticPirates.Missiles.SalvoSeeksAndDamages", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPMissileSalvoSeeksAndDamages::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!World)
	{
		AddError(TEXT("Could not create a game world"));
		return false;
	}

	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AWalkableShip* Shooter = World->SpawnActor<AWalkableShip>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	AWalkableShip* Target = World->SpawnActor<AWalkableShip>(FVector(2800.0f, 0.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f), SpawnParams);
	if (!TestNotNull(TEXT("shooter"), Shooter) || !TestNotNull(TEXT("target"), Target) || !TestNotNull(TEXT("salvo"), Shooter->MissileSalvo))
	{
		World->DestroyWorld(false);
		return false;
	}

	if (Target->HoloPoi)
	{
		Target->HoloPoi->Kind = EHoloMapPoiKind::EnemyShip;
	}

	const float HealthBefore = Target->GetHealth();
	TestTrue(TEXT("ships have authority in this world"), Shooter->HasAuthority());
	TestTrue(TEXT("server salvo fire succeeds"), Shooter->MissileSalvo->FireIgnoringTerminalRange(nullptr));
	TestTrue(TEXT("salvo entered cooldown"), !Shooter->MissileSalvo->CanFire());
	TestEqual(TEXT("one salvo recorded"), Shooter->MissileSalvo->GetSalvosFired(), 1);

	for (int32 Burst = 0; Burst < 8; ++Burst)
	{
		Shooter->MissileSalvo->AdvanceSalvo(0.2f);
	}

	int32 MissileCount = 0;
	AHeatseekingMissile* Sample = nullptr;
	for (TActorIterator<AHeatseekingMissile> It(World); It; ++It)
	{
		++MissileCount;
		Sample = *It;
	}

	TestTrue(TEXT("salvo spawned heat-seeking missiles"), MissileCount >= 1);
	if (Sample)
	{
		TestEqual(TEXT("missile source is firing ship"), Sample->GetSourceShip(), Shooter);
		AWalkableShip* Locked = Sample->GetLockedTarget();
		if (!Locked)
		{
			Locked = Sample->FindHottestTarget();
		}
		TestEqual(TEXT("heat lock is the enemy ship"), Locked, Target);
		Target->ApplyShipDamage(Sample->Damage, nullptr, Sample);
	}

	TestTrue(TEXT("server damage reduces hull"), Target->GetHealth() < HealthBefore);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPMissileSalvoDeniedWithoutAuthorityWorldActor, "GalacticPirates.Missiles.FireRequiresOwnerAuthority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPMissileSalvoDeniedWithoutAuthorityWorldActor::RunTest(const FString& Parameters)
{
	UShipMissileSalvoComponent* CDO = GetMutableDefault<UShipMissileSalvoComponent>();
	TestNotNull(TEXT("salvo CDO"), CDO);
	TestFalse(TEXT("CDO fire is rejected"), CDO && CDO->Fire(nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPMinigunPodsOnWalkableShip, "GalacticPirates.Minigun.PodsOnEitherSide", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPMinigunPodsOnWalkableShip::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!World)
	{
		AddError(TEXT("Could not create a game world"));
		return false;
	}

	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Ship = World->SpawnActor<AWalkableShip>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!TestNotNull(TEXT("ship"), Ship) || !TestNotNull(TEXT("port minigun"), Ship->PortMinigun) || !TestNotNull(TEXT("starboard minigun"), Ship->StarboardMinigun))
	{
		World->DestroyWorld(false);
		return false;
	}

	TestTrue(TEXT("port pod sits just outboard of the hull wall"),
		Ship->PortMinigun->GetRelativeLocation().Y < -450.0f && Ship->PortMinigun->GetRelativeLocation().Y > -900.0f);
	TestTrue(TEXT("starboard pod sits just outboard of the hull wall"),
		Ship->StarboardMinigun->GetRelativeLocation().Y > 450.0f && Ship->StarboardMinigun->GetRelativeLocation().Y < 900.0f);
	TestTrue(TEXT("yaw travel is wide"), Ship->PortMinigun->YawMax - Ship->PortMinigun->YawMin >= 200.0f);
	TestTrue(TEXT("pitch travel is wide"), Ship->PortMinigun->PitchMax - Ship->PortMinigun->PitchMin >= 120.0f);
	TestTrue(TEXT("walkable ships default armored"), Ship->ArmorClass == EShipArmorClass::Armored);
	TestTrue(TEXT("armored chip damage is small"), Ship->PortMinigun->ArmoredShipDamage < 15.0f);
	TestTrue(TEXT("missile damage exceeds missile health"), Ship->PortMinigun->MissileDamage > 0.15f * 18.0f);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPMinigunPodsAreWalkable, "GalacticPirates.Minigun.PodsAreWalkable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPMinigunPodsAreWalkable::RunTest(const FString& Parameters)
{
	UClass* ShipClass = LoadClass<AWalkableShip>(nullptr, TEXT("/Game/Ships/Debug/BP_DebugWalkableShip.BP_DebugWalkableShip_C"));
	if (!TestNotNull(TEXT("debug ship blueprint"), ShipClass))
	{
		return false;
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Ship = Cast<AWalkableShip>(World->SpawnActor<AActor>(ShipClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams));
	if (!TestNotNull(TEXT("ship instance"), Ship))
	{
		World->DestroyWorld(false);
		return false;
	}

	// This bare world has no game mode, so spawned actors need BeginPlay kicked by hand.
	if (!Ship->HasActorBegunPlay())
	{
		Ship->DispatchBeginPlay();
	}

	// A character-sized capsule must be able to walk from the cabin out to each pod.
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(34.0f, 96.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MinigunDoorwayTest), false);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Sign = (Side == 0) ? -1.0f : 1.0f;
		UMinigunPodComponent* Pod = (Side == 0) ? Ship->PortMinigun : Ship->StarboardMinigun;
		const FVector CabinStart(AWalkableShip::PodDoorwayCenterX, Sign * 200.0f, 115.0f);
		const FVector PodSeat(AWalkableShip::PodDoorwayCenterX, Sign * AWalkableShip::PodCenterY, 115.0f);

		FHitResult Hit;
		const bool bBlocked = World->SweepSingleByChannel(Hit, CabinStart, PodSeat, FQuat::Identity, ECC_Pawn, Capsule, Params);
		if (bBlocked)
		{
			AddError(FString::Printf(TEXT("side %d walk-in blocked by %s at %s"),
				Side, *GetNameSafe(Hit.GetComponent()), *Hit.ImpactPoint.ToCompactString()));
		}
		TestFalse(FString::Printf(TEXT("side %d walk-in is clear"), Side), bBlocked);
		TestTrue(FString::Printf(TEXT("side %d pod has a deck"), Side), Ship->HasDeckBelow(PodSeat));
		TestTrue(FString::Printf(TEXT("side %d gun is in the pod"), Side),
			Pod && FVector::Dist(Pod->GetComponentLocation(), PodSeat) < AWalkableShip::PodBubbleDiameter);
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPMinigunShotHasFeedback, "GalacticPirates.Minigun.ShotHasFeedback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPMinigunShotHasFeedback::RunTest(const FString& Parameters)
{
	// The game spawns the blueprint, not the raw C++ class, so test what the player actually flies.
	UClass* ShipClass = LoadClass<AWalkableShip>(nullptr, TEXT("/Game/Ships/Debug/BP_DebugWalkableShip.BP_DebugWalkableShip_C"));
	if (!TestNotNull(TEXT("debug ship blueprint"), ShipClass))
	{
		return false;
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Ship = Cast<AWalkableShip>(World->SpawnActor<AActor>(ShipClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams));
	UMinigunPodComponent* Pod = Ship ? Ship->PortMinigun : nullptr;
	if (!TestNotNull(TEXT("pod"), Pod))
	{
		World->DestroyWorld(false);
		return false;
	}

	if (!Ship->HasActorBegunPlay())
	{
		Ship->DispatchBeginPlay();
	}

	// A stale blueprint leaves new C++ subobjects owned by the CDO instead of the spawned actor.
	USceneComponent* FxParts[] = { Pod->MuzzleFlashMesh, Pod->TracerMesh, Pod->TracerRibbon, Pod->ImpactFlashMesh, Pod->MuzzleLight };
	for (USceneComponent* Part : FxParts)
	{
		if (!TestNotNull(TEXT("fx component exists"), Part))
		{
			World->DestroyWorld(false);
			return false;
		}
		TestTrue(FString::Printf(TEXT("%s belongs to the spawned ship"), *Part->GetName()), Part->GetOwner() == Ship);
		TestTrue(FString::Printf(TEXT("%s is registered"), *Part->GetName()), Part->IsRegistered());
		TestNotNull(FString::Printf(TEXT("%s has an attach parent"), *Part->GetName()), Part->GetAttachParent());
	}
	TestTrue(TEXT("muzzle flash rides the pitch mount"), Pod->MuzzleFlashMesh->GetAttachParent() == Pod->PitchMount);
	TestTrue(TEXT("muzzle flash has a mesh"), Pod->MuzzleFlashMesh->GetStaticMesh() != nullptr);
	TestTrue(TEXT("tracer has a mesh"), Pod->TracerMesh->GetStaticMesh() != nullptr);
	TestTrue(TEXT("muzzle flash has a material"), Pod->MuzzleFlashMesh->GetMaterial(0) != nullptr);

	Pod->PlayShotFx(Pod->GetMuzzleLocation(), Pod->GetMuzzleLocation() + Pod->GetMuzzleForward() * 5000.0f, true);
	TestTrue(TEXT("muzzle flash is a pulse, not a hold"), Pod->MuzzleFlashSeconds < Pod->FireInterval * 0.4f);
	TestTrue(TEXT("muzzle flash is visible"), Pod->MuzzleFlashMesh && Pod->MuzzleFlashMesh->IsVisible());
	TestTrue(TEXT("tracer is visible"), Pod->TracerMesh && Pod->TracerMesh->IsVisible());
	TestTrue(TEXT("tracer ribbon is visible"), Pod->TracerRibbon && Pod->TracerRibbon->IsVisible());
	TestTrue(TEXT("impact flash is visible"), Pod->ImpactFlashMesh && Pod->ImpactFlashMesh->IsVisible());
	TestTrue(TEXT("muzzle light is lit"), Pod->MuzzleLight && Pod->MuzzleLight->Intensity > 0.0f);
	TestTrue(TEXT("tracer is a short fat bolt"), Pod->TracerMesh
		&& Pod->TracerMesh->GetComponentScale().Z > 10.0f
		&& Pod->TracerMesh->GetComponentScale().X > 0.4f);
	int32 VisibleSparks = 0;
	for (UStaticMeshComponent* Spark : Pod->SparkMeshes)
	{
		if (Spark && Spark->IsVisible())
		{
			++VisibleSparks;
		}
	}
	TestTrue(TEXT("impact spawned sparks"), VisibleSparks >= 8);

	Pod->UpdateShotFx(Pod->MuzzleFlashSeconds + 0.01f);
	TestFalse(TEXT("muzzle flash clears"), Pod->MuzzleFlashMesh->IsVisible());
	TestTrue(TEXT("tracer outlasts the muzzle flash"), Pod->TracerMesh->IsVisible());
	TestTrue(TEXT("muzzle light goes dark"), Pod->MuzzleLight->Intensity <= 0.0f);

	Pod->UpdateShotFx(Pod->TracerSeconds);
	TestFalse(TEXT("tracer clears"), Pod->TracerMesh->IsVisible());

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPMinigunCameraFollowsAim, "GalacticPirates.Minigun.CameraFollowsAim", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPMinigunCameraFollowsAim::RunTest(const FString& Parameters)
{
	UClass* ShipClass = LoadClass<AWalkableShip>(nullptr, TEXT("/Game/Ships/Debug/BP_DebugWalkableShip.BP_DebugWalkableShip_C"));
	UClass* CharacterClass = LoadClass<AGalacticPiratesCharacter>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
	if (!TestNotNull(TEXT("debug ship blueprint"), ShipClass) || !TestNotNull(TEXT("character blueprint"), CharacterClass))
	{
		return false;
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Ship = Cast<AWalkableShip>(World->SpawnActor<AActor>(ShipClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams));
	AGalacticPiratesCharacter* Gunner = Cast<AGalacticPiratesCharacter>(World->SpawnActor<AActor>(CharacterClass, FVector(0.0f, 0.0f, 120.0f), FRotator::ZeroRotator, SpawnParams));
	UMinigunPodComponent* Pod = Ship ? Ship->PortMinigun : nullptr;
	if (!TestNotNull(TEXT("ship"), Ship) || !TestNotNull(TEXT("gunner"), Gunner) || !TestNotNull(TEXT("pod"), Pod))
	{
		World->DestroyWorld(false);
		return false;
	}

	if (!Ship->HasActorBegunPlay())
	{
		Ship->DispatchBeginPlay();
	}
	if (!Gunner->HasActorBegunPlay())
	{
		Gunner->DispatchBeginPlay();
	}

	Gunner->BoardShip(Ship);
	Gunner->SetActorLocation(Pod->GetComponentLocation());
	TestTrue(TEXT("gunner occupies the pod"), Pod->TryInteract(Gunner));

	Pod->ApplyAim(35.0f, -20.0f);
	Pod->ApplyGunnerCamera();
	UQuatCamera* Camera = Gunner->GetQuatCameraComponent();
	if (!TestNotNull(TEXT("camera"), Camera) || !TestTrue(TEXT("pitch mount"), Pod->PitchMount != nullptr))
	{
		World->DestroyWorld(false);
		return false;
	}

	TestTrue(TEXT("camera rides the pitch mount"), Camera->GetAttachParent() == Pod->PitchMount);
	TestTrue(TEXT("camera looks down the barrel"),
		FVector::DotProduct(Camera->GetForwardVector().GetSafeNormal(), Pod->GetMuzzleForward().GetSafeNormal()) > 0.95f);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPMinigunDestroysMissiles, "GalacticPirates.Minigun.DestroysMissiles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPMinigunDestroysMissiles::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!World)
	{
		AddError(TEXT("Could not create a game world"));
		return false;
	}

	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHeatseekingMissile* Missile = World->SpawnActor<AHeatseekingMissile>(FVector(0.0f, 0.0f, 200.0f), FRotator::ZeroRotator, SpawnParams);
	AWalkableShip* Armored = World->SpawnActor<AWalkableShip>(FVector(4000.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	if (!TestNotNull(TEXT("missile"), Missile) || !TestNotNull(TEXT("armored ship"), Armored))
	{
		World->DestroyWorld(false);
		return false;
	}

	Armored->ArmorClass = EShipArmorClass::Armored;
	const float HullBefore = Armored->GetHealth();
	TestTrue(TEXT("authority world"), Missile->HasAuthority());
	TestTrue(TEXT("minigun burst destroys missile"), Missile->ApplyMinigunHit(50.0f, nullptr));
	TestTrue(TEXT("missile is gone"), !IsValid(Missile));

	Armored->ApplyShipDamage(6.0f, nullptr, nullptr);
	TestTrue(TEXT("armored hull barely scratched"), Armored->GetHealth() > HullBefore - 10.0f);
	TestTrue(TEXT("armored hull did take a chip"), Armored->GetHealth() < HullBefore);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPEnemyShipCrewsStations, "GalacticPirates.CrewAI.EnemyShipsCrewStations", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPEnemyShipCrewsStations::RunTest(const FString& Parameters)
{
	UClass* ShipClass = LoadClass<AWalkableShip>(nullptr, TEXT("/Game/Ships/Debug/BP_DebugWalkableShip.BP_DebugWalkableShip_C"));
	UClass* CharacterClass = LoadClass<AGalacticPiratesCharacter>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
	if (!TestNotNull(TEXT("ship class"), ShipClass) || !TestNotNull(TEXT("character class"), CharacterClass))
	{
		return false;
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* PlayerShip = Cast<AWalkableShip>(World->SpawnActor<AActor>(ShipClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams));
	AWalkableShip* EnemyShip = Cast<AWalkableShip>(World->SpawnActor<AActor>(ShipClass, FVector(7000.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams));
	AGalacticPiratesCharacter* Human = Cast<AGalacticPiratesCharacter>(World->SpawnActor<AActor>(CharacterClass, FVector(0.0f, 0.0f, 120.0f), FRotator::ZeroRotator, SpawnParams));
	if (!TestNotNull(TEXT("player ship"), PlayerShip) || !TestNotNull(TEXT("enemy ship"), EnemyShip) || !TestNotNull(TEXT("human"), Human))
	{
		World->DestroyWorld(false);
		return false;
	}

	if (PlayerShip->HoloPoi)
	{
		PlayerShip->HoloPoi->Kind = EHoloMapPoiKind::OwnShip;
	}
	if (!PlayerShip->HasActorBegunPlay()) { PlayerShip->DispatchBeginPlay(); }
	if (!EnemyShip->HasActorBegunPlay()) { EnemyShip->DispatchBeginPlay(); }
	if (!Human->HasActorBegunPlay()) { Human->DispatchBeginPlay(); }

	Human->BoardShip(PlayerShip);
	TestTrue(TEXT("player ship has a human"), PlayerShip->HasHumanCrew());
	TestFalse(TEXT("enemy ship has no human"), EnemyShip->HasHumanCrew());

	if (!TestNotNull(TEXT("crew AI"), EnemyShip->CrewAI))
	{
		World->DestroyWorld(false);
		return false;
	}

	EnemyShip->CrewAI->SpawnCrew();
	TestEqual(TEXT("four crew spawned"), EnemyShip->CrewAI->GetCrewCount(), 4);
	TestTrue(TEXT("AI is at the helm"), EnemyShip->GetCurrentPilot() && EnemyShip->GetCurrentPilot()->IsAiCrew());
	TestTrue(TEXT("port gun is manned by AI"), EnemyShip->PortMinigun && EnemyShip->PortMinigun->GetGunner() && EnemyShip->PortMinigun->GetGunner()->IsAiCrew());
	TestTrue(TEXT("starboard gun is manned by AI"), EnemyShip->StarboardMinigun && EnemyShip->StarboardMinigun->GetGunner() && EnemyShip->StarboardMinigun->GetGunner()->IsAiCrew());
	TestFalse(TEXT("orbit AI still treats this as an AI ship"), EnemyShip->HasHumanCrew());

	if (EnemyShip->PortMinigun)
	{
		EnemyShip->PortMinigun->AimAtWorldLocation(PlayerShip->GetActorLocation());
		const FVector ToPlayer = (PlayerShip->GetActorLocation() - EnemyShip->PortMinigun->GetMuzzleLocation()).GetSafeNormal();
		TestTrue(TEXT("port gun points at the player ship"),
			FVector::DotProduct(EnemyShip->PortMinigun->GetMuzzleForward(), ToPlayer) > 0.85f);
	}

	World->DestroyWorld(false);
	return true;
}

#endif
