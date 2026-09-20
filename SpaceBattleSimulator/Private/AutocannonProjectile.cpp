#include "AutocannonProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Autocannon.h"
#include "Engine/World.h" // Added for TActorIterator

AAutocannonProjectile::AAutocannonProjectile()
{
    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    RootComponent = ProjectileMesh;
    ProjectileMesh->SetMobility(EComponentMobility::Movable);
    ProjectileMesh->SetCollisionProfileName(TEXT("Projectile"));
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Disable collision initially
    ProjectileMesh->OnComponentHit.AddDynamic(this, &AAutocannonProjectile::OnProjectileHit);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMeshAsset.Succeeded())
    {
        ProjectileMesh->SetStaticMesh(SphereMeshAsset.Object);
    }

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->InitialSpeed = 1000.0f;
    ProjectileMovement->MaxSpeed = 1000.0f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->ProjectileGravityScale = 0.0f;
    ProjectileMovement->SetActive(false); // Defer activation

    DamageAmount = 0.0f;
    ImpactFX = nullptr;

    InitialLifeSpan = 3.0f;

    Tags.Add(TEXT("projectile"));
}

void AAutocannonProjectile::Initialize(float Speed, FVector InheritedVelocity, float Damage, UParticleSystem* ImpactEffect)
{
    if (GEngine && GEngine->IsValidLowLevel())
    {
        UE_LOG(LogTemp, Log, TEXT("Initializing AutocannonProjectile with Speed=%f, Damage=%f"), Speed, Damage);
    }

    DamageAmount = Damage;
    ImpactFX = ImpactEffect;

    // Set up collision ignores for owner, parent owner, attached actors, and friendly tagged actors
    if (GetOwner())
    {
        ProjectileMesh->IgnoreActorWhenMoving(GetOwner(), true);
        if (GEngine && GEngine->IsValidLowLevel())
        {
            UE_LOG(LogTemp, Log, TEXT("AutocannonProjectile: Ignoring owner %s for collision"), *GetOwner()->GetName());
        }

        if (AActor* ParentOwnerInitial = GetOwner()->GetOwner())
        {
            ProjectileMesh->IgnoreActorWhenMoving(ParentOwnerInitial, true);
            if (GEngine && GEngine->IsValidLowLevel())
            {
                UE_LOG(LogTemp, Log, TEXT("AutocannonProjectile: Ignoring parent owner %s for collision"), *ParentOwnerInitial->GetName());
            }

            TArray<AActor*> AttachedActorsInitial;
            ParentOwnerInitial->GetAttachedActors(AttachedActorsInitial);
            for (AActor* AttachedActor : AttachedActorsInitial)
            {
                if (AttachedActor)
                {
                    ProjectileMesh->IgnoreActorWhenMoving(AttachedActor, true);
                    if (GEngine && GEngine->IsValidLowLevel())
                    {
                        UE_LOG(LogTemp, Log, TEXT("AutocannonProjectile: Ignoring attached actor %s for collision"), *AttachedActor->GetName());
                    }
                }
            }

            // Replace TActorIterator with GetAllActorsWithTag
            TArray<AActor*> FriendlyShips;
            UGameplayStatics::GetAllActorsWithTag(GetWorld(), TEXT("FriendlyShip"), FriendlyShips);
            for (AActor* Actor : FriendlyShips)
            {
                if (Actor)
                {
                    ProjectileMesh->IgnoreActorWhenMoving(Actor, true);
                    if (GEngine && GEngine->IsValidLowLevel())
                    {
                        UE_LOG(LogTemp, Log, TEXT("AutocannonProjectile: Ignoring FriendlyShip tagged actor %s for collision"), *Actor->GetName());
                    }
                }
            }
        }
    }

    // Explicitly ignore common and custom collision channels
    ProjectileMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    ProjectileMesh->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
    ProjectileMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore); // In case ship uses static for parts
    ProjectileMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore); // In case ship uses dynamic

    // Enable collision after ignores
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    // Configure and activate projectile movement after collision setup
    ProjectileMovement->InitialSpeed = Speed;
    ProjectileMovement->MaxSpeed = Speed;
    ProjectileMovement->Velocity = GetActorForwardVector() * Speed + InheritedVelocity;
    ProjectileMovement->SetUpdatedComponent(RootComponent);
    ProjectileMovement->SetActive(true);

    if (GEngine && GEngine->IsValidLowLevel())
    {
        UE_LOG(LogTemp, Log, TEXT("Projectile Velocity: %s, Damage: %f, CollisionEnabled: %s, MovementActive: %s"),
            *ProjectileMovement->Velocity.ToString(), DamageAmount,
            ProjectileMesh->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics ? TEXT("True") : TEXT("False"),
            ProjectileMovement->IsActive() ? TEXT("True") : TEXT("False"));
    }
}

void AAutocannonProjectile::SetMaterial(UMaterialInterface* Material)
{
    if (Material && ProjectileMesh)
    {
        ProjectileMesh->SetMaterial(0, Material);
    }
}

void AAutocannonProjectile::SetProjectileScale(float Scale)
{
    if (ProjectileMesh)
    {
        ProjectileMesh->SetWorldScale3D(FVector(Scale));
        if (GEngine && GEngine->IsValidLowLevel())
        {
            UE_LOG(LogTemp, Log, TEXT("AutocannonProjectile: Set scale to %f"), Scale);
        }
    }
}

void AAutocannonProjectile::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (!HitComponent || !GetWorld())
    {
        Destroy();
        return;
    }

    // Skip processing if hitting owner, parent owner, or FriendlyShip tagged actor
    if (OtherActor && OtherActor != GetOwner() && OtherActor != GetOwner()->GetOwner() && !OtherActor->ActorHasTag(TEXT("FriendlyShip")))
    {
        if (GEngine && GEngine->IsValidLowLevel())
        {
            UE_LOG(LogTemp, Log, TEXT("Projectile hit %s (Component: %s) at location %s, dealing %f damage"),
                *OtherActor->GetName(), OtherComp ? *OtherComp->GetName() : TEXT("None"), *Hit.Location.ToString(), DamageAmount);
        }

        UGameplayStatics::ApplyDamage(
            OtherActor,
            DamageAmount,
            GetOwner() ? GetOwner()->GetInstigatorController() : nullptr,
            GetOwner(),
            UDamageType::StaticClass()
        );
    }
    else
    {
        if (GEngine && GEngine->IsValidLowLevel())
        {
            UE_LOG(LogTemp, Warning, TEXT("Projectile hit ignored actor %s (Component: %s) at location %s"),
                OtherActor ? *OtherActor->GetName() : TEXT("null"), OtherComp ? *OtherComp->GetName() : TEXT("None"), *Hit.Location.ToString());
        }
    }

    if (ImpactFX)
    {
        UAutocannon* Autocannon = Cast<UAutocannon>(GetOwner() ? GetOwner()->GetComponentByClass(UAutocannon::StaticClass()) : nullptr);
        if (Autocannon)
        {
            UParticleSystemComponent* ImpactEffectComponent = Autocannon->GetPooledParticleComponent(Autocannon->ImpactEffectPool, ImpactFX);
            if (ImpactEffectComponent)
            {
                ImpactEffectComponent->SetWorldLocation(Hit.Location);
                ImpactEffectComponent->SetWorldRotation(Hit.ImpactNormal.Rotation());
                Autocannon->ActivateParticleComponent(ImpactEffectComponent, ImpactFX->SecondsBeforeInactive > 0.0f ? ImpactFX->SecondsBeforeInactive : 2.0f);
            }
        }
    }

    Destroy();
}