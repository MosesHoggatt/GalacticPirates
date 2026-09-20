#pragma once

#include "CoreMinimal.h"

class UObject;
class USoundBase;
class UTexture2D;
class UWorld;
class AActor;
class USceneComponent;
class UPrimitiveComponent;
class UMaterialInstanceDynamic;

USoundBase* GPLoadPolishSound(const TCHAR* AssetName);
UTexture2D* GPLoadPolishTexture(const TCHAR* AssetName);
UMaterialInstanceDynamic* GPApplyPolishVfxMaterial(UPrimitiveComponent* Mesh, const TCHAR* TextureName, const FLinearColor& Color);
UMaterialInstanceDynamic* GPApplyPolishSolidEmissive(UPrimitiveComponent* Mesh, const FLinearColor& Color);
void GPPlayPolishSound2D(const UObject* WorldContext, const TCHAR* AssetName, float Volume = 1.0f);
void GPPlayPolishSoundAt(const UObject* WorldContext, const TCHAR* AssetName, const FVector& Location, float Volume = 1.0f);
void GPAttachStationLabel(USceneComponent* Parent, const FText& Label, const FColor& Color);
void GPPlayCannonCameraShake(UWorld* World, const FVector& Epicenter, float InnerRadius, float OuterRadius, float Scale);
void GPPlayExplosionCameraShake(UWorld* World, const FVector& Epicenter, float InnerRadius, float OuterRadius, float Scale);
