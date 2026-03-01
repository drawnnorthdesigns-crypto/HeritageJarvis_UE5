#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "HJApiClient.h"
#include "HJEventPoller.h"
#include "HJFlaskProcess.h"
#include "HJGameEventStream.h"
#include "HJGameInstance.generated.h"

class UHJSaveGame;
class UHJWebSocketClient;

// Broadcast when Flask health check first succeeds after launch
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFlaskReady);

// A request held in the offline queue until Flask comes back
USTRUCT()
struct FHJQueuedRequest
{
	GENERATED_BODY()

	FString         Endpoint;
	FString         Verb;      // POST | PUT | DELETE
	FString         Body;
	FOnApiResponse  Callback;
};

// Fix 3.5: Delegate broadcast when offline queue size changes
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQueueCountChanged, int32, Count);

/**
 * UHJGameInstance
 * Persists across all level changes (main menu <-> Tartaria).
 * Owns: ApiClient, EventPoller, SaveGame, offline request queue.
 *
 * Resilience features:
 *   - Offline queue capped at QueueCapacity (FIFO eviction)
 *   - Gradual drain on reconnect (DrainBatchSize per tick, not burst)
 *   - Health checking consolidated to EventPoller (no legacy timer)
 */
UCLASS()
class HERITAGEJARVIS_API UHJGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	// -------------------------------------------------------
	// Configuration
	// -------------------------------------------------------

	/** Max queued offline requests before FIFO eviction. */
	static constexpr int32 QueueCapacity = 100;

	/** Number of requests drained per batch on reconnect. */
	static constexpr int32 DrainBatchSize = 10;

	/** Delay between drain batches in seconds. */
	static constexpr float DrainIntervalSec = 0.5f;

	// -------------------------------------------------------
	// Core systems
	// -------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "HJ")
	UHJApiClient* ApiClient;

	UPROPERTY(BlueprintReadOnly, Category = "HJ")
	UHJEventPoller* EventPoller;

	UPROPERTY(BlueprintReadOnly, Category = "HJ")
	UHJFlaskProcess* FlaskProcess;

	/** SSE game event stream — replaces polling for game state. */
	UPROPERTY(BlueprintReadOnly, Category = "HJ")
	UHJGameEventStream* GameEventStream;

	/** WebSocket client for real-time Flask events. */
	UPROPERTY(BlueprintReadOnly, Category = "HJ")
	UHJWebSocketClient* WebSocketClient;

	/** Broadcast when Flask first comes online after auto-launch. */
	UPROPERTY(BlueprintAssignable, Category = "HJ|Events")
	FOnFlaskReady OnFlaskReady;

	// -------------------------------------------------------
	// Active project (set when user selects a card)
	// -------------------------------------------------------

	UPROPERTY(BlueprintReadWrite, Category = "HJ")
	FString ActiveProjectId;

	UPROPERTY(BlueprintReadWrite, Category = "HJ")
	FString ActiveProjectName;

	// -------------------------------------------------------
	// Flask health
	// -------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "HJ")
	bool bFlaskOnline = false;

	UFUNCTION(BlueprintCallable, Category = "HJ")
	void RefreshHealth();

	// -------------------------------------------------------
	// Pipeline watching
	// -------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "HJ")
	void StartWatchingPipeline(const FString& ProjectId);

	// -------------------------------------------------------
	// Save / Load
	// -------------------------------------------------------

	/** Optional seal sound effect — assign in Blueprint if desired. */
	UPROPERTY(EditDefaultsOnly, Category = "HJ|Save")
	USoundBase* SealSound = nullptr;

	UFUNCTION(BlueprintCallable, Category = "HJ")
	void SaveSession();

	UFUNCTION(BlueprintCallable, Category = "HJ")
	void LoadSession();

	/** Save full game state (credits, inventory, factions, fleet, tech, mining, combat). */
	UFUNCTION(BlueprintCallable, Category = "HJ")
	void SaveGameState();

	/** Restore game state from save into WorldSubsystem + Character. */
	UFUNCTION(BlueprintCallable, Category = "HJ")
	void LoadGameState();

	/** Update the last active tab name so it survives restarts */
	UFUNCTION(BlueprintCallable, Category = "HJ")
	void SetLastTab(const FString& TabName);

	// -------------------------------------------------------
	// Offline request queue (drains when Flask comes back)
	// -------------------------------------------------------

	/**
	 * Queue a write request when Flask is offline.
	 * The request is replayed automatically when health check succeeds.
	 * Queue is capped at QueueCapacity; oldest entries are evicted first.
	 */
	void EnqueueRequest(const FString& Endpoint, const FString& Verb,
	                    const FString& Body, FOnApiResponse Callback);

	UFUNCTION(BlueprintPure, Category = "HJ")
	int32 GetQueuedRequestCount() const { return RequestQueue.Num(); }

	/** Fix 3.5: Broadcast when queue count changes so HUD can update badge. */
	UPROPERTY(BlueprintAssignable, Category = "HJ|Events")
	FOnQueueCountChanged OnQueueCountChanged;

	// -------------------------------------------------------
	// Convenience getter
	// -------------------------------------------------------

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HJ",
	          meta = (WorldContext = "WorldContextObject"))
	static UHJGameInstance* Get(const UObject* WorldContextObject);

private:
	void OnHealthResponse(bool bSuccess, const FString& Body);
	void DrainRequestQueue();

	// Adapter so AddDynamic can bind EventPoller.OnHealthStatus
	UFUNCTION()
	void OnHealthResponse_FromPoller(bool bOnline, const FString& Message);

	/** Fix 2.4: Fires when ApiClient exhausts retries — triggers immediate health check */
	UFUNCTION()
	void OnConnectionExhausted_Trigger(const FString& Endpoint);

	/** Timer for gradual queue drain (fires every DrainIntervalSec during drain). */
	FTimerHandle DrainTimerHandle;

	TArray<FHJQueuedRequest> RequestQueue;

	/** Track whether OnFlaskReady has been broadcast (fires only once). */
	bool bFlaskReadyFired = false;

	UPROPERTY()
	UHJSaveGame* SaveData = nullptr;

public:
	/** Timestamp of last successful save (for HUD "Last sealed: Xs ago" display). */
	UPROPERTY(BlueprintReadOnly, Category = "HJ|Save")
	FDateTime LastSaveTime;
};
