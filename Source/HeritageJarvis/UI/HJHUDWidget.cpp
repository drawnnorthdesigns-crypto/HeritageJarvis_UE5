#include "HJHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Spacer.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Styling/CoreStyle.h"

// -----------------------------------------------------------------------------
// Programmatic layout
// -----------------------------------------------------------------------------

void UHJHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!WidgetTree) return;

    // Skip if a Blueprint child already placed content
    UPanelWidget* ExistingRoot = Cast<UPanelWidget>(WidgetTree->RootWidget);
    if (ExistingRoot && ExistingRoot->GetChildrenCount() > 0) return;

    BuildProgrammaticLayout();
}

void UHJHUDWidget::BuildProgrammaticLayout()
{
    // --- Root canvas for absolute positioning ---
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), FName("HUDRoot"));
    WidgetTree->RootWidget = Root;

    // --- Dark semi-transparent background border ---
    UBorder* BarBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName("HUDBarBg"));
    BarBg->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.08f, 0.85f));

    UCanvasPanelSlot* BarSlot = Root->AddChildToCanvas(BarBg);
    if (BarSlot)
    {
        // Full width at top, 40px tall
        BarSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 0.0f));
        BarSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 40.0f));
        BarSlot->SetAlignment(FVector2D(0.0f, 0.0f));
    }

    // --- Horizontal box inside the bar ---
    UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName("HUDHBox"));
    BarBg->AddChild(HBox);

    // --- Left spacer 12px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpLeft"));
        Sp->SetSize(FVector2D(12, 0));
        HBox->AddChild(Sp);
    }

    // --- Project name ---
    ProjectNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("ProjectNameText"));
    ProjectNameText->SetText(FText::FromString(TEXT("No Project")));
    ProjectNameText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f)));
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 13);
        ProjectNameText->SetFont(Font);
    }
    HBox->AddChild(ProjectNameText);
    // Vertically center the text within the 40px bar
    UHorizontalBoxSlot* ProjectSlot = Cast<UHorizontalBoxSlot>(ProjectNameText->Slot);
    if (ProjectSlot)
    {
        ProjectSlot->SetVerticalAlignment(VAlign_Center);
    }

    // --- Spacer 20px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpProject"));
        Sp->SetSize(FVector2D(20, 0));
        HBox->AddChild(Sp);
    }

    // --- Stage text ---
    StageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("StageText"));
    StageText->SetText(FText::GetEmpty());
    StageText->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.7f, 1.0f)));
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 11);
        StageText->SetFont(Font);
    }
    HBox->AddChild(StageText);
    UHorizontalBoxSlot* StageSlot = Cast<UHorizontalBoxSlot>(StageText->Slot);
    if (StageSlot)
    {
        StageSlot->SetVerticalAlignment(VAlign_Center);
    }

    // --- Spacer 12px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpStage"));
        Sp->SetSize(FVector2D(12, 0));
        HBox->AddChild(Sp);
    }

    // --- Progress bar in SizeBox (150 x 16) ---
    {
        USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName("ProgressBarBox"));
        BarBox->SetWidthOverride(150);
        BarBox->SetHeightOverride(16);
        HBox->AddChild(BarBox);
        UHorizontalBoxSlot* BarBoxSlot = Cast<UHorizontalBoxSlot>(BarBox->Slot);
        if (BarBoxSlot)
        {
            BarBoxSlot->SetVerticalAlignment(VAlign_Center);
        }

        PipelineProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), FName("PipelineProgressBar"));
        PipelineProgressBar->SetPercent(0.0f);
        PipelineProgressBar->SetFillColorAndOpacity(FLinearColor(0.2f, 0.6f, 1.0f));
        BarBox->AddChild(PipelineProgressBar);
    }

    // --- Spacer (fills remaining space) ---
    {
        USpacer* SpFill = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpFill"));
        SpFill->SetSize(FVector2D(0, 0));
        HBox->AddChild(SpFill);
        UHorizontalBoxSlot* FillSlot = Cast<UHorizontalBoxSlot>(SpFill->Slot);
        if (FillSlot)
        {
            FillSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        }
    }

    // --- Flask dot (10x10 colored square) ---
    {
        USizeBox* DotBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName("FlaskDotBox"));
        DotBox->SetWidthOverride(10);
        DotBox->SetHeightOverride(10);
        HBox->AddChild(DotBox);
        UHorizontalBoxSlot* DotBoxSlot = Cast<UHorizontalBoxSlot>(DotBox->Slot);
        if (DotBoxSlot)
        {
            DotBoxSlot->SetVerticalAlignment(VAlign_Center);
        }

        FlaskDot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName("FlaskDot"));
        FlaskDot->SetBrushColor(FLinearColor::Red);
        DotBox->AddChild(FlaskDot);
    }

    // --- Spacer 6px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpDot"));
        Sp->SetSize(FVector2D(6, 0));
        HBox->AddChild(Sp);
    }

    // --- Flask status label ---
    FlaskStatusLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("FlaskStatusLabel"));
    FlaskStatusLabel->SetText(FText::FromString(TEXT("Offline")));
    FlaskStatusLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.3f, 0.3f)));
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 11);
        FlaskStatusLabel->SetFont(Font);
    }
    HBox->AddChild(FlaskStatusLabel);
    UHorizontalBoxSlot* FlaskLabelSlot = Cast<UHorizontalBoxSlot>(FlaskStatusLabel->Slot);
    if (FlaskLabelSlot)
    {
        FlaskLabelSlot->SetVerticalAlignment(VAlign_Center);
    }

    // --- Spacer 12px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpFlask"));
        Sp->SetSize(FVector2D(12, 0));
        HBox->AddChild(Sp);
    }

    // --- Fix 3.5: Queue count badge (initially collapsed) ---
    QueueBadge = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("QueueBadge"));
    QueueBadge->SetText(FText::GetEmpty());
    QueueBadge->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.8f, 0.2f)));  // amber
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 10);
        QueueBadge->SetFont(Font);
    }
    QueueBadge->SetVisibility(ESlateVisibility::Collapsed);
    HBox->AddChild(QueueBadge);
    UHorizontalBoxSlot* QueueSlot = Cast<UHorizontalBoxSlot>(QueueBadge->Slot);
    if (QueueSlot)
    {
        QueueSlot->SetVerticalAlignment(VAlign_Center);
    }

    // --- Spacer 12px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpQueue"));
        Sp->SetSize(FVector2D(12, 0));
        HBox->AddChild(Sp);
    }

    // --- Esc hint text ---
    EscHintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("EscHintText"));
    EscHintText->SetText(FText::FromString(TEXT("ESC: Menu | F3: Debug")));
    EscHintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)));
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 10);
        EscHintText->SetFont(Font);
    }
    HBox->AddChild(EscHintText);
    UHorizontalBoxSlot* EscSlot = Cast<UHorizontalBoxSlot>(EscHintText->Slot);
    if (EscSlot)
    {
        EscSlot->SetVerticalAlignment(VAlign_Center);
    }

    // --- Right spacer 12px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpRight"));
        Sp->SetSize(FVector2D(12, 0));
        HBox->AddChild(Sp);
    }
}

// -----------------------------------------------------------------------------
// BlueprintNativeEvent _Implementation methods
// -----------------------------------------------------------------------------

void UHJHUDWidget::OnProjectInfoUpdated_Implementation()
{
    if (ProjectNameText)
    {
        ProjectNameText->SetText(FText::FromString(
            ActiveProjectName.IsEmpty() ? TEXT("No Project") : ActiveProjectName));
    }

    if (StageText)
    {
        if (bPipelineActive && !PipelineStage.IsEmpty())
        {
            StageText->SetText(FText::FromString(PipelineStage));
            StageText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        else
        {
            StageText->SetText(FText::GetEmpty());
            StageText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (PipelineProgressBar)
    {
        PipelineProgressBar->SetPercent(PipelineProgressPct);
    }
}

void UHJHUDWidget::OnFlaskStatusUpdated_Implementation()
{
    if (FlaskDot)
    {
        FlaskDot->SetBrushColor(bFlaskOnline
            ? FLinearColor(0.2f, 0.9f, 0.3f)   // green
            : FLinearColor(0.9f, 0.2f, 0.2f));  // red
    }

    if (FlaskStatusLabel)
    {
        FlaskStatusLabel->SetText(FText::FromString(bFlaskOnline ? TEXT("Online") : TEXT("Offline")));
        FlaskStatusLabel->SetColorAndOpacity(FSlateColor(bFlaskOnline
            ? FLinearColor(0.2f, 0.9f, 0.3f)
            : FLinearColor(0.9f, 0.3f, 0.3f)));
    }
}

// -----------------------------------------------------------------------------
// Original logic (preserved)
// -----------------------------------------------------------------------------

void UHJHUDWidget::SetProjectInfo(const FString& ProjectName,
                                   const FString& Stage,
                                   int32          StageIndex,
                                   bool           bActive,
                                   int32          TotalStages)
{
    ActiveProjectName    = ProjectName;
    PipelineStage        = Stage;
    PipelineStageIndex   = StageIndex;
    bPipelineActive      = bActive;
    // Fix 1.2: Use TotalStages parameter instead of hardcoded 7
    float DivStages = (TotalStages > 0) ? static_cast<float>(TotalStages) : 7.0f;
    PipelineProgressPct  = bActive
        ? FMath::Clamp((StageIndex + 1) / DivStages, 0.0f, 1.0f)
        : 0.0f;

    OnProjectInfoUpdated();
}

void UHJHUDWidget::SetFlaskStatus(bool bOnline)
{
    bFlaskOnline = bOnline;
    OnFlaskStatusUpdated();
}

void UHJHUDWidget::SetQueueCount(int32 Count)
{
    if (!QueueBadge) return;

    if (Count > 0)
    {
        QueueBadge->SetText(FText::FromString(
            FString::Printf(TEXT("Q:%d"), Count)));
        QueueBadge->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    else
    {
        QueueBadge->SetVisibility(ESlateVisibility::Collapsed);
    }
}
