#include "WeaponHardpointComponent.h"
#include "WeaponComponent.h"
#include "Net/UnrealNetwork.h"

UWeaponHardpointComponent::UWeaponHardpointComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetMobility(EComponentMobility::Movable);
	SetIsReplicatedByDefault(true);
}

void UWeaponHardpointComponent::BeginPlay()
{
	Super::BeginPlay();
	DiscoverEquippedWeapon();
	if (!EquippedWeapon && WeaponClass)
	{
		EquipWeaponClass(WeaponClass);
	}
}

void UWeaponHardpointComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWeaponHardpointComponent, WeaponClass);
}

void UWeaponHardpointComponent::OnRep_WeaponClass()
{
	if (WeaponClass)
	{
		EquipWeaponClass(WeaponClass);
	}
}

void UWeaponHardpointComponent::DiscoverEquippedWeapon()
{
	if (EquippedWeapon)
	{
		return;
	}

	const TArray<USceneComponent*> Children = GetAttachChildren();
	for (USceneComponent* Child : Children)
	{
		if (UWeaponComponent* Weapon = Cast<UWeaponComponent>(Child))
		{
			EquippedWeapon = Weapon;
			return;
		}
	}
}

bool UWeaponHardpointComponent::EquipWeaponClass(TSubclassOf<UWeaponComponent> NewClass)
{
	AActor* Owner = GetOwner();
	if (!Owner || !NewClass)
	{
		return false;
	}

	if (EquippedWeapon && EquippedWeapon->GetClass() == NewClass)
	{
		return true;
	}

	if (EquippedWeapon)
	{
		EquippedWeapon->DestroyComponent();
		EquippedWeapon = nullptr;
	}

	UWeaponComponent* Spawned = NewObject<UWeaponComponent>(Owner, NewClass, NAME_None, RF_Transactional);
	if (!Spawned)
	{
		return false;
	}

	Spawned->SetIsReplicated(true);
	Spawned->SetupAttachment(this);
	Spawned->RegisterComponent();
	EquippedWeapon = Spawned;
	WeaponClass = NewClass;
	return true;
}

bool UWeaponHardpointComponent::TryFire(APawn* InstigatorPawn)
{
	DiscoverEquippedWeapon();
	if (!EquippedWeapon || !EquippedWeapon->CanFireWeapon())
	{
		return false;
	}
	return EquippedWeapon->TryFireWeapon(InstigatorPawn);
}
