#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HJDebugWidget.generated.h"

class UOverlay;
class UCanvasPanel;
class UVerticalBox;
class UTextBlock;
class UBorder;

USTRUCT(BlueprintType)
struct FHJDebugInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString FlaskUrl;
    UPROPERTY(BlueprintReadOnly) bool    bFlaskOnline    = false;
    UPROPERTY(BlueprintReadOnly) FString ActiveProjectId;
    UPROPERTY(BlueprintReadOnly) FString ActiveProjectName;
    UPROPERTY(BlueprintReadOnly) int32   QueuedRequests  = 0;
    UPROPERTY(BlueprintReadOnly) FString LastPollTime;
    UPROPERTY(BlueprintReadOnly) float   PollInterval    = 3.0f;
    UPROPERTY(BlueprintReadOnly) float   FPS             = 0.0f;
    UPROPERTY(BlueprintReadOnly) FString PipelineStage;
    UPROPERTY(BlueprintReadOnly) bool    bPipelineActive = false;
};

/**
 * UHJDebugWidget — Developer diagnostics overlay.
 * Toggle with IA_DebugToggle (F3 by default).
 *
 * Builds its UMG layout programmatically in NativeOnInitialized().
 * Blueprint children can still override OnDebugInfoRefreshed.
 *
 * Refreshes twice per second while visible via NativeTick.
 */
UCLASS(Abstract)
class HERITAGEJARVIS_API UHJDebugWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(BlueprintReadOnly, Category = "HJ|Debug")
    FHJDebugInfo DebugInfo;

    UPROPERTY(BlueprintReadOnly, Category = "HJ|Debug")
    bool bDebugVisible = false;

    UFUNCTION(BlueprintCallable, Category = "HJ|Debug")
    void ToggleDebugOverlay();

    UFUNCTION(BlueprintCallable, Category = "HJ|Debug")
    void RefreshDebugInfo();

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|Debug")
    void OnDebugInfoRefreshed(const FHJDebugInfo& Info);

protected:
    virtual void NativeOnInitialized() override;

private:
    float TickAccum = 0.0f;

    void BuildProgrammaticLayout();

    UPROPERTY()
    UVerticalBox* DebugInfoBox = nullptr;

    UPROPERTY()
    UTextBlock* FlaskUrlText = nullptr;

    UPROPERTY()
    UTextBlock* FlaskOnlineText = nullptr;

    UPROPERTY()
    UTextBlock* ProjectIdText = nullptr;

    UPROPERTY()
    UTextBlock* ProjectNameText = nullptr;

    UPROPERTY()
    UTextBlock* QueuedText = nullptr;

    UPROPERTY()
    UTextBlock* PollTimeText = nullptr;

    UPROPERTY()
    UTextBlock* PollIntervalText = nullptr;

    UPROPERTY()
    UTextBlock* FPSText = nullptr;

    UPROPERTY()
    UTextBlock* PipelineStageText = nullptr;

    UPROPERTY()
    UTextBlock* PipelineActiveText = nullptr;
};
