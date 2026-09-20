#include "Autocannon.h"
#include "AutocannonProjectile.h"
#include "Kismet/GameplayStatics.h"
#include "Components/ArrowComponent.h"
#include "DrawDebugHelpers.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"

UAutocannon::UAutocannon()
{
	PrimaryComponentTick.bCanEverTick = true;

	FireRate = 0.1f;
	ProjectileSpeed = 100000.0f;
	DamageAmount = 10.0f;
	FirstFireDelay = 0.0f;
	ProjectileScale = 0.2f;
	CurrentFireCooldown = 0.0f;
	IsFiring = false;
	MuzzleFlashRotationOffset = FRotator::ZeroRotator;
	MuzzleFlashOffset = FVector(0.0f, 0.0f, 0.0f);
	MuzzleFlashScale = 1.0f;
	TargetActor = nullptr;
	EnableDebugDraws = false;
	EnableDebugLogs = false;
	ImpactFX = nullptr;
	ParticlePoolSize = 20;

	DebugArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DebugArrow"));
	DebugArrow->SetupAttachment(this);
	DebugArrow->SetRelativeLocation(FVector::ZeroVector);
	DebugArrow->ArrowSize = 0.5f;
	DebugArrow->ArrowColor = FColor::White;
	DebugArrow->bHiddenInGame = true;

	MuzzleFlashFX = nullptr;
	FiringSFX = nullptr;
}

void UAutocannon::InitializeParticlePool()
{
	ImpactEffectPool.Reserve(ParticlePoolSize);
	MuzzleFlashPool.Reserve(ParticlePoolSize);

	for (int32 Index = 0; Index < ParticlePoolSize; Index++)
	{
		UParticleSystemComponent* ImpactComponent = NewObject<UParticleSystemComponent>(this, FName(*FString::Printf(TEXT("ImpactParticle_%d"), Index)));
		ImpactComponent->bAutoActivate = false;
		ImpactComponent->RegisterComponent();
		ImpactEffectPool.Add(ImpactComponent);

		UParticleSystemComponent* MuzzleComponent = NewObject<UParticleSystemComponent>(this, FName(*FString::Printf(TEXT("MuzzleParticle_%d"), Index)));
		MuzzleComponent->bAutoActivate = false;
		MuzzleComponent->SetupAttachment(this);
		MuzzleComponent->SetRelativeLocation(MuzzleFlashOffset);
		MuzzleComponent->SetRelativeRotation(MuzzleFlashRotationOffset);
		MuzzleComponent->SetRelativeScale3D(FVector(MuzzleFlashScale));
		MuzzleComponent->RegisterComponent();

		if (!MuzzleComponent->IsAttachedTo(this))
		{
			UE_LOG(LogTemp, Warning, TEXT("InitializeParticlePool: MuzzleParticle_%d failed to attach to %s"), Index, *GetName());
		}

		MuzzleFlashPool.Add(MuzzleComponent);

		if (EnableDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("InitializeParticlePool: MuzzleParticle_%d attached to %s, RelativeLocation=%s, WorldLocation=%s"),
				Index, *GetName(), *MuzzleFlashOffset.ToString(), *MuzzleComponent->GetComponentLocation().ToString());
		}
	}
}

UParticleSystemComponent* UAutocannon::GetPooledParticleComponent(TArray<UParticleSystemComponent*>& Pool, UParticleSystem* Template)
{
	int32 ActiveCount = 0;
	for (UParticleSystemComponent* Component : Pool)
	{
		if (Component)
		{
			if (Component->IsActive())
			{
				ActiveCount++;
			}
			else
			{
				Component->SetTemplate(Template);
				if (EnableDebugLogs)
				{
					UE_LOG(LogTemp, Log, TEXT("GetPooledParticleComponent: Reusing component %s with template %s"), *Component->GetName(), *Template->GetName());
				}
				return Component;
			}
		}
	}
	if (EnableDebugLogs)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetPooledParticleComponent: No inactive components found, %d/%d active"), ActiveCount, Pool.Num());
	}
	return nullptr;
}

void UAutocannon::ActivateParticleComponent(UParticleSystemComponent* Component, float Duration, bool IsMuzzleFlash)
{
	if (!Component || !Component->Template)
	{
		if (EnableDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("ActivateParticleComponent: Invalid component or template"));
		}
		return;
	}

	if (IsMuzzleFlash)
	{
		if (!Component->IsAttachedTo(this))
		{
			Component->SetupAttachment(this);
			if (EnableDebugLogs)
			{
				UE_LOG(LogTemp, Warning, TEXT("ActivateParticleComponent: Re-attached MuzzleFlash %s to %s"), *Component->GetName(), *GetName());
			}
		}

		Component->SetRelativeLocation(MuzzleFlashOffset);
		Component->SetRelativeRotation(MuzzleFlashRotationOffset);
		Component->SetRelativeScale3D(FVector(MuzzleFlashScale));

		if (EnableDebugLogs)
		{
			FVector WorldLocation = Component->GetComponentLocation();
			UE_LOG(LogTemp, Log, TEXT("ActivateParticleComponent: MuzzleFlash %s at RelativeLocation=%s, WorldLocation=%s, AutocannonLocation=%s, Duration=%f"),
				*Component->GetName(), *MuzzleFlashOffset.ToString(), *WorldLocation.ToString(), *GetComponentLocation().ToString(), Duration);

			if (WorldLocation.IsNearlyZero())
			{
				UE_LOG(LogTemp, Warning, TEXT("ActivateParticleComponent: MuzzleFlash %s has world location at origin!"), *Component->GetName());
			}
		}
	}

	Component->ActivateSystem(true);
	ActiveParticles.Add(FParticleInstance(Component, Duration));
}

void UAutocannon::BeginPlay()
{
	Super::BeginPlay();
	CurrentFireCooldown = 0.0f;
	IsFiring = false;
	TargetActor = nullptr;

	InitializeParticlePool();

	if (DebugArrow)
	{
		DebugArrow->SetVisibility(EnableDebugDraws);
	}
}

void UAutocannon::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CurrentFireCooldown > 0.0f)
	{
		CurrentFireCooldown -= DeltaTime;
	}

	if (IsFiring && CurrentFireCooldown <= 0.0f)
	{
		Fire();
	}

	for (int32 Index = ActiveParticles.Num() - 1; Index >= 0; Index--)
	{
		FParticleInstance& Particle = ActiveParticles[Index];
		if (Particle.Component)
		{
			Particle.RemainingLifetime -= DeltaTime;
			if (Particle.RemainingLifetime <= 0.0f)
			{
				Particle.Component->DeactivateSystem();
				Particle.Component->SetActive(false);
				Particle.Component->SetTemplate(nullptr);
				if (EnableDebugLogs)
				{
					UE_LOG(LogTemp, Log, TEXT("TickComponent: Deactivated particle %s, IsActive=%s, returning to pool"),
						*Particle.Component->GetName(), Particle.Component->IsActive() ? TEXT("true") : TEXT("false"));
				}
				ActiveParticles.RemoveAt(Index);
			}
		}
		else
		{
			ActiveParticles.RemoveAt(Index);
		}
	}
}

void UAutocannon::StartFiring()
{
	if (!IsFiring)
	{
		IsFiring = true;
		CurrentFireCooldown = FirstFireDelay;
		if (CurrentFireCooldown <= 0.0f)
		{
			Fire();
		}
	}
}

void UAutocannon::StopFiring()
{
	IsFiring = false;
}

void UAutocannon::SetTargetActor(AActor* NewTarget)
{
	TargetActor = NewTarget;
}

void UAutocannon::Fire()
{
	if (CurrentFireCooldown > 0.0f || !GetWorld())
	{
		return;
	}

	IsFiring = true;

	FVector SpawnLocation = GetComponentTransform().TransformPosition(MuzzleFlashOffset);
	FRotator SpawnRotation = GetComponentRotation();

	if (IsValid(TargetActor))
	{
		FVector DirectionToTarget = (TargetActor->GetActorLocation() - SpawnLocation).GetSafeNormal();
		SpawnRotation = DirectionToTarget.Rotation();
		if (DebugArrow && EnableDebugDraws)
		{
			DebugArrow->SetWorldRotation(SpawnRotation);
		}
	}

	if (MuzzleFlashFX)
	{
		if (EnableDebugLogs)
		{
			UE_LOG(LogTemp, Log, TEXT("Autocannon: Attempting to spawn MuzzleFlash with Template=%s"), *MuzzleFlashFX->GetName());
		}

		UParticleSystemComponent* MuzzleFlash = GetPooledParticleComponent(MuzzleFlashPool, MuzzleFlashFX);
		if (MuzzleFlash)
		{
			float Duration = MuzzleFlashFX->SecondsBeforeInactive > 0.0f ? MuzzleFlashFX->SecondsBeforeInactive : 0.5f;
			if (EnableDebugLogs)
			{
				UE_LOG(LogTemp, Log, TEXT("Autocannon: Using MuzzleFlash duration=%f"), Duration);
			}
			ActivateParticleComponent(MuzzleFlash, Duration, true);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Autocannon: Failed to get pooled MuzzleFlash component"));
		}
	}

	if (FiringSFX)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FiringSFX, SpawnLocation, SpawnRotation);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = GetOwner()->GetInstigator();

	AAutocannonProjectile* Projectile = GetWorld()->SpawnActor<AAutocannonProjectile>(
		AAutocannonProjectile::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);

	if (Projectile)
	{
		FVector OwnerVelocity = GetOwner() ? GetOwner()->GetVelocity() : FVector::ZeroVector;
		Projectile->Initialize(ProjectileSpeed, OwnerVelocity, DamageAmount, ImpactFX);
		Projectile->SetMaterial(ProjectileMaterial);
		Projectile->SetProjectileScale(ProjectileScale);

		// Additional safety: Ensure the projectile's mesh ignores the owner and its hierarchy
		if (GetOwner())
		{
			UStaticMeshComponent* ProjectileMesh = Projectile->FindComponentByClass<UStaticMeshComponent>();
			if (ProjectileMesh)
			{
				ProjectileMesh->IgnoreActorWhenMoving(GetOwner(), true);
				if (AActor* ParentOwner = GetOwner()->GetOwner())
				{
					ProjectileMesh->IgnoreActorWhenMoving(ParentOwner, true);
					TArray<AActor*> AttachedActors;
					ParentOwner->GetAttachedActors(AttachedActors);
					for (AActor* AttachedActor : AttachedActors)
					{
						if (AttachedActor)
						{
							ProjectileMesh->IgnoreActorWhenMoving(AttachedActor, true);
						}
					}
				}
			}
		}
	}

	CurrentFireCooldown = FireRate;
}

void UAutocannon::ToggleDebugArrow(bool ShowDebugArrow)
{
	if (DebugArrow && EnableDebugDraws)
	{
		DebugArrow->SetVisibility(ShowDebugArrow);
	}
}