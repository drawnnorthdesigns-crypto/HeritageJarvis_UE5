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
    void SetStaminaInfo(float InStamina, float InMaxStamina);

    UFUNCTION(BlueprintCallable, Category = "HJ|HUD")
    void SetFactionInfo(const TArray<FTartariaFactionInfo>& InFactions);

    UFUNCTION(BlueprintCallable, Category = "HJ|HUD")
    void SetStrategicInfo(const FTartariaFleetSummary& InFleet,
                          const FTartariaTechSummary& InTech,
                          const FTartariaMiningSummary& InMining);

    /** Fix 3.5: Update the offline queue count badge */
    UFUNCTION(BlueprintCallable, Category = "HJ|HUD")
    void SetQueueCount(int32 Count);

    // -------------------------------------------------------
    // Phase 2+3: Navigation UI + Combat Feedback
    // -------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "HJ|HUD")
    void SetCompassHeading(float Yaw);

    UFUNCTION(BlueprintCallable, Category = "HJ|HUD")
    void SetActiveQuest(const FString& QuestTitle, const FString& ObjectiveText, int32 Step, int32 TotalSteps);

    UFUNCTION(BlueprintCallable, Category = "HJ|HUD")
    void ShowZoneToast(const FString& ZoneName, int32 Difficulty);

    UFUNCTION(BlueprintCallable, Category = "HJ|HUD")
    void ShowDamageDirection(float DirectionAngle, float Intensity);

    UFUNCTION(BlueprintCallable, Category = "HJ|HUD")
    void ShowVictoryFanfare(const FString& Message, int32 XPGained);

    UFUNCTION(BlueprintCallable, Category = "HJ|HUD")
    void ShowLevelUp(int32 NewLevel, const FString& Title);

    // -------------------------------------------------------
    // Minimap + Interaction Prompt
    // -------------------------------------------------------

    /** Refresh the minimap with world data. */
    void UpdateMinimap();

    /** Show/hide the interaction prompt. */
    void ShowInteractPrompt(const FString& Prompt);
    void HideInteractPrompt();

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
    void OnStaminaInfoUpdated();

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|HUD")
    void OnFactionInfoUpdated();

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|HUD")
    void OnStrategicInfoUpdated();

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

    // Stamina bar widgets
    UPROPERTY()
    UProgressBar* StaminaBar = nullptr;

    UPROPERTY()
    UTextBlock* StaminaText = nullptr;

    // Cached stamina state
    float CachedStamina = 100.f;
    float CachedMaxStamina = 100.f;

    // Faction bar widgets (Phase 4) — 4 factions
    UPROPERTY()
    TArray<UTextBlock*> FactionLabels;

    UPROPERTY()
    TArray<UProgressBar*> FactionBars;

    UPROPERTY()
    TArray<UTextBlock*> FactionValues;

    TArray<FTartariaFactionInfo> CachedFactions;

    // Strategic indicators (Phase 5)
    UPROPERTY()
    UTextBlock* FleetText = nullptr;

    UPROPERTY()
    UTextBlock* TechText = nullptr;

    UPROPERTY()
    UTextBlock* MiningText = nullptr;

    FTartariaFleetSummary CachedFleet;
    FTartariaTechSummary CachedTech;
    FTartariaMiningSummary CachedMining;

    // Last-saved indicator (Save/Load UX)
    UPROPERTY()
    UTextBlock* LastSavedText = nullptr;

    FDateTime LastSaveTime;
    float LastSavedFadeTimer = 0.f;

    // ── Minimap ──────────────────────────────────────────────
    UPROPERTY() UBorder* MinimapBg = nullptr;
    UPROPERTY() UCanvasPanel* MinimapCanvas = nullptr;
    UPROPERTY() UTextBlock* MinimapNorth = nullptr;
    UPROPERTY() TArray<UBorder*> MinimapDots;
    UPROPERTY() UBorder* MinimapPlayerArrow = nullptr;

    // ── Interaction Prompt ───────────────────────────────────
    UPROPERTY() UBorder* InteractPromptBg = nullptr;
    UPROPERTY() UTextBlock* InteractPromptText = nullptr;

    // Compass (Phase 3)
    UPROPERTY()
    UTextBlock* CompassText = nullptr;
    float CurrentCompassYaw = 0.f;

    // Quest tracker (Phase 3)
    UPROPERTY()
    UTextBlock* QuestTitleText = nullptr;
    UPROPERTY()
    UTextBlock* QuestObjectiveText = nullptr;

    // Zone toast (Phase 3)
    UPROPERTY()
    UTextBlock* ZoneToastText = nullptr;
    float ZoneToastTimer = 0.f;

    // Damage direction (Phase 2)
    float DamageDirectionAngle = 0.f;
    float DamageDirectionIntensity = 0.f;
    float DamageDirectionTimer = 0.f;

    // Victory/Level-up overlay
    float VictoryTimer = 0.f;
    FString VictoryMessage;
    float LevelUpTimer = 0.f;
    int32 LevelUpLevel = 0;
    FString LevelUpTitle;

public:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
};
