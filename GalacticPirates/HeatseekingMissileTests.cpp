#include "HeatseekingMissile.h"
#include "WalkableShip.h"
#include "BulldogFighter.h"
#include "ShipMissileSalvoComponent.h"
#include "HoloMapPoiComponent.h"
#include "HoloMapTypes.h"
#include "HolographicMapTableComponent.h"
#include "MinigunPodComponent.h"
#include "CombatTypes.h"
#include "HullHealthComponent.h"
#include "ShipCrewAiComponent.h"
#include "ShipOrbitAiComponent.h"
#include "ShipMovementComponent.h"
#include "HelmComponent.h"
#include "OccupancyComponent.h"
#include "CraftWreck.h"
#include "SpaceCraft.h"
#include "WeaponHardpointComponent.h"
#include "WeaponComponent.h"
#include "ShipPulseCannonComponent.h"
#include "GalacticPiratesCharacter.h"
#include "QuatCamera.h"
#include "HoloMapScanRange.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "ShipMovementComponent.h"

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
			Locked = Cast<AWalkableShip>(Sample->FindHottestTarget());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPHullArmorScalesBallistic, "GalacticPirates.Combat.HullArmorScalesBallistic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPHullArmorScalesBallistic::RunTest(const FString& Parameters)
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
	AWalkableShip* Armored = World->SpawnActor<AWalkableShip>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	ABulldogFighter* Fighter = World->SpawnActor<ABulldogFighter>(FVector(2000.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	if (!TestNotNull(TEXT("armored ship"), Armored) || !TestNotNull(TEXT("fighter"), Fighter) || !TestNotNull(TEXT("ship hull"), Armored->HullHealth) || !TestNotNull(TEXT("fighter hull"), Fighter->HullHealth.Get()))
	{
		World->DestroyWorld(false);
		return false;
	}

	Armored->ArmorClass = EShipArmorClass::Armored;
	Armored->HullHealth->ArmorClass = EShipArmorClass::Armored;
	const float ArmoredBefore = Armored->GetHealth();
	FSpaceDamageEvent Ballistic;
	Ballistic.Amount = 40.0f;
	Ballistic.Kind = ESpaceDamageKind::Ballistic;
	const float Applied = GPApplySpaceDamage(Armored, Ballistic);
	TestTrue(TEXT("armored ballistic applies 6"), FMath::IsNearlyEqual(Applied, 6.0f, 0.01f));
	TestTrue(TEXT("armored hull dropped by 6"), FMath::IsNearlyEqual(Armored->GetHealth(), ArmoredBefore - 6.0f, 0.01f));

	const float LightBefore = Fighter->HullHealth->GetHealth();
	const float LightApplied = GPApplySpaceDamage(Fighter, Ballistic);
	TestTrue(TEXT("light ballistic applies full 40"), FMath::IsNearlyEqual(LightApplied, 40.0f, 0.01f));
	TestTrue(TEXT("fighter hull dropped by 40"), FMath::IsNearlyEqual(Fighter->HullHealth->GetHealth(), LightBefore - 40.0f, 0.01f));

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
	TestTrue(TEXT("port gun stays unmanned until used"), EnemyShip->PortMinigun && !EnemyShip->PortMinigun->GetGunner());
	TestTrue(TEXT("starboard gun stays unmanned until used"), EnemyShip->StarboardMinigun && !EnemyShip->StarboardMinigun->GetGunner());
	TestFalse(TEXT("orbit AI still treats this as an AI ship"), EnemyShip->HasHumanCrew());

	if (EnemyShip->PortMinigun)
	{
		const FVector RestForward = EnemyShip->PortMinigun->GetMuzzleForward();
		EnemyShip->PortMinigun->AimAtWorldLocation(PlayerShip->GetActorLocation());
		TestTrue(TEXT("unmanned gun does not aim"),
			FVector::DotProduct(EnemyShip->PortMinigun->GetMuzzleForward(), RestForward) > 0.99f);

		AGalacticPiratesCharacter* TempGunner = Cast<AGalacticPiratesCharacter>(World->SpawnActor<AActor>(
			CharacterClass, EnemyShip->PortMinigun->GetComponentLocation(), FRotator::ZeroRotator, SpawnParams));
		if (TempGunner)
		{
			if (!TempGunner->HasActorBegunPlay()) { TempGunner->DispatchBeginPlay(); }
			TempGunner->BoardShip(EnemyShip);
			TempGunner->SetActorLocation(EnemyShip->PortMinigun->GetComponentLocation());
			EnemyShip->PortMinigun->TryInteract(TempGunner);
			if (TestTrue(TEXT("temp gunner occupies port gun"), EnemyShip->PortMinigun->GetGunner() == TempGunner))
			{
				EnemyShip->PortMinigun->AimAtWorldLocation(PlayerShip->GetActorLocation());
				const FVector ToPlayer = (PlayerShip->GetActorLocation() - EnemyShip->PortMinigun->GetMuzzleLocation()).GetSafeNormal();
				TestTrue(TEXT("manned gun points at the player ship"),
					FVector::DotProduct(EnemyShip->PortMinigun->GetMuzzleForward(), ToPlayer) > 0.85f);
				EnemyShip->PortMinigun->ForceRelease();
				TestTrue(TEXT("released gun returns to rest"),
					FVector::DotProduct(EnemyShip->PortMinigun->GetMuzzleForward(), RestForward) > 0.99f);
			}
		}
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPShipMoveProbe, "GalacticPirates.Ship.MoveProbe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPShipMoveProbe::RunTest(const FString& Parameters)
{
	auto LogShip = [this](const TCHAR* Tag, AWalkableShip* Ship)
	{
		if (!Ship || !Ship->ShipMovement)
		{
			AddError(FString::Printf(TEXT("[%s] missing ship or movement"), Tag));
			return;
		}
		UShipMovementComponent* Move = Ship->ShipMovement;
		USceneComponent* Root = Ship->GetRootComponent();
		const FString Line = FString::Printf(
			TEXT("[ShipMoveProbe] %s name=%s auth=%d wreck=%d loc=%s yaw=%.1f lin=%.1f ang=%.1f thrust=%s rot=%s override=%d tickOn=%d mobility=%d replicateMove=%d orbitOn=%d crewOn=%d pilot=%s"),
			Tag,
			*Ship->GetName(),
			Ship->HasAuthority() ? 1 : 0,
			Ship->IsWrecked() ? 1 : 0,
			*Ship->GetActorLocation().ToCompactString(),
			Ship->GetActorRotation().Yaw,
			Move->GetLinearVelocity().Size(),
			Move->GetAngularVelocity().Size(),
			*Move->GetAppliedThrustInput().ToCompactString(),
			*Move->GetAppliedRotationInput().ToCompactString(),
			Move->IsVelocityOverride() ? 1 : 0,
			Move->PrimaryComponentTick.IsTickFunctionEnabled() ? 1 : 0,
			Root ? static_cast<int32>(Root->Mobility.GetValue()) : -1,
			Ship->IsReplicatingMovement() ? 1 : 0,
			(Ship->OrbitAI && Ship->OrbitAI->bEnabled) ? 1 : 0,
			(Ship->CrewAI && Ship->CrewAI->bEnabled) ? 1 : 0,
			*GetNameSafe(Ship->GetCurrentPilot()));
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Line);
		AddInfo(Line);
	};

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("world"), World))
	{
		return false;
	}

	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	auto TickWorld = [World](float Seconds)
	{
		const float Dt = 1.0f / 60.0f;
		const int32 Steps = FMath::Max(1, FMath::RoundToInt(Seconds / Dt));
		for (int32 i = 0; i < Steps; ++i)
		{
			for (TActorIterator<AWalkableShip> It(World); It; ++It)
			{
				if (AWalkableShip* LiveShip = *It)
				{
					if (LiveShip->ShipMovement)
					{
						LiveShip->ShipMovement->TickPhysics(Dt);
					}
				}
			}
		}
	};

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	TArray<AWalkableShip*> Ships;
	AWalkableShip* NativeShip = World->SpawnActor<AWalkableShip>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	Ships.Add(NativeShip);

	UClass* BpClass = LoadClass<AWalkableShip>(nullptr, TEXT("/Game/Ships/Debug/BP_DebugWalkableShip.BP_DebugWalkableShip_C"));
	if (BpClass)
	{
		AWalkableShip* BpShip = Cast<AWalkableShip>(World->SpawnActor<AActor>(BpClass, FVector(0.0f, 4000.0f, 0.0f), FRotator::ZeroRotator, SpawnParams));
		if (BpShip)
		{
			if (!BpShip->HasActorBegunPlay())
			{
				BpShip->DispatchBeginPlay();
			}
			Ships.Add(BpShip);
		}
	}
	else
	{
		AddInfo(TEXT("[ShipMoveProbe] BP_DebugWalkableShip did not load; native class only"));
	}

	bool bDirectMoved = false;
	bool bDirectTurned = false;
	bool bPilotMoved = false;

	for (AWalkableShip* Ship : Ships)
	{
		if (!TestNotNull(TEXT("ship"), Ship) || !TestNotNull(TEXT("movement"), Ship->ShipMovement))
		{
			World->DestroyWorld(false);
			return false;
		}

		if (Ship->OrbitAI)
		{
			Ship->OrbitAI->bEnabled = false;
		}
		if (Ship->CrewAI)
		{
			Ship->CrewAI->bEnabled = false;
		}
		Ship->ShipMovement->SetVelocityOverride(FVector::ZeroVector, FVector::ZeroVector, false);
		LogShip(TEXT("START"), Ship);

		const FVector StartLoc = Ship->GetActorLocation();
		const float StartYaw = Ship->GetActorRotation().Yaw;
		Ship->ShipMovement->SetThrustInput(FVector(1.0f, 0.0f, 0.0f));
		Ship->ShipMovement->SetRotationInput(FVector(0.0f, 0.0f, 1.0f));
		LogShip(TEXT("DIRECT_SET"), Ship);
		TickWorld(1.2f);
		LogShip(TEXT("AFTER_DIRECT"), Ship);

		const float Dist = FVector::Dist(Ship->GetActorLocation(), StartLoc);
		const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(StartYaw, Ship->GetActorRotation().Yaw));
		UShipMovementComponent* MoveComp = Ship->ShipMovement;
		const float Accel = MoveComp->ForwardThrustPower / FMath::Max(MoveComp->ShipMass, 1.0f);
		const float MinDist = FMath::Max(2.0f, Accel * 0.25f);
		AddInfo(FString::Printf(TEXT("[ShipMoveProbe] after-direct %s dist=%.1f yawDelta=%.1f accel=%.2f minDist=%.1f lin=%.2f ang=%.3f"),
			*Ship->GetName(), Dist, YawDelta, Accel, MinDist, MoveComp->GetLinearVelocity().Size(), MoveComp->GetAngularVelocity().Size()));
		bDirectMoved |= Dist > MinDist;
		bDirectTurned |= MoveComp->GetAngularVelocity().Size() > 0.001f;
		{
			const FString MoveMsg = FString::Printf(TEXT("%s integrated thrust (dist=%.1f min=%.1f)"), *Ship->GetName(), Dist, MinDist);
			const FString VelMsg = FString::Printf(TEXT("%s has linear velocity after thrust (lin=%.2f)"), *Ship->GetName(), MoveComp->GetLinearVelocity().Size());
			const FString YawMsg = FString::Printf(TEXT("%s has angular velocity after torque (ang=%.3f)"), *Ship->GetName(), MoveComp->GetAngularVelocity().Size());
			TestTrue(*MoveMsg, Dist > MinDist);
			TestTrue(*VelMsg, MoveComp->GetLinearVelocity().Size() > 0.5f);
			TestTrue(*YawMsg, MoveComp->GetAngularVelocity().Size() > 0.001f);
		}

		Ship->ShipMovement->SetThrustInput(FVector::ZeroVector);
		Ship->ShipMovement->SetRotationInput(FVector::ZeroVector);
		Ship->ShipMovement->SetLinearVelocity(FVector::ZeroVector);
		Ship->ShipMovement->SetAngularVelocity(FVector::ZeroVector);

		AGalacticPiratesCharacter* Pilot = nullptr;
		UClass* CharacterClass = LoadClass<AGalacticPiratesCharacter>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
		if (CharacterClass)
		{
			Pilot = Cast<AGalacticPiratesCharacter>(World->SpawnActor<AActor>(CharacterClass, Ship->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f), FRotator::ZeroRotator, SpawnParams));
		}
		if (Pilot && !Pilot->HasActorBegunPlay())
		{
			Pilot->DispatchBeginPlay();
		}
		if (Pilot)
		{
			Pilot->BoardShip(Ship);
			if (Ship->Helm)
			{
				if (Ship->HelmOccupancy)
				{
					Pilot->SetActorLocation(Ship->HelmOccupancy->GetComponentLocation());
				}
				else
				{
					Pilot->SetActorLocation(Ship->Helm->GetComponentLocation());
				}
				if (!Ship->Helm->TryInteract(Pilot))
				{
					Ship->RequestPilotAssignment(Pilot);
				}
			}
			Ship->ApplyPilotInput(Pilot, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f));
		}
		LogShip(TEXT("PILOT_SET"), Ship);
		const FVector PilotStart = Ship->GetActorLocation();
		TickWorld(1.2f);
		LogShip(TEXT("AFTER_PILOT"), Ship);
		const float PilotDist = FVector::Dist(Ship->GetActorLocation(), PilotStart);
		AddInfo(FString::Printf(TEXT("[ShipMoveProbe] after-pilot %s dist=%.1f currentPilot=%s helmOcc=%d"),
			*Ship->GetName(),
			PilotDist,
			*GetNameSafe(Ship->GetCurrentPilot()),
			(Ship->Helm && Ship->Helm->IsOccupied()) ? 1 : 0));
		bPilotMoved |= PilotDist > MinDist;
		{
			const FString PilotMsg = FString::Printf(TEXT("%s moved under ApplyPilotInput (dist=%.1f min=%.1f pilot=%s)"), *Ship->GetName(), PilotDist, MinDist, *GetNameSafe(Ship->GetCurrentPilot()));
			TestTrue(*PilotMsg, PilotDist > MinDist);
		}
	}

	if (!bDirectMoved && !bDirectTurned)
	{
		AddError(TEXT("[ShipMoveProbe] diagnosis: movement component is not integrating"));
	}
	else if (bDirectMoved && !bPilotMoved)
	{
		AddError(TEXT("[ShipMoveProbe] diagnosis: physics works; ApplyPilotInput is rejected"));
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPBulldogFiresOneForwardMissile, "GalacticPirates.Fighter.BulldogOneForwardMissile", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPBulldogFiresOneForwardMissile::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("world"), World))
	{
		return false;
	}

	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Target = World->SpawnActor<AWalkableShip>(FVector(4000.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	ABulldogFighter* Fighter = World->SpawnActor<ABulldogFighter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!TestNotNull(TEXT("target"), Target) || !TestNotNull(TEXT("fighter"), Fighter))
	{
		World->DestroyWorld(false);
		return false;
	}

	if (!Fighter->HasActorBegunPlay())
	{
		Fighter->DispatchBeginPlay();
	}
	if (Fighter->ShipMovement)
	{
		Fighter->ShipMovement->SetLinearVelocity(FVector(1000.0f, 0.0f, 0.0f));
	}

	TestTrue(TEXT("first missile launches"), Fighter->FireSeekingMissile(Target));
	TestFalse(TEXT("second missile is blocked while one is in flight"), Fighter->FireSeekingMissile(Target));

	AHeatseekingMissile* Missile = Fighter->GetActiveMissile();
	if (!TestNotNull(TEXT("active missile"), Missile))
	{
		World->DestroyWorld(false);
		return false;
	}

	TestTrue(TEXT("missile faces fighter forward"),
		FVector::DotProduct(Missile->GetActorForwardVector(), Fighter->GetActorForwardVector()) > 0.99f);
	TestEqual(TEXT("lock is the player ship"), Missile->GetLockedTarget(), Target);

	const FVector ExpectedVel = Fighter->GetActorForwardVector() * Fighter->MissileLaunchSpeed + FVector(1000.0f, 0.0f, 0.0f);
	if (Missile->ProjectileMovement)
	{
		TestTrue(TEXT("launch velocity includes fighter speed"),
			FVector::Dist(Missile->ProjectileMovement->Velocity, ExpectedVel) < 25.0f);
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAffiliationIsFriendOrFoeOnly, "GalacticPirates.Craft.AffiliationIsFriendOrFoeOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPAffiliationIsFriendOrFoeOnly::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("world"), World))
	{
		return false;
	}

	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Red = World->SpawnActor<AWalkableShip>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	AWalkableShip* Blue = World->SpawnActor<AWalkableShip>(FVector(4000.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	AWalkableShip* Untagged = World->SpawnActor<AWalkableShip>(FVector(8000.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	ABulldogFighter* Escort = World->SpawnActor<ABulldogFighter>(FVector(200.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	if (!TestNotNull(TEXT("red"), Red) || !TestNotNull(TEXT("blue"), Blue) || !TestNotNull(TEXT("untagged"), Untagged) || !TestNotNull(TEXT("escort"), Escort))
	{
		World->DestroyWorld(false);
		return false;
	}

	Red->AffiliationId = FName(TEXT("Red"));
	Blue->AffiliationId = FName(TEXT("Blue"));
	Escort->RegisterHomeCraft(Red);

	AWalkableShip* RedAlly = World->SpawnActor<AWalkableShip>(FVector(-2000.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	AWalkableShip* OtherUntagged = World->SpawnActor<AWalkableShip>(FVector(12000.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	if (!TestNotNull(TEXT("red ally"), RedAlly) || !TestNotNull(TEXT("other untagged"), OtherUntagged))
	{
		World->DestroyWorld(false);
		return false;
	}
	RedAlly->AffiliationId = FName(TEXT("Red"));

	TestFalse(TEXT("same actor is not hostile"), GPAreHostile(Red, Red));
	TestTrue(TEXT("walkable ships treat other walkables as enemies"), GPAreHostile(Red, RedAlly));
	TestTrue(TEXT("different tags are hostile"), GPAreHostile(Red, Blue));
	TestTrue(TEXT("untagged is hostile to tagged"), GPAreHostile(Red, Untagged));
	TestTrue(TEXT("two untagged crafts are hostile"), GPAreHostile(Untagged, OtherUntagged));
	TestTrue(TEXT("home-craft fighter is still an enemy until deploy FOF exists"), GPAreHostile(Escort, Red));
	TestTrue(TEXT("escort is hostile to the other tag"), GPAreHostile(Escort, Blue));
	TestFalse(TEXT("deployed-fighter hook is unused"), GPIsOwnDeployedFighter(Red, Escort));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPOccupancyIgnoresAffiliation, "GalacticPirates.Craft.OccupancyIgnoresAffiliation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPOccupancyIgnoresAffiliation::RunTest(const FString& Parameters)
{
	UClass* CharacterClass = LoadClass<AGalacticPiratesCharacter>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
	if (!TestNotNull(TEXT("character class"), CharacterClass))
	{
		return false;
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Navy = World->SpawnActor<AWalkableShip>(FVector(-2000.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	ABulldogFighter* Fighter = World->SpawnActor<ABulldogFighter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	AGalacticPiratesCharacter* Boarder = Cast<AGalacticPiratesCharacter>(World->SpawnActor<AActor>(CharacterClass, FVector(0.0f, 0.0f, 80.0f), FRotator::ZeroRotator, SpawnParams));
	if (!TestNotNull(TEXT("navy"), Navy) || !TestNotNull(TEXT("fighter"), Fighter) || !TestNotNull(TEXT("boarder"), Boarder))
	{
		World->DestroyWorld(false);
		return false;
	}

	Navy->AffiliationId = FName(TEXT("Navy"));
	Fighter->AffiliationId = FName(TEXT("Pirate"));
	Fighter->RegisterHomeCraft(Navy);
	if (!Fighter->HasActorBegunPlay()) { Fighter->DispatchBeginPlay(); }
	if (!Boarder->HasActorBegunPlay()) { Boarder->DispatchBeginPlay(); }
	Boarder->SetActorLocation(Fighter->CockpitOccupancy->GetComponentLocation());

	TestTrue(TEXT("own pirate tag stays hostile to navy home"), GPAreHostile(Fighter, Navy));
	TestTrue(TEXT("hostile occupancy is allowed"), Fighter->TryCockpitInteract(Boarder));
	TestTrue(TEXT("boarder is the occupant"), Fighter->CockpitOccupancy && Fighter->CockpitOccupancy->IsOccupant(Boarder));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPHardpointSwapsWeaponClass, "GalacticPirates.Craft.HardpointSwapsWeaponClass", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPHardpointSwapsWeaponClass::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Ship = World->SpawnActor<AWalkableShip>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!TestNotNull(TEXT("ship"), Ship) || !TestNotNull(TEXT("pulse hardpoint"), Ship->PulseHardpoint))
	{
		World->DestroyWorld(false);
		return false;
	}

	if (!Ship->HasActorBegunPlay()) { Ship->DispatchBeginPlay(); }
	TestTrue(TEXT("pulse is the starting loadout"), Ship->PulseHardpoint->GetEquippedWeapon() == static_cast<UWeaponComponent*>(Ship->PulseCannon));
	TestTrue(TEXT("salvo class equips"), Ship->PulseHardpoint->EquipWeaponClass(UShipMissileSalvoComponent::StaticClass()));
	TestNotNull(TEXT("new weapon exists"), Ship->PulseHardpoint->GetEquippedWeapon());
	TestEqual(TEXT("hardpoint class is salvo"), Ship->PulseHardpoint->GetEquippedWeapon()->GetClass(), UShipMissileSalvoComponent::StaticClass());

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPCraftWreckBlocksOccupancy, "GalacticPirates.Craft.WreckBlocksOccupancy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPCraftWreckBlocksOccupancy::RunTest(const FString& Parameters)
{
	UClass* CharacterClass = LoadClass<AGalacticPiratesCharacter>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
	if (!TestNotNull(TEXT("character class"), CharacterClass))
	{
		return false;
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABulldogFighter* Fighter = World->SpawnActor<ABulldogFighter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	AWalkableShip* Ship = World->SpawnActor<AWalkableShip>(FVector(5000.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	AGalacticPiratesCharacter* Pilot = Cast<AGalacticPiratesCharacter>(World->SpawnActor<AActor>(CharacterClass, FVector(0.0f, 0.0f, 80.0f), FRotator::ZeroRotator, SpawnParams));
	if (!TestNotNull(TEXT("fighter"), Fighter) || !TestNotNull(TEXT("ship"), Ship) || !TestNotNull(TEXT("pilot"), Pilot))
	{
		World->DestroyWorld(false);
		return false;
	}

	if (!Fighter->HasActorBegunPlay()) { Fighter->DispatchBeginPlay(); }
	if (!Ship->HasActorBegunPlay()) { Ship->DispatchBeginPlay(); }
	if (!Pilot->HasActorBegunPlay()) { Pilot->DispatchBeginPlay(); }

	Fighter->NotifyCraftWrecked();
	TestTrue(TEXT("fighter is wrecked"), Fighter->IsCraftWrecked());
	TestFalse(TEXT("wrecked fighter refuses occupy"), Fighter->TryCockpitInteract(Pilot));

	Ship->Explode();
	TestTrue(TEXT("capital is wrecked"), Ship->IsCraftWrecked());
	TestTrue(TEXT("movement disabled"), Ship->ShipMovement && !Ship->ShipMovement->IsComponentTickEnabled());
	if (Ship->HelmOccupancy)
	{
		Pilot->SetActorLocation(Ship->HelmOccupancy->GetComponentLocation());
		TestFalse(TEXT("wrecked helm refuses occupy"), Ship->HelmOccupancy->TryOccupy(Pilot));
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPCraftNetRejectsAlwaysRelevant, "GalacticPirates.Craft.NetRejectsAlwaysRelevant", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPCraftNetRejectsAlwaysRelevant::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Ship = World->SpawnActor<AWalkableShip>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	ABulldogFighter* Fighter = World->SpawnActor<ABulldogFighter>(FVector(400.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	AHeatseekingMissile* Missile = World->SpawnActor<AHeatseekingMissile>(FVector(800.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	if (!TestNotNull(TEXT("ship"), Ship) || !TestNotNull(TEXT("fighter"), Fighter) || !TestNotNull(TEXT("missile"), Missile))
	{
		World->DestroyWorld(false);
		return false;
	}

	TestFalse(TEXT("capital is not always relevant"), Ship->bAlwaysRelevant);
	TestFalse(TEXT("fighter is not always relevant"), Fighter->bAlwaysRelevant);
	TestFalse(TEXT("missile is not always relevant"), Missile->bAlwaysRelevant);
	TestEqual(TEXT("capital net rate"), Ship->GetNetUpdateFrequency(), 30.0f);
	TestEqual(TEXT("fighter net rate"), Fighter->GetNetUpdateFrequency(), 45.0f);
	TestEqual(TEXT("missile net rate"), Missile->GetNetUpdateFrequency(), 60.0f);
	if (!Ship->HasActorBegunPlay()) { Ship->DispatchBeginPlay(); }
	if (!Fighter->HasActorBegunPlay()) { Fighter->DispatchBeginPlay(); }
	TestTrue(TEXT("helm occupancy replicates"), Ship->HelmOccupancy && Ship->HelmOccupancy->GetIsReplicated());
	TestTrue(TEXT("hull replicates"), Ship->HullHealth && Ship->HullHealth->GetIsReplicated());
	TestTrue(TEXT("hardpoint replicates"), Ship->PulseHardpoint && Ship->PulseHardpoint->GetIsReplicated());
	TestTrue(TEXT("cockpit occupancy replicates"), Fighter->CockpitOccupancy && Fighter->CockpitOccupancy->GetIsReplicated());
	TestTrue(TEXT("fighter hull replicates"), Fighter->GetHullHealth() && Fighter->GetHullHealth()->GetIsReplicated());
	if (Ship->PortMinigun)
	{
		if (UOccupancyComponent* Seat = Ship->PortMinigun->GetOccupancy())
		{
			TestTrue(TEXT("minigun occupancy replicates"), Seat->GetIsReplicated());
		}
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPStationsShareOccupancy, "GalacticPirates.Craft.StationsShareOccupancy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPStationsShareOccupancy::RunTest(const FString& Parameters)
{
	UClass* CharacterClass = LoadClass<AGalacticPiratesCharacter>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
	if (!TestNotNull(TEXT("character class"), CharacterClass))
	{
		return false;
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Ship = World->SpawnActor<AWalkableShip>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	AGalacticPiratesCharacter* Crew = Cast<AGalacticPiratesCharacter>(World->SpawnActor<AActor>(CharacterClass, FVector(0.0f, 0.0f, 120.0f), FRotator::ZeroRotator, SpawnParams));
	if (!TestNotNull(TEXT("ship"), Ship) || !TestNotNull(TEXT("crew"), Crew))
	{
		World->DestroyWorld(false);
		return false;
	}
	if (!Ship->HasActorBegunPlay()) { Ship->DispatchBeginPlay(); }
	if (!Crew->HasActorBegunPlay()) { Crew->DispatchBeginPlay(); }

	Crew->BoardShip(Ship);
	Crew->SetActorLocation(Ship->Helm->GetComponentLocation());
	TestTrue(TEXT("helm occupy"), Ship->Helm->TryInteract(Crew));
	TestTrue(TEXT("helm occupancy matches pilot"), Ship->HelmOccupancy && Ship->HelmOccupancy->IsOccupant(Crew));
	TestTrue(TEXT("current pilot is crew"), Ship->GetCurrentPilot() == Crew);

	TestTrue(TEXT("leave helm"), Ship->Helm->TryInteract(Crew));
	Crew->SetActorLocation(Ship->PortMinigun->GetComponentLocation());
	TestTrue(TEXT("minigun occupy"), Ship->PortMinigun->TryInteract(Crew));
	TestTrue(TEXT("minigun occupancy matches gunner"), Ship->PortMinigun->GetOccupancy() && Ship->PortMinigun->GetOccupancy()->IsOccupant(Crew));
	TestTrue(TEXT("get gunner uses occupancy"), Ship->PortMinigun->GetGunner() == Crew);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPHostileFighterOccupyIsAllowed, "GalacticPirates.Craft.HostileFighterOccupyIsAllowed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPHostileFighterOccupyIsAllowed::RunTest(const FString& Parameters)
{
	UClass* CharacterClass = LoadClass<AGalacticPiratesCharacter>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
	if (!TestNotNull(TEXT("character class"), CharacterClass))
	{
		return false;
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Navy = World->SpawnActor<AWalkableShip>(FVector(-8000.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	ABulldogFighter* Fighter = World->SpawnActor<ABulldogFighter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	AGalacticPiratesCharacter* Boarder = Cast<AGalacticPiratesCharacter>(World->SpawnActor<AActor>(CharacterClass, FVector(0.0f, 0.0f, 80.0f), FRotator::ZeroRotator, SpawnParams));
	if (!TestNotNull(TEXT("navy"), Navy) || !TestNotNull(TEXT("fighter"), Fighter) || !TestNotNull(TEXT("boarder"), Boarder))
	{
		World->DestroyWorld(false);
		return false;
	}

	Navy->AffiliationId = FName(TEXT("Navy"));
	Fighter->AffiliationId = FName(TEXT("Pirate"));
	if (!Fighter->HasActorBegunPlay()) { Fighter->DispatchBeginPlay(); }
	if (!Boarder->HasActorBegunPlay()) { Boarder->DispatchBeginPlay(); }
	Boarder->SetActorLocation(Fighter->CockpitOccupancy->GetComponentLocation());

	TestTrue(TEXT("tags are hostile"), GPAreHostile(Fighter, Navy));
	TestTrue(TEXT("second body can take the hostile fighter"), Fighter->TryCockpitInteract(Boarder));
	TestTrue(TEXT("cockpit occupant replicated field is set"), Fighter->CockpitOccupancy && Fighter->CockpitOccupancy->IsOccupant(Boarder));
	TestTrue(TEXT("hull is still live on that craft"), Fighter->GetHullHealth() && !Fighter->GetHullHealth()->IsDestroyed());

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPHoloMapEnemyPrimitives, "GalacticPirates.HoloMap.EnemyPrimitives", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPHoloMapEnemyPrimitives::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Viewer = World->SpawnActor<AWalkableShip>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	AWalkableShip* Enemy = World->SpawnActor<AWalkableShip>(FVector(4000.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	ABulldogFighter* Fighter = World->SpawnActor<ABulldogFighter>(FVector(2000.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	if (!TestNotNull(TEXT("viewer"), Viewer) || !TestNotNull(TEXT("enemy"), Enemy) || !TestNotNull(TEXT("fighter"), Fighter) || !TestNotNull(TEXT("map"), Viewer->MapTable))
	{
		World->DestroyWorld(false);
		return false;
	}

	Viewer->AffiliationId = FName(TEXT("Navy"));
	Enemy->AffiliationId = FName(TEXT("Navy"));
	Fighter->RegisterHomeCraft(Viewer);
	if (!Viewer->HasActorBegunPlay()) { Viewer->DispatchBeginPlay(); }
	if (!Enemy->HasActorBegunPlay()) { Enemy->DispatchBeginPlay(); }
	if (!Fighter->HasActorBegunPlay()) { Fighter->DispatchBeginPlay(); }

	TestTrue(TEXT("enemy walkable defaults to a cube"), Enemy->HoloPoi && Enemy->HoloPoi->GetResolvedPrimitive() == EHoloMapPrimitive::Cube);
	TestTrue(TEXT("fighter defaults to a triangle"), Fighter->HoloPoi && Fighter->HoloPoi->GetResolvedPrimitive() == EHoloMapPrimitive::Triangle);
	if (UStaticMesh* CubeMesh = Enemy->HoloPoi->ResolveMarkerMesh())
	{
		TestTrue(TEXT("holopoi cube resolves the cube mesh"), CubeMesh->GetName().Contains(TEXT("Cube")));
		TestFalse(TEXT("holopoi cube does not resolve a cone"), CubeMesh->GetName().Contains(TEXT("Cone")));
	}
	else
	{
		AddError(TEXT("holopoi cube ResolveMarkerMesh was null"));
	}
	if (UStaticMesh* ConeAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone")))
	{
		Enemy->HoloPoi->OverrideMesh = ConeAsset;
		if (UStaticMesh* CubeMesh = Enemy->HoloPoi->ResolveMarkerMesh())
		{
			TestTrue(TEXT("cube primitive wins over cone OverrideMesh"), CubeMesh->GetName().Contains(TEXT("Cube")));
		}
	}

	const FVector SpawnOffset = Viewer->GetActorForwardVector() * 11000.0f
		+ Viewer->GetActorRightVector() * 4200.0f
		+ Viewer->GetActorUpVector() * 800.0f;
	const float SpawnDist = SpawnOffset.Size();
	ABulldogFighter* SpawnedFighter = World->SpawnActor<ABulldogFighter>(Viewer->GetActorLocation() + SpawnOffset, FRotator::ZeroRotator, SpawnParams);
	if (!TestNotNull(TEXT("spawn-distance fighter"), SpawnedFighter))
	{
		World->DestroyWorld(false);
		return false;
	}
	SpawnedFighter->RegisterHomeCraft(Viewer);
	if (!SpawnedFighter->HasActorBegunPlay()) { SpawnedFighter->DispatchBeginPlay(); }

	TestTrue(TEXT("scan range is the original 125m bubble"), FMath::IsNearlyEqual(GPHoloMapScanRangeCm(), 12500.0f));
	TestTrue(TEXT("fighter spawn sits inside that bubble"), SpawnDist <= GPHoloMapScanRangeCm());

	Viewer->MapTable->ScanRangeCm = GPHoloMapScanRangeCm();
	Viewer->MapTable->RebuildTrackedPois();

	bool bFoundEnemyCube = false;
	bool bFoundFighterTriangle = false;
	bool bFoundSpawnedFighter = false;
	FRotator EnemyRot = FRotator::ZeroRotator;
	FVector EnemyScale = FVector::ZeroVector;
	for (const FHoloMapTrackedPoi& Poi : Viewer->MapTable->GetTrackedPois())
	{
		if (Poi.Actor.Get() == Enemy)
		{
			bFoundEnemyCube = Poi.Kind == EHoloMapPoiKind::EnemyShip && Poi.Primitive == EHoloMapPrimitive::Cube;
			EnemyRot = Poi.RelativeRotation;
			EnemyScale = Poi.MarkerScale;
		}
		if (Poi.Actor.Get() == Fighter)
		{
			bFoundFighterTriangle = Poi.Kind == EHoloMapPoiKind::EnemyShip && Poi.Primitive == EHoloMapPrimitive::Triangle;
		}
		if (Poi.Actor.Get() == SpawnedFighter)
		{
			bFoundSpawnedFighter = Poi.Primitive == EHoloMapPrimitive::Triangle;
		}
	}

	TestTrue(TEXT("same-tag walkable is an enemy cube on the map"), bFoundEnemyCube);
	TestTrue(TEXT("fighter is an enemy triangle until we deploy our own"), bFoundFighterTriangle);
	TestTrue(TEXT("fighter at SpawnNearShip offset is on the map"), bFoundSpawnedFighter);
	TestTrue(TEXT("cube and triangle share a normalized icon size"), FMath::IsNearlyEqual(EnemyScale.GetAbsMax(), 0.10f, 0.001f));
	TestTrue(TEXT("enemy cube stays upright"), FMath::Abs(EnemyRot.Pitch) < 1.0f && FMath::Abs(EnemyRot.Roll) < 1.0f);

	if (UStaticMeshComponent* OwnMarker = Viewer->MapTable->OwnShipMarker)
	{
		TestTrue(TEXT("own ship marker is a cube mesh"), OwnMarker->GetStaticMesh() && OwnMarker->GetStaticMesh()->GetName().Contains(TEXT("Cube")));
		TestTrue(TEXT("own ship marker matches the normalized icon size"), FMath::IsNearlyEqual(OwnMarker->GetRelativeScale3D().GetAbsMax(), 0.10f, 0.001f));
	}
	if (UStaticMeshComponent* EnemyMarker = Viewer->MapTable->FindDisplayedMarker(Enemy))
	{
		TestTrue(TEXT("displayed enemy marker is a cube mesh"), EnemyMarker->GetStaticMesh() && EnemyMarker->GetStaticMesh()->GetName().Contains(TEXT("Cube")));
		TestFalse(TEXT("displayed enemy marker is not a cone"), EnemyMarker->GetStaticMesh() && EnemyMarker->GetStaticMesh()->GetName().Contains(TEXT("Cone")));
	}
	if (UStaticMeshComponent* FighterMarker = Viewer->MapTable->FindDisplayedMarker(SpawnedFighter))
	{
		TestTrue(TEXT("displayed fighter marker uses the cone triangle mesh"), FighterMarker->GetStaticMesh() && FighterMarker->GetStaticMesh()->GetName().Contains(TEXT("Cone")));
	}

	World->DestroyWorld(false);
	return true;
}

#endif
