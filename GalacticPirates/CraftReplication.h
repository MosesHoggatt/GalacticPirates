#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

struct GALACTICPIRATES_API FCraftReplicationProfile
{
	float NetUpdateFrequency = 30.0f;
	float MinNetUpdateFrequency = 10.0f;
	float CullDistanceCm = 80000.0f;
	bool bReplicateMovement = true;
};

namespace GPCraftNet
{
	inline FCraftReplicationProfile CapitalShip()
	{
		FCraftReplicationProfile Profile;
		Profile.NetUpdateFrequency = 30.0f;
		Profile.MinNetUpdateFrequency = 10.0f;
		Profile.CullDistanceCm = 80000.0f;
		return Profile;
	}

	inline FCraftReplicationProfile Fighter()
	{
		FCraftReplicationProfile Profile;
		Profile.NetUpdateFrequency = 45.0f;
		Profile.MinNetUpdateFrequency = 15.0f;
		Profile.CullDistanceCm = 80000.0f;
		return Profile;
	}

	inline FCraftReplicationProfile Missile()
	{
		FCraftReplicationProfile Profile;
		Profile.NetUpdateFrequency = 60.0f;
		Profile.MinNetUpdateFrequency = 20.0f;
		Profile.CullDistanceCm = 80000.0f;
		return Profile;
	}

	inline FCraftReplicationProfile Crew()
	{
		FCraftReplicationProfile Profile;
		Profile.NetUpdateFrequency = 20.0f;
		Profile.MinNetUpdateFrequency = 5.0f;
		Profile.CullDistanceCm = 80000.0f;
		return Profile;
	}

	inline void Apply(AActor* Actor, const FCraftReplicationProfile& Profile)
	{
		if (!Actor)
		{
			return;
		}
		Actor->SetReplicates(true);
		Actor->bAlwaysRelevant = false;
		Actor->SetReplicateMovement(Profile.bReplicateMovement);
		Actor->SetNetUpdateFrequency(Profile.NetUpdateFrequency);
		Actor->SetMinNetUpdateFrequency(Profile.MinNetUpdateFrequency);
		Actor->SetNetCullDistanceSquared(Profile.CullDistanceCm * Profile.CullDistanceCm);
	}
}
