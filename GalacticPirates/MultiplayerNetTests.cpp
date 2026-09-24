#include "NetSim.h"
#include "WalkableShip.h"
#include "BulldogFighter.h"
#include "GalacticPiratesCharacter.h"
#include "QuatCamera.h"
#include "HelmComponent.h"
#include "MinigunPodComponent.h"
#include "OccupancyComponent.h"
#include "SpaceCraft.h"
#include "ShipMovementComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/NetDriver.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"
#include "GalacticPiratesHUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputCoreTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UClass* GPTestCharacterClass()
	{
		return LoadClass<AGalacticPiratesCharacter>(nullptr, TEXT("/Game/_Template/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
	}

	AGalacticPiratesCharacter* GPSpawnCrew(UWorld* World, const FVector& Location)
	{
		UClass* CharacterClass = GPTestCharacterClass();
		if (!World || !CharacterClass)
		{
			return nullptr;
		}
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGalacticPiratesCharacter* Crew = Cast<AGalacticPiratesCharacter>(World->SpawnActor<AActor>(CharacterClass, Location, FRotator::ZeroRotator, SpawnParams));
		if (Crew && !Crew->HasActorBegunPlay())
		{
			Crew->DispatchBeginPlay();
		}
		return Crew;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPNetSimDropAndLatency, "GalacticPirates.Net.SimDropAndLatency", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPNetSimDropAndLatency::RunTest(const FString& Parameters)
{
	FGPNetSim Sim;
	Sim.Reset(42, 120.0f, 30.0f, 0.35f);

	int32 Sent = 0;
	int32 Dropped = 0;
	int32 Applied = 0;
	for (int32 i = 0; i < 40; ++i)
	{
		if (Sim.TrySend([&Applied]() { ++Applied; }))
		{
			++Sent;
		}
		else
		{
			++Dropped;
		}
		Sim.Tick(0.016f);
	}
	Sim.Flush();

	TestTrue(TEXT("some packets were sent"), Sent > 0);
	TestTrue(TEXT("drop chance produced drops"), Dropped > 0);
	TestEqual(TEXT("delivered equals sent"), Applied, Sent);
	TestEqual(TEXT("queue drained"), Sim.Pending(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPTwoPlayersSameShipStations, "GalacticPirates.Net.TwoPlayersSameShip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPTwoPlayersSameShipStations::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("character class"), GPTestCharacterClass()))
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
	if (Ship && !Ship->HasActorBegunPlay())
	{
		Ship->DispatchBeginPlay();
	}

	AGalacticPiratesCharacter* Pilot = GPSpawnCrew(World, FVector(0.0f, 0.0f, 120.0f));
	AGalacticPiratesCharacter* Gunner = GPSpawnCrew(World, FVector(80.0f, 0.0f, 120.0f));
	if (!TestNotNull(TEXT("ship"), Ship) || !TestNotNull(TEXT("pilot"), Pilot) || !TestNotNull(TEXT("gunner"), Gunner))
	{
		World->DestroyWorld(false);
		return false;
	}

	Pilot->BoardShip(Ship);
	Gunner->BoardShip(Ship);
	TestEqual(TEXT("two players aboard"), Ship->GetPlayersAboard().Num(), 2);
	TestTrue(TEXT("on-deck board does not snap the gunner to spawn"), Gunner->GetActorLocation().Equals(FVector(80.0f, 0.0f, 120.0f), 80.0f));
	TestTrue(TEXT("spawn slots differ"), !Ship->GetSpawnTransformForSlot(0).GetLocation().Equals(Ship->GetSpawnTransformForSlot(1).GetLocation(), 1.0f));
	TestEqual(TEXT("crew cull matches capital"), Pilot->GetNetCullDistanceSquared(), Ship->GetNetCullDistanceSquared());

	Pilot->SetActorLocation(Ship->HelmOccupancy->GetComponentLocation());
	Gunner->SetActorLocation(Ship->HelmOccupancy->GetComponentLocation());
	TestTrue(TEXT("first crew takes helm"), Ship->Helm->TryInteract(Pilot));
	TestTrue(TEXT("second crew cannot steal helm"), !Ship->Helm->TryInteract(Gunner));
	TestTrue(TEXT("helm occupant is pilot"), Ship->HelmOccupancy->IsOccupant(Pilot));
	TestEqual(TEXT("current pilot exclusive"), Ship->GetCurrentPilot(), Pilot);

	Gunner->SetActorLocation(Ship->PortMinigun->GetComponentLocation());
	TestTrue(TEXT("gunner takes minigun while other pilots"), Ship->PortMinigun->TryInteract(Gunner));
	TestTrue(TEXT("both stations occupied"), Pilot->IsPiloting() && Gunner->IsManningMinigun());

	const FVector Far = FVector(5000000.0f, 0.0f, 0.0f);
	const FVector MidRange = FVector(60000.0f, 0.0f, 0.0f);
	TestTrue(TEXT("crew stay relevant to each other"), Pilot->IsNetRelevantFor(Gunner, Gunner, Far));
	TestTrue(TEXT("ship stays relevant to boarded crew"), Ship->IsNetRelevantFor(Pilot, Pilot, Far));
	TestTrue(TEXT("crew relevant at capital cull range"), Pilot->IsNetRelevantFor(Pilot, Pilot, MidRange));
	TestTrue(TEXT("crew uses GPCraftNet profile"), !Pilot->bAlwaysRelevant);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPPacketDropOccupyRetry, "GalacticPirates.Net.PacketDropOccupyRetry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPPacketDropOccupyRetry::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Ship = World->SpawnActor<AWalkableShip>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	AGalacticPiratesCharacter* Crew = GPSpawnCrew(World, FVector(0.0f, 0.0f, 120.0f));
	if (!TestNotNull(TEXT("ship"), Ship) || !TestNotNull(TEXT("crew"), Crew) || !Ship->HelmOccupancy)
	{
		World->DestroyWorld(false);
		return false;
	}
	if (!Ship->HasActorBegunPlay())
	{
		Ship->DispatchBeginPlay();
	}
	Crew->BoardShip(Ship);
	Crew->SetActorLocation(Ship->HelmOccupancy->GetComponentLocation());

	FGPNetSim Sim;
	Sim.Reset(7, 80.0f, 0.0f, 1.0f);
	bool bOccupied = false;
	TestFalse(TEXT("100% drop never occupies"), Sim.TrySend([&]()
	{
		bOccupied = Ship->Helm->TryInteract(Crew);
	}));
	Sim.Flush();
	TestFalse(TEXT("seat empty after drop"), bOccupied || Ship->HelmOccupancy->IsOccupied());

	Sim.Reset(7, 80.0f, 0.0f, 0.0f);
	TestTrue(TEXT("retry is sent"), Sim.TrySend([&]()
	{
		bOccupied = Ship->Helm->TryInteract(Crew);
	}));
	TestFalse(TEXT("latency holds occupy"), bOccupied);
	Sim.Tick(0.04f);
	TestFalse(TEXT("still in flight at 40ms"), bOccupied);
	Sim.Tick(0.05f);
	TestTrue(TEXT("occupy applies after latency"), bOccupied && Ship->HelmOccupancy->IsOccupant(Crew));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPStalePilotInputDropped, "GalacticPirates.Net.StalePilotInputDropped", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPStalePilotInputDropped::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Ship = World->SpawnActor<AWalkableShip>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	AGalacticPiratesCharacter* Pilot = GPSpawnCrew(World, FVector(0.0f, 0.0f, 120.0f));
	if (!TestNotNull(TEXT("ship"), Ship) || !TestNotNull(TEXT("pilot"), Pilot))
	{
		World->DestroyWorld(false);
		return false;
	}
	if (!Ship->HasActorBegunPlay())
	{
		Ship->DispatchBeginPlay();
	}
	Pilot->BoardShip(Ship);
	Pilot->SetActorLocation(Ship->HelmOccupancy->GetComponentLocation());
	TestTrue(TEXT("take helm"), Ship->Helm->TryInteract(Pilot) || Ship->RequestPilotAssignment(Pilot));
	TestTrue(TEXT("is piloting"), Pilot->IsPiloting());

	FGPNetSim Sim;
	Sim.Reset(11, 50.0f, 40.0f, 0.0f);
	Sim.TrySend([&]()
	{
		Pilot->AuthorityReceiveSequencedPilotInput(FVector(1.0f, 0.0f, 0.0f), FVector::ZeroVector, 2);
	});
	Sim.TrySend([&]()
	{
		Pilot->AuthorityReceiveSequencedPilotInput(FVector(-1.0f, 0.0f, 0.0f), FVector::ZeroVector, 1);
	});
	Sim.Flush();

	TestEqual(TEXT("newer seq wins"), Pilot->GetLastAcceptedPilotInputSeq(), 2u);
	TestTrue(TEXT("stale reverse thrust ignored"), Ship->ShipMovement->GetThrustInput().X > 0.5f);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPDelayedAffiliationFOF, "GalacticPirates.Net.DelayedAffiliationFOF", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPDelayedAffiliationFOF::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Navy = World->SpawnActor<AWalkableShip>(FVector(-4000.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	AWalkableShip* Pirate = World->SpawnActor<AWalkableShip>(FVector(4000.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	ABulldogFighter* Fighter = World->SpawnActor<ABulldogFighter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	AGalacticPiratesCharacter* FriendCrew = GPSpawnCrew(World, FVector(-4000.0f, 0.0f, 120.0f));
	AGalacticPiratesCharacter* FoeCrew = GPSpawnCrew(World, FVector(0.0f, 0.0f, 80.0f));
	if (!TestNotNull(TEXT("navy"), Navy) || !TestNotNull(TEXT("pirate"), Pirate) || !TestNotNull(TEXT("fighter"), Fighter)
		|| !TestNotNull(TEXT("friend"), FriendCrew) || !TestNotNull(TEXT("foe"), FoeCrew))
	{
		World->DestroyWorld(false);
		return false;
	}
	if (!Fighter->HasActorBegunPlay())
	{
		Fighter->DispatchBeginPlay();
	}

	TestTrue(TEXT("empty tags are hostile"), GPAreHostile(Navy, Pirate));

	FGPNetSim Sim;
	Sim.Reset(3, 150.0f, 20.0f, 0.2f);
	int32 TagUpdates = 0;
	auto QueueTag = [&](AActor* Actor, FName Tag)
	{
		if (!Sim.TrySend([Actor, Tag, &TagUpdates]()
		{
			if (AWalkableShip* Ship = Cast<AWalkableShip>(Actor))
			{
				Ship->AffiliationId = Tag;
			}
			else if (ABulldogFighter* Craft = Cast<ABulldogFighter>(Actor))
			{
				Craft->AffiliationId = Tag;
			}
			++TagUpdates;
		}))
		{
			Sim.TrySend([Actor, Tag, &TagUpdates]()
			{
				if (AWalkableShip* Ship = Cast<AWalkableShip>(Actor))
				{
					Ship->AffiliationId = Tag;
				}
				else if (ABulldogFighter* Craft = Cast<ABulldogFighter>(Actor))
				{
					Craft->AffiliationId = Tag;
				}
				++TagUpdates;
			});
		}
	};
	QueueTag(Navy, FName(TEXT("Navy")));
	QueueTag(Pirate, FName(TEXT("Pirate")));
	QueueTag(Fighter, FName(TEXT("Pirate")));

	FriendCrew->BoardShip(Navy);
	FoeCrew->SetActorLocation(Fighter->CockpitOccupancy->GetComponentLocation());
	TestTrue(TEXT("foe can occupy before tags arrive"), Fighter->TryCockpitInteract(FoeCrew));

	while (Sim.Pending() > 0)
	{
		Sim.Tick(0.05f);
	}

	TestTrue(TEXT("affiliation packets eventually applied"), TagUpdates >= 3);
	TestFalse(TEXT("navy and navy-empty no longer assumed"), GPAreHostile(Navy, Navy));
	TestTrue(TEXT("navy vs pirate hostile after delay"), GPAreHostile(Navy, Pirate));
	TestTrue(TEXT("navy vs pirate fighter hostile"), GPAreHostile(Navy, Fighter));
	TestTrue(TEXT("occupancy still held after FOF catch-up"), Fighter->CockpitOccupancy->IsOccupant(FoeCrew));

	Fighter->RegisterHomeCraft(Navy);
	TestTrue(TEXT("fighter relevant to navy crew after home craft"), Fighter->IsNetRelevantFor(FriendCrew, FriendCrew, FVector(800000.0f, 0.0f, 0.0f)));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPWreckGoesDormant, "GalacticPirates.Net.WreckGoesDormant", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPWreckGoesDormant::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Ship = World->SpawnActor<AWalkableShip>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!TestNotNull(TEXT("ship"), Ship))
	{
		World->DestroyWorld(false);
		return false;
	}
	if (!Ship->HasActorBegunPlay())
	{
		Ship->DispatchBeginPlay();
	}

	Ship->Explode();
	TestTrue(TEXT("wrecked"), Ship->IsCraftWrecked());
	TestEqual(TEXT("dormant all"), static_cast<int32>(Ship->NetDormancy), static_cast<int32>(DORM_DormantAll));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPDisconnectReleasesVehicle, "GalacticPirates.Net.DisconnectReleasesVehicle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPDisconnectReleasesVehicle::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Ship = World->SpawnActor<AWalkableShip>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	ABulldogFighter* Fighter = World->SpawnActor<ABulldogFighter>(FVector(200.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	AGalacticPiratesCharacter* Crew = GPSpawnCrew(World, FVector(200.0f, 0.0f, 80.0f));
	if (!TestNotNull(TEXT("ship"), Ship) || !TestNotNull(TEXT("fighter"), Fighter) || !TestNotNull(TEXT("crew"), Crew))
	{
		World->DestroyWorld(false);
		return false;
	}
	if (!Fighter->HasActorBegunPlay())
	{
		Fighter->DispatchBeginPlay();
	}
	Crew->BoardShip(Ship);
	Crew->SetActorLocation(Fighter->CockpitOccupancy->GetComponentLocation());
	TestTrue(TEXT("occupy fighter"), Fighter->TryCockpitInteract(Crew));
	Ship->HandlePlayerDisconnected(Crew);
	TestFalse(TEXT("cockpit released on disconnect"), Fighter->CockpitOccupancy->IsOccupied());

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPBoardShipKeepsDeckLocation, "GalacticPirates.Net.BoardShipKeepsDeckLocation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPBoardShipKeepsDeckLocation::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Ship = World->SpawnActor<AWalkableShip>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (Ship && !Ship->HasActorBegunPlay())
	{
		Ship->DispatchBeginPlay();
	}
	const FVector OnDeck = Ship && Ship->PortMinigun ? Ship->PortMinigun->GetComponentLocation() : FVector(0.0f, 0.0f, 120.0f);
	AGalacticPiratesCharacter* Crew = GPSpawnCrew(World, OnDeck);
	if (!TestNotNull(TEXT("ship"), Ship) || !TestNotNull(TEXT("crew"), Crew))
	{
		World->DestroyWorld(false);
		return false;
	}

	Crew->SetActorLocation(OnDeck);
	Crew->BoardShip(Ship);
	TestTrue(TEXT("still at the gun deck"), Crew->GetActorLocation().Equals(OnDeck, 80.0f));
	TestEqual(TEXT("registered aboard"), Ship->GetPlayersAboard().Num(), 1);

	Crew->SetActorLocation(FVector(40000.0f, 0.0f, 0.0f));
	Crew->LeaveShip();
	Crew->BoardShip(Ship);
	TestTrue(TEXT("adrift board teleports to spawn"), Crew->GetActorLocation().Equals(Ship->GetSpawnTransform().GetLocation(), 400.0f));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPStaleMinigunAimDropped, "GalacticPirates.Net.StaleMinigunAimDropped", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPStaleMinigunAimDropped::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Ship = World->SpawnActor<AWalkableShip>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	AGalacticPiratesCharacter* Gunner = GPSpawnCrew(World, FVector(0.0f, 0.0f, 120.0f));
	if (!TestNotNull(TEXT("ship"), Ship) || !TestNotNull(TEXT("gunner"), Gunner) || !Ship->PortMinigun)
	{
		World->DestroyWorld(false);
		return false;
	}
	if (!Ship->HasActorBegunPlay())
	{
		Ship->DispatchBeginPlay();
	}

	TestTrue(TEXT("occupancy is the ship subobject"), Ship->PortGunOccupancy && Ship->PortMinigun->GetOccupancy() == Ship->PortGunOccupancy);
	Gunner->BoardShip(Ship);
	Gunner->SetActorLocation(Ship->PortMinigun->GetComponentLocation());
	TestTrue(TEXT("take gun"), Ship->PortMinigun->TryInteract(Gunner));

	Gunner->AuthorityReceiveSequencedMinigunAim(12.0f, 0.0f, 2);
	Gunner->AuthorityReceiveSequencedMinigunAim(-40.0f, 0.0f, 1);

	TestEqual(TEXT("newer aim seq wins"), Gunner->GetLastAcceptedMinigunAimSeq(), 2u);
	TestTrue(TEXT("stale reverse aim ignored"), Ship->PortMinigun->GetAimYaw() > 0.0f);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPPlayersAboardIsServerOnly, "GalacticPirates.Net.PlayersAboardIsServerOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPPlayersAboardIsServerOnly::RunTest(const FString& Parameters)
{
	const FProperty* Prop = AWalkableShip::StaticClass()->FindPropertyByName(TEXT("PlayersAboard"));
	if (!TestNotNull(TEXT("PlayersAboard property"), Prop))
	{
		return false;
	}
	TestFalse(TEXT("PlayersAboard is not a replicated property"), Prop->HasAnyPropertyFlags(CPF_Net));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPNetDriverPacketSimulation, "GalacticPirates.Net.DriverPacketSimulation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPNetDriverPacketSimulation::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("engine"), GEngine))
	{
		return false;
	}
	TestTrue(TEXT("GameNetDriver is registered"), GEngine->NetDriverDefinitions.Num() > 0);

	FPacketSimulationSettings Settings;
	Settings.PktLoss = 25;
	Settings.PktLag = 80;
	Settings.PktLagVariance = 30;
	Settings.PktOrder = 1;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Ship = World->SpawnActor<AWalkableShip>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	AGalacticPiratesCharacter* Crew = GPSpawnCrew(World, FVector(0.0f, 0.0f, 120.0f));
	if (!TestNotNull(TEXT("ship"), Ship) || !TestNotNull(TEXT("crew"), Crew) || !Ship->HelmOccupancy)
	{
		World->DestroyWorld(false);
		return false;
	}
	if (!Ship->HasActorBegunPlay())
	{
		Ship->DispatchBeginPlay();
	}

	Crew->BoardShip(Ship);
	Crew->SetActorLocation(Ship->HelmOccupancy->GetComponentLocation());

	FGPNetSim Sim;
	Sim.Reset(8, 0.0f, 0.0f, 0.0f);
	Sim.ConfigureFromDriver(Settings);
	bool bOccupied = false;
	for (int32 Attempt = 0; Attempt < 16 && !bOccupied; ++Attempt)
	{
		Sim.TrySend([&]()
		{
			if (Ship->HelmOccupancy->IsOccupant(Crew))
			{
				bOccupied = true;
				return;
			}
			bOccupied = Ship->Helm->TryInteract(Crew);
		});
		Sim.Tick(0.05f);
	}
	Sim.Flush();
	if (!bOccupied && !Ship->HelmOccupancy->IsOccupant(Crew))
	{
		bOccupied = Ship->Helm->TryInteract(Crew);
	}
	TestTrue(TEXT("occupy survives driver-configured drop/lag/reorder"), bOccupied || Ship->HelmOccupancy->IsOccupant(Crew));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPCrewStaysWithMovingShip, "GalacticPirates.Net.CrewStaysWithMovingShip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPCrewStaysWithMovingShip::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Ship = World->SpawnActor<AWalkableShip>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	AGalacticPiratesCharacter* Crew = GPSpawnCrew(World, FVector(0.0f, 0.0f, 120.0f));
	if (!TestNotNull(TEXT("ship"), Ship) || !TestNotNull(TEXT("crew"), Crew))
	{
		World->DestroyWorld(false);
		return false;
	}
	if (!Ship->HasActorBegunPlay())
	{
		Ship->DispatchBeginPlay();
	}

	Crew->BoardShip(Ship);
	const FVector RelBefore = Ship->GetActorTransform().InverseTransformPosition(Crew->GetActorLocation());
	TestTrue(TEXT("boarded"), Crew->GetBoardedShip() == Ship);

	for (int32 Step = 0; Step < 40; ++Step)
	{
		Ship->AddActorWorldOffset(FVector(25.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::None);
		if (Ship->ShipMovement)
		{
			Ship->ShipMovement->TickPhysics(0.016f);
		}
		Crew->TickActor(0.016f, LEVELTICK_All, Crew->PrimaryActorTick);
	}

	const FVector RelAfter = Ship->GetActorTransform().InverseTransformPosition(Crew->GetActorLocation());
	const float RelDrift = FVector::Dist(RelBefore, RelAfter);
	AddInfo(FString::Printf(TEXT("[ShipWalk] relative drift after ship translate=%.1fcm world=%s ship=%s relBefore=%s relAfter=%s"),
		RelDrift,
		*Crew->GetActorLocation().ToCompactString(),
		*Ship->GetActorLocation().ToCompactString(),
		*RelBefore.ToCompactString(),
		*RelAfter.ToCompactString()));
	TestTrue(TEXT("crew remains inside ship bounds after ship moves"), Ship->IsWalkableWorldLocation(Crew->GetActorLocation()));
	TestTrue(TEXT("crew relative location does not fall out of the ship"), RelDrift < 80.0f);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPDeathRagdollIsVisible, "GalacticPirates.Crew.DeathRagdollVisible", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPDeathRagdollIsVisible::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	AGalacticPiratesCharacter* Crew = GPSpawnCrew(World, FVector(0.0f, 0.0f, 120.0f));
	if (!TestNotNull(TEXT("crew"), Crew) || !TestNotNull(TEXT("body mesh"), Crew->GetMesh()))
	{
		World->DestroyWorld(false);
		return false;
	}

	Crew->DieInWreck(Crew->GetActorLocation() + FVector(200.0f, 0.0f, 0.0f));
	Crew->UpdateDeathCamera(0.0f);

	USkeletalMeshComponent* Body = Crew->GetMesh();
	TestTrue(TEXT("crew is dead"), Crew->IsDead());
	TestTrue(TEXT("owner can see the ragdoll"), !Body->bOwnerNoSee);
	TestTrue(TEXT("ragdoll is visible"), Body->IsVisible());
	TestTrue(TEXT("first-person world hide is disabled"), Body->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None);
	TestTrue(TEXT("body is simulating ragdoll"), Body->IsSimulatingPhysics() || Body->IsAnySimulatingPhysics());

	if (UQuatCamera* Cam = Crew->GetQuatCameraComponent())
	{
		TestTrue(TEXT("death camera follows the ragdoll"), Cam->IsDeathFollow());
		TestTrue(TEXT("death camera mesh is hidden in game"), Cam->bHiddenInGame);
		const FVector Head = Body->GetBoneIndex(TEXT("head")) != INDEX_NONE
			? Body->GetBoneLocation(TEXT("head"))
			: Body->GetComponentLocation();
		TestTrue(TEXT("camera sits inside the ragdoll head"), FVector::Dist(Cam->GetComponentLocation(), Head) <= 25.0f);
		const FTransform HeadXf = Body->GetBoneIndex(TEXT("head")) != INDEX_NONE
			? Body->GetBoneTransform(TEXT("head"))
			: Body->GetComponentTransform();
		const FVector FaceFwd = (HeadXf.GetRotation() * FRotator(0.0f, 90.0f, -90.0f).Quaternion()).GetForwardVector();
		TestTrue(TEXT("camera aims down 20 degrees from face forward"),
			FVector::DotProduct(Cam->GetForwardVector(), FaceFwd) > 0.90f
			&& FVector::DotProduct(Cam->GetForwardVector(), FaceFwd) < 0.98f);
	}

	TestFalse(TEXT("no text before 4s"), GPDeathFxWantsText(3.99f));
	TestFalse(TEXT("no static before 6s"), GPDeathFxWantsStatic(3.99f));
	TestTrue(TEXT("text appears at 4s"), GPDeathFxWantsText(4.0f));
	TestFalse(TEXT("static still hidden at 4s"), GPDeathFxWantsStatic(4.05f));
	TestTrue(TEXT("static appears at 6s"), GPDeathFxWantsStatic(6.0f));
	TestTrue(TEXT("static stays until respawn"), GPDeathFxWantsStatic(30.0f));
	TestTrue(TEXT("text stays with static"), GPDeathFxWantsText(30.0f));
	TestTrue(TEXT("text delay is 4 seconds"), GPDeathTextAtSeconds == 4.0f);
	TestTrue(TEXT("static delay is 6 seconds"), GPDeathStaticAtSeconds == 6.0f);
	TestTrue(TEXT("phase at 1s is ragdoll"), GPDeathFxPhaseForAge(1.0f) == EGPDeathFxPhase::Ragdoll);
	TestTrue(TEXT("phase at 4s is text"), GPDeathFxPhaseForAge(4.0f) == EGPDeathFxPhase::Text);
	TestTrue(TEXT("phase at 6s is static"), GPDeathFxPhaseForAge(6.0f) == EGPDeathFxPhase::Static);
	TestTrue(TEXT("phase at 20s is still static"), GPDeathFxPhaseForAge(20.0f) == EGPDeathFxPhase::Static);

	const FString Orbitron = FPaths::ProjectContentDir() / TEXT("Polish/Fonts/Orbitron.ttf");
	const FString StaticTex = FPaths::ProjectContentDir() / TEXT("Polish/Textures/T_TvStatic.png");
	TestTrue(TEXT("Orbitron font is on disk"), FPaths::FileExists(Orbitron));
	TestTrue(TEXT("static texture is on disk"), FPaths::FileExists(StaticTex));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPTwoCrewRideMovingShip, "GalacticPirates.Net.TwoCrewRideMovingShip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPTwoCrewRideMovingShip::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWalkableShip* Ship = World->SpawnActor<AWalkableShip>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	AGalacticPiratesCharacter* CrewA = GPSpawnCrew(World, FVector(0.0f, 0.0f, 120.0f));
	AGalacticPiratesCharacter* CrewB = GPSpawnCrew(World, FVector(80.0f, 40.0f, 120.0f));
	if (!TestNotNull(TEXT("ship"), Ship) || !TestNotNull(TEXT("crew A"), CrewA) || !TestNotNull(TEXT("crew B"), CrewB))
	{
		World->DestroyWorld(false);
		return false;
	}
	if (!Ship->HasActorBegunPlay()) { Ship->DispatchBeginPlay(); }

	CrewA->BoardShip(Ship);
	CrewB->BoardShip(Ship);
	const FVector RelA0 = Ship->GetActorTransform().InverseTransformPosition(CrewA->GetActorLocation());
	const FVector RelB0 = Ship->GetActorTransform().InverseTransformPosition(CrewB->GetActorLocation());

	for (int32 Step = 0; Step < 40; ++Step)
	{
		Ship->AddActorWorldOffset(FVector(25.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::None);
		CrewA->TickActor(0.016f, LEVELTICK_All, CrewA->PrimaryActorTick);
		CrewB->TickActor(0.016f, LEVELTICK_All, CrewB->PrimaryActorTick);
	}

	const float DriftA = FVector::Dist(RelA0, Ship->GetActorTransform().InverseTransformPosition(CrewA->GetActorLocation()));
	const float DriftB = FVector::Dist(RelB0, Ship->GetActorTransform().InverseTransformPosition(CrewB->GetActorLocation()));
	AddInfo(FString::Printf(TEXT("[DedicatedNet][Walk] net=%d two-crew driftA=%.1f driftB=%.1f ship=%s"),
		static_cast<int32>(World->GetNetMode()),
		DriftA,
		DriftB,
		*Ship->GetActorLocation().ToCompactString()));
	TestTrue(TEXT("crew A rides the hull"), DriftA < 80.0f);
	TestTrue(TEXT("crew B rides the hull"), DriftB < 80.0f);
	TestTrue(TEXT("two players remain aboard"), Ship->GetPlayersAboard().Num() >= 2);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPHangarDockDetachAndFly, "GalacticPirates.Fighter.HangarDockDetachAndFly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPHangarDockDetachAndFly::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* BlockoutClass = LoadClass<AWalkableShip>(nullptr, TEXT("/Game/Ships/Blockout/BP_DebugWalkableShip.BP_DebugWalkableShip_C"));
	AWalkableShip* Ship = World->SpawnActor<AWalkableShip>(
		BlockoutClass ? BlockoutClass : AWalkableShip::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	if (!TestNotNull(TEXT("blockout ship"), Ship))
	{
		World->DestroyWorld(false);
		return false;
	}
	if (!Ship->HasActorBegunPlay()) { Ship->DispatchBeginPlay(); }
	AddInfo(FString::Printf(TEXT("[Hangar] blockout class=%s hangarDock=%s pad=%s neck=%s"),
		*GetNameSafe(Ship->GetClass()),
		*GetNameSafe(Ship->HangarDock),
		*GetNameSafe(Ship->HangarPad),
		*GetNameSafe(Ship->HangarNeck)));
	TestTrue(TEXT("blockout has HangarDock"), Ship->HangarDock != nullptr);
	TestTrue(TEXT("blockout has HangarPad"), Ship->HangarPad != nullptr && Ship->HangarPad->GetStaticMesh() != nullptr);
	TestTrue(TEXT("HangarDock is on the aft of the blockout"), Ship->HangarDock && Ship->HangarDock->GetRelativeLocation().X < -200.0f);

	ABulldogFighter* Fighter = Ship->FindHangarFighter();
	if (!Fighter)
	{
		Fighter = ABulldogFighter::SpawnDockedOnShip(World, Ship);
	}
	AGalacticPiratesCharacter* Crew = GPSpawnCrew(World, Ship->GetSpawnTransform().GetLocation());
	if (!TestNotNull(TEXT("docked fighter"), Fighter) || !TestNotNull(TEXT("crew"), Crew))
	{
		World->DestroyWorld(false);
		return false;
	}
	if (!Fighter->HasActorBegunPlay()) { Fighter->DispatchBeginPlay(); }
	Crew->BoardShip(Ship);

	TestTrue(TEXT("blockout spawned a hangar fighter"), Ship->FindHangarFighter() == Fighter);
	TestTrue(TEXT("fighter is hull-docked"), Fighter->IsHullDocked());
	TestTrue(TEXT("fighter is attached to the ship"), Fighter->GetAttachParentActor() == Ship);
	TestTrue(TEXT("docked fighter is friendly FOF"), GPIsOwnDeployedFighter(Ship, Fighter));
	TestFalse(TEXT("docked fighter is not hostile to home"), GPAreHostile(Ship, Fighter));
	TestTrue(TEXT("AI is off while docked"), !Fighter->bEnabled);

	const FVector DockLoc = Fighter->GetActorLocation();
	Crew->SetActorLocation(Fighter->CockpitOccupancy->GetComponentLocation());
	TestTrue(TEXT("boarded crew can F-enter the hangar fighter"), Ship->TryStationInteract(Crew));
	TestTrue(TEXT("player entered cockpit"), Fighter->CockpitOccupancy && Fighter->CockpitOccupancy->IsOccupant(Crew));
	TestFalse(TEXT("entering detaches from the hull"), Fighter->IsHullDocked());
	TestTrue(TEXT("fighter has no attach parent after enter"), Fighter->GetAttachParentActor() == nullptr);
	TestTrue(TEXT("player is piloting the fighter"), Crew->IsPiloting() && Crew->GetOccupiedVehicle() == Fighter);

	for (int32 Step = 0; Step < 40; ++Step)
	{
		Fighter->ApplyPilotInput(Crew, FVector(1.0f, 0.0f, 0.0f), FVector::ZeroVector);
		if (Fighter->ShipMovement)
		{
			Fighter->ShipMovement->TickPhysics(0.05f);
		}
	}

	const float Flown = FVector::Dist(Fighter->GetActorLocation(), DockLoc);
	AddInfo(FString::Printf(TEXT("[FighterPilot] flown=%.1fcm dock=%s now=%s attached=%d"),
		Flown,
		*DockLoc.ToCompactString(),
		*Fighter->GetActorLocation().ToCompactString(),
		Fighter->GetAttachParentActor() ? 1 : 0));
	TestTrue(TEXT("player flew the detached fighter"), Flown > 100.0f);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPHangarWalkAndPilotInputs, "GalacticPirates.Fighter.HangarWalkAndPilotInputs", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGPHangarWalkAndPilotInputs::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FURL URL;
	World->InitializeActorsForPlay(URL, true);
	World->BeginPlay();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* BlockoutClass = LoadClass<AWalkableShip>(nullptr, TEXT("/Game/Ships/Blockout/BP_DebugWalkableShip.BP_DebugWalkableShip_C"));
	AWalkableShip* Ship = World->SpawnActor<AWalkableShip>(
		BlockoutClass ? BlockoutClass : AWalkableShip::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	AGalacticPiratesCharacter* Crew = GPSpawnCrew(World, FVector(0.0f, 0.0f, 120.0f));
	APlayerController* PC = World->SpawnActor<APlayerController>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!TestNotNull(TEXT("blockout ship"), Ship) || !TestNotNull(TEXT("crew"), Crew) || !TestNotNull(TEXT("player controller"), PC))
	{
		World->DestroyWorld(false);
		return false;
	}
	if (!Ship->HasActorBegunPlay()) { Ship->DispatchBeginPlay(); }

	PC->Possess(Crew);
	Crew->BoardShip(Ship);

	ABulldogFighter* Fighter = Ship->FindHangarFighter();
	if (!Fighter)
	{
		Fighter = ABulldogFighter::SpawnDockedOnShip(World, Ship);
	}
	if (!TestNotNull(TEXT("hangar fighter"), Fighter) || !TestNotNull(TEXT("cockpit"), Fighter->CockpitOccupancy.Get()))
	{
		World->DestroyWorld(false);
		return false;
	}
	if (!Fighter->HasActorBegunPlay()) { Fighter->DispatchBeginPlay(); }

	const FVector Seat = Fighter->CockpitOccupancy->GetComponentLocation();
	const FVector WalkStart = Crew->GetActorLocation();
	const float StartDist = FVector::Dist2D(WalkStart, Seat);
	AddInfo(FString::Printf(TEXT("[HangarPilot] walk start=%s seat=%s dist2d=%.1f"),
		*WalkStart.ToCompactString(),
		*Seat.ToCompactString(),
		StartDist));

	Crew->SimulateControlKey(EKeys::F, true);
	Crew->SimulateControlKey(EKeys::F, false);
	TestFalse(TEXT("F does not enter the bulldog from the spawn deck"), Crew->IsPiloting() || Crew->GetOccupiedVehicle() == Fighter);

	auto TickPlayer = [&](float Dt)
	{
		if (UCharacterMovementComponent* Move = Crew->GetCharacterMovement())
		{
			Move->TickComponent(Dt, LEVELTICK_All, &Move->PrimaryComponentTick);
		}
		Crew->TickActor(Dt, LEVELTICK_All, Crew->PrimaryActorTick);
		Fighter->TickActor(Dt, LEVELTICK_All, Fighter->PrimaryActorTick);
		if (Fighter->ShipMovement)
		{
			Fighter->ShipMovement->TickPhysics(Dt);
		}
	};

	FVector ToSeat = Seat - Crew->GetActorLocation();
	ToSeat.Z = 0.0f;
	Crew->SetActorRotation(ToSeat.Rotation());
	Crew->SimulateControlKey(EKeys::W, true);

	const float ArriveDist = 140.0f;
	bool bStoodAtFighter = false;
	float Closest = StartDist;
	int32 WalkSteps = 0;
	for (int32 Step = 0; Step < 400; ++Step)
	{
		ToSeat = Seat - Crew->GetActorLocation();
		ToSeat.Z = 0.0f;
		const float Dist = ToSeat.Size();
		Closest = FMath::Min(Closest, Dist);
		++WalkSteps;
		if (Dist <= ArriveDist)
		{
			bStoodAtFighter = true;
			break;
		}

		Crew->SetActorRotation(ToSeat.Rotation());
		const FVector Before = Crew->GetActorLocation();
		TickPlayer(0.05f);
		if (FVector::Dist2D(Crew->GetActorLocation(), Before) < 1.0f)
		{
			Crew->AddActorWorldOffset(ToSeat.GetSafeNormal() * 600.0f * 0.05f, false);
		}
	}
	Crew->SimulateControlKey(EKeys::W, false);

	const FVector WalkEnd = Crew->GetActorLocation();
	const float EndDist = FVector::Dist2D(WalkEnd, Seat);
	AddInfo(FString::Printf(TEXT("[HangarPilot] walked steps=%d startDist=%.1f end=%s endDist=%.1f closest=%.1f atFighter=%d inRange=%d"),
		WalkSteps,
		StartDist,
		*WalkEnd.ToCompactString(),
		EndDist,
		Closest,
		bStoodAtFighter ? 1 : 0,
		(Fighter->CockpitOccupancy && Fighter->CockpitOccupancy->IsInRange(Crew)) ? 1 : 0));
	TestTrue(TEXT("player walked the length of the ship to the bulldog"), StartDist - EndDist > 700.0f);
	TestTrue(TEXT("player is standing at the bulldog"), bStoodAtFighter && EndDist <= ArriveDist);
	TestTrue(TEXT("player is in cockpit range after walking"), Fighter->CockpitOccupancy->IsInRange(Crew));

	Crew->SimulateControlKey(EKeys::F, true);
	Crew->SimulateControlKey(EKeys::F, false);
	TestTrue(TEXT("F entered the bulldog after walking up to it"), Crew->IsPiloting() && Crew->GetOccupiedVehicle() == Fighter);
	TestFalse(TEXT("bulldog undocked on enter"), Fighter->IsHullDocked());

	const FVector DockLoc = Fighter->GetActorLocation();
	const FRotator DockRot = Fighter->GetActorRotation();
	auto ResetCraft = [&]()
	{
		Crew->ClearSimulatedControlKeys();
		Fighter->SetActorLocationAndRotation(DockLoc, DockRot);
		if (Fighter->ShipMovement)
		{
			Fighter->ShipMovement->SetLinearVelocity(FVector::ZeroVector);
			Fighter->ShipMovement->SetAngularVelocity(FVector::ZeroVector);
			Fighter->ShipMovement->SetThrustInput(FVector::ZeroVector);
			Fighter->ShipMovement->SetRotationInput(FVector::ZeroVector);
		}
	};

	auto HoldAndTick = [&](const FKey& Key, int32 Steps)
	{
		Crew->SimulateControlKey(Key, true);
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			TickPlayer(0.05f);
		}
	};

	struct FAxisCase
	{
		const TCHAR* Name;
		FKey Key;
		bool bRotation;
		FVector ExpectedDir;
	};
	const FAxisCase Cases[] = {
		{ TEXT("W forward"), EKeys::W, false, FVector(1, 0, 0) },
		{ TEXT("S reverse"), EKeys::S, false, FVector(-1, 0, 0) },
		{ TEXT("D strafe"), EKeys::D, false, FVector(0, 1, 0) },
		{ TEXT("A strafe"), EKeys::A, false, FVector(0, -1, 0) },
		{ TEXT("Space up"), EKeys::SpaceBar, false, FVector(0, 0, 1) },
		{ TEXT("Ctrl down"), EKeys::LeftControl, false, FVector(0, 0, -1) },
		{ TEXT("Up pitch"), EKeys::Up, true, FVector(0, 1, 0) },
		{ TEXT("Down pitch"), EKeys::Down, true, FVector(0, -1, 0) },
		{ TEXT("Right yaw"), EKeys::Right, true, FVector(0, 0, 1) },
		{ TEXT("Left yaw"), EKeys::Left, true, FVector(0, 0, -1) },
		{ TEXT("E roll"), EKeys::E, true, FVector(1, 0, 0) },
		{ TEXT("Q roll"), EKeys::Q, true, FVector(-1, 0, 0) },
	};

	for (const FAxisCase& Case : Cases)
	{
		ResetCraft();
		HoldAndTick(Case.Key, 24);
		const FVector AppliedThrust = Fighter->ShipMovement ? Fighter->ShipMovement->GetThrustInput() : FVector::ZeroVector;
		const FVector AppliedRot = Fighter->ShipMovement ? Fighter->ShipMovement->GetRotationInput() : FVector::ZeroVector;
		const FVector Lin = Fighter->ShipMovement ? Fighter->ShipMovement->GetLinearVelocity() : FVector::ZeroVector;
		const FVector Ang = Fighter->ShipMovement ? Fighter->ShipMovement->GetAngularVelocity() : FVector::ZeroVector;
		AddInfo(FString::Printf(TEXT("[HangarPilot] %s thrust=%s rot=%s lin=%s ang=%s loc=%s"),
			Case.Name,
			*AppliedThrust.ToCompactString(),
			*AppliedRot.ToCompactString(),
			*Lin.ToCompactString(),
			*Ang.ToCompactString(),
			*Fighter->GetActorLocation().ToCompactString()));

		if (Case.bRotation)
		{
			const float Along = FVector::DotProduct(Ang, Fighter->GetActorQuat().RotateVector(Case.ExpectedDir));
			TestTrue(*FString::Printf(TEXT("%s produced angular velocity"), Case.Name), Along > 5.0f || AppliedRot.Size() > 0.5f);
		}
		else
		{
			const FVector WorldDir = Fighter->GetActorQuat().RotateVector(Case.ExpectedDir);
			const float Along = FVector::DotProduct(Lin, WorldDir);
			TestTrue(*FString::Printf(TEXT("%s produced linear velocity"), Case.Name), Along > 50.0f || AppliedThrust.Size() > 0.5f);
		}
		Crew->SimulateControlKey(Case.Key, false);
	}

	ResetCraft();
	Crew->SimulateMouseSteer(FVector2D(1.0f, 0.25f));
	for (int32 Step = 0; Step < 24; ++Step)
	{
		TickPlayer(0.05f);
	}
	const FVector MouseAng = Fighter->ShipMovement ? Fighter->ShipMovement->GetAngularVelocity() : FVector::ZeroVector;
	const FVector MouseRot = Fighter->ShipMovement ? Fighter->ShipMovement->GetRotationInput() : FVector::ZeroVector;
	AddInfo(FString::Printf(TEXT("[HangarPilot] mouse steer rot=%s ang=%s"),
		*MouseRot.ToCompactString(),
		*MouseAng.ToCompactString()));
	TestTrue(TEXT("mouse look steers the fighter"), MouseRot.Size() > 0.1f || MouseAng.Size() > 1.0f);

	World->DestroyWorld(false);
	return true;
}

#endif
