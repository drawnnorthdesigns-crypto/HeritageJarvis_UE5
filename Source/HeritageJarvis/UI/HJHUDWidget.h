#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/TartariaTypes.h"
#include "HJHUDWidget.generated.h"

class UOverlay;
class UCanvasPanel;
class UHorizontalBox;
class UVerticalBox;
class UTextBlock;
class UProgressBar;
class UBorder;
class USpacer;
class USizeBox;

/**
 * UHJHUDWidget -- In-game heads-up display.
 * Displayed while the player is in the TartariaWorld level.
 * Spawned by TartariaGameMode::BeginPlay().
 *
 * Shows:
 *   - Active project name + pipeline stage progress bar
 *   - Flask online/offline indicator
 *   - Notification badge (routes to UHJNotificationWidget)
 *   - Menu hint (Escape key)
 *
 * Blueprint child (WBP_HUD) builds the actual layout.
 * TartariaGameMode pushes updates via SetProjectInfo() / SetFlaskStatus().
 */
UCLASS(Abstract)
class HERITAGEJARVIS_API UHJHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;

    // -------------------------------------------------------
    // State
    // -------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "HJ|HUD")
    FString ActiveProjectName;

    UPROPERTY(BlueprintReadOnly, Category = "HJ|HUD")
    FString PipelineStage;

    UPROPERTY(BlueprintReadOnly, Category = "HJ|HUD")
    int32 PipelineStageIndex = 0;

    UPROPERTY(BlueprintReadOnly, Category = "HJ|HUD")
    bool bPipelineActive = false;

    UPROPERTY(BlueprintReadOnly, Category = "HJ|HUD")
    float PipelineProgressPct = 0.0f;   // 0-1

    UPROPERTY(BlueprintReadOnly, Category = "HJ|HUD")
    bool bFlaskOnline = false;

    // -------------------------------------------------------
    // Economy state (Phase 1)
    // -------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "HJ|HUD")
    int32 Credits = 0;

    UPROPERTY(BlueprintReadOnly, Category = "HJ|HUD")
    FString EraDisplay;  // "FOUNDATION -- Day 7"

    UPROPERTY(BlueprintReadOnly, Category = "HJ|HUD")
    int32 IronCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "HJ|HUD")
    int32 StoneCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "HJ|HUD")
    int32 KnowledgeCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "HJ|HUD")
    int32 CrystalCount = 0;

    // -------------------------------------------------------
    // Push updates from TartariaGameMode / EventPoller
    // -------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "HJ|HUD")
    void SetProjectInfo(const FString& ProjectName,
                        const FString& Stage,
                        int32          StageIndex,
                        bool           bActive,
                        int32          TotalStages = 7);

    UFUNCTION(BlueprintCallable, Category = "HJ|HUD")
    void SetFlaskStatus(bool bOnline);

    UFUNCTION(BlueprintCallable, Category = "HJ|HUD")
    void SetGameEconomy(int32 InCredits, const FString& InEra, int32 InDay,
                        int32 InIron, int32 InStone, int32 InKnowledge, int32 InCrystal);

    UFUNCTION(BlueprintCallable, Category = "HJ|HUD")
    void SetHealthInfo(float InHealth, float InMaxHealth, const FString& InZone, int32 InDifficulty);

    UFUNCTION(BlueprintCallable, Category = "HJ|HUD")
    void SetFactionInfo(const TArray<FTartariaFactionInfo>& InFactions);

    /** Fix 3.5: Update the offline queue count badge */
    UFUNCTION(BlueprintCallable, Category = "HJ|HUD")
    void SetQueueCount(int32 Count);

    // -------------------------------------------------------
    // Blueprint events (BlueprintNativeEvent -- C++ default + BP override)
    // -------------------------------------------------------

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|HUD")
    void OnProjectInfoUpdated();

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|HUD")
    void OnFlaskStatusUpdated();

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|HUD")
    void OnGameEconomyUpdated();

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|HUD")
    void OnHealthInfoUpdated();

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|HUD")
    void OnFactionInfoUpdated();

private:
    // -------------------------------------------------------
    // Programmatic layout
    // -------------------------------------------------------

    void BuildProgrammaticLayout();

    // -------------------------------------------------------
    // Widget references (prevent GC)
    // -------------------------------------------------------

    UPROPERTY()
    UTextBlock* ProjectNameText = nullptr;

    UPROPERTY()
    UTextBlock* StageText = nullptr;

    UPROPERTY()
    UProgressBar* PipelineProgressBar = nullptr;

    UPROPERTY()
    UBorder* FlaskDot = nullptr;

    UPROPERTY()
    UTextBlock* FlaskStatusLabel = nullptr;

    UPROPERTY()
    UTextBlock* EscHintText = nullptr;

    /** Fix 3.5: Queue count badge (visible when > 0) */
    UPROPERTY()
    UTextBlock* QueueBadge = nullptr;

    // Economy bar widgets (Phase 1)
    UPROPERTY()
    UTextBlock* CreditsText = nullptr;

    UPROPERTY()
    UTextBlock* EraText = nullptr;

    UPROPERTY()
    UTextBlock* ResourcesText = nullptr;

    // Health bar widgets (Phase 2)
    UPROPERTY()
    UProgressBar* HealthBar = nullptr;

    UPROPERTY()
    UTextBlock* HealthText = nullptr;

    UPROPERTY()
    UTextBlock* ZoneText = nullptr;

    // Cached health state
    float CachedHealth = 100.f;
    float CachedMaxHealth = 100.f;
    FString CachedZoneName;
    int32 CachedDifficulty = 1;

    // Faction bar widgets (Phase 4) — 4 factions
    UPROPERTY()
    TArray<UTextBlock*> FactionLabels;

    UPROPERTY()
    TArray<UProgressBar*> FactionBars;

    UPROPERTY()
    TArray<UTextBlock*> FactionValues;

    TArray<FTartariaFactionInfo> CachedFactions;
};
