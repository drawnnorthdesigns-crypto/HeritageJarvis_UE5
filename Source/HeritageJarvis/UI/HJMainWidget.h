#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HJMainWidget.generated.h"

class UHJNotificationWidget;
class UHJSettingsWidget;
class UHJDebugWidget;
class UHJLoadingWidget;
class UHJProjectsWidget;
class UHJLibraryWidget;
class UHJChatWidget;
class UOverlay;
class UHorizontalBox;
class UVerticalBox;
class UBorder;
class UTextBlock;
class UButton;
class UWidgetSwitcher;

/**
 * UHJMainWidget — Root UI Widget (main menu level).
 * Tabs: Projects | Library | Settings
 * Blueprint child: WBP_Main in Content/UI/
 *
 * On NativeOnInitialized:
 *   - Builds programmatic sidebar + content switcher layout if Blueprint is empty
 *
 * On NativeConstruct:
 *   - Restores last active tab from HJSaveGame
 *   - Spawns NotificationWidget, DebugWidget, LoadingWidget overlays
 *   - Populates ContentSwitcher with tab sub-widgets
 *   - Subscribes to HJEventPoller for live health + pipeline events
 */
UCLASS(Abstract)
class HERITAGEJARVIS_API UHJMainWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// -------------------------------------------------------
	// Widget classes — assign in BP_HJMainGameMode or WBP_Main
	// -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|UI")
	TSubclassOf<UHJNotificationWidget> NotificationWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|UI")
	TSubclassOf<UHJDebugWidget> DebugWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|UI")
	TSubclassOf<UHJLoadingWidget> LoadingWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|UI")
	TSubclassOf<UHJSettingsWidget> SettingsWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|UI")
	TSubclassOf<UHJProjectsWidget> ProjectsWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|UI")
	TSubclassOf<UHJLibraryWidget> LibraryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|UI")
	TSubclassOf<UHJChatWidget> ChatWidgetClass;

	// -------------------------------------------------------
	// Tab navigation
	// -------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "HJ|Navigation")
	void ShowProjectsTab();

	UFUNCTION(BlueprintCallable, Category = "HJ|Navigation")
	void ShowChatTab();

	UFUNCTION(BlueprintCallable, Category = "HJ|Navigation")
	void ShowLibraryTab();

	UFUNCTION(BlueprintCallable, Category = "HJ|Navigation")
	void ShowSettingsTab();

	/** Transition to Tartaria. Shows loading screen, saves session, opens level. */
	UFUNCTION(BlueprintCallable, Category = "HJ|Navigation")
	void EnterGameMode(const FString& ProjectId);

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Navigation")
	FString ActiveTab = TEXT("projects");

	// -------------------------------------------------------
	// Project selection (called by HJProjectsWidget on card click)
	// Keeps chat context in sync.
	// -------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "HJ|Context")
	void SetActiveProject(const FString& ProjectId, const FString& ProjectName);

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Context")
	FString ActiveProjectId;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Context")
	FString ActiveProjectName;

	// -------------------------------------------------------
	// Blueprint native events (C++ provides default, BP can override)
	// -------------------------------------------------------

	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Status")
	void OnFlaskStatusChanged(bool bOnline);

	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Status")
	void OnProjectSelected(const FString& ProjectId, const FString& ProjectName);

	// Tab visibility — C++ default switches ContentSwitcher, BP can override
	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Navigation")
	void BP_ShowProjectsTab();

	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Navigation")
	void BP_ShowChatTab();

	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Navigation")
	void BP_ShowLibraryTab();

	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Navigation")
	void BP_ShowSettingsTab();

	// -------------------------------------------------------
	// Debug overlay toggle (backtick / F3)
	// -------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "HJ|Debug")
	void ToggleDebugOverlay();

private:
	void CheckFlaskHealth();
	void BuildProgrammaticLayout();

	/** Helper: highlight the active sidebar button, dim the rest. */
	void UpdateSidebarHighlight(UButton* ActiveBtn);

	UFUNCTION()
	void OnHealthStatusFromPoller(bool bOnline, const FString& Message);

	// Button click handlers
	UFUNCTION()
	void OnProjectsBtnClicked();

	UFUNCTION()
	void OnChatBtnClicked();

	UFUNCTION()
	void OnLibraryBtnClicked();

	UFUNCTION()
	void OnSettingsBtnClicked();

	UFUNCTION()
	void OnEnterTartariaBtnClicked();

	FTimerHandle StatusTimer;

	// -------------------------------------------------------
	// Overlay widget instances (spawned in NativeConstruct)
	// -------------------------------------------------------

	UPROPERTY()
	UHJNotificationWidget* NotifWidget = nullptr;

	UPROPERTY()
	UHJDebugWidget* DebugWidget = nullptr;

	UPROPERTY()
	UHJLoadingWidget* LoadingWidget = nullptr;

	UPROPERTY()
	UHJSettingsWidget* SettingsWidget = nullptr;

	// -------------------------------------------------------
	// Programmatic UMG layout widgets (built in NativeOnInitialized)
	// -------------------------------------------------------

	UPROPERTY()
	UOverlay* RootOverlay = nullptr;

	UPROPERTY()
	UVerticalBox* SidebarBox = nullptr;

	UPROPERTY()
	UWidgetSwitcher* ContentSwitcher = nullptr;

	UPROPERTY()
	UTextBlock* TitleText = nullptr;

	UPROPERTY()
	UTextBlock* FlaskStatusText = nullptr;

	UPROPERTY()
	UButton* ProjectsTabBtn = nullptr;

	UPROPERTY()
	UButton* ChatTabBtn = nullptr;

	UPROPERTY()
	UButton* LibraryTabBtn = nullptr;

	UPROPERTY()
	UButton* SettingsTabBtn = nullptr;

	UPROPERTY()
	UButton* EnterTartariaBtn = nullptr;

	// Content panels (created in NativeConstruct, added to ContentSwitcher)
	UPROPERTY()
	UUserWidget* ProjectsPanel = nullptr;

	UPROPERTY()
	UUserWidget* ChatPanel = nullptr;

	UPROPERTY()
	UUserWidget* LibraryPanel = nullptr;

	UPROPERTY()
	UUserWidget* SettingsPanel = nullptr;
};
