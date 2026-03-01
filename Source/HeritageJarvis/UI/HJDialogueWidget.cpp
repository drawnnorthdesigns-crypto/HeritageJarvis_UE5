#include "HJDialogueWidget.h"

#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/Spacer.h"
#include "Components/SizeBox.h"
#include "Styling/CoreStyle.h"

// Helper: concise text block creation
static UTextBlock* MakeDlgText(UWidgetTree* Tree, const FName& Name,
	const FString& Content, FLinearColor Color, const FString& Style, int32 Size)
{
	UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	T->SetText(FText::FromString(Content));
	T->SetColorAndOpacity(FSlateColor(Color));
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(*Style, Size);
	T->SetFont(Font);
	return T;
}

// -----------------------------------------------------------------------------
// Faction color mapping
// -----------------------------------------------------------------------------

FLinearColor UHJDialogueWidget::GetFactionColor(const FString& Faction)
{
	if (Faction.Contains(TEXT("FERRUM")))
		return FLinearColor(0.85f, 0.45f, 0.15f);   // Forge orange-bronze

	if (Faction.Contains(TEXT("OBSIDIAN")))
		return FLinearColor(0.55f, 0.35f, 0.75f);    // Alchemist purple

	if (Faction.Contains(TEXT("AUREATE")))
		return FLinearColor(0.95f, 0.85f, 0.30f);    // Scribe gold

	if (Faction.Contains(TEXT("ARGENTUM")))
		return FLinearColor(0.70f, 0.80f, 0.92f);    // Silver-blue

	if (Faction.Contains(TEXT("STEWARD")))
		return FLinearColor(0.90f, 0.85f, 0.60f);    // Default golden

	if (Faction.Contains(TEXT("VOID")))
		return FLinearColor(0.40f, 0.90f, 0.80f);    // Teal

	// Default golden
	return FLinearColor(0.90f, 0.85f, 0.60f);
}

// -----------------------------------------------------------------------------
// Themed error messages
// -----------------------------------------------------------------------------

FString UHJDialogueWidget::GetThemedError(const FString& RawError)
{
	FString Lower = RawError.ToLower();

	if (Lower.Contains(TEXT("timeout")))
		return TEXT("The aetheric link has weakened... the crystal relay cannot sustain the connection.");

	if (Lower.Contains(TEXT("offline")) || Lower.Contains(TEXT("connect")))
		return TEXT("The crystal relay has gone dark. No signal reaches this chamber.");

	if (Lower.Contains(TEXT("500")) || Lower.Contains(TEXT("server")))
		return TEXT("A disturbance ripples through the aether. The specialist struggles to manifest a reply.");

	if (Lower.Contains(TEXT("circuit")) || Lower.Contains(TEXT("overload")))
		return TEXT("The channels are overloaded — too many souls seek counsel at once.");

	if (Lower.Contains(TEXT("404")))
		return TEXT("This specialist has wandered beyond the known wards. Their location is uncertain.");

	return FString::Printf(TEXT("The winds of the void carry no reply... [%s]"), *RawError);
}

// -----------------------------------------------------------------------------
void UHJDialogueWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree) return;

	UPanelWidget* ExistingRoot = Cast<UPanelWidget>(WidgetTree->RootWidget);
	if (ExistingRoot && ExistingRoot->GetChildrenCount() > 0) return;

	BuildProgrammaticLayout();
	SetVisibility(ESlateVisibility::Collapsed);
}

// -----------------------------------------------------------------------------
void UHJDialogueWidget::BuildProgrammaticLayout()
{
	// Root canvas
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), FName("DlgRoot"));
	WidgetTree->RootWidget = Root;

	// Dimmer — clickable background to dismiss dialogue
	DimmerButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), FName("DlgDimmer"));
	FButtonStyle DimStyle;
	DimStyle.Normal.TintColor = FSlateColor(FLinearColor(0, 0, 0, 0.4f));
	DimStyle.Hovered.TintColor = FSlateColor(FLinearColor(0, 0, 0, 0.45f));
	DimStyle.Pressed.TintColor = FSlateColor(FLinearColor(0, 0, 0, 0.5f));
	DimmerButton->SetStyle(DimStyle);
	DimmerButton->OnClicked.AddDynamic(this, &UHJDialogueWidget::OnDimmerClicked);
	UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(DimmerButton);
	if (DimSlot) { DimSlot->SetAnchors(FAnchors(0, 0, 1, 1)); DimSlot->SetOffsets(FMargin(0)); }

	// Panel — bottom-center, 600x240 (slightly larger for action buttons)
	PanelBg = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), FName("DlgPanelBg"));
	PanelBg->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.09f, 0.94f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(PanelBg);
	if (PanelSlot)
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		PanelSlot->SetOffsets(FMargin(0, -80, 600, 240));
	}

	// Main vertical layout
	UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), FName("DlgMainVBox"));
	PanelBg->AddChild(MainVBox);

	// --- Header row: NPC name + faction + close button ---
	{
		UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), FName("DlgHeaderRow"));
		MainVBox->AddChild(HeaderRow);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(HeaderRow->Slot))
			Slot->SetPadding(FMargin(16, 10, 16, 4));

		// Portrait icon — colored circle with specialist symbol
		PortraitBorder = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), FName("DlgPortrait"));
		PortraitBorder->SetBrushColor(FLinearColor(0.15f, 0.12f, 0.08f));
		PortraitBorder->SetPadding(FMargin(8));
		HeaderRow->AddChild(PortraitBorder);
		if (UHorizontalBoxSlot* PSlot = Cast<UHorizontalBoxSlot>(PortraitBorder->Slot))
		{
			PSlot->SetVerticalAlignment(VAlign_Center);
			PSlot->SetPadding(FMargin(0, 0, 8, 0));
		}

		// Symbol text inside portrait (single character)
		PortraitSymbol = MakeDlgText(WidgetTree, FName("DlgPortraitSym"),
			TEXT("*"), FLinearColor(0.9f, 0.85f, 0.6f), TEXT("Bold"), 20);
		PortraitBorder->AddChild(PortraitSymbol);

		// NPC name (faction-colored, set dynamically)
		NPCNameText = MakeDlgText(WidgetTree, FName("DlgNPCName"),
			TEXT("NPC"), FLinearColor(0.9f, 0.85f, 0.6f), TEXT("Bold"), 15);
		HeaderRow->AddChild(NPCNameText);
		if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(NPCNameText->Slot))
			Slot->SetVerticalAlignment(VAlign_Center);

		// Spacer
		USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("DlgSpName"));
		Sp->SetSize(FVector2D(12, 0));
		HeaderRow->AddChild(Sp);

		// Faction badge
		FactionText = MakeDlgText(WidgetTree, FName("DlgFaction"),
			TEXT(""), FLinearColor(0.5f, 0.6f, 0.7f), TEXT("Regular"), 10);
		HeaderRow->AddChild(FactionText);
		if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(FactionText->Slot))
			Slot->SetVerticalAlignment(VAlign_Center);

		// Specialist title (smaller text below name)
		TitleText = MakeDlgText(WidgetTree, FName("DlgTitle"),
			TEXT(""), FLinearColor(0.5f, 0.5f, 0.55f), TEXT("Regular"), 9);
		HeaderRow->AddChild(TitleText);
		if (UHorizontalBoxSlot* TSlot = Cast<UHorizontalBoxSlot>(TitleText->Slot))
			TSlot->SetVerticalAlignment(VAlign_Center);

		USpacer* SpTitle = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), FName("DlgSpTitle"));
		SpTitle->SetSize(FVector2D(0, 0));
		HeaderRow->AddChild(SpTitle);

		// Fill
		USpacer* SpFill = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("DlgSpFill"));
		SpFill->SetSize(FVector2D(0, 0));
		HeaderRow->AddChild(SpFill);
		if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(SpFill->Slot))
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		// Close button
		CloseButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), FName("DlgCloseBtn"));
		CloseButton->SetBackgroundColor(FLinearColor(0.5f, 0.15f, 0.15f));
		CloseButton->OnClicked.AddDynamic(this, &UHJDialogueWidget::OnCloseClicked);
		HeaderRow->AddChild(CloseButton);
		if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(CloseButton->Slot))
			Slot->SetVerticalAlignment(VAlign_Center);

		UTextBlock* CloseLabel = MakeDlgText(WidgetTree, FName("DlgCloseLabel"),
			TEXT(" X "), FLinearColor::White, TEXT("Bold"), 11);
		CloseButton->AddChild(CloseLabel);
	}

	// --- Scrollable dialogue text area ---
	TextScroll = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(), FName("DlgTextScroll"));
	MainVBox->AddChild(TextScroll);
	if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(TextScroll->Slot))
	{
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Slot->SetPadding(FMargin(16, 4, 16, 6));
	}

	// Dialogue text (wraps automatically)
	DialogueText = MakeDlgText(WidgetTree, FName("DlgText"),
		TEXT("..."), FLinearColor(0.85f, 0.85f, 0.85f), TEXT("Regular"), 12);
	DialogueText->SetAutoWrapText(true);
	TextScroll->AddChild(DialogueText);

	// --- Action buttons row ---
	ActionBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), FName("DlgActionBox"));
	MainVBox->AddChild(ActionBox);
	if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(ActionBox->Slot))
	{
		Slot->SetHorizontalAlignment(HAlign_Center);
		Slot->SetPadding(FMargin(16, 2, 16, 4));
	}
	ActionBox->SetVisibility(ESlateVisibility::Collapsed);

	// --- Bottom hint ---
	{
		HintText = MakeDlgText(WidgetTree, FName("DlgHint"),
			TEXT("Press E or click outside to close"),
			FLinearColor(0.4f, 0.4f, 0.4f), TEXT("Regular"), 9);
		HintText->SetJustification(ETextJustify::Center);
		MainVBox->AddChild(HintText);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(HintText->Slot))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetPadding(FMargin(0, 0, 0, 6));
		}
	}
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

void UHJDialogueWidget::ShowDialogue(const FString& InNPCName, const FString& InFaction)
{
	bIsOpen = true;
	bIsThinking = true;
	bTypewriterActive = false;
	bIsStreaming = false;
	ThinkingTimer = 0.f;
	ThinkingDotCount = 0;
	TypewriterIndex = 0;
	TypewriterAccum = 0.f;
	FullResponseText.Empty();
	PendingActions.Empty();
	CurrentNPCName = InNPCName;
	CurrentFaction = InFaction;

	// Faction-colored NPC name
	FLinearColor NameColor = GetFactionColor(InFaction);

	if (NPCNameText)
	{
		NPCNameText->SetText(FText::FromString(InNPCName));
		NPCNameText->SetColorAndOpacity(FSlateColor(NameColor));
	}

	if (FactionText)
		FactionText->SetText(FText::FromString(
			FString::Printf(TEXT("[%s]"), *InFaction)));

	// Update portrait symbol based on faction/specialist
	if (PortraitSymbol)
	{
		FString Symbol = TEXT("*");
		if (InFaction.Contains(TEXT("FERRUM")))      Symbol = TEXT("\x2692");  // Hammer and pick
		else if (InFaction.Contains(TEXT("AUREATE"))) Symbol = TEXT("\x270E");  // Pen
		else if (InFaction.Contains(TEXT("OBSIDIAN")))Symbol = TEXT("\x2697");  // Alembic
		else if (InFaction.Contains(TEXT("ARGENTUM")))Symbol = TEXT("\x2694");  // Swords
		else if (InFaction.Contains(TEXT("VOID")))    Symbol = TEXT("\x2609");  // Sun
		else if (InFaction.Contains(TEXT("STEWARD"))) Symbol = TEXT("\x2696");  // Scale
		PortraitSymbol->SetText(FText::FromString(Symbol));
		PortraitSymbol->SetColorAndOpacity(FSlateColor(NameColor));
	}

	if (PortraitBorder)
	{
		FLinearColor BorderColor = FLinearColor::LerpUsingHSV(
			FLinearColor(0.15f, 0.12f, 0.08f), NameColor, 0.3f);
		PortraitBorder->SetBrushColor(BorderColor);
	}

	// Set specialist title
	if (TitleText)
	{
		FString Title;
		if (InFaction.Contains(TEXT("FERRUM")))       Title = TEXT("Master of the Forge");
		else if (InFaction.Contains(TEXT("AUREATE"))) Title = TEXT("Keeper of Records");
		else if (InFaction.Contains(TEXT("OBSIDIAN")))Title = TEXT("Adept of Transmutation");
		else if (InFaction.Contains(TEXT("ARGENTUM")))Title = TEXT("Commander of Arms");
		else if (InFaction.Contains(TEXT("VOID")))    Title = TEXT("Warden of the Reaches");
		else if (InFaction.Contains(TEXT("STEWARD"))) Title = TEXT("Steward of the Empire");
		else                                          Title = TEXT("Council Member");
		TitleText->SetText(FText::FromString(Title));
	}

	if (DialogueText)
	{
		DialogueText->SetText(FText::FromString(TEXT("Thinking.")));
		DialogueText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)));
	}

	ClearActionButtons();

	FadeAlpha = 0.f;
	FadeDir = 1;
	SetRenderOpacity(0.f);
	SetVisibility(ESlateVisibility::Visible);
	OnDialogueShown();
}

void UHJDialogueWidget::SetResponse(const FString& ResponseText)
{
	bIsThinking = false;
	bIsStreaming = false;
	FullResponseText = ResponseText;
	TypewriterIndex = 0;
	TypewriterAccum = 0.f;
	bTypewriterActive = true;
	PendingActions.Empty();

	if (DialogueText)
	{
		DialogueText->SetText(FText::FromString(TEXT("")));
		DialogueText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f)));
	}
}

void UHJDialogueWidget::SetResponseWithActions(const FString& ResponseText,
	const TArray<FString>& Actions)
{
	bIsThinking = false;
	bIsStreaming = false;
	FullResponseText = ResponseText;
	TypewriterIndex = 0;
	TypewriterAccum = 0.f;
	bTypewriterActive = true;
	PendingActions = Actions;

	if (DialogueText)
	{
		DialogueText->SetText(FText::FromString(TEXT("")));
		DialogueText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f)));
	}
}

void UHJDialogueWidget::AppendToken(const FString& Token)
{
	bIsThinking = false;

	if (!bIsStreaming)
	{
		// First token — start streaming mode
		bIsStreaming = true;
		FullResponseText.Empty();
		TypewriterIndex = 0;
		TypewriterAccum = 0.f;
		bTypewriterActive = true;

		if (DialogueText)
		{
			DialogueText->SetText(FText::FromString(TEXT("")));
			DialogueText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f)));
		}
	}

	FullResponseText += Token;
}

void UHJDialogueWidget::FinalizeResponse(const TArray<FString>& Actions)
{
	bIsStreaming = false;
	PendingActions = Actions;

	// If typewriter is far behind, let it catch up naturally.
	// Actions will appear when typewriter finishes.
}

void UHJDialogueWidget::ShowError(const FString& ErrorText)
{
	bIsThinking = false;
	bTypewriterActive = false;
	bIsStreaming = false;

	FString ThemedMsg = GetThemedError(ErrorText);

	if (DialogueText)
	{
		DialogueText->SetText(FText::FromString(ThemedMsg));
		DialogueText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.3f, 0.3f)));
	}

	// Restore NPC name to faction color
	if (NPCNameText)
		NPCNameText->SetColorAndOpacity(FSlateColor(GetFactionColor(CurrentFaction)));
}

void UHJDialogueWidget::HideDialogue()
{
	bIsOpen = false;
	bIsThinking = false;
	bTypewriterActive = false;
	bIsStreaming = false;
	FadeDir = -1;  // NativeTick will collapse when alpha reaches 0
	ClearActionButtons();
	OnDialogueHidden();
}

void UHJDialogueWidget::OnCloseClicked()
{
	HideDialogue();
}

void UHJDialogueWidget::OnDimmerClicked()
{
	HideDialogue();
}

// -----------------------------------------------------------------------------
// Action buttons
// -----------------------------------------------------------------------------

void UHJDialogueWidget::PopulateActionButtons(const TArray<FString>& Actions)
{
	ClearActionButtons();
	if (!ActionBox || Actions.Num() == 0) return;

	for (int32 i = 0; i < Actions.Num() && i < 6; ++i)
	{
		const FString& ActionKey = Actions[i];

		// Format display label: "assign_task" -> "Assign Task"
		FString Label = ActionKey;
		Label.ReplaceInline(TEXT("_"), TEXT(" "));
		// Title case: capitalize first letter of each word
		bool bCapNext = true;
		for (int32 c = 0; c < Label.Len(); ++c)
		{
			if (bCapNext && FChar::IsAlpha(Label[c]))
			{
				Label[c] = FChar::ToUpper(Label[c]);
				bCapNext = false;
			}
			else if (Label[c] == ' ')
			{
				bCapNext = true;
			}
		}

		FName BtnName(*FString::Printf(TEXT("DlgActionBtn_%d"), i));
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), BtnName);

		// Style: faction-tinted button
		FLinearColor FactionCol = GetFactionColor(CurrentFaction);
		FLinearColor BtnCol = FLinearColor::LerpUsingHSV(
			FLinearColor(0.12f, 0.12f, 0.18f), FactionCol, 0.3f);
		Btn->SetBackgroundColor(BtnCol);
		Btn->OnClicked.AddDynamic(this, &UHJDialogueWidget::OnActionButtonClicked);

		// Label text
		FName LblName(*FString::Printf(TEXT("DlgActionLbl_%d"), i));
		UTextBlock* BtnLabel = MakeDlgText(WidgetTree, LblName,
			Label, FLinearColor(0.9f, 0.9f, 0.9f), TEXT("Bold"), 10);
		Btn->AddChild(BtnLabel);

		ActionBox->AddChild(Btn);
		if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(Btn->Slot))
		{
			Slot->SetPadding(FMargin(4, 0, 4, 0));
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		ActionButtons.Add(Btn);
	}

	ActionBox->SetVisibility(ESlateVisibility::Visible);
}

void UHJDialogueWidget::ClearActionButtons()
{
	if (ActionBox)
	{
		ActionBox->ClearChildren();
		ActionBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	ActionButtons.Empty();
}

void UHJDialogueWidget::OnActionButtonClicked()
{
	// Find which button was clicked by checking IsHovered state
	for (int32 i = 0; i < ActionButtons.Num(); ++i)
	{
		if (ActionButtons[i] && ActionButtons[i]->IsHovered())
		{
			if (PendingActions.IsValidIndex(i))
			{
				OnActionSelected.Broadcast(PendingActions[i]);
			}
			break;
		}
	}
}

// -----------------------------------------------------------------------------
// Tick — typewriter reveal + thinking animation + fade
// -----------------------------------------------------------------------------

void UHJDialogueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Fade transition
	if (FadeDir != 0)
	{
		float Speed = FadeDir > 0 ? (1.f / 0.2f) : (1.f / 0.15f);
		FadeAlpha = FMath::Clamp(FadeAlpha + FadeDir * Speed * InDeltaTime, 0.f, 1.f);
		SetRenderOpacity(FadeAlpha);

		if (FadeDir < 0 && FadeAlpha <= 0.f)
		{
			FadeDir = 0;
			SetVisibility(ESlateVisibility::Collapsed);
		}
		else if (FadeDir > 0 && FadeAlpha >= 1.f)
		{
			FadeDir = 0;
		}
	}

	// --- Typewriter reveal ---
	if (bTypewriterActive && DialogueText && FullResponseText.Len() > 0)
	{
		TypewriterAccum += TypewriterSpeed * InDeltaTime;
		int32 CharsToReveal = static_cast<int32>(TypewriterAccum);
		if (CharsToReveal > 0)
		{
			TypewriterAccum -= static_cast<float>(CharsToReveal);
			TypewriterIndex = FMath::Min(TypewriterIndex + CharsToReveal, FullResponseText.Len());

			FString Visible = FullResponseText.Left(TypewriterIndex);
			DialogueText->SetText(FText::FromString(Visible));

			// Auto-scroll to bottom as text appears
			if (TextScroll)
				TextScroll->ScrollToEnd();
		}

		// Check if typewriter is complete
		if (TypewriterIndex >= FullResponseText.Len() && !bIsStreaming)
		{
			bTypewriterActive = false;

			// Restore NPC name to faction color (stop pulsing)
			if (NPCNameText)
				NPCNameText->SetColorAndOpacity(FSlateColor(GetFactionColor(CurrentFaction)));

			// Show action buttons if any
			if (PendingActions.Num() > 0)
				PopulateActionButtons(PendingActions);
		}
	}

	// --- Thinking animation ---
	if (!bIsThinking) return;

	ThinkingTimer += InDeltaTime;

	// Cycle ellipsis every 0.4 seconds: "Thinking." -> ".." -> "..."
	float DotCycleTime = 0.4f;
	int32 NewDotCount = (static_cast<int32>(ThinkingTimer / DotCycleTime) % 3) + 1;

	if (NewDotCount != ThinkingDotCount && DialogueText)
	{
		ThinkingDotCount = NewDotCount;
		FString Dots;
		for (int32 i = 0; i < ThinkingDotCount; ++i) Dots += TEXT(".");
		DialogueText->SetText(FText::FromString(
			FString::Printf(TEXT("Thinking%s"), *Dots)));
	}

	// Golden glow pulse on NPC name using sine wave (period ~1.5s)
	if (NPCNameText)
	{
		FLinearColor BaseColor = GetFactionColor(CurrentFaction);
		float Pulse = 0.5f + 0.5f * FMath::Sin(ThinkingTimer * 4.2f);
		float R = FMath::Lerp(BaseColor.R * 0.6f, BaseColor.R, Pulse);
		float G = FMath::Lerp(BaseColor.G * 0.6f, BaseColor.G, Pulse);
		float B = FMath::Lerp(BaseColor.B * 0.6f, BaseColor.B, Pulse);
		NPCNameText->SetColorAndOpacity(FSlateColor(FLinearColor(R, G, B)));
	}
}

// -----------------------------------------------------------------------------
// BlueprintNativeEvent defaults
// -----------------------------------------------------------------------------

void UHJDialogueWidget::OnDialogueShown_Implementation() {}

void UHJDialogueWidget::OnDialogueHidden_Implementation()
{
	// Return to game-only input when dialogue closes
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
	}
}
