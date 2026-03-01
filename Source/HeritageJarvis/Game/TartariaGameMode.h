#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TartariaTypes.h"
#include "TartariaGameMode.generated.h"

class UHJHUDWidget;
class UHJPauseWidget;
class UHJNotificationWidget;
class UHJDebugWidget;
class UHJLoadingWidget;
class UHJDashboardWidget;
class UHJThreatWidget;
class UHJInventoryWidget;
class UHJDialogueWidget;
class UTartariaWorldPopulator;
class ATartariaBiomeVolume;
class ATartariaNPC;
class ATartariaSolarSystemManager;
class ATartariaQuestMarker;
class ATartariaLevelStreamManager;

/** Delegate broadcast when a pipeline project completes materialization. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProjectCompleted, const FString&, ProjectId);

/**
 * ATartariaGameMode — Game mode for the Tartaria open world.
 *
 * On BeginPlay:
 *   1. Spawns HJHUDWidget (always-on overlay)
 *   2. Spawns HJNotificationWidget (toast stack)
 *   3. Spawns HJDebugWidget (hidden by default, toggle with F3)
 *   4. Hides loading screen that was shown during level transition
 *   5. Subscribes to HJGameInstance.EventPoller for live pipeline updates
 *
 * TogglePauseMenu() spawns / destroys HJPauseWidget on demand.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATartariaGameMode();
	virtual void BeginPlay() override;

	// -------------------------------------------------------
	// Widget classes — assign BPs in editor (BP_TartariaGameMode)
	// -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HJ|UI")
	TSubclassOf<UHJHUDWidget> HUDWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HJ|UI")
	TSubclassOf<UHJPauseWidget> PauseWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HJ|UI")
	TSubclassOf<UHJNotificationWidget> NotificationWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HJ|UI")
	TSubclassOf<UHJDebugWidget> DebugWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HJ|UI")
	TSubclassOf<UHJThreatWidget> ThreatWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HJ|UI")
	TSubclassOf<UHJInventoryWidget> InventoryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HJ|UI")
	TSubclassOf<UHJDialogueWidget> DialogueWidgetClass;

	// -------------------------------------------------------
	// Live widget references
	// -------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "HJ|UI")
	UHJHUDWidget* HUDWidget;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|UI")
	UHJPauseWidget* PauseWidget;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|UI")
	UHJNotificationWidget* NotifWidget;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|UI")
	UHJDebugWidget* DebugWidget;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|UI")
	UHJThreatWidget* ThreatWidget = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|UI")
	UHJInventoryWidget* InventoryWidget = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|UI")
	UHJDialogueWidget* DialogueWidget = nullptr;

	// -------------------------------------------------------
	// Actions
	// -------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Tartaria")
	void ReturnToMainMenu();

	UFUNCTION(BlueprintCallable, Category = "Tartaria")
	void TogglePauseMenu();

	UFUNCTION(BlueprintCallable, Category = "Tartaria")
	void ToggleDebugOverlay();

	/** Toggle CEF dashboard overlay on/off while in-game. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria")
	void ToggleDashboardOverlay();

	/** Open the CEF dashboard and navigate to a specific route. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria")
	void OpenDashboardToRoute(const FString& Route);

	/** Toggle inventory + crafting panel (bound to I key). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria")
	void ToggleInventory();

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria")
	bool bPauseMenuOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria")
	bool bDashboardOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria")
	bool bInventoryOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria")
	UHJDashboardWidget* DashOverlayWidget = nullptr;

	/** World populator — spawns biomes, resources, buildings, NPCs at runtime. */
	UPROPERTY()
	UTartariaWorldPopulator* WorldPopulator = nullptr;

	/** Solar system manager — orbital mechanics, transit calculations. */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria")
	ATartariaSolarSystemManager* SolarSystem = nullptr;

	/** Level stream manager — seamless body transitions, visual profile swaps. */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria")
	ATartariaLevelStreamManager* LevelStreamManager = nullptr;

	/** Current zone difficulty (updated on zone change). */
	int32 CurrentZoneDifficulty = 1;

	// -------------------------------------------------------
	// Project Completion Delegate
	// -------------------------------------------------------

	/** Broadcast when a pipeline project finishes materialization. */
	UPROPERTY(BlueprintAssignable, Category = "Tartaria")
	FOnProjectCompleted OnProjectCompleted;

	/** Notify all listeners that a project has completed. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria")
	void NotifyProjectCompleted(const FString& ProjectId);

private:

	// EventPoller delegate receivers
	UFUNCTION()
	void OnPipelineStatusUpdate(const FString& ProjectId, const FString& Stage,
	                             int32 StageIndex, bool bActive);

	UFUNCTION()
	void OnHealthStatusUpdate(bool bOnline, const FString& Message);

	/** Fix 3.5: Receives queue count changes from GameInstance. */
	UFUNCTION()
	void OnQueueCountChanged(int32 Count);

	// Phase 1: Economy delegate receivers
	UFUNCTION()
	void OnGameStateUpdated();

	UFUNCTION()
	void OnTickCompleted(const TArray<FTartariaTickEvent>& Events);

	// Phase 2: Combat delegate receivers
	UFUNCTION()
	void OnThreatDetected(const FTartariaThreatInfo& Threat, ATartariaBiomeVolume* Volume);

	UFUNCTION()
	void OnZoneChanged(const FString& NewBiome, int32 NewDifficulty);

	UFUNCTION()
	void OnEncounterResolved(const FTartariaEncounterResult& Result);

	UFUNCTION()
	void OnPlayerHealthChanged(float NewHealth, float MaxHealthVal);

	UFUNCTION()
	void OnPlayerDeath();

	void SubscribeToBiomeVolumes();
	void SubscribeToNPCs();
	void SubscribeToQuestMarkers();

	UFUNCTION()
	void OnNPCDialogueStarted(const FString& NPCName, const FString& Faction);

	UFUNCTION()
	void OnNPCDialogueReceived(const FString& NPCName, const FString& ResponseText, const TArray<FString>& Actions);

	UFUNCTION()
	void OnNPCDialogueErrored(const FString& NPCName);

	UFUNCTION()
	void OnDialogueActionSelected(const FString& ActionKey);

	UFUNCTION()
	void OnQuestInteracted(const FString& QuestId, int32 Step);

	// SSE game event stream handlers
	UFUNCTION()
	void OnSSESnapshotReceived();

	UFUNCTION()
	void OnSSETickEvents(const TArray<FTartariaTickEvent>& Events);
};
