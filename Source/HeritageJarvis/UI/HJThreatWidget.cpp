#include "HJThreatWidget.h"
#include "HJNotificationWidget.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"

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
#include "Components/Spacer.h"
#include "Components/SizeBox.h"
#include "Styling/CoreStyle.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

// -----------------------------------------------------------------------------
void UHJThreatWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree) return;

	UPanelWidget* ExistingRoot = Cast<UPanelWidget>(WidgetTree->RootWidget);
	if (ExistingRoot && ExistingRoot->GetChildrenCount() > 0) return;

	BuildProgrammaticLayout();

	// Start hidden
	SetVisibility(ESlateVisibility::Collapsed);
}

// -----------------------------------------------------------------------------
void UHJThreatWidget::BuildProgrammaticLayout()
{
	// Root canvas (fullscreen overlay)
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), FName("ThreatRoot"));
	WidgetTree->RootWidget = Root;

	// Semi-transparent fullscreen dimmer
	UBorder* Dimmer = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), FName("Dimmer"));
	Dimmer->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));
	UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dimmer);
	if (DimSlot)
	{
		DimSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		DimSlot->SetOffsets(FMargin(0, 0, 0, 0));
	}

	// Modal panel — centered, 420x280
	ModalBg = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), FName("ModalBg"));
	ModalBg->SetBrushColor(FLinearColor(0.08f, 0.06f, 0.12f, 0.95f));
	UCanvasPanelSlot* ModalSlot = Root->AddChildToCanvas(ModalBg);
	if (ModalSlot)
	{
		ModalSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		ModalSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ModalSlot->SetOffsets(FMargin(0, 0, 420, 280));
	}

	// Vertical layout inside modal
	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), FName("ModalVBox"));
	ModalBg->AddChild(VBox);

	// --- Title: "THREAT DETECTED" (red, bold 16) ---
	{
		USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpTop"));
		Sp->SetSize(FVector2D(0, 16));
		VBox->AddChild(Sp);
	}

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("THREAT DETECTED")));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.2f, 0.2f)));
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 16);
		TitleText->SetFont(Font);
	}
	TitleText->SetJustification(ETextJustify::Center);
	VBox->AddChild(TitleText);
	if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(TitleText->Slot))
		Slot->SetHorizontalAlignment(HAlign_Center);

	// --- Spacer ---
	{
		USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpTitle"));
		Sp->SetSize(FVector2D(0, 12));
		VBox->AddChild(Sp);
	}

	// --- Enemy name (amber, bold 14) ---
	EnemyNameText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName("EnemyNameText"));
	EnemyNameText->SetText(FText::FromString(TEXT("Unknown Enemy")));
	EnemyNameText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.7f, 0.2f)));
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 14);
		EnemyNameText->SetFont(Font);
	}
	EnemyNameText->SetJustification(ETextJustify::Center);
	VBox->AddChild(EnemyNameText);
	if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(EnemyNameText->Slot))
		Slot->SetHorizontalAlignment(HAlign_Center);

	// --- Enemy power (white, regular 11) ---
	EnemyPowerText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName("EnemyPowerText"));
	EnemyPowerText->SetText(FText::FromString(TEXT("Power: 0")));
	EnemyPowerText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f)));
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 11);
		EnemyPowerText->SetFont(Font);
	}
	EnemyPowerText->SetJustification(ETextJustify::Center);
	VBox->AddChild(EnemyPowerText);
	if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(EnemyPowerText->Slot))
		Slot->SetHorizontalAlignment(HAlign_Center);

	// --- Message (gray, regular 10) ---
	{
		USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpMsg"));
		Sp->SetSize(FVector2D(0, 8));
		VBox->AddChild(Sp);
	}

	MessageText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName("MessageText"));
	MessageText->SetText(FText::GetEmpty());
	MessageText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)));
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 10);
		MessageText->SetFont(Font);
	}
	MessageText->SetJustification(ETextJustify::Center);
	MessageText->SetAutoWrapText(true);
	VBox->AddChild(MessageText);
	if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(MessageText->Slot))
	{
		Slot->SetHorizontalAlignment(HAlign_Center);
		Slot->SetPadding(FMargin(20, 0, 20, 0));
	}

	// --- Result text (hidden until resolve, center) ---
	ResultText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName("ResultText"));
	ResultText->SetText(FText::GetEmpty());
	ResultText->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.9f, 0.3f)));
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 13);
		ResultText->SetFont(Font);
	}
	ResultText->SetJustification(ETextJustify::Center);
	ResultText->SetVisibility(ESlateVisibility::Collapsed);
	VBox->AddChild(ResultText);
	if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(ResultText->Slot))
		Slot->SetHorizontalAlignment(HAlign_Center);

	// --- Fill spacer ---
	{
		USpacer* SpFill = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpFill"));
		SpFill->SetSize(FVector2D(0, 0));
		VBox->AddChild(SpFill);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(SpFill->Slot))
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// --- Button row ---
	UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), FName("ButtonRow"));
	VBox->AddChild(ButtonRow);
	if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(ButtonRow->Slot))
	{
		Slot->SetHorizontalAlignment(HAlign_Center);
		Slot->SetPadding(FMargin(0, 0, 0, 20));
	}

	// Fight button
	{
		USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpBtnL"));
		Sp->SetSize(FVector2D(30, 0));
		ButtonRow->AddChild(Sp);
	}

	FightButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), FName("FightButton"));
	FightButton->SetBackgroundColor(FLinearColor(0.7f, 0.15f, 0.15f));
	FightButton->OnClicked.AddDynamic(this, &UHJThreatWidget::OnFightClicked);
	ButtonRow->AddChild(FightButton);

	FightLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName("FightLabel"));
	FightLabel->SetText(FText::FromString(TEXT("  FIGHT  ")));
	FightLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 13);
		FightLabel->SetFont(Font);
	}
	FightButton->AddChild(FightLabel);

	// Spacer between buttons
	{
		USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpBtnMid"));
		Sp->SetSize(FVector2D(30, 0));
		ButtonRow->AddChild(Sp);
	}

	// Evade button
	EvadeButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), FName("EvadeButton"));
	EvadeButton->SetBackgroundColor(FLinearColor(0.2f, 0.4f, 0.7f));
	EvadeButton->OnClicked.AddDynamic(this, &UHJThreatWidget::OnEvadeClicked);
	ButtonRow->AddChild(EvadeButton);

	EvadeLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName("EvadeLabel"));
	EvadeLabel->SetText(FText::FromString(TEXT("  EVADE  ")));
	EvadeLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 13);
		EvadeLabel->SetFont(Font);
	}
	EvadeButton->AddChild(EvadeLabel);

	{
		USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpBtnR"));
		Sp->SetSize(FVector2D(30, 0));
		ButtonRow->AddChild(Sp);
	}
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

void UHJThreatWidget::ShowEncounter(const FTartariaThreatInfo& Threat)
{
	ActiveThreat = Threat;
	bResolving = false;

	// Format enemy name: replace underscores with spaces
	FString DisplayName = Threat.EnemyKey.Replace(TEXT("_"), TEXT(" "));

	if (EnemyNameText) EnemyNameText->SetText(FText::FromString(DisplayName));
	if (EnemyPowerText) EnemyPowerText->SetText(FText::FromString(
		FString::Printf(TEXT("Power: %d  (Your defense: %d)"),
			Threat.EnemyPower, Threat.EffectivePower)));
	if (MessageText) MessageText->SetText(FText::FromString(Threat.Message));

	// Reset result
	if (ResultText)
	{
		ResultText->SetText(FText::GetEmpty());
		ResultText->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Enable buttons
	if (FightButton) FightButton->SetIsEnabled(true);
	if (EvadeButton) EvadeButton->SetIsEnabled(true);

	FadeAlpha = 0.f;
	FadeDir = 1;
	SetRenderOpacity(0.f);
	SetVisibility(ESlateVisibility::Visible);
	OnEncounterShown();
}

void UHJThreatWidget::HideEncounter()
{
	FadeDir = -1;  // NativeTick will collapse when alpha reaches 0
	OnEncounterHidden();
}

// -----------------------------------------------------------------------------
// Fade transition
// -----------------------------------------------------------------------------

void UHJThreatWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

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
}

// -----------------------------------------------------------------------------
// BlueprintNativeEvent defaults
// -----------------------------------------------------------------------------

void UHJThreatWidget::OnEncounterShown_Implementation() {}
void UHJThreatWidget::OnEncounterHidden_Implementation() {}
void UHJThreatWidget::OnResultShown_Implementation(const FTartariaEncounterResult& Result) {}

// -----------------------------------------------------------------------------
// Button handlers
// -----------------------------------------------------------------------------

void UHJThreatWidget::OnFightClicked()
{
	if (bResolving) return;
	ResolveEncounter(TEXT("fight"));
}

void UHJThreatWidget::OnEvadeClicked()
{
	if (bResolving) return;
	ResolveEncounter(TEXT("evade"));
}

void UHJThreatWidget::ResolveEncounter(const FString& Action)
{
	bResolving = true;

	// Disable buttons
	if (FightButton) FightButton->SetIsEnabled(false);
	if (EvadeButton) EvadeButton->SetIsEnabled(false);

	if (Action == TEXT("evade"))
	{
		// Evade always succeeds — take minor damage, close
		FTartariaEncounterResult Result;
		Result.bVictory = false;
		Result.DamageTaken = FMath::Max(1, ActiveThreat.EnemyPower / 5);  // 20% of power as flee damage

		if (ResultText)
		{
			ResultText->SetText(FText::FromString(
				FString::Printf(TEXT("Escaped! (-%d HP)"), Result.DamageTaken)));
			ResultText->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.7f, 1.0f)));
			ResultText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}

		OnResultShown(Result);
		OnEncounterResolved.Broadcast(Result);

		// Auto-hide after 2 seconds
		FTimerHandle Handle;
		GetWorld()->GetTimerManager().SetTimer(Handle, this,
			&UHJThreatWidget::HideEncounter, 2.0f, false);
		return;
	}

	// Fight — call /api/game/threat/resolve
	UHJGameInstance* GI = UHJGameInstance::Get(GetWorld());
	if (!GI || !GI->ApiClient)
	{
		bResolving = false;
		return;
	}

	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject());
	Body->SetStringField(TEXT("enemy_key"), ActiveThreat.EnemyKey);
	Body->SetNumberField(TEXT("player_power"), ActiveThreat.EffectivePower);
	Body->SetNumberField(TEXT("player_defense"), ActiveThreat.EffectivePower);

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	FOnApiResponse CB;
	CB.BindUObject(this, &UHJThreatWidget::OnResolveResponse);
	GI->ApiClient->Post(TEXT("/api/game/threat/resolve"), BodyStr, CB);
}

void UHJThreatWidget::OnResolveResponse(bool bSuccess, const FString& Body)
{
	FTartariaEncounterResult Result;

	if (bSuccess)
	{
		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			Root->TryGetBoolField(TEXT("victory"), Result.bVictory);

			double Cr = 0;
			Root->TryGetNumberField(TEXT("reward_credits"), Cr);
			Result.RewardCredits = static_cast<int32>(Cr);

			double Rep = 0;
			Root->TryGetNumberField(TEXT("faction_rep"), Rep);
			Result.FactionRep = static_cast<float>(Rep);
		}
	}

	// Calculate damage: victory = 10-30% of enemy power, defeat = 50-80%
	if (Result.bVictory)
	{
		Result.DamageTaken = FMath::Max(1, ActiveThreat.EnemyPower / 5);
	}
	else
	{
		Result.DamageTaken = FMath::Max(5, ActiveThreat.EnemyPower * 3 / 5);
	}

	// Show result
	if (ResultText)
	{
		if (Result.bVictory)
		{
			ResultText->SetText(FText::FromString(
				FString::Printf(TEXT("VICTORY! +%d cr  (-%d HP)"),
					Result.RewardCredits, Result.DamageTaken)));
			ResultText->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.9f, 0.3f)));
		}
		else
		{
			ResultText->SetText(FText::FromString(
				FString::Printf(TEXT("DEFEATED! -%d HP"), Result.DamageTaken)));
			ResultText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.2f, 0.2f)));
		}
		ResultText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	OnResultShown(Result);
	OnEncounterResolved.Broadcast(Result);

	// Auto-hide after 3 seconds
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, this,
		&UHJThreatWidget::HideEncounter, 3.0f, false);
}
