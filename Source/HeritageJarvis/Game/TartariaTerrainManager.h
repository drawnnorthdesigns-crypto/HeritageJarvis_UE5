#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaTerrainManager.generated.h"

class UStaticMeshComponent;
class UProceduralMeshComponent;

/**
 * ATartariaTerrainManager — Spawns procedural ground terrain for each biome zone.
 *
 * UE5 Landscape actors require editor-created heightmap assets that cannot be
 * generated from C++ alone. This manager creates large scaled plane meshes
 * per biome with distinct ground materials (color, roughness) to provide
 * a visible terrain foundation.
 *
 * Each biome gets:
 * - A large ground plane (Engine/BasicShapes/Plane scaled to biome radius)
 * - Biome-specific ground color and material properties
 * - Edge blend zones via transparent perimeter planes
 *
 * Future upgrade: replace with ALandscapeProxy when editor-authored heightmaps
 * become available, or switch to RuntimeLandscape plugin.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaTerrainManager : public AActor
{
	GENERATED_BODY()

public:
	ATartariaTerrainManager();

	/**
	 * Spawn ground planes for all biome zones.
	 * Called from WorldPopulator::PopulateWorld().
	 */
	void SpawnTerrainPlanes(UWorld* World);

private:
	/** Spawn a single ground plane at a biome location. */
	void SpawnGroundPlane(UWorld* World, const FName& Tag, const FVector& Center,
		float Radius, const FLinearColor& GroundColor, float Roughness);
};
