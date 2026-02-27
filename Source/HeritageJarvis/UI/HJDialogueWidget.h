#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HJDialogueWidget.generated.h"

class UCanvasPanel;
class UBorder;
class UTextBlock;
class UButton;
class UVerticalBox;
class UScrollBox;

/**
 * UHJDialogueWidget — Native NPC dialogue overlay.
 * Shows NPC name, faction badge, dialogue text, and close button.
 * Triggered by NPC interaction, auto-hides on close or timeout.
 */
UCLASS(Abstract)
class HERITAGEJARVIS_API UHJDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	/** Show the dialogue panel with "Thinking..." while awaiting response. */
	UFUNCTION(BlueprintCallable, Category = "HJ|Dialogue")
	void ShowDialogue(const FString& InNPCName, const FString& InFaction);

	/** Update with the NPC's actual response text. */
	UFUNCTION(BlueprintCallable, Category = "HJ|Dialogue")
	void SetResponse(const FString& ResponseText);

	/** Show an error state. */
	UFUNCTION(BlueprintCallable, Category = "HJ|Dialogue")
	void ShowError(const FString& ErrorText);

	/** Hide the dialogue panel. */
	UFUNCTION(BlueprintCallable, Category = "HJ|Dialogue")
	void HideDialogue();

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Dialogue")
	bool bIsOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Dialogue")
	FString CurrentNPCName;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Dialogue")
	FString CurrentFaction;

	// Blueprint events
	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Dialogue")
	void OnDialogueShown();

	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Dialogue")
	void OnDialogueHidden();

private:
	void BuildProgrammaticLayout();

	UPROPERTY() UBorder* PanelBg = nullptr;
	UPROPERTY() UTextBlock* NPCNameText = nullptr;
	UPROPERTY() UTextBlock* FactionText = nullptr;
	UPROPERTY() UTextBlock* DialogueText = nullptr;
	UPROPERTY() UButton* CloseButton = nullptr;
	UPROPERTY() UScrollBox* TextScroll = nullptr;

	UFUNCTION()
	void OnCloseClicked();
};
