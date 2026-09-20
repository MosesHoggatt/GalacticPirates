#include "ShipWreckDebris.h"
#include "WalkableShip.h"
#include "GalacticPirates.h"
#include "ShipPolish.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

AShipWreckDebris::AShipWreckDebris()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	DebrisRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DebrisRoot"));
	RootComponent = DebrisRoot;
	DebrisRoot->SetMobility(EComponentMobility::Movable);
}

void AShipWreckDebris::BeginPlay()
{
	Super::BeginPlay();
}

void AShipWreckDebris::ApplyEmissive(UPrimitiveComponent* Mesh, const FLinearColor& Color) const
{
	if (!Mesh)
	{
		return;
	}

	if (UMaterialInstanceDynamic* MID = GPApplyPolishVfxMaterial(Mesh, TEXT("VFX_Ember"), Color))
	{
		return;
	}

	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultWhiteGrid.DefaultWhiteGrid")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Mesh);
		Mesh->SetMaterial(0, MID);
	}
}

UPrimitiveComponent* AShipWreckDebris::SpawnPhysicsCube(const FTransform& Transform, UStaticMesh* CubeMesh, const FLinearColor& Color)
{
	if (!CubeMesh)
	{
		return nullptr;
	}

	UStaticMeshComponent* Chunk = NewObject<UStaticMeshComponent>(this);
	Chunk->SetMobility(EComponentMobility::Movable);
	Chunk->SetStaticMesh(CubeMesh);
	Chunk->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Chunk->SetCollisionObjectType(ECC_PhysicsBody);
	Chunk->SetCollisionResponseToAllChannels(ECR_Block);
	Chunk->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Chunk->SetCastShadow(true);
	Chunk->RegisterComponent();
	Chunk->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	Chunk->SetWorldTransform(Transform);
	ApplyEmissive(Chunk, Color);
	PhysicsChunks.Add(Chunk);
	return Chunk;
}

void AShipWreckDebris::KickFragments()
{
	int32 Kicked = 0;
	for (UPrimitiveComponent* Chunk : PhysicsChunks)
	{
		if (!Chunk)
		{
			continue;
		}

		Chunk->SetSimulatePhysics(true);
		Chunk->SetEnableGravity(false);
		Chunk->SetLinearDamping(0.04f);
		Chunk->SetAngularDamping(0.06f);
		Chunk->SetMassOverrideInKg(NAME_None, 40.0f, true);
		Chunk->WakeAllRigidBodies();

		const FVector Away = (Chunk->GetComponentLocation() - Epicenter).GetSafeNormal();
		const FVector Dir = Away.IsNearlyZero() ? FMath::VRand() : Away;
		Chunk->AddImpulse(Dir * 1800.0f + FMath::VRand() * 500.0f, NAME_None, true);
		Chunk->AddTorqueInRadians(FMath::VRand() * 12.0f, NAME_None, true);
		++Kicked;
	}

	UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipCombat] Wreck impulse applied to %d fragments at %s"),
		Kicked,
		*Epicenter.ToCompactString());
}

void AShipWreckDebris::InitializeFromShip(AWalkableShip* Ship)
{
	if (!Ship)
	{
		return;
	}

	UWorld* World = GetWorld();
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!World || !CubeMesh)
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipCombat] Wreck debris aborted: missing world or cube mesh"));
		return;
	}

	Epicenter = Ship->GetActorLocation();
	SetActorLocationAndRotation(Epicenter, Ship->GetActorRotation());
	SetLifeSpan(24.0f);

	UPointLightComponent* BlastLight = NewObject<UPointLightComponent>(this);
	BlastLight->SetupAttachment(DebrisRoot);
	BlastLight->SetIntensity(160000.0f);
	BlastLight->SetAttenuationRadius(6000.0f);
	BlastLight->SetLightColor(FLinearColor(1.0f, 0.45f, 0.12f));
	BlastLight->SetCastShadows(false);
	BlastLight->RegisterComponent();

	TArray<UStaticMeshComponent*> SourceMeshes;
	Ship->GetComponents<UStaticMeshComponent>(SourceMeshes);

	int32 ClonedMeshes = 0;
	for (UStaticMeshComponent* Source : SourceMeshes)
	{
		if (!Source || !Source->GetStaticMesh() || !Source->IsVisible())
		{
			continue;
		}

		const FBoxSphereBounds Bounds = Source->Bounds;
		const FVector Extent = Bounds.BoxExtent;
		if (Extent.GetAbsMax() < 20.0f)
		{
			continue;
		}

		const int32 DivX = Extent.X > 250.0f ? 3 : 2;
		const int32 DivY = Extent.Y > 180.0f ? 3 : 2;
		const int32 DivZ = Extent.Z > 120.0f ? 2 : 1;
		const FVector PieceExtent = FVector(Extent.X / DivX, Extent.Y / DivY, Extent.Z / FMath::Max(DivZ, 1)) * 0.92f;
		const FVector Scale = (PieceExtent / 50.0f).ComponentMax(FVector(0.35f));

		for (int32 X = 0; X < DivX; ++X)
		{
			for (int32 Y = 0; Y < DivY; ++Y)
			{
				for (int32 Z = 0; Z < DivZ; ++Z)
				{
					const FVector Local = FVector(
						(X + 0.5f) / DivX * 2.0f - 1.0f,
						(Y + 0.5f) / DivY * 2.0f - 1.0f,
						(Z + 0.5f) / FMath::Max(DivZ, 1) * 2.0f - 1.0f) * Extent * 0.72f;
					FTransform PieceTM;
					PieceTM.SetLocation(Bounds.Origin + Ship->GetActorQuat().RotateVector(Local));
					PieceTM.SetRotation(Source->GetComponentQuat());
					PieceTM.SetScale3D(Scale);
					SpawnPhysicsCube(PieceTM, CubeMesh, FLinearColor(0.55f, 0.42f, 0.28f, 1.0f));
				}
			}
		}
		++ClonedMeshes;
	}

	const FVector HullExtent = Ship->CombatHull ? Ship->CombatHull->GetScaledBoxExtent() : FVector(900.0f, 500.0f, 320.0f);
	const FVector HullCenter = Ship->CombatHull ? Ship->CombatHull->GetComponentLocation() : Epicenter;
	const FQuat ShipRot = Ship->GetActorQuat();
	for (int32 Index = 0; Index < 18; ++Index)
	{
		const FVector LocalOffset = FVector(
			FMath::FRandRange(-HullExtent.X, HullExtent.X),
			FMath::FRandRange(-HullExtent.Y, HullExtent.Y),
			FMath::FRandRange(-HullExtent.Z, HullExtent.Z)) * 0.8f;
		FTransform PieceTM;
		PieceTM.SetLocation(HullCenter + ShipRot.RotateVector(LocalOffset));
		PieceTM.SetRotation(ShipRot * FQuat(FMath::VRand(), FMath::FRandRange(0.0f, PI)));
		PieceTM.SetScale3D(FVector(
			FMath::FRandRange(1.6f, 5.5f),
			FMath::FRandRange(0.18f, 0.85f),
			FMath::FRandRange(0.12f, 0.7f)));
		SpawnPhysicsCube(PieceTM, CubeMesh, FLinearColor(1.0f, 0.38f, 0.08f, 1.0f));
	}

	UE_LOG(LogGalacticPirates, Warning, TEXT("[ShipCombat] Wreck debris spawned chunks=%d fromMeshes=%d"),
		PhysicsChunks.Num(),
		ClonedMeshes);

	KickFragments();
	FTimerHandle KickHandle;
	World->GetTimerManager().SetTimer(KickHandle, this, &AShipWreckDebris::KickFragments, 0.05f, false);
}
