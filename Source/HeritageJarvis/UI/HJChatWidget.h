#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HJChatWidget.generated.h"

class UOverlay;
class UVerticalBox;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UButton;
class UEditableTextBox;
class USpacer;

USTRUCT(BlueprintType)
struct FHJChatMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Role;     // "user" or "assistant"
	UPROPERTY(BlueprintReadOnly) FString Content;
	UPROPERTY(BlueprintReadOnly) FString Timestamp;
};

/**
 * UHJChatWidget -- Steward AI Chat Panel
 * Sends messages to Flask /api/chat and displays the conversation.
 * NativeOnInitialized builds a default UMG layout programmatically.
 * Blueprint child (WBP_Chat) can still override the visual.
 */
UCLASS(Abstract)
class HERITAGEJARVIS_API UHJChatWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	// Full conversation history shown in the panel
	UPROPERTY(BlueprintReadOnly, Category = "HJ|Chat")
	TArray<FHJChatMessage> Messages;

	// Currently active project context (empty = no context)
	UPROPERTY(BlueprintReadWrite, Category = "HJ|Chat")
	FString ProjectContext;

	// Whether we're waiting for a response (disable send button)
	UPROPERTY(BlueprintReadOnly, Category = "HJ|Chat")
	bool bWaitingForResponse = false;

	// Send a message -- bind to the Send button's OnClicked
	UFUNCTION(BlueprintCallable, Category = "HJ|Chat")
	void SendMessage(const FString& Message);

	// Clear conversation history
	UFUNCTION(BlueprintCallable, Category = "HJ|Chat")
	void ClearChat();

	// -------------------------------------------------------
	// Blueprint events
	// -------------------------------------------------------

	// Called when a new message is added -- update the scroll list
	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Chat")
	void OnMessageAdded(const FHJChatMessage& Message);

	// Called when a streaming message is updated in-place (Fix 1.1)
	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Chat")
	void OnMessageUpdated(int32 MessageIndex);

	// Called when response streaming is done
	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Chat")
	void OnResponseComplete();

private:
	// --- Streaming state (Fix 1.1) ---
	bool bStreamingResponse = false;

	UPROPERTY()
	UTextBlock* StreamingTextBlock = nullptr;

	void OnStreamToken(const FString& Token);
	void FinalizeStreamingResponse(const FString& State, const FString& Flags);
	void FallbackToNonStreaming(const FString& Message);
	void BuildProgrammaticLayout();

	UFUNCTION()
	void OnSendBtnClicked();

	// --- Programmatic widget references (GC-safe) ---

	UPROPERTY()
	UScrollBox* ChatScrollBox = nullptr;

	UPROPERTY()
	UEditableTextBox* ChatInput = nullptr;

	UPROPERTY()
	UButton* SendBtn = nullptr;

	UPROPERTY()
	UTextBlock* WaitingText = nullptr;
};
