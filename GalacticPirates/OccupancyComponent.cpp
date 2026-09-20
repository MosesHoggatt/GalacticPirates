#include "OccupancyComponent.h"
#include "SpaceCraft.h"
#include "Net/UnrealNetwork.h"

UOccupancyComponent::UOccupancyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	SetMobility(EComponentMobility::Movable);
}

void UOccupancyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UOccupancyComponent, Occupant);
}

bool UOccupancyComponent::IsInRange(const APawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}
	return FVector::Dist(Pawn->GetActorLocation(), GetComponentLocation()) <= InteractRange;
}

bool UOccupancyComponent::TryOccupy(APawn* Pawn)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !Pawn)
	{
		return false;
	}

	if (GPIsCraftWrecked(Owner))
	{
		return false;
	}

	if (Occupant == Pawn)
	{
		SetOccupant(nullptr);
		return true;
	}

	if (Occupant)
	{
		return false;
	}

	if (!IsInRange(Pawn))
	{
		return false;
	}

	SetOccupant(Pawn);
	return true;
}

void UOccupancyComponent::ForceRelease()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		SetOccupant(nullptr);
	}
}

void UOccupancyComponent::SetOccupant(APawn* NewOccupant)
{
	APawn* Old = Occupant;
	if (Old == NewOccupant)
	{
		return;
	}
	Occupant = NewOccupant;
	OnRep_Occupant(Old);
}

void UOccupancyComponent::OnRep_Occupant(APawn* OldOccupant)
{
	OnOccupancyChanged.Broadcast(Occupant, OldOccupant);
}
