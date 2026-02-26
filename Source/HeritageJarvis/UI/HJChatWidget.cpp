#include "HJChatWidget.h"
#include "Core/HJGameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Spacer.h"
#include "Styling/CoreStyle.h"
#include "UI/HJLoadingWidget.h"

// -------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------

void UHJChatWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildProgrammaticLayout();
}

// -------------------------------------------------------------------
// Programmatic UMG layout
// -------------------------------------------------------------------

void UHJChatWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree) return;

	// Guard: if Blueprint already placed children, skip programmatic build
	UPanelWidget* ExistingRoot = Cast<UPanelWidget>(WidgetTree->RootWidget);
	if (ExistingRoot && ExistingRoot->GetChildrenCount() > 0) return;

	// --- Root Overlay ---
	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), FName("RootOverlay"));
	WidgetTree->RootWidget = RootOverlay;

	// --- Main VBox ---
	UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName("MainVBox"));
	RootOverlay->AddChild(MainVBox);

	// --- Title ---
	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("Steward AI Chat")));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 16);
	TitleText->SetFont(TitleFont);
	MainVBox->AddChild(TitleText);

	// --- Spacer 8px ---
	USpacer* Spacer1 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("Spacer1"));
	Spacer1->SetSize(FVector2D(0.f, 8.f));
	MainVBox->AddChild(Spacer1);

	// --- Chat ScrollBox (Fill) ---
	ChatScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), FName("ChatScrollBox"));
	MainVBox->AddChild(ChatScrollBox);
	if (UVerticalBoxSlot* ScrollSlot = Cast<UVerticalBoxSlot>(ChatScrollBox->Slot))
	{
		ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// --- Spacer 4px ---
	USpacer* Spacer2 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("Spacer2"));
	Spacer2->SetSize(FVector2D(0.f, 4.f));
	MainVBox->AddChild(Spacer2);

	// --- Waiting text (initially collapsed) ---
	WaitingText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("WaitingText"));
	WaitingText->SetText(FText::FromString(TEXT("Thinking...")));
	WaitingText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)));
	FSlateFontInfo WaitFont = FCoreStyle::GetDefaultFontStyle("Italic", 12);
	WaitingText->SetFont(WaitFont);
	WaitingText->SetVisibility(ESlateVisibility::Collapsed);
	MainVBox->AddChild(WaitingText);

	// --- Input row: EditableTextBox + Send button ---
	UHorizontalBox* InputRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName("InputRow"));
	MainVBox->AddChild(InputRow);

	// Chat input
	ChatInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), FName("ChatInput"));
	ChatInput->SetHintText(FText::FromString(TEXT("Ask Jarvis...")));
	InputRow->AddChild(ChatInput);
	if (UHorizontalBoxSlot* InputSlot = Cast<UHorizontalBoxSlot>(ChatInput->Slot))
	{
		InputSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// Send button
	SendBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName("SendBtn"));
	SendBtn->SetBackgroundColor(FLinearColor(0.15f, 0.55f, 0.95f));
	UTextBlock* SendBtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("SendBtnLabel"));
	SendBtnLabel->SetText(FText::FromString(TEXT("Send")));
	SendBtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	SendBtn->AddChild(SendBtnLabel);
	InputRow->AddChild(SendBtn);
	SendBtn->OnClicked.AddDynamic(this, &UHJChatWidget::OnSendBtnClicked);
}

// -------------------------------------------------------------------
// Button handlers
// -------------------------------------------------------------------

void UHJChatWidget::OnSendBtnClicked()
{
	if (!ChatInput) return;

	FString Text = ChatInput->GetText().ToString();
	if (Text.IsEmpty()) return;

	SendMessage(Text);

	// Clear input field
	ChatInput->SetText(FText::GetEmpty());

	// Show waiting indicator, disable send
	if (WaitingText) WaitingText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (SendBtn) SendBtn->SetIsEnabled(false);
}

// -------------------------------------------------------------------
// BlueprintNativeEvent _Implementation defaults
// -------------------------------------------------------------------

void UHJChatWidget::OnMessageAdded_Implementation(const FHJChatMessage& Message)
{
	if (!ChatScrollBox) return;

	UTextBlock* MsgBlock = NewObject<UTextBlock>(this);

	// Format: "User: ..." or "Jarvis: ..."
	FString Prefix = (Message.Role == TEXT("user")) ? TEXT("User: ") : TEXT("Jarvis: ");
	MsgBlock->SetText(FText::FromString(Prefix + Message.Content));

	// Color: white for user, light blue for assistant
	FLinearColor MsgColor = (Message.Role == TEXT("user"))
		? FLinearColor::White
		: FLinearColor(0.6f, 0.8f, 1.f);
	MsgBlock->SetColorAndOpacity(FSlateColor(MsgColor));

	FSlateFontInfo MsgFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
	MsgBlock->SetFont(MsgFont);

	// Enable text wrapping for longer messages
	MsgBlock->SetAutoWrapText(true);

	ChatScrollBox->AddChild(MsgBlock);

	// Add small spacer between messages
	USpacer* MsgSpacer = NewObject<USpacer>(this);
	MsgSpacer->SetSize(FVector2D(0.f, 4.f));
	ChatScrollBox->AddChild(MsgSpacer);

	// Scroll to bottom
	ChatScrollBox->ScrollToEnd();
}

void UHJChatWidget::OnMessageUpdated_Implementation(int32 MessageIndex)
{
	// Default: no-op — the streaming text block is updated directly in OnStreamToken.
	// Blueprint children can override this for custom streaming display.
	if (ChatScrollBox) ChatScrollBox->ScrollToEnd();
}

void UHJChatWidget::OnResponseComplete_Implementation()
{
	// Hide waiting text, re-enable send button
	if (WaitingText) WaitingText->SetVisibility(ESlateVisibility::Collapsed);
	if (SendBtn) SendBtn->SetIsEnabled(true);
}

// -------------------------------------------------------------------
// Existing logic (preserved)
// -------------------------------------------------------------------

void UHJChatWidget::SendMessage(const FString& Message)
{
	if (Message.IsEmpty() || bWaitingForResponse) return;

	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI) return;

	// Add user message immediately
	FHJChatMessage UserMsg;
	UserMsg.Role    = TEXT("user");
	UserMsg.Content = Message;
	Messages.Add(UserMsg);
	OnMessageAdded(UserMsg);

	bWaitingForResponse = true;
	UHJLoadingWidget::Show(this, TEXT("Generating response..."));

	// Fix 1.1: Add placeholder assistant message for streaming
	FHJChatMessage Placeholder;
	Placeholder.Role    = TEXT("assistant");
	Placeholder.Content = TEXT("");
	Messages.Add(Placeholder);
	bStreamingResponse = true;

	// Add the placeholder text block to the scroll view immediately
	if (ChatScrollBox)
	{
		StreamingTextBlock = NewObject<UTextBlock>(this);
		StreamingTextBlock->SetText(FText::FromString(TEXT("Jarvis: ")));
		StreamingTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.8f, 1.f)));
		FSlateFontInfo MsgFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
		StreamingTextBlock->SetFont(MsgFont);
		StreamingTextBlock->SetAutoWrapText(true);
		ChatScrollBox->AddChild(StreamingTextBlock);

		USpacer* MsgSpacer = NewObject<USpacer>(this);
		MsgSpacer->SetSize(FVector2D(0.f, 4.f));
		ChatScrollBox->AddChild(MsgSpacer);

		ChatScrollBox->ScrollToEnd();
	}

	// Token callback — appends each token to the streaming message
	FOnStreamToken TokenCB;
	TokenCB.BindUObject(this, &UHJChatWidget::OnStreamToken);

	// Stream complete callback
	FOnStreamComplete DoneCB;
	DoneCB.BindUObject(this, &UHJChatWidget::FinalizeStreamingResponse);

	// Fallback callback — if streaming fails, fall back to non-streaming
	// Fix: Weak pointer prevents crash if widget is destroyed before callback fires
	FString CapturedMessage = Message;
	TWeakObjectPtr<UHJChatWidget> WeakFallback(this);
	FOnApiResponse FallbackCB;
	FallbackCB.BindLambda([WeakFallback, CapturedMessage](bool bOk, const FString& Body)
	{
		if (UHJChatWidget* Self = WeakFallback.Get())
			Self->FallbackToNonStreaming(CapturedMessage);
	});

	if (!GI->ApiClient) return;
	GI->ApiClient->SendStreamingChatMessage(Message, ProjectContext, TokenCB, DoneCB, FallbackCB);
}

// -------------------------------------------------------------------
// Streaming handlers (Fix 1.1)
// -------------------------------------------------------------------

void UHJChatWidget::OnStreamToken(const FString& Token)
{
	if (!bStreamingResponse || Messages.Num() == 0) return;

	// Hide "Thinking..." and loading overlay on first token
	if (WaitingText && WaitingText->GetVisibility() != ESlateVisibility::Collapsed)
	{
		WaitingText->SetVisibility(ESlateVisibility::Collapsed);
		UHJLoadingWidget::Hide(this);
	}

	// Append token to the last message's content
	int32 LastIdx = Messages.Num() - 1;
	Messages[LastIdx].Content += Token;

	// Update the streaming text block directly (no full rebuild)
	if (StreamingTextBlock)
	{
		StreamingTextBlock->SetText(FText::FromString(
			TEXT("Jarvis: ") + Messages[LastIdx].Content));
	}

	// Notify Blueprint
	OnMessageUpdated(LastIdx);

	// Keep scroll at bottom
	if (ChatScrollBox) ChatScrollBox->ScrollToEnd();
}

void UHJChatWidget::FinalizeStreamingResponse(const FString& State, const FString& Flags)
{
	bStreamingResponse = false;
	bWaitingForResponse = false;
	UHJLoadingWidget::Hide(this);
	StreamingTextBlock = nullptr;

	OnResponseComplete();
}

void UHJChatWidget::FallbackToNonStreaming(const FString& Message)
{
	// Remove the empty placeholder message
	if (Messages.Num() > 0 && Messages.Last().Content.IsEmpty())
	{
		Messages.RemoveAt(Messages.Num() - 1);
	}

	// Remove the streaming text block from scroll view
	if (StreamingTextBlock && ChatScrollBox)
	{
		StreamingTextBlock->RemoveFromParent();
	}
	StreamingTextBlock = nullptr;
	bStreamingResponse = false;

	UE_LOG(LogTemp, Warning, TEXT("HJChatWidget: Streaming failed, falling back to non-streaming chat."));

	// Fall back to the original non-streaming path
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI) { bWaitingForResponse = false; return; }

	// Fix: Weak pointer prevents crash if widget is destroyed before callback fires
	TWeakObjectPtr<UHJChatWidget> WeakThis(this);

	FOnApiResponse CB;
	CB.BindLambda([WeakThis](bool bOk, const FString& Body)
	{
		UHJChatWidget* Self = WeakThis.Get();
		if (!Self) return;

		Self->bWaitingForResponse = false;

		FHJChatMessage Response;
		Response.Role = TEXT("assistant");

		if (bOk)
		{
			// Parse {"response": "..."} from Flask
			TSharedPtr<FJsonObject> Obj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
			if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
			{
				if (!Obj->TryGetStringField(TEXT("response"), Response.Content))
				{
					Response.Content = Body;
				}
			}
			else
				Response.Content = Body;
		}
		else
		{
			Response.Content = TEXT("I'm having trouble connecting to the backend. Is Flask running?");
		}

		Self->Messages.Add(Response);
		Self->OnMessageAdded(Response);
		UHJLoadingWidget::Hide(Self);
		Self->OnResponseComplete();
	});

	if (!GI->ApiClient) { bWaitingForResponse = false; return; }
	GI->ApiClient->SendChatMessage(Message, ProjectContext, CB);
}

void UHJChatWidget::ClearChat()
{
	Messages.Empty();

	// Also clear the visual scroll box if it exists
	if (ChatScrollBox)
	{
		ChatScrollBox->ClearChildren();
	}
}
