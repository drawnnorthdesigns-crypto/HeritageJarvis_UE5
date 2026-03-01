#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HJDialogueWidget.generated.h"

class UCanvasPanel;
class UBorder;
class UTextBlock;
class UButton;
class UVerticalBox;
class UHorizontalBox;
class UScrollBox;

/**
 * UHJDialogueWidget — Native NPC dialogue overlay.
 * Shows NPC name (faction-colored), dialogue text with typewriter reveal,
 * available action buttons, and themed error messages.
 * Triggered by NPC interaction, dismissible via E-key, X button, or dimmer click.
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

	/** Update with the NPC's actual response text (typewriter reveal). */
	UFUNCTION(BlueprintCallable, Category = "HJ|Dialogue")
	void SetResponse(const FString& ResponseText);

	/** Update with response text + available action buttons. */
	UFUNCTION(BlueprintCallable, Category = "HJ|Dialogue")
	void SetResponseWithActions(const FString& ResponseText, const TArray<FString>& Actions);

	/** Append a streaming token (typewriter continues from current position). */
	UFUNCTION(BlueprintCallable, Category = "HJ|Dialogue")
	void AppendToken(const FString& Token);

	/** Finalize streaming response — complete typewriter and show actions. */
	UFUNCTION(BlueprintCallable, Category = "HJ|Dialogue")
	void FinalizeResponse(const TArray<FString>& Actions);

	/** Show a themed error state (maps raw errors to narrative text). */
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

	// -------------------------------------------------------
	// Delegates
	// -------------------------------------------------------

	/** Fired when player clicks an action button. Carries the action key. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueActionSelected, const FString&, ActionKey);

	UPROPERTY(BlueprintAssignable, Category = "HJ|Dialogue")
	FOnDialogueActionSelected OnActionSelected;

	// Blueprint events
	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Dialogue")
	void OnDialogueShown();

	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Dialogue")
	void OnDialogueHidden();

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Get faction-specific color for NPC name. */
	static FLinearColor GetFactionColor(const FString& Faction);

	/** Map raw error text to narrative-themed error. */
	static FString GetThemedError(const FString& RawError);

private:
	void BuildProgrammaticLayout();
	void PopulateActionButtons(const TArray<FString>& Actions);
	void ClearActionButtons();

	UPROPERTY() UBorder* PanelBg = nullptr;
	UPROPERTY() UBorder* PortraitBorder = nullptr;
	UPROPERTY() UTextBlock* PortraitSymbol = nullptr;
	UPROPERTY() UTextBlock* NPCNameText = nullptr;
	UPROPERTY() UTextBlock* FactionText = nullptr;
	UPROPERTY() UTextBlock* TitleText = nullptr;
	UPROPERTY() UTextBlock* DialogueText = nullptr;
	UPROPERTY() UButton* CloseButton = nullptr;
	UPROPERTY() UButton* DimmerButton = nullptr;
	UPROPERTY() UScrollBox* TextScroll = nullptr;
	UPROPERTY() UHorizontalBox* ActionBox = nullptr;
	UPROPERTY() UTextBlock* HintText = nullptr;

	/** Fade animation state. */
	float FadeAlpha = 0.f;
	int32 FadeDir = 0;  // 0=idle, +1=fading in, -1=fading out

	/** True while awaiting NPC response (enables loading animation). */
	bool bIsThinking = false;

	/** Timer for cycling the ellipsis and NPC name glow. */
	float ThinkingTimer = 0.f;

	/** Current dot count for "Thinking." -> "Thinking.." -> "Thinking..." cycle. */
	int32 ThinkingDotCount = 0;

	// -------------------------------------------------------
	// Typewriter state
	// -------------------------------------------------------

	/** Full response text to be revealed. */
	FString FullResponseText;

	/** How many characters are currently visible. */
	int32 TypewriterIndex = 0;

	/** True while typewriter is actively revealing characters. */
	bool bTypewriterActive = false;

	/** Characters revealed per second. */
	float TypewriterSpeed = 60.f;

	/** Fractional character accumulator for sub-frame precision. */
	float TypewriterAccum = 0.f;

	/** True if currently in streaming mode (tokens arrive incrementally). */
	bool bIsStreaming = false;

	/** Pending actions to show after typewriter completes. */
	TArray<FString> PendingActions;

	// -------------------------------------------------------
	// Action button tracking
	// -------------------------------------------------------

	UPROPERTY()
	TArray<UButton*> ActionButtons;

	UFUNCTION()
	void OnCloseClicked();

	UFUNCTION()
	void OnDimmerClicked();

	UFUNCTION()
	void OnActionButtonClicked();
};
