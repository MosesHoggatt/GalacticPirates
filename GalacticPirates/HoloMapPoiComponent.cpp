#include "HoloMapPoiComponent.h"
#include "SpaceCraft.h"
#include "Engine/StaticMesh.h"

UStaticMesh* GPLoadHoloPrimitiveMesh(EHoloMapPrimitive Primitive)
{
	const TCHAR* Path = TEXT("/Engine/BasicShapes/Cube.Cube");
	switch (Primitive)
	{
	case EHoloMapPrimitive::Triangle:
	case EHoloMapPrimitive::Cone:
		Path = TEXT("/Engine/BasicShapes/Cone.Cone");
		break;
	case EHoloMapPrimitive::Sphere:
		Path = TEXT("/Engine/BasicShapes/Sphere.Sphere");
		break;
	case EHoloMapPrimitive::Cylinder:
		Path = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
		break;
	case EHoloMapPrimitive::Plane:
		Path = TEXT("/Engine/BasicShapes/Plane.Plane");
		break;
	case EHoloMapPrimitive::Cube:
	default:
		Path = TEXT("/Engine/BasicShapes/Cube.Cube");
		break;
	}
	return LoadObject<UStaticMesh>(nullptr, Path);
}

UHoloMapPoiComponent::UHoloMapPoiComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetMobility(EComponentMobility::Movable);
}

FLinearColor UHoloMapPoiComponent::GetResolvedColor() const
{
	if (bOverrideColor)
	{
		return MarkerColor;
	}

	switch (Kind)
	{
	case EHoloMapPoiKind::OwnShip:
		return FLinearColor(0.25f, 0.95f, 1.0f, 1.0f);
	case EHoloMapPoiKind::FriendlyShip:
		return FLinearColor(0.2f, 1.0f, 0.35f, 1.0f);
	case EHoloMapPoiKind::NeutralShip:
		return FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);
	case EHoloMapPoiKind::Station:
		return FLinearColor(0.45f, 0.75f, 1.0f, 1.0f);
	case EHoloMapPoiKind::Asteroid:
		return FLinearColor(0.65f, 0.55f, 0.4f, 1.0f);
	case EHoloMapPoiKind::Missile:
		return FLinearColor(1.0f, 0.92f, 0.15f, 1.0f);
	case EHoloMapPoiKind::EnemyShip:
	default:
		return FLinearColor(1.0f, 0.12f, 0.08f, 1.0f);
	}
}

EHoloMapPrimitive UHoloMapPoiComponent::GetResolvedPrimitive() const
{
	if (bOverridePrimitive)
	{
		return Primitive;
	}

	switch (Kind)
	{
	case EHoloMapPoiKind::OwnShip:
		return EHoloMapPrimitive::Cube;
	case EHoloMapPoiKind::FriendlyShip:
		return EHoloMapPrimitive::Triangle;
	case EHoloMapPoiKind::NeutralShip:
		return EHoloMapPrimitive::Sphere;
	case EHoloMapPoiKind::Station:
		return EHoloMapPrimitive::Cube;
	case EHoloMapPoiKind::Asteroid:
		return EHoloMapPrimitive::Sphere;
	case EHoloMapPoiKind::Missile:
		return EHoloMapPrimitive::Sphere;
	case EHoloMapPoiKind::EnemyShip:
	default:
		return EHoloMapPrimitive::Cube;
	}
}

UStaticMesh* UHoloMapPoiComponent::ResolveMarkerMesh() const
{
	return GPLoadHoloPrimitiveMesh(GetResolvedPrimitive());
}

bool UHoloMapPoiComponent::IsHostileTo(const AActor* Viewer) const
{
	AActor* Owner = GetOwner();
	if (!Viewer || Owner == Viewer)
	{
		return false;
	}

	if (GPAsSpaceCraft(Owner) && GPAsSpaceCraft(const_cast<AActor*>(Viewer)))
	{
		return GPAreHostile(const_cast<AActor*>(Viewer), Owner);
	}

	return Kind == EHoloMapPoiKind::EnemyShip || Kind == EHoloMapPoiKind::Unknown;
}
