#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HJPauseWidget.generated.h"

class UOverlay;
class UVerticalBox;
class UHorizontalBox;
class UTextBlock;
class UButton;
class UEditableTextBox;
class UBorder;
class USpacer;
class USizeBox;
class UCanvasPanel;

/**
 * UHJPauseWidget -- In-game pause / quick-access overlay.
 * Spawned by TartariaGameMode::TogglePauseMenu().
 * Blueprint child: WBP_Pause in Content/UI/.
 *
 * Shows:
 *   - Active project name + live pipeline stage
 *   - Mini chat input (sends to Flask /chat without leaving game)
 *   - Resume button  -> calls ResumeGame()
 *   - Main Menu button -> calls ReturnToMainMenu()
 */
UCLASS(Abstract)
class HERITAGEJARVIS_API UHJPauseWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;

    // -------------------------------------------------------
    // State -- populated by TartariaGameMode before showing
    // -------------------------------------------------------

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Pause")
    FString ActiveProjectName;

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Pause")
    FString PipelineStage;

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Pause")
    bool bPipelineActive = false;

    // -------------------------------------------------------
    // Actions -- bind to UMG buttons
    // -------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "HJ|Pause")
    void ResumeGame();

    UFUNCTION(BlueprintCallable, Category = "HJ|Pause")
    void ReturnToMainMenu();

    /** Send a quick chat message without leaving the game */
    UFUNCTION(BlueprintCallable, Category = "HJ|Pause")
    void SendQuickChat(const FString& Message);

    // -------------------------------------------------------
    // Blueprint events (BlueprintNativeEvent -- C++ default + BP override)
    // -------------------------------------------------------

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|Pause")
    void OnPipelineStatusUpdated();

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|Pause")
    void OnChatResponse(const FString& Response);

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|Pause")
    void OnChatError();

private:
    // -------------------------------------------------------
    // Programmatic layout
    // -------------------------------------------------------

    void BuildProgrammaticLayout();

    UFUNCTION()
    void OnResumeBtnClicked();

    UFUNCTION()
    void OnMainMenuBtnClicked();

    UFUNCTION()
    void OnChatSendBtnClicked();

    UFUNCTION()
    void OnDashboardBtnClicked();

    // -------------------------------------------------------
    // Widget references (prevent GC)
    // -------------------------------------------------------

    UPROPERTY()
    UTextBlock* ProjectInfoText = nullptr;

    UPROPERTY()
    UTextBlock* PipelineStatusText = nullptr;

    UPROPERTY()
    UEditableTextBox* ChatInput = nullptr;

    UPROPERTY()
    UTextBlock* ChatResponseText = nullptr;

    UPROPERTY()
    UButton* ResumeBtn = nullptr;

    UPROPERTY()
    UButton* MainMenuBtn = nullptr;

    UPROPERTY()
    UButton* ChatSendBtn = nullptr;

    UPROPERTY()
    UButton* DashboardBtn = nullptr;
};
