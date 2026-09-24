#include "IroncladFighter.h"
#include "WalkableShip.h"
#include "GalacticPirates.h"
#include "OccupancyComponent.h"
#include "WeaponHardpointComponent.h"
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Texture2D.h"

namespace
{
	bool ParseObjFile(const FString& Path, TArray<FVector>& OutVerts, TArray<int32>& OutTris)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			return false;
		}

		TArray<FVector> Positions;
		OutVerts.Reset();
		OutTris.Reset();

		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);
		Positions.Reserve(13000);
		for (const FString& RawLine : Lines)
		{
			const FString Line = RawLine.TrimStartAndEnd();
			if (Line.StartsWith(TEXT("v ")))
			{
				TArray<FString> Parts;
				Line.ParseIntoArrayWS(Parts);
				if (Parts.Num() >= 4)
				{
					// Blender OBJ: Y-forward Z-up. UE is X-forward Z-up — keep XYZ as authored after our bake.
					Positions.Add(FVector(FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]), FCString::Atof(*Parts[3])));
				}
			}
			else if (Line.StartsWith(TEXT("f ")))
			{
				TArray<FString> Parts;
				Line.ParseIntoArrayWS(Parts);
				TArray<int32, TInlineAllocator<8>> FaceIdx;
				for (int32 i = 1; i < Parts.Num(); ++i)
				{
					FString VertToken = Parts[i];
					int32 Slash;
					if (VertToken.FindChar(TEXT('/'), Slash))
					{
						VertToken = VertToken.Left(Slash);
					}
					const int32 Index1 = FCString::Atoi(*VertToken);
					if (Index1 == 0)
					{
						continue;
					}
					const int32 Index0 = Index1 > 0 ? Index1 - 1 : Positions.Num() + Index1;
					FaceIdx.Add(Index0);
				}
				for (int32 i = 1; i + 1 < FaceIdx.Num(); ++i)
				{
					OutTris.Add(FaceIdx[0]);
					OutTris.Add(FaceIdx[i + 1]);
					OutTris.Add(FaceIdx[i]);
				}
			}
		}

		if (Positions.Num() == 0 || OutTris.Num() < 3)
		{
			return false;
		}
		OutVerts = MoveTemp(Positions);
		return true;
	}
}

AIroncladFighter::AIroncladFighter()
{
	if (Collision)
	{
		Collision->SetBoxExtent(FVector(280.0f, 220.0f, 60.0f));
	}
	if (CockpitOccupancy)
	{
		CockpitOccupancy->SetRelativeLocation(FVector(-40.0f, 0.0f, 50.0f));
	}
	if (GunHardpoint)
	{
		GunHardpoint->SetRelativeLocation(FVector(260.0f, 0.0f, 20.0f));
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(TEXT("/Game/Ships/Fighters/Meshes/ARC72_Ironclad"));
	if (MeshFinder.Succeeded() && HullMesh)
	{
		HullMesh->SetStaticMesh(MeshFinder.Object);
	}

	GeneratedHull = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedHull"));
	GeneratedHull->SetupAttachment(HullMesh);
	GeneratedHull->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GeneratedHull->SetMobility(EComponentMobility::Movable);
	GeneratedHull->SetCastShadow(true);
}

void AIroncladFighter::BeginPlay()
{
	Super::BeginPlay();
	ApplyIroncladHull();
}

void AIroncladFighter::ApplyIroncladHull()
{
	if (TryLoadImportedStaticMesh())
	{
		if (GeneratedHull)
		{
			GeneratedHull->SetVisibility(false);
		}
		ApplyIroncladAlbedo();
		return;
	}

	if (!LoadGeneratedObjMesh())
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[Ironclad] no imported mesh or OBJ at Content/Blueprints/Spaceships/Models/ARC72_Ironclad"));
	}
	ApplyIroncladAlbedo();
}

void AIroncladFighter::ApplyIroncladAlbedo()
{
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ships/Fighters/Materials/A-53Bulldog_Mat.A-53Bulldog_Mat"));
	UTexture* Albedo = LoadObject<UTexture>(nullptr, TEXT("/Game/Ships/Fighters/Textures/ARC72_Ironclad_T.ARC72_Ironclad_T"));
	if (!Parent || !Albedo)
	{
		UE_LOG(LogGalacticPirates, Warning, TEXT("[Ironclad] albedo apply skipped parent=%s tex=%s"),
			*GetNameSafe(Parent), *GetNameSafe(Albedo));
		return;
	}

	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Parent, this);
	Mid->SetTextureParameterValue(TEXT("DiffuseColorMap"), Albedo);
	Mid->SetScalarParameterValue(TEXT("DiffuseColorMapWeight"), 1.0f);
	if (HullMesh)
	{
		HullMesh->SetMaterial(0, Mid);
	}
	if (GeneratedHull)
	{
		GeneratedHull->SetMaterial(0, Mid);
	}
}

bool AIroncladFighter::TryLoadImportedStaticMesh()
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Ships/Fighters/Meshes/ARC72_Ironclad.ARC72_Ironclad"));
	if (!Mesh || !HullMesh)
	{
		return false;
	}
	HullMesh->SetStaticMesh(Mesh);
	return true;
}

bool AIroncladFighter::LoadGeneratedObjMesh()
{
	if (!GeneratedHull)
	{
		return false;
	}

	const FString ObjPath = FPaths::ProjectContentDir() / TEXT("Ships/Fighters/Meshes/ARC72_Ironclad.obj");
	TArray<FVector> Verts;
	TArray<int32> Tris;
	if (!ParseObjFile(ObjPath, Verts, Tris))
	{
		return false;
	}

	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	Normals.SetNum(Verts.Num());
	UV0.SetNum(Verts.Num());
	for (int32 i = 0; i < Verts.Num(); ++i)
	{
		Normals[i] = FVector::UpVector;
		UV0[i] = FVector2D(0.5f, 0.5f);
	}

	for (int32 i = 0; i + 2 < Tris.Num(); i += 3)
	{
		const int32 A = Tris[i];
		const int32 B = Tris[i + 1];
		const int32 C = Tris[i + 2];
		if (!Verts.IsValidIndex(A) || !Verts.IsValidIndex(B) || !Verts.IsValidIndex(C))
		{
			continue;
		}
		const FVector N = FVector::CrossProduct(Verts[B] - Verts[A], Verts[C] - Verts[A]).GetSafeNormal();
		Normals[A] += N;
		Normals[B] += N;
		Normals[C] += N;
	}
	for (FVector& N : Normals)
	{
		N = N.GetSafeNormal();
		if (N.IsNearlyZero())
		{
			N = FVector::UpVector;
		}
	}

	GeneratedHull->CreateMeshSection(0, Verts, Tris, Normals, UV0, Colors, Tangents, false);
	GeneratedHull->SetVisibility(true);
	if (HullMesh)
	{
		HullMesh->SetStaticMesh(nullptr);
	}
	UE_LOG(LogGalacticPirates, Warning, TEXT("[Ironclad] loaded generated hull verts=%d tris=%d from %s"), Verts.Num(), Tris.Num() / 3, *ObjPath);
	return true;
}

AIroncladFighter* AIroncladFighter::SpawnDockedOnShip(UWorld* World, AWalkableShip* TargetShip)
{
	if (!World || !TargetShip)
	{
		return nullptr;
	}

	for (TActorIterator<ABulldogFighter> It(World); It; ++It)
	{
		ABulldogFighter* Existing = *It;
		if (Existing && Existing->GetHomeCraft() == TargetShip)
		{
			if (AIroncladFighter* Ironclad = Cast<AIroncladFighter>(Existing))
			{
				if (!Ironclad->IsHullDocked() && !Ironclad->HasHumanOccupant())
				{
					Ironclad->DockToHull(TargetShip);
				}
				return Ironclad;
			}
		}
	}

	USceneComponent* Dock = TargetShip->HangarDock ? static_cast<USceneComponent*>(TargetShip->HangarDock) : TargetShip->GetRootComponent();
	const FTransform DockTM = Dock ? Dock->GetComponentTransform() : TargetShip->GetActorTransform();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AIroncladFighter* Fighter = World->SpawnActor<AIroncladFighter>(DockTM.GetLocation(), DockTM.Rotator(), Params);
	if (Fighter)
	{
		Fighter->DockToHull(TargetShip);
	}
	UE_LOG(LogGalacticPirates, Warning, TEXT("[Hangar] spawned docked Ironclad %s on %s"),
		*GetNameSafe(Fighter),
		*GetNameSafe(TargetShip));
	return Fighter;
}
