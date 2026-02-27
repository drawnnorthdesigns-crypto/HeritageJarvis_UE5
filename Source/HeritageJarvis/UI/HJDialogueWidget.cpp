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

	// Dimmer
	UBorder* Dimmer = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), FName("DlgDimmer"));
	Dimmer->SetBrushColor(FLinearColor(0, 0, 0, 0.3f));
	UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dimmer);
	if (DimSlot) { DimSlot->SetAnchors(FAnchors(0, 0, 1, 1)); DimSlot->SetOffsets(FMargin(0)); }

	// Panel — bottom-center, 550x200
	PanelBg = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), FName("DlgPanelBg"));
	PanelBg->SetBrushColor(FLinearColor(0.06f, 0.06f, 0.10f, 0.92f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(PanelBg);
	if (PanelSlot)
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		PanelSlot->SetOffsets(FMargin(0, -100, 550, 200));
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

		// NPC name (bright)
		NPCNameText = MakeDlgText(WidgetTree, FName("DlgNPCName"),
			TEXT("NPC"), FLinearColor(0.9f, 0.85f, 0.6f), TEXT("Bold"), 14);
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
		Slot->SetPadding(FMargin(16, 4, 16, 8));
	}

	// Dialogue text (wraps automatically)
	DialogueText = MakeDlgText(WidgetTree, FName("DlgText"),
		TEXT("..."), FLinearColor(0.85f, 0.85f, 0.85f), TEXT("Regular"), 12);
	DialogueText->SetAutoWrapText(true);
	TextScroll->AddChild(DialogueText);

	// --- Bottom hint ---
	{
		UTextBlock* Hint = MakeDlgText(WidgetTree, FName("DlgHint"),
			TEXT("Press E or click X to close"), FLinearColor(0.4f, 0.4f, 0.4f), TEXT("Regular"), 9);
		Hint->SetJustification(ETextJustify::Center);
		MainVBox->AddChild(Hint);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Hint->Slot))
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
	CurrentNPCName = InNPCName;
	CurrentFaction = InFaction;

	if (NPCNameText)
		NPCNameText->SetText(FText::FromString(InNPCName));

	if (FactionText)
		FactionText->SetText(FText::FromString(
			FString::Printf(TEXT("[%s]"), *InFaction)));

	if (DialogueText)
	{
		DialogueText->SetText(FText::FromString(TEXT("Thinking...")));
		DialogueText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)));
	}

	SetVisibility(ESlateVisibility::Visible);
	OnDialogueShown();
}

void UHJDialogueWidget::SetResponse(const FString& ResponseText)
{
	if (DialogueText)
	{
		DialogueText->SetText(FText::FromString(ResponseText));
		DialogueText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f)));
	}
}

void UHJDialogueWidget::ShowError(const FString& ErrorText)
{
	if (DialogueText)
	{
		DialogueText->SetText(FText::FromString(
			FString::Printf(TEXT("[Error: %s]"), *ErrorText)));
		DialogueText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.3f, 0.3f)));
	}
}

void UHJDialogueWidget::HideDialogue()
{
	bIsOpen = false;
	SetVisibility(ESlateVisibility::Collapsed);
	OnDialogueHidden();
}

void UHJDialogueWidget::OnCloseClicked()
{
	HideDialogue();
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
