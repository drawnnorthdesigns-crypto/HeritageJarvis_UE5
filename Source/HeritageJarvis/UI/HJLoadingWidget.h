#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HJLoadingWidget.generated.h"

class UOverlay;
class UBorder;
class UTextBlock;
class UProgressBar;
class USizeBox;

/**
 * UHJLoadingWidget — Full-screen loading overlay for level transitions
 * and pipeline progress display.
 *
 * Created and added to viewport in HJMainWidget::EnterGameMode()
 * and removed when the new level's BeginPlay() fires.
 *
 * Builds its UMG layout programmatically in NativeOnInitialized().
 * Blueprint children can still override OnLoadingStateChanged.
 *
 * Static helpers (Show / Hide) let any C++ code trigger it without
 * needing a direct reference.
 *
 * Fix 1.2: Added pipeline progress overlay with stage name + progress bar.
 */
UCLASS(Abstract)
class HERITAGEJARVIS_API UHJLoadingWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    // -------------------------------------------------------
    // Static singleton access
    // -------------------------------------------------------

    static UHJLoadingWidget* Instance;

    static void Show(const UObject* WorldContext,
                     const FString& Message = TEXT("Loading..."));
    static void Hide(const UObject* WorldContext);

    // -------------------------------------------------------
    // State
    // -------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "HJ|Loading")
    FString LoadingMessage;

    UPROPERTY(BlueprintReadOnly, Category = "HJ|Loading")
    bool bIsLoading = false;

    // -------------------------------------------------------
    // Pipeline progress state (Fix 1.2)
    // -------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "HJ|Loading")
    float PipelineProgressPct = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "HJ|Loading")
    FString CurrentStageName;

    // -------------------------------------------------------
    // Direct control
    // -------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "HJ|Loading")
    void ShowLoading(const FString& Message = TEXT("Loading..."));

    UFUNCTION(BlueprintCallable, Category = "HJ|Loading")
    void HideLoading();

    // -------------------------------------------------------
    // Pipeline progress control (Fix 1.2)
    // -------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "HJ|Loading")
    void ShowPipelineProgress(const FString& StageName, int32 StageIndex, int32 TotalStages);

    UFUNCTION(BlueprintCallable, Category = "HJ|Loading")
    void UpdatePipelineProgress(const FString& StageName, int32 StageIndex, int32 TotalStages);

    UFUNCTION(BlueprintCallable, Category = "HJ|Loading")
    void HidePipelineProgress();

    // -------------------------------------------------------
    // Blueprint native event — default C++ impl, overridable in BP
    // -------------------------------------------------------

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|Loading")
    void OnLoadingStateChanged(bool bLoading, const FString& Message);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct()  override;

private:
    void BuildProgrammaticLayout();

    UPROPERTY()
    UBorder* DarkOverlay = nullptr;

    UPROPERTY()
    UTextBlock* LoadingText = nullptr;

    // Pipeline progress widgets (Fix 1.2)
    UPROPERTY()
    UProgressBar* PipelineProgressBar = nullptr;

    UPROPERTY()
    UTextBlock* StageText = nullptr;
};
