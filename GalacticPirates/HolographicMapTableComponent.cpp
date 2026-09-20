#include "HolographicMapTableComponent.h"
#include "HoloMapPoiComponent.h"
#include "HeatseekingMissile.h"
#include "WalkableShip.h"
#include "BulldogFighter.h"
#include "SpaceCraft.h"
#include "GalacticPiratesCharacter.h"
#include "ShipPolish.h"
#include "GalacticPirates.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/BoxComponent.h"
#include "HoloMapScanRange.h"

namespace
{
	bool CanMutateHoloMapAttachments(const UActorComponent* Comp)
	{
		if (!Comp || Comp->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			return false;
		}
		if (GIsReinstancing.load() || GIsReconstructingBlueprintInstances)
		{
			return false;
		}
		const AActor* Owner = Comp->GetOwner();
		return Owner && !Owner->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject);
	}
}

UHolographicMapTableComponent::UHolographicMapTableComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetMobility(EComponentMobility::Movable);

	// Children are built in BuildRig() at BeginPlay. Default subobjects made inside a
	// component constructor cannot be instanced on a blueprint-derived actor.
}

void UHolographicMapTableComponent::BuildRig()
{
	AActor* Owner = GetOwner();
	if (bRigBuilt || !Owner)
	{
		return;
	}
	bRigBuilt = true;

	// A blueprint saved against the old constructor hands us its own loose copies of these.
	USceneComponent* Stale[] = { TableMesh, HoloVolumeMesh, OwnShipMarker, HoloLight, EquatorRing, MeridianRing, TransverseRing };
	for (USceneComponent* Old : Stale)
	{
		if (Old && Old->GetOwner() == Owner)
		{
			Old->DestroyComponent();
		}
	}

	auto NameFor = [this](const TCHAR* Suffix)
	{
		return FName(*FString::Printf(TEXT("%s_%s"), *GetName(), Suffix));
	};

	auto InitChild = [this](USceneComponent* Child)
	{
		Child->SetMobility(EComponentMobility::Movable);
		Child->SetUsingAbsoluteLocation(false);
		Child->SetUsingAbsoluteRotation(false);
		Child->SetUsingAbsoluteScale(false);
		Child->RegisterComponent();
		Child->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	};

	TableMesh = NewObject<UStaticMeshComponent>(Owner, NameFor(TEXT("TableMesh")));
	InitChild(TableMesh);
	TableMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TableMesh->SetCollisionObjectType(ECC_WorldDynamic);
	TableMesh->SetCollisionResponseToAllChannels(ECR_Block);
	TableMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	TableMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 66.0f));
	TableMesh->SetRelativeScale3D(FVector(2.4f, 2.4f, 1.32f));
	TableMesh->SetCastShadow(true);

	OwnShipMarker = NewObject<UStaticMeshComponent>(Owner, NameFor(TEXT("OwnShipMarker")));
	InitChild(OwnShipMarker);
	OwnShipMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OwnShipMarker->SetRelativeLocation(FVector(0.0f, 0.0f, VolumeCenterZ));
	OwnShipMarker->SetRelativeScale3D(IconScaleForPrimitive(EHoloMapPrimitive::Cube));
	OwnShipMarker->SetCastShadow(false);

	HoloVolumeMesh = NewObject<UStaticMeshComponent>(Owner, NameFor(TEXT("HoloVolumeMesh")));
	InitChild(HoloVolumeMesh);
	HoloVolumeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HoloVolumeMesh->SetCastShadow(false);
	HoloVolumeMesh->SetVisibility(false);
	HoloVolumeMesh->SetHiddenInGame(true);

	HoloLight = NewObject<UPointLightComponent>(Owner, NameFor(TEXT("HoloLight")));
	InitChild(HoloLight);
	HoloLight->SetRelativeLocation(FVector(0.0f, 0.0f, VolumeCenterZ));
	HoloLight->SetIntensity(1200.0f);
	HoloLight->SetAttenuationRadius(180.0f);
	HoloLight->SetLightColor(FLinearColor(0.25f, 0.85f, 1.0f));
	HoloLight->SetCastShadows(false);
	HoloLight->bUseInverseSquaredFalloff = false;

	EquatorRing = NewObject<UStaticMeshComponent>(Owner, NameFor(TEXT("EquatorRing")));
	InitChild(EquatorRing);
	MeridianRing = NewObject<UStaticMeshComponent>(Owner, NameFor(TEXT("MeridianRing")));
	InitChild(MeridianRing);
	TransverseRing = NewObject<UStaticMeshComponent>(Owner, NameFor(TEXT("TransverseRing")));
	InitChild(TransverseRing);

	if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		TableMesh->SetStaticMesh(CubeMesh);
		OwnShipMarker->SetStaticMesh(CubeMesh);
	}
	HideCylinderMesh(EquatorRing);
	HideCylinderMesh(MeridianRing);
	HideCylinderMesh(TransverseRing);
	HideCylinderMesh(HoloVolumeMesh);
	HideStaleVolumeCones();
}

void UHolographicMapTableComponent::OnRegister()
{
	Super::OnRegister();
}

void UHolographicMapTableComponent::BeginPlay()
{
	Super::BeginPlay();
	BuildRig();
	ScanRangeCm = GPHoloMapScanRangeCm();
	ResolveOwningShip();
	PlaceInCabin();

	TintMarker(TableMesh, FLinearColor(0.55f, 0.58f, 0.62f, 1.0f));
	TintMarker(OwnShipMarker, FLinearColor(0.15f, 0.55f, 1.0f, 1.0f));

	SetVisualsVisible(ShouldDrawVisuals());
	RebuildTrackedPois();
	RefreshMarkers();
}

bool UHolographicMapTableComponent::ShouldDrawVisuals() const
{
	if (!OwningShip || OwningShip->IsWrecked() || GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	for (AGalacticPiratesCharacter* Character : OwningShip->GetPlayersAboard())
	{
		if (Character && Character->IsLocallyControlled())
		{
			return true;
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (AGalacticPiratesCharacter* Pawn = Cast<AGalacticPiratesCharacter>(PC->GetPawn()))
			{
				return Pawn->GetBoardedShip() == OwningShip;
			}
		}
	}

	return false;
}

void UHolographicMapTableComponent::SetVisualsVisible(bool bShowVisuals)
{
	if (TableMesh) { TableMesh->SetVisibility(bShowVisuals); }
	if (HoloVolumeMesh) { HoloVolumeMesh->SetVisibility(false); }
	if (OwnShipMarker) { OwnShipMarker->SetVisibility(bShowVisuals); }
	if (HoloLight) { HoloLight->SetVisibility(bShowVisuals); }
	if (EquatorRing) { EquatorRing->SetVisibility(false); }
	if (MeridianRing) { MeridianRing->SetVisibility(false); }
	if (TransverseRing) { TransverseRing->SetVisibility(false); }
}

void UHolographicMapTableComponent::ResolveOwningShip()
{
	OwningShip = Cast<AWalkableShip>(GetOwner());
	if (!OwningShip)
	{
		OwningShip = Cast<AWalkableShip>(GetTypedOuter<AWalkableShip>());
	}
}

void UHolographicMapTableComponent::BindVisualToTable(USceneComponent* Child, const FVector& RelLoc, const FRotator& RelRot, const FVector& RelScale)
{
	if (!Child)
	{
		return;
	}

	Child->SetMobility(EComponentMobility::Movable);
	Child->SetUsingAbsoluteLocation(false);
	Child->SetUsingAbsoluteRotation(false);
	Child->SetUsingAbsoluteScale(false);
	if (Child->GetOwner() != GetOwner()
		|| Child->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
		|| HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}
	if (Child->GetAttachParent() != this)
	{
		Child->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	Child->SetRelativeLocationAndRotation(RelLoc, RelRot);
	Child->SetRelativeScale3D(RelScale);
}

void UHolographicMapTableComponent::PlaceInCabin()
{
	if (!CanMutateHoloMapAttachments(this))
	{
		return;
	}

	ResolveOwningShip();
	if (!OwningShip)
	{
		return;
	}

	USceneComponent* ShipOrigin = OwningShip->ShipRoot ? OwningShip->ShipRoot : OwningShip->GetRootComponent();
	if (!ShipOrigin)
	{
		return;
	}

	SetMobility(EComponentMobility::Movable);
	SetUsingAbsoluteLocation(false);
	SetUsingAbsoluteRotation(false);
	SetUsingAbsoluteScale(false);

	if (GetAttachParent() != ShipOrigin)
	{
		AttachToComponent(ShipOrigin, FAttachmentTransformRules::KeepWorldTransform);
	}

	BindVisualToTable(TableMesh, FVector(0.0f, 0.0f, 66.0f), FRotator::ZeroRotator, FVector(2.4f, 2.4f, 1.32f));
	BindVisualToTable(OwnShipMarker, FVector(0.0f, 0.0f, VolumeCenterZ), FRotator::ZeroRotator, IconScaleForPrimitive(EHoloMapPrimitive::Cube));
	BindVisualToTable(HoloLight, FVector(0.0f, 0.0f, VolumeCenterZ), FRotator::ZeroRotator, FVector::OneVector);
	HideCylinderMesh(EquatorRing);
	HideCylinderMesh(MeridianRing);
	HideCylinderMesh(TransverseRing);
	HideCylinderMesh(HoloVolumeMesh);

	const float DriftFromShip = FVector::Dist(GetComponentLocation(), OwningShip->GetActorLocation());
	if (GetAttachParent() != ShipOrigin)
	{
		UE_LOG(LogGalacticPirates, Warning,
			TEXT("[HoloMap] Table parent is not ship root ship=%s parent=%s drift=%.1f tableWorld=%s shipWorld=%s"),
			*GetNameSafe(OwningShip),
			*GetNameSafe(GetAttachParent()),
			DriftFromShip,
			*GetComponentLocation().ToCompactString(),
			*OwningShip->GetActorLocation().ToCompactString());
	}
}

UStaticMesh* UHolographicMapTableComponent::LoadPrimitiveMesh(EHoloMapPrimitive Primitive) const
{
	return GPLoadHoloPrimitiveMesh(Primitive);
}

FVector UHolographicMapTableComponent::IconScaleForPrimitive(EHoloMapPrimitive Primitive) const
{
	switch (Primitive)
	{
	case EHoloMapPrimitive::Plane:
		return FVector(0.10f, 0.10f, 0.02f);
	default:
		return FVector(0.10f, 0.10f, 0.10f);
	}
}

void UHolographicMapTableComponent::HideStaleVolumeCones()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TInlineComponentArray<UStaticMeshComponent*> Meshes(Owner);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (!Mesh || Mesh == TableMesh || Mesh == OwnShipMarker || MarkerPool.Contains(Mesh))
		{
			continue;
		}
		if (Mesh->GetAttachParent() != this)
		{
			continue;
		}
		const UStaticMesh* Asset = Mesh->GetStaticMesh();
		if (Asset && Asset->GetName().Contains(TEXT("Cone")))
		{
			HideCylinderMesh(Mesh);
		}
	}
}

FRotator UHolographicMapTableComponent::MarkerHeading(EHoloMapPrimitive Primitive, const FVector& RelForward) const
{
	if (RelForward.IsNearlyZero())
	{
		return FRotator::ZeroRotator;
	}

	if (Primitive == EHoloMapPrimitive::Triangle || Primitive == EHoloMapPrimitive::Cone)
	{
		return FRotationMatrix::MakeFromZ(RelForward).Rotator();
	}

	return FRotator(0.0f, RelForward.Rotation().Yaw, 0.0f);
}

UStaticMeshComponent* UHolographicMapTableComponent::FindDisplayedMarker(const AActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	for (const FHoloMarkerInterp& State : MarkerInterps)
	{
		if (State.Actor.Get() == Actor && MarkerPool.IsValidIndex(State.PoolIndex))
		{
			return MarkerPool[State.PoolIndex].Get();
		}
	}

	return nullptr;
}

void UHolographicMapTableComponent::BindPooledMesh(UStaticMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return;
	}

	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetUsingAbsoluteLocation(false);
	Mesh->SetUsingAbsoluteRotation(false);
	Mesh->SetUsingAbsoluteScale(false);
	if (Mesh->GetAttachParent() != this)
	{
		Mesh->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
}

UStaticMeshComponent* UHolographicMapTableComponent::GetOrCreatePooledMesh(TArray<TObjectPtr<UStaticMeshComponent>>& Pool, int32 Index)
{
	while (Pool.Num() <= Index)
	{
		if (GIsReinstancing.load())
		{
			return nullptr;
		}

		AActor* OwnerActor = OwningShip ? static_cast<AActor*>(OwningShip.Get()) : GetOwner();
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(OwnerActor);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetupAttachment(this);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetUsingAbsoluteLocation(false);
		Mesh->SetUsingAbsoluteRotation(false);
		Mesh->SetUsingAbsoluteScale(false);
		Mesh->RegisterComponent();
		BindPooledMesh(Mesh);
		Pool.Add(Mesh);
	}

	BindPooledMesh(Pool[Index]);
	return Pool[Index];
}

FVector UHolographicMapTableComponent::WorldOffsetToVolume(const FVector& ShipLocalOffset) const
{
	const float Range = FMath::Max(ScanRangeCm, 1.0f);
	FVector Scaled = (ShipLocalOffset / Range) * VolumeRadiusCm;
	const float Radius = Scaled.Size();
	if (Radius > VolumeRadiusCm)
	{
		Scaled *= VolumeRadiusCm / Radius;
	}
	return FVector(Scaled.X, Scaled.Y, VolumeCenterZ + Scaled.Z);
}

void UHolographicMapTableComponent::HideCylinderMesh(UStaticMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return;
	}
	Mesh->SetStaticMesh(nullptr);
	Mesh->SetVisibility(false);
	Mesh->SetHiddenInGame(true);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

FVector UHolographicMapTableComponent::GetActorSizeCm(const AActor* Actor) const
{
	if (const AWalkableShip* Ship = Cast<AWalkableShip>(Actor))
	{
		if (Ship->CombatHull)
		{
			return (Ship->CombatHull->GetScaledBoxExtent() * 2.0f).GetAbs();
		}

		if (Ship->HullMesh && Ship->HullMesh->GetStaticMesh())
		{
			return (Ship->HullMesh->GetStaticMesh()->GetBoundingBox().GetSize() * Ship->HullMesh->GetComponentScale()).GetAbs();
		}
	}

	if (!Actor)
	{
		return FVector(2200.0f, 1200.0f, 800.0f);
	}

	FVector Origin = FVector::ZeroVector;
	FVector Extent = FVector::ZeroVector;
	Actor->GetActorBounds(true, Origin, Extent, false);
	return (Extent * 2.0f).GetAbs();
}

FVector UHolographicMapTableComponent::WorldSizeToMarkerScale(const FVector& WorldSizeCm) const
{
	const float MapScale = VolumeRadiusCm / FMath::Max(ScanRangeCm, 1.0f);
	const FVector HoloSizeCm = WorldSizeCm.GetAbs() * MapScale;
	return HoloSizeCm / 100.0f;
}

void UHolographicMapTableComponent::TintMarker(UStaticMeshComponent* Mesh, const FLinearColor& Color) const
{
	GPApplyPolishSolidEmissive(Mesh, Color);
}

void UHolographicMapTableComponent::RebuildTrackedPois()
{
	TrackedPois.Reset();
	UWorld* World = GetWorld();
	if (!World || !OwningShip || OwningShip->IsWrecked())
	{
		return;
	}

	const FVector Origin = OwningShip->GetActorLocation();
	const FQuat InvRot = OwningShip->GetActorQuat().Inverse();
	const float Range = FMath::Max(ScanRangeCm, 1.0f);

	for (TActorIterator<AWalkableShip> It(World); It; ++It)
	{
		AWalkableShip* OtherShip = *It;
		if (!OtherShip || OtherShip == OwningShip || OtherShip->IsWrecked())
		{
			continue;
		}

		UHoloMapPoiComponent* Poi = OtherShip->HoloPoi;
		if (!Poi)
		{
			Poi = OtherShip->FindComponentByClass<UHoloMapPoiComponent>();
		}
		if (!Poi || !Poi->bVisibleOnMaps || Poi->GetOwner() != OtherShip)
		{
			continue;
		}

		AActor* Actor = OtherShip;

		const FVector WorldLoc = Actor->GetActorLocation();
		const float Dist = FVector::Dist(Origin, WorldLoc);
		if (Dist > Range)
		{
			continue;
		}

		FHoloMapTrackedPoi Entry;
		Entry.Actor = Actor;
		Entry.Kind = EHoloMapPoiKind::EnemyShip;
		Entry.Primitive = Poi->GetResolvedPrimitive();
		Entry.Color = FLinearColor(1.0f, 0.12f, 0.08f, 1.0f);
		Entry.WorldLocation = WorldLoc;
		Entry.DistanceCm = Dist;

		const FVector Local = InvRot.RotateVector(WorldLoc - Origin);
		Entry.TableRelative = WorldOffsetToVolume(Local);

		const FVector RelFwd = InvRot.RotateVector(Actor->GetActorForwardVector()).GetSafeNormal();
		Entry.RelativeRotation = MarkerHeading(Entry.Primitive, RelFwd);
		Entry.MarkerScale = IconScaleForPrimitive(Entry.Primitive);
		TrackedPois.Add(Entry);
	}

	for (TActorIterator<ABulldogFighter> It(World); It; ++It)
	{
		ABulldogFighter* Fighter = *It;
		if (!Fighter || Fighter == static_cast<AActor*>(OwningShip) || GPIsCraftWrecked(Fighter))
		{
			continue;
		}

		UHoloMapPoiComponent* Poi = Fighter->HoloPoi;
		if (!Poi)
		{
			Poi = Fighter->FindComponentByClass<UHoloMapPoiComponent>();
		}
		if (!Poi || !Poi->bVisibleOnMaps || Poi->GetOwner() != Fighter)
		{
			continue;
		}

		const FVector WorldLoc = Fighter->GetActorLocation();
		const float Dist = FVector::Dist(Origin, WorldLoc);
		if (Dist > Range)
		{
			continue;
		}

		FHoloMapTrackedPoi Entry;
		Entry.Actor = Fighter;
		Entry.Kind = EHoloMapPoiKind::EnemyShip;
		Entry.Primitive = Poi->GetResolvedPrimitive();
		Entry.Color = FLinearColor(1.0f, 0.12f, 0.08f, 1.0f);
		Entry.WorldLocation = WorldLoc;
		Entry.DistanceCm = Dist;

		const FVector Local = InvRot.RotateVector(WorldLoc - Origin);
		Entry.TableRelative = WorldOffsetToVolume(Local);

		const FVector RelFwd = InvRot.RotateVector(Fighter->GetActorForwardVector()).GetSafeNormal();
		Entry.RelativeRotation = MarkerHeading(Entry.Primitive, RelFwd);
		Entry.MarkerScale = IconScaleForPrimitive(Entry.Primitive);
		if (GPIsOwnDeployedFighter(OwningShip, Fighter))
		{
			Entry.Kind = EHoloMapPoiKind::FriendlyShip;
			Entry.Color = FLinearColor(0.2f, 1.0f, 0.35f, 1.0f);
		}
		TrackedPois.Add(Entry);
	}

	for (TActorIterator<AHeatseekingMissile> It(World); It; ++It)
	{
		AHeatseekingMissile* Missile = *It;
		if (!Missile || Missile->IsActorBeingDestroyed())
		{
			continue;
		}

		UHoloMapPoiComponent* Poi = Missile->HoloPoi;
		if (!Poi)
		{
			Poi = Missile->FindComponentByClass<UHoloMapPoiComponent>();
		}
		if (!Poi || !Poi->bVisibleOnMaps)
		{
			continue;
		}

		const FVector WorldLoc = Missile->GetActorLocation();
		const float Dist = FVector::Dist(Origin, WorldLoc);
		if (Dist > Range)
		{
			continue;
		}

		FHoloMapTrackedPoi Entry;
		Entry.Actor = Missile;
		Entry.Kind = EHoloMapPoiKind::Missile;
		Entry.Primitive = EHoloMapPrimitive::Sphere;
		Entry.Color = Poi->GetResolvedColor();
		Entry.WorldLocation = WorldLoc;
		Entry.DistanceCm = Dist;

		const FVector Local = InvRot.RotateVector(WorldLoc - Origin);
		Entry.TableRelative = WorldOffsetToVolume(Local);
		Entry.RelativeRotation = FRotator::ZeroRotator;
		Entry.MarkerScale = Poi->MarkerScale.IsNearlyZero() ? FVector(0.04f) : Poi->MarkerScale;
		TrackedPois.Add(Entry);
	}

	TrackedPois.Sort([](const FHoloMapTrackedPoi& A, const FHoloMapTrackedPoi& B)
	{
		return A.DistanceCm < B.DistanceCm;
	});

	CaptureMarkerInterpTargets();
	RefreshMarkers();
}

void UHolographicMapTableComponent::CaptureMarkerInterpTargets()
{
	TArray<FHoloMarkerInterp> Previous = MarkerInterps;
	TSet<int32> UsedSlots;
	MarkerInterps.Reset();
	MarkerInterps.Reserve(TrackedPois.Num());

	auto AllocateSlot = [this, &UsedSlots]() -> int32
	{
		for (int32 Slot = 0; Slot < MarkerPool.Num(); ++Slot)
		{
			if (!UsedSlots.Contains(Slot))
			{
				return Slot;
			}
		}
		const int32 Slot = MarkerPool.Num();
		GetOrCreatePooledMesh(MarkerPool, Slot);
		return Slot;
	};

	for (const FHoloMapTrackedPoi& Poi : TrackedPois)
	{
		FHoloMarkerInterp State;
		State.Actor = Poi.Actor;
		State.ToLoc = Poi.TableRelative;
		State.ToRot = Poi.RelativeRotation.Quaternion();

		const FHoloMarkerInterp* Prev = Previous.FindByPredicate([&Poi](const FHoloMarkerInterp& Other)
		{
			return Other.Actor.HasSameIndexAndSerialNumber(Poi.Actor);
		});

		if (Prev && Prev->PoolIndex != INDEX_NONE && !UsedSlots.Contains(Prev->PoolIndex))
		{
			State.PoolIndex = Prev->PoolIndex;
			State.FromLoc = Prev->DisplayedLoc;
			State.FromRot = Prev->DisplayedRot;
		}
		else
		{
			State.PoolIndex = AllocateSlot();
			State.FromLoc = State.ToLoc;
			State.FromRot = State.ToRot;
		}

		UsedSlots.Add(State.PoolIndex);
		State.DisplayedLoc = State.FromLoc;
		State.DisplayedRot = State.FromRot;
		MarkerInterps.Add(State);
	}
}

void UHolographicMapTableComponent::InterpolateMarkers()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const float Alpha = FMath::Clamp(RefreshTimer / FMath::Max(RefreshInterval, 0.0001f), 0.0f, 1.0f);
	for (FHoloMarkerInterp& State : MarkerInterps)
	{
		State.DisplayedLoc = FMath::Lerp(State.FromLoc, State.ToLoc, Alpha);
		State.DisplayedRot = FQuat::Slerp(State.FromRot, State.ToRot, Alpha);

		UStaticMeshComponent* Marker = MarkerPool.IsValidIndex(State.PoolIndex) ? MarkerPool[State.PoolIndex].Get() : nullptr;
		if (!Marker || !Marker->IsVisible())
		{
			continue;
		}

		Marker->SetRelativeLocation(State.DisplayedLoc);
		Marker->SetRelativeRotation(State.DisplayedRot.Rotator());
	}
}

void UHolographicMapTableComponent::RefreshMarkers()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const bool bShow = ShouldDrawVisuals();
	SetVisualsVisible(bShow);
	if (OwnShipMarker)
	{
		if (UStaticMesh* CubeMesh = LoadPrimitiveMesh(EHoloMapPrimitive::Cube))
		{
			OwnShipMarker->SetStaticMesh(CubeMesh);
		}
		OwnShipMarker->SetVisibility(bShow);
		OwnShipMarker->SetRelativeLocation(FVector(0.0f, 0.0f, VolumeCenterZ));
		OwnShipMarker->SetRelativeRotation(FRotator::ZeroRotator);
		OwnShipMarker->SetRelativeScale3D(IconScaleForPrimitive(EHoloMapPrimitive::Cube));
		TintMarker(OwnShipMarker, FLinearColor(0.15f, 0.55f, 1.0f, 1.0f));
	}
	HideCylinderMesh(HoloVolumeMesh);
	HideCylinderMesh(EquatorRing);
	HideCylinderMesh(MeridianRing);
	HideCylinderMesh(TransverseRing);
	HideStaleVolumeCones();
	for (UStaticMeshComponent* Mesh : AltitudeLinePool)
	{
		HideCylinderMesh(Mesh);
	}

	for (int32 Index = 0; Index < TrackedPois.Num(); ++Index)
	{
		const FHoloMapTrackedPoi& Poi = TrackedPois[Index];
		const int32 PoolIndex = MarkerInterps.IsValidIndex(Index) ? MarkerInterps[Index].PoolIndex : Index;
		UStaticMeshComponent* Marker = GetOrCreatePooledMesh(MarkerPool, PoolIndex);
		if (!Marker)
		{
			continue;
		}

		UStaticMesh* DesiredMesh = LoadPrimitiveMesh(Poi.Primitive);
		if (AActor* Actor = Poi.Actor.Get())
		{
			if (UHoloMapPoiComponent* Comp = Actor->FindComponentByClass<UHoloMapPoiComponent>())
			{
				DesiredMesh = Comp->ResolveMarkerMesh();
			}
		}
		if (DesiredMesh)
		{
			Marker->SetStaticMesh(DesiredMesh);
		}

		const FVector Scale = Poi.MarkerScale.IsNearlyZero() ? IconScaleForPrimitive(Poi.Primitive) : Poi.MarkerScale;

		Marker->SetVisibility(bShow);
		Marker->SetHiddenInGame(!bShow);
		Marker->SetRelativeScale3D(Scale);
		TintMarker(Marker, Poi.Color);
	}

	TSet<int32> UsedSlots;
	for (const FHoloMarkerInterp& State : MarkerInterps)
	{
		UsedSlots.Add(State.PoolIndex);
	}
	for (int32 Slot = 0; Slot < MarkerPool.Num(); ++Slot)
	{
		if (!UsedSlots.Contains(Slot) && MarkerPool[Slot])
		{
			MarkerPool[Slot]->SetVisibility(false);
		}
	}
	for (int32 Index = TrackedPois.Num(); Index < AltitudeLinePool.Num(); ++Index)
	{
		if (AltitudeLinePool[Index])
		{
			AltitudeLinePool[Index]->SetVisibility(false);
		}
	}
}

AActor* UHolographicMapTableComponent::GetBestAutoTarget() const
{
	AActor* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (const FHoloMapTrackedPoi& Poi : TrackedPois)
	{
		AActor* Actor = Poi.Actor.Get();
		if (!Actor)
		{
			continue;
		}

		bool bHostile = Poi.Kind == EHoloMapPoiKind::EnemyShip;
		if (UHoloMapPoiComponent* Comp = Actor->FindComponentByClass<UHoloMapPoiComponent>())
		{
			bHostile = Comp->IsHostileTo(OwningShip);
		}
		if (!bHostile)
		{
			continue;
		}

		if (Poi.DistanceCm < BestDist)
		{
			BestDist = Poi.DistanceCm;
			Best = Actor;
		}
	}
	return Best;
}

void UHolographicMapTableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ScanRangeCm = GPHoloMapScanRangeCm();

	RefreshTimer += DeltaTime;
	if (RefreshTimer >= RefreshInterval)
	{
		RefreshTimer = 0.0f;
		if (!GIsReinstancing.load())
		{
			PlaceInCabin();
		}
		RebuildTrackedPois();
		RefreshMarkers();
	}

	InterpolateMarkers();

	if (HoloLight && GetNetMode() != NM_DedicatedServer)
	{
		const float Pulse = 0.85f + 0.15f * FMath::Sin(GetWorld()->GetTimeSeconds() * 3.2f);
		HoloLight->SetIntensity(1200.0f * Pulse);
	}
}
