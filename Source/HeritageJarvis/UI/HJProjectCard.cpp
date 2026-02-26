#include "HJProjectCard.h"
#include "HJProjectsWidget.h"
#include "Blueprint/WidgetTree.h"

#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Spacer.h"
#include "Styling/CoreStyle.h"

// -------------------------------------------------------------------
// Init — builds the card layout and wires button callbacks
// -------------------------------------------------------------------

void UHJProjectCard::Init(const FString& Id, const FString& Name, const FString& Desc,
                           const FString& Status, UHJProjectsWidget* Owner)
{
	ProjectId   = Id;
	ProjectName = Name;
	OwnerWidget = Owner;

	if (!WidgetTree) return;

	// --- Card border (dark background) ---
	// Use WidgetTree->ConstructWidget for all children so UE5 manages ownership correctly.
	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), FName(*FString::Printf(TEXT("Card_%s"), *Id)));
	Card->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.16f, 1.f));
	Card->SetPadding(FMargin(10.f));
	WidgetTree->RootWidget = Card;

	UVerticalBox* CardVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), FName(*FString::Printf(TEXT("CardVBox_%s"), *Id)));
	Card->AddChild(CardVBox);

	// --- Top row: Name + Status ---
	UHorizontalBox* TopRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("TopRow_%s"), *Id)));
	CardVBox->AddChild(TopRow);

	// Project Name
	UTextBlock* NameLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("NameLabel_%s"), *Id)));
	NameLabel->SetText(FText::FromString(Name));
	NameLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Bold", 14);
	NameLabel->SetFont(NameFont);
	TopRow->AddChild(NameLabel);
	if (UHorizontalBoxSlot* NameSlot = Cast<UHorizontalBoxSlot>(NameLabel->Slot))
	{
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// Status
	UTextBlock* StatusLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("StatusLabel_%s"), *Id)));
	StatusLabel->SetText(FText::FromString(Status));
	FSlateFontInfo StatusFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
	StatusLabel->SetFont(StatusFont);

	FLinearColor StatusColor = FLinearColor(0.6f, 0.6f, 0.6f);
	if (Status == TEXT("Running"))       StatusColor = FLinearColor(0.2f, 0.8f, 0.4f);
	else if (Status == TEXT("Complete")) StatusColor = FLinearColor(0.4f, 0.7f, 1.f);
	else if (Status == TEXT("Error"))    StatusColor = FLinearColor(1.f, 0.3f, 0.3f);
	StatusLabel->SetColorAndOpacity(FSlateColor(StatusColor));
	TopRow->AddChild(StatusLabel);

	// Description (truncated)
	UTextBlock* DescLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("DescLabel_%s"), *Id)));
	FString DescTrunc = Desc.Left(120);
	if (Desc.Len() > 120) DescTrunc += TEXT("...");
	DescLabel->SetText(FText::FromString(DescTrunc));
	DescLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)));
	FSlateFontInfo DescFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);
	DescLabel->SetFont(DescFont);
	CardVBox->AddChild(DescLabel);

	// --- Spacer 4px ---
	USpacer* BtnSpacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), FName(*FString::Printf(TEXT("BtnSpacer_%s"), *Id)));
	BtnSpacer->SetSize(FVector2D(0.f, 4.f));
	CardVBox->AddChild(BtnSpacer);

	// --- Button row: Run | Stop | Delete ---
	UHorizontalBox* BtnRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("BtnRow_%s"), *Id)));
	CardVBox->AddChild(BtnRow);

	// Run button
	RunBtn = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), FName(*FString::Printf(TEXT("RunBtn_%s"), *Id)));
	RunBtn->SetBackgroundColor(FLinearColor(0.15f, 0.5f, 0.2f));
	UTextBlock* RunLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("RunLabel_%s"), *Id)));
	RunLabel->SetText(FText::FromString(TEXT("Run")));
	RunLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	RunBtn->AddChild(RunLabel);
	BtnRow->AddChild(RunBtn);
	RunBtn->OnClicked.AddDynamic(this, &UHJProjectCard::OnRunClicked);

	// Spacer between buttons
	USpacer* Sp1 = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), FName(*FString::Printf(TEXT("Sp1_%s"), *Id)));
	Sp1->SetSize(FVector2D(4.f, 0.f));
	BtnRow->AddChild(Sp1);

	// Stop button
	StopBtn = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), FName(*FString::Printf(TEXT("StopBtn_%s"), *Id)));
	StopBtn->SetBackgroundColor(FLinearColor(0.6f, 0.5f, 0.1f));
	UTextBlock* StopLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("StopLabel_%s"), *Id)));
	StopLabel->SetText(FText::FromString(TEXT("Stop")));
	StopLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	StopBtn->AddChild(StopLabel);
	BtnRow->AddChild(StopBtn);
	StopBtn->OnClicked.AddDynamic(this, &UHJProjectCard::OnStopClicked);

	// Spacer between buttons
	USpacer* Sp2 = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), FName(*FString::Printf(TEXT("Sp2_%s"), *Id)));
	Sp2->SetSize(FVector2D(4.f, 0.f));
	BtnRow->AddChild(Sp2);

	// Delete button
	DelBtn = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), FName(*FString::Printf(TEXT("DelBtn_%s"), *Id)));
	DelBtn->SetBackgroundColor(FLinearColor(0.7f, 0.15f, 0.15f));
	UTextBlock* DelLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("DelLabel_%s"), *Id)));
	DelLabel->SetText(FText::FromString(TEXT("Delete")));
	DelLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	DelBtn->AddChild(DelLabel);
	BtnRow->AddChild(DelBtn);
	DelBtn->OnClicked.AddDynamic(this, &UHJProjectCard::OnDeleteClicked);

	// --- Confirmation text (hidden by default) ---
	ConfirmText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("ConfirmText_%s"), *Id)));
	ConfirmText->SetText(FText::FromString(TEXT("Click Delete again to confirm")));
	ConfirmText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.6f, 0.2f)));
	FSlateFontInfo ConfirmFont = FCoreStyle::GetDefaultFontStyle("Italic", 10);
	ConfirmText->SetFont(ConfirmFont);
	ConfirmText->SetVisibility(ESlateVisibility::Collapsed);
	CardVBox->AddChild(ConfirmText);
}

// -------------------------------------------------------------------
// Button handlers — delegate to OwnerWidget with stored ProjectId
// -------------------------------------------------------------------

void UHJProjectCard::OnRunClicked()
{
	bDeleteConfirmPending = false;
	if (ConfirmText) ConfirmText->SetVisibility(ESlateVisibility::Collapsed);

	if (OwnerWidget.IsValid())
	{
		OwnerWidget->RunPipeline(ProjectId);
	}
}

void UHJProjectCard::OnStopClicked()
{
	bDeleteConfirmPending = false;
	if (ConfirmText) ConfirmText->SetVisibility(ESlateVisibility::Collapsed);

	if (OwnerWidget.IsValid())
	{
		OwnerWidget->StopPipeline(ProjectId);
	}
}

void UHJProjectCard::OnDeleteClicked()
{
	if (!bDeleteConfirmPending)
	{
		// First click: show confirmation
		bDeleteConfirmPending = true;
		if (ConfirmText) ConfirmText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return;
	}

	// Second click: actually delete
	bDeleteConfirmPending = false;
	if (ConfirmText) ConfirmText->SetVisibility(ESlateVisibility::Collapsed);

	if (OwnerWidget.IsValid())
	{
		OwnerWidget->DeleteProject(ProjectId);
	}
}
