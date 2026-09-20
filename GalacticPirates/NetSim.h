#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Engine/NetDriver.h"

/** Send-side model matching UNetDriver FPacketSimulationSettings (PktLoss/PktLag/PktOrder). */
struct FGPNetSim
{
	float LatencySeconds = 0.0f;
	float JitterSeconds = 0.0f;
	float DropChance = 0.0f;
	bool bReorder = false;
	FRandomStream Rng;
	double Now = 0.0;

	struct FPacket
	{
		double DeliverAt = 0.0;
		uint32 Seq = 0;
		TFunction<void()> Apply;
	};
	TArray<FPacket> Queue;
	uint32 NextSeq = 1;

	void Reset(int32 Seed, float InLatencyMs, float InJitterMs, float InDropChance)
	{
		LatencySeconds = FMath::Max(0.0f, InLatencyMs) * 0.001f;
		JitterSeconds = FMath::Max(0.0f, InJitterMs) * 0.001f;
		DropChance = FMath::Clamp(InDropChance, 0.0f, 1.0f);
		bReorder = false;
		Rng.Initialize(Seed);
		Now = 0.0;
		NextSeq = 1;
		Queue.Reset();
	}

	void ConfigureFromDriver(const FPacketSimulationSettings& Settings)
	{
		LatencySeconds = FMath::Max(0, Settings.PktLag) * 0.001f;
		JitterSeconds = FMath::Max(0, Settings.PktLagVariance) * 0.001f;
		DropChance = FMath::Clamp(Settings.PktLoss, 0, 100) / 100.0f;
		bReorder = Settings.PktOrder != 0;
	}

	bool TrySend(TFunction<void()> Apply)
	{
		if (Rng.FRand() < DropChance)
		{
			return false;
		}

		FPacket Packet;
		const float Jitter = JitterSeconds > 0.0f ? Rng.FRandRange(-JitterSeconds, JitterSeconds) : 0.0f;
		Packet.DeliverAt = Now + FMath::Max(0.0f, LatencySeconds + Jitter);
		Packet.Seq = NextSeq++;
		Packet.Apply = MoveTemp(Apply);
		if (bReorder && Queue.Num() > 0 && Rng.FRand() < 0.5f)
		{
			const int32 SwapIndex = Rng.RandRange(0, Queue.Num() - 1);
			Swap(Packet.DeliverAt, Queue[SwapIndex].DeliverAt);
		}
		Queue.Add(MoveTemp(Packet));
		return true;
	}

	int32 Tick(float DeltaSeconds)
	{
		Now += FMath::Max(0.0f, DeltaSeconds);
		int32 Delivered = 0;
		for (int32 Index = Queue.Num() - 1; Index >= 0; --Index)
		{
			if (Queue[Index].DeliverAt <= Now)
			{
				if (Queue[Index].Apply)
				{
					Queue[Index].Apply();
				}
				Queue.RemoveAt(Index);
				++Delivered;
			}
		}
		return Delivered;
	}

	int32 Flush()
	{
		int32 Delivered = 0;
		Queue.Sort([](const FPacket& A, const FPacket& B)
		{
			if (!FMath::IsNearlyEqual(A.DeliverAt, B.DeliverAt))
			{
				return A.DeliverAt < B.DeliverAt;
			}
			return A.Seq < B.Seq;
		});
		for (FPacket& Packet : Queue)
		{
			if (Packet.Apply)
			{
				Packet.Apply();
				++Delivered;
			}
		}
		Queue.Reset();
		return Delivered;
	}

	int32 Pending() const { return Queue.Num(); }
};
