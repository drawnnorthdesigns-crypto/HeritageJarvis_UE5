#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HJSettingsWidget.generated.h"

class UOverlay;
class UVerticalBox;
class UHorizontalBox;
class UTextBlock;
class UButton;
class UEditableTextBox;
class UBorder;
class USpacer;

/**
 * UHJSettingsWidget -- Settings panel on the Main Menu.
 * Blueprint child: WBP_Settings in Content/UI/.
 *
 * Allows editing:
 *   - Flask server URL (persisted via HJSaveGame)
 *   - Test connection button
 *
 * Extend in Blueprint to add graphics quality, keybind remapping, etc.
 */
UCLASS(Abstract)
class HERITAGEJARVIS_API UHJSettingsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

    // -------------------------------------------------------
    // State
    // -------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "HJ|Settings")
    FString CurrentFlaskUrl;

    UPROPERTY(BlueprintReadOnly, Category = "HJ|Settings")
    bool bLastConnectionOk = false;

    // -------------------------------------------------------
    // Actions
    // -------------------------------------------------------

    /** Load current values from HJSaveGame and push to UI */
    UFUNCTION(BlueprintCallable, Category = "HJ|Settings")
    void LoadSettings();

    /** Write edited values to HJSaveGame and update HJApiClient */
    UFUNCTION(BlueprintCallable, Category = "HJ|Settings")
    void ApplyFlaskUrl(const FString& NewUrl);

    /** Ping /health on the current URL and fire OnConnectionTested */
    UFUNCTION(BlueprintCallable, Category = "HJ|Settings")
    void TestConnection();

    /** Reset all settings to factory defaults */
    UFUNCTION(BlueprintCallable, Category = "HJ|Settings")
    void ResetToDefaults();

    // -------------------------------------------------------
    // Blueprint events (BlueprintNativeEvent — C++ default + BP override)
    // -------------------------------------------------------

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|Settings")
    void OnSettingsLoaded(const FString& FlaskUrl);

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|Settings")
    void OnConnectionTested(bool bOk, const FString& Message);

private:
    // -------------------------------------------------------
    // Programmatic layout
    // -------------------------------------------------------

    void BuildProgrammaticLayout();

    UFUNCTION()
    void OnTestBtnClicked();

    UFUNCTION()
    void OnApplyBtnClicked();

    UFUNCTION()
    void OnResetBtnClicked();

    // -------------------------------------------------------
    // Widget references (prevent GC)
    // -------------------------------------------------------

    UPROPERTY()
    UEditableTextBox* FlaskUrlInput = nullptr;

    UPROPERTY()
    UButton* TestBtn = nullptr;

    UPROPERTY()
    UButton* ApplyBtn = nullptr;

    UPROPERTY()
    UButton* ResetBtn = nullptr;

    UPROPERTY()
    UTextBlock* StatusText = nullptr;

    UPROPERTY()
    UTextBlock* ConnectionStatusText = nullptr;
};
