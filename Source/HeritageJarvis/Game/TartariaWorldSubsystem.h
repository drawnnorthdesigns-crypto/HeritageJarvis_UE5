#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "TartariaTypes.h"
#include "TartariaWorldSubsystem.generated.h"

class ATartariaQuestMarker;
class ATartariaEnemyActor;

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
	// Economy
	// -------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Economy")
	int32 Credits = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Economy")
	FString Phase;  // "idle", "running", etc.

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Economy")
	TArray<FTartariaFactionInfo> Factions;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Economy")
	TArray<FTartariaInventoryItem> Inventory;

	// -------------------------------------------------------
	// Strategic (fleet, tech, mining)
	// -------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Strategic")
	FTartariaFleetSummary Fleet;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Strategic")
	FTartariaTechSummary Tech;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Strategic")
	FTartariaMiningSummary Mining;

	// -------------------------------------------------------
	// Delegates
	// -------------------------------------------------------

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameStateUpdated);

	UPROPERTY(BlueprintAssignable, Category = "Tartaria|Economy")
	FOnGameStateUpdated OnGameStateUpdated;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTickCompleted, const TArray<FTartariaTickEvent>&, Events);

	UPROPERTY(BlueprintAssignable, Category = "Tartaria|Economy")
	FOnTickCompleted OnTickCompleted;

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

	/** When true, SSE stream is providing state — skip polling. */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|World")
	bool bUseSSE = false;

	// -------------------------------------------------------
	// Enemy Spawning & Combat
	// -------------------------------------------------------

	/** Spawn an enemy actor near the player from threat data. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Enemy")
	void SpawnEnemyFromThreat(const FString& InEnemyName, int32 Power, int32 InDifficulty, int32 Reward, const FString& InScenarioKey);

	/** Resolve combat with the nearest active enemy. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Enemy")
	void ResolveCombatWithNearestEnemy(bool bPlayerWins);

	// -------------------------------------------------------
	// World Consequences — visual zone modifiers
	// -------------------------------------------------------

	/** Handle incoming WebSocket messages from Flask hub. */
	UFUNCTION()
	void OnWebSocketMessage(const FString& Channel, const FString& Payload);

	/** Poll world consequences for visual modifiers. */
	void PollWorldConsequences();

	/** Apply visual modifiers to biome volumes. */
	void ApplyZoneVisualModifiers(const TSharedPtr<FJsonObject>& ModifiersJson);

private:
	void OnWorldStateResponse(bool bSuccess, const FString& Body);
	void ParseWorldState(const FString& JsonBody);

	void PollInventory();
	void OnInventoryResponse(bool bSuccess, const FString& Body);
	void ParseInventory(const FString& JsonBody);

	void ExecuteGameTick();
	void OnTickResponse(bool bSuccess, const FString& Body);
	void ParseTickResponse(const FString& JsonBody);

	void PollStrategicStatus();
	void OnStrategicResponse(bool bSuccess, const FString& Body);
	void ParseStrategicStatus(const FString& JsonBody);

	/** Time accumulator for periodic backend sync. */
	float SyncTimer = 0.f;

	/** Sync interval in seconds. */
	static constexpr float SyncIntervalSec = 30.f;

	/** Time accumulator for inventory polling. */
	float InventoryTimer = 0.f;
	static constexpr float InventoryPollSec = 10.f;

	/** Time accumulator for game ticks. */
	float TickTimer = 0.f;
	static constexpr float TickIntervalSec = 30.f;  // 1 game tick per 30s real time

	/** Time accumulator for strategic status poll. */
	float StrategicTimer = 0.f;
	static constexpr float StrategicPollSec = 15.f;

	/** Spawned quest markers tracked for cleanup. */
	UPROPERTY()
	TArray<ATartariaQuestMarker*> SpawnedMarkers;

	/** Process threat data from backend JSON to spawn enemies. */
	void ProcessThreatData(const TSharedPtr<FJsonObject>& ThreatJson);

	/** Currently alive enemy actors (weak refs auto-clear on destroy). */
	TArray<TWeakObjectPtr<ATartariaEnemyActor>> ActiveEnemies;

	/** Time accumulator for world consequence polling. */
	float ConsequencePollTimer = 0.f;

	/** Consequence poll interval (15 seconds). */
	static constexpr float ConsequencePollSec = 15.f;
};
