#include "HullHealthComponent.h"
#include "GalacticPirates.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

UHullHealthComponent::UHullHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	CurrentHealth = MaxHealth;
}

void UHullHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() && GetOwner()->HasAuthority() && !bDestroyed)
	{
		CurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, MaxHealth);
		if (CurrentHealth <= 0.0f)
		{
			CurrentHealth = MaxHealth;
		}
	}
	NotifyHealthChanged();
}

void UHullHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHullHealthComponent, CurrentHealth);
	DOREPLIFETIME(UHullHealthComponent, MaxHealth);
	DOREPLIFETIME(UHullHealthComponent, bDestroyed);
}

void UHullHealthComponent::ResetToFull()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bDestroyed)
	{
		return;
	}

	CurrentHealth = MaxHealth;
	NotifyHealthChanged();
}

void UHullHealthComponent::SetHealth(float NewHealth)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bDestroyed)
	{
		return;
	}

	CurrentHealth = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
	NotifyHealthChanged();
	if (CurrentHealth <= 0.0f)
	{
		DestroyHull();
	}
}

float UHullHealthComponent::ApplyDamage(const FSpaceDamageEvent& Event)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bDestroyed || Event.Amount <= 0.0f)
	{
		return 0.0f;
	}

	const float Scaled = Event.Amount * GPArmorDamageScale(ArmorClass, Event.Kind);
	CurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, MaxHealth);
	const float Applied = FMath::Min(CurrentHealth, Scaled);
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - Applied);
	NotifyHealthChanged();

	if (GPCombatLogEnabled())
	{
		UE_LOG(LogGalacticPirates, Log,
			TEXT("[Combat] %s took %.1f (raw %.1f kind=%d armor=%d) from %s via %s remaining=%.1f"),
			*Owner->GetName(),
			Applied,
			Event.Amount,
			static_cast<int32>(Event.Kind),
			static_cast<int32>(ArmorClass),
			*GetNameSafe(Event.InstigatorPawn.Get()),
			*GetNameSafe(Event.Causer.Get()),
			CurrentHealth);
	}

	if (CurrentHealth <= 0.0f)
	{
		DestroyHull();
	}

	return Applied;
}

void UHullHealthComponent::DestroyHull()
{
	if (bDestroyed)
	{
		return;
	}

	bDestroyed = true;
	CurrentHealth = 0.0f;
	NotifyHealthChanged();
	OnHullDestroyed.Broadcast();
	OnRep_Destroyed();
}

void UHullHealthComponent::NotifyHealthChanged()
{
	OnRep_CurrentHealth();
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UHullHealthComponent::OnRep_CurrentHealth()
{
}

void UHullHealthComponent::OnRep_Destroyed()
{
}

UHullHealthComponent* GPFindHullHealth(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}
	if (UHullHealthComponent* Hull = Actor->FindComponentByClass<UHullHealthComponent>())
	{
		return Hull;
	}
	if (AActor* Owner = Actor->GetOwner())
	{
		return Owner->FindComponentByClass<UHullHealthComponent>();
	}
	return nullptr;
}

float GPApplySpaceDamage(AActor* Target, const FSpaceDamageEvent& Event)
{
	if (UHullHealthComponent* Hull = GPFindHullHealth(Target))
	{
		return Hull->ApplyDamage(Event);
	}
	return 0.0f;
}
