#include "HJLoadingWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Spacer.h"
#include "Styling/CoreStyle.h"

UHJLoadingWidget* UHJLoadingWidget::Instance = nullptr;

// -------------------------------------------------------
// Programmatic layout
// -------------------------------------------------------

void UHJLoadingWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!WidgetTree) return;

    // If a Blueprint child already has a layout, don't overwrite it
    UPanelWidget* ExistingRoot = Cast<UPanelWidget>(WidgetTree->RootWidget);
    if (ExistingRoot && ExistingRoot->GetChildrenCount() > 0) return;

    BuildProgrammaticLayout();
}

void UHJLoadingWidget::BuildProgrammaticLayout()
{
    // Root: Overlay for layered full-screen content
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(
        UOverlay::StaticClass(), FName("Root"));
    WidgetTree->RootWidget = Root;

    // Dark parchment background (Tartarian themed)
    DarkOverlay = WidgetTree->ConstructWidget<UBorder>(
        UBorder::StaticClass(), FName("DarkOverlay"));
    DarkOverlay->SetBrushColor(FLinearColor(0.08f, 0.06f, 0.04f, 0.92f));
    Root->AddChild(DarkOverlay);

    // --- Centered VBox for loading text + progress bar + stage text ---
    UVerticalBox* CenterVBox = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), FName("CenterVBox"));
    Root->AddChild(CenterVBox);

    UOverlaySlot* VBoxSlot = Cast<UOverlaySlot>(CenterVBox->Slot);
    if (VBoxSlot)
    {
        VBoxSlot->SetHorizontalAlignment(HAlign_Center);
        VBoxSlot->SetVerticalAlignment(VAlign_Center);
    }

    // Centered loading text
    LoadingText = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), FName("LoadingText"));
    LoadingText->SetText(FText::FromString(TEXT("Loading...")));
    LoadingText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.9f, 0.8f)));
    FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 24);
    LoadingText->SetFont(Font);
    LoadingText->SetJustification(ETextJustify::Center);
    CenterVBox->AddChild(LoadingText);

    // --- Spacer 12px ---
    USpacer* Sp1 = WidgetTree->ConstructWidget<USpacer>(
        USpacer::StaticClass(), FName("ProgressSpacer"));
    Sp1->SetSize(FVector2D(0.f, 12.f));
    CenterVBox->AddChild(Sp1);

    // --- Pipeline progress bar (Fix 1.2) ---
    USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(
        USizeBox::StaticClass(), FName("PipelineBarBox"));
    BarBox->SetWidthOverride(300);
    BarBox->SetHeightOverride(20);
    CenterVBox->AddChild(BarBox);

    PipelineProgressBar = WidgetTree->ConstructWidget<UProgressBar>(
        UProgressBar::StaticClass(), FName("PipelineProgressBar"));
    PipelineProgressBar->SetPercent(0.0f);
    PipelineProgressBar->SetFillColorAndOpacity(FLinearColor(0.78f, 0.63f, 0.30f));  // Gold
    PipelineProgressBar->SetVisibility(ESlateVisibility::Collapsed);
    BarBox->AddChild(PipelineProgressBar);

    // --- Spacer 6px ---
    USpacer* Sp2 = WidgetTree->ConstructWidget<USpacer>(
        USpacer::StaticClass(), FName("StageSpacer"));
    Sp2->SetSize(FVector2D(0.f, 6.f));
    CenterVBox->AddChild(Sp2);

    // --- Stage text (Fix 1.2) ---
    StageText = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), FName("StageText"));
    StageText->SetText(FText::GetEmpty());
    StageText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.7f, 1.0f)));
    FSlateFontInfo StageFont = FCoreStyle::GetDefaultFontStyle("Regular", 14);
    StageText->SetFont(StageFont);
    StageText->SetJustification(ETextJustify::Center);
    StageText->SetVisibility(ESlateVisibility::Collapsed);
    CenterVBox->AddChild(StageText);
}

// -------------------------------------------------------
// Lifecycle
// -------------------------------------------------------

void UHJLoadingWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Instance = this;
}

void UHJLoadingWidget::NativeDestruct()
{
    if (Instance == this) Instance = nullptr;
    Super::NativeDestruct();
}

// Themed loading messages for Tartarian atmosphere
static const TCHAR* LOADING_MESSAGES[] = {
    TEXT("Unsealing the chronicle..."),
    TEXT("Consulting the Akashic Record..."),
    TEXT("Awakening the resonance field..."),
    TEXT("Reconstructing the temporal lattice..."),
    TEXT("The scribe reads your scrolls..."),
};
static constexpr int32 NUM_LOADING_MESSAGES = 5;

void UHJLoadingWidget::ShowLoading(const FString& Message)
{
    // If generic "Loading...", use a themed message instead
    FString DisplayMsg = Message;
    if (Message == TEXT("Loading...") || Message.IsEmpty())
    {
        int32 Idx = FMath::RandRange(0, NUM_LOADING_MESSAGES - 1);
        DisplayMsg = LOADING_MESSAGES[Idx];
    }

    LoadingMessage = DisplayMsg;
    bIsLoading     = true;
    SetVisibility(ESlateVisibility::Visible);
    OnLoadingStateChanged(true, DisplayMsg);
}

void UHJLoadingWidget::HideLoading()
{
    bIsLoading = false;
    HidePipelineProgress();
    SetVisibility(ESlateVisibility::Collapsed);
    OnLoadingStateChanged(false, TEXT(""));
}

// static
void UHJLoadingWidget::Show(const UObject* /*WorldContext*/, const FString& Message)
{
    if (Instance) Instance->ShowLoading(Message);
}

// static
void UHJLoadingWidget::Hide(const UObject* /*WorldContext*/)
{
    if (Instance) Instance->HideLoading();
}

// -------------------------------------------------------
// Pipeline progress (Fix 1.2)
// -------------------------------------------------------

void UHJLoadingWidget::ShowPipelineProgress(const FString& StageName, int32 StageIndex, int32 TotalStages)
{
    bIsLoading = true;
    SetVisibility(ESlateVisibility::Visible);
    UpdatePipelineProgress(StageName, StageIndex, TotalStages);
}

void UHJLoadingWidget::UpdatePipelineProgress(const FString& StageName, int32 StageIndex, int32 TotalStages)
{
    CurrentStageName = StageName;

    if (TotalStages > 0)
    {
        PipelineProgressPct = FMath::Clamp(
            static_cast<float>(StageIndex + 1) / static_cast<float>(TotalStages),
            0.0f, 1.0f);
    }
    else
    {
        PipelineProgressPct = 0.0f;
    }

    if (PipelineProgressBar)
    {
        PipelineProgressBar->SetPercent(PipelineProgressPct);
        PipelineProgressBar->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }

    if (StageText)
    {
        FString StageStr = FString::Printf(
            TEXT("Stage %d/%d: %s"), StageIndex + 1, TotalStages, *StageName);
        StageText->SetText(FText::FromString(StageStr));
        StageText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }

    if (LoadingText)
    {
        LoadingText->SetText(FText::FromString(TEXT("Pipeline Running...")));
    }
}

void UHJLoadingWidget::HidePipelineProgress()
{
    PipelineProgressPct = 0.0f;
    CurrentStageName.Empty();

    if (PipelineProgressBar)
    {
        PipelineProgressBar->SetPercent(0.0f);
        PipelineProgressBar->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (StageText)
    {
        StageText->SetText(FText::GetEmpty());
        StageText->SetVisibility(ESlateVisibility::Collapsed);
    }
}

// -------------------------------------------------------
// BlueprintNativeEvent default implementation
// -------------------------------------------------------

void UHJLoadingWidget::OnLoadingStateChanged_Implementation(bool bLoading, const FString& Message)
{
    if (!LoadingText) return;

    if (bLoading)
    {
        LoadingText->SetText(FText::FromString(Message));
    }
    else
    {
        LoadingText->SetText(FText::GetEmpty());
    }
}
