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
class UTartariaWorldPopulator;

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

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria")
	bool bPauseMenuOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria")
	bool bDashboardOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria")
	UHJDashboardWidget* DashOverlayWidget = nullptr;

	/** World populator — spawns biomes, resources, buildings, NPCs at runtime. */
	UPROPERTY()
	UTartariaWorldPopulator* WorldPopulator = nullptr;

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
};
