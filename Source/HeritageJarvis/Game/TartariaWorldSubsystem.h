#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "TartariaTypes.h"
#include "TartariaWorldSubsystem.generated.h"

class ATartariaQuestMarker;

/**
 * UTartariaWorldSubsystem — Manages open world state.
 * Syncs with Flask backend every 30s, tracks day/night, biome zones, POIs.
 * Lives as long as the UWorld exists (auto-created for Tartaria levels).
 */
UCLASS()
class HERITAGEJARVIS_API UTartariaWorldSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UTartariaWorldSubsystem, STATGROUP_Tickables); }

	// -------------------------------------------------------
	// World state
	// -------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|World")
	TArray<FTartariaBiomeZone> BiomeZones;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|World")
	TArray<FTartariaPOI> ActivePOIs;

	// -------------------------------------------------------
	// Day/Night
	// -------------------------------------------------------

	/** Current time of day (0-24 range). */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|World")
	float TimeOfDay = 8.0f;

	/** Current in-game day counter. */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|World")
	int32 CurrentDay = 1;

	/** Current era name from backend. */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|World")
	FString CurrentEra;

	// -------------------------------------------------------
	// Sync
	// -------------------------------------------------------

	/** Force an immediate sync with the backend. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|World")
	void SyncWithBackend();

private:
	void OnWorldStateResponse(bool bSuccess, const FString& Body);
	void ParseWorldState(const FString& JsonBody);

	/** Time accumulator for periodic backend sync. */
	float SyncTimer = 0.f;

	/** Sync interval in seconds. */
	static constexpr float SyncIntervalSec = 30.f;

	/** Spawned quest markers tracked for cleanup. */
	UPROPERTY()
	TArray<ATartariaQuestMarker*> SpawnedMarkers;
};
