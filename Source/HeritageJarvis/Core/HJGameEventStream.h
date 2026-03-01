#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Game/TartariaTypes.h"
#include "HJGameEventStream.generated.h"

class UHJApiClient;

/**
 * Delegate fired when a state snapshot arrives from the SSE game stream.
 * Contains credits, temporal data, factions, fleet, tech, mining, inventory.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameSnapshotReceived);

/** Delegate fired when tick events arrive from SSE. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameTickEventsReceived,
	const TArray<FTartariaTickEvent>&, Events);

/**
 * UHJGameEventStream — Consumes the SSE game event stream from Python.
 *
 * Connects to /api/game/events/stream and receives:
 *   - state_snapshot (every 5s): full economy/faction/fleet/tech/mining state
 *   - tick_events: game loop tick results with raid/book/annexation events
 *   - credits_changed: credit delta notifications
 *
 * Replaces 4 separate polling timers in TartariaWorldSubsystem with a
 * single persistent HTTP connection.
 */
UCLASS()
class HERITAGEJARVIS_API UHJGameEventStream : public UObject
{
	GENERATED_BODY()

public:
	/** Start listening to the SSE game event stream. */
	void Connect(UHJApiClient* InApiClient);

	/** Stop listening and clean up. */
	void Disconnect();

	/** Whether the stream is currently connected. */
	UPROPERTY(BlueprintReadOnly, Category = "HJ|Events")
	bool bConnected = false;

	// -------------------------------------------------------
	// Latest state (updated by SSE snapshots)
	// -------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Events")
	int32 Credits = 0;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Events")
	FString Phase;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Events")
	FString Era;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Events")
	int32 CurrentDay = 1;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Events")
	float TimeOfDay = 8.0f;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Events")
	TArray<FTartariaFactionInfo> Factions;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Events")
	TArray<FTartariaInventoryItem> Inventory;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Events")
	FTartariaFleetSummary Fleet;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Events")
	FTartariaTechSummary Tech;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Events")
	FTartariaMiningSummary Mining;

	// -------------------------------------------------------
	// Delegates
	// -------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "HJ|Events")
	FOnGameSnapshotReceived OnSnapshotReceived;

	UPROPERTY(BlueprintAssignable, Category = "HJ|Events")
	FOnGameTickEventsReceived OnTickEventsReceived;

private:
	void StartSSERequest();
	void OnSSEData(const FString& Line);
	void ParseStateSnapshot(const TSharedPtr<class FJsonObject>& Json);
	void ParseTickEvents(const TSharedPtr<class FJsonObject>& Json);

	/** Reconnect after connection drop. */
	void ScheduleReconnect();

	UPROPERTY()
	UHJApiClient* ApiClient = nullptr;

	/** Accumulated SSE data buffer for line parsing. */
	FString SSEBuffer;

	/** Current SSE event type (set by 'event:' line). */
	FString CurrentEventType;

	/** Timer for reconnect on failure. */
	FTimerHandle ReconnectTimerHandle;

	/** Whether we intentionally disconnected (don't reconnect). */
	bool bIntentionalDisconnect = false;
};
