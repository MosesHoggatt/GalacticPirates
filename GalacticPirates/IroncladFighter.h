#pragma once

#include "CoreMinimal.h"
#include "BulldogFighter.h"
#include "IroncladFighter.generated.h"

class UProceduralMeshComponent;

/** ARC-72 Ironclad: Hunyuan3D hull from the industrial fighter concept. */
UCLASS()
class GALACTICPIRATES_API AIroncladFighter : public ABulldogFighter
{
	GENERATED_BODY()

public:
	AIroncladFighter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> GeneratedHull;

	static AIroncladFighter* SpawnDockedOnShip(UWorld* World, AWalkableShip* TargetShip);

protected:
	virtual void BeginPlay() override;

private:
	void ApplyIroncladHull();
	void ApplyIroncladAlbedo();
	bool TryLoadImportedStaticMesh();
	bool LoadGeneratedObjMesh();
};
