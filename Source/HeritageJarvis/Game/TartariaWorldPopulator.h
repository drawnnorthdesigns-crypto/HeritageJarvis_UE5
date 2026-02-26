#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TartariaWorldPopulator.generated.h"

/**
 * UTartariaWorldPopulator — Spawns Tartaria world content programmatically.
 *
 * Called once from TartariaGameMode::BeginPlay(). Each spawn function checks
 * for existing actors via tag before spawning (no duplicates with editor-placed content).
 */
UCLASS()
class HERITAGEJARVIS_API UTartariaWorldPopulator : public UObject
{
	GENERATED_BODY()

public:
	/** Main entry point — spawns all world content. */
	void PopulateWorld(UWorld* World);

private:
	void SpawnBiomes(UWorld* World);
	void SpawnDayNightCycle(UWorld* World);
	void SpawnResourceNodes(UWorld* World);
	void SpawnBuildings(UWorld* World);
	void SpawnNPCs(UWorld* World);

	/** Returns true if an actor with the given tag already exists. */
	bool HasActorWithTag(UWorld* World, FName Tag) const;

	/** Returns a random offset within Radius * 0.7 of Center. */
	FVector RandomOffsetInRadius(const FVector& Center, float Radius) const;
};
