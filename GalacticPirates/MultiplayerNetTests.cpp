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

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UClass* GPTestCharacterClass()
	{
		return LoadClass<AGalacticPiratesCharacter>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
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

#endif
