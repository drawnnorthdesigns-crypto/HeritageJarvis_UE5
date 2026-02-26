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
    EscHintText->SetText(FText::FromString(TEXT("ESC: Menu | I: Inventory | G: Gov | F3: Debug")));
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

    // =====================================================================
    // Economy bar (Phase 1) — second bar at y=40
    // =====================================================================

    UBorder* BarBg2 = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName("EconBarBg"));
    BarBg2->SetBrushColor(FLinearColor(0.04f, 0.04f, 0.07f, 0.75f));

    UCanvasPanelSlot* Bar2Slot = Root->AddChildToCanvas(BarBg2);
    if (Bar2Slot)
    {
        Bar2Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 0.0f));
        Bar2Slot->SetOffsets(FMargin(0.0f, 40.0f, 0.0f, 28.0f));
        Bar2Slot->SetAlignment(FVector2D(0.0f, 0.0f));
    }

    UHorizontalBox* HBox2 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName("EconHBox"));
    BarBg2->AddChild(HBox2);

    // --- Left spacer 12px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpEconLeft"));
        Sp->SetSize(FVector2D(12, 0));
        HBox2->AddChild(Sp);
    }

    // --- Credits text (gold, bold 12) ---
    CreditsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("CreditsText"));
    CreditsText->SetText(FText::FromString(TEXT("0 cr")));
    CreditsText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.84f, 0.0f)));  // gold
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 12);
        CreditsText->SetFont(Font);
    }
    HBox2->AddChild(CreditsText);
    if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(CreditsText->Slot))
        Slot->SetVerticalAlignment(VAlign_Center);

    // --- Spacer 20px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpEconCredits"));
        Sp->SetSize(FVector2D(20, 0));
        HBox2->AddChild(Sp);
    }

    // --- Era text (cyan, regular 11) ---
    EraText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("EraText"));
    EraText->SetText(FText::FromString(TEXT("FOUNDATION -- Day 1")));
    EraText->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.85f, 0.9f)));  // cyan
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 11);
        EraText->SetFont(Font);
    }
    HBox2->AddChild(EraText);
    if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(EraText->Slot))
        Slot->SetVerticalAlignment(VAlign_Center);

    // --- Spacer 20px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpEconEra"));
        Sp->SetSize(FVector2D(20, 0));
        HBox2->AddChild(Sp);
    }

    // --- Resources text (white, regular 10) ---
    ResourcesText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("ResourcesText"));
    ResourcesText->SetText(FText::FromString(TEXT("Fe:0 St:0 Kn:0 Cr:0")));
    ResourcesText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f)));
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 10);
        ResourcesText->SetFont(Font);
    }
    HBox2->AddChild(ResourcesText);
    if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(ResourcesText->Slot))
        Slot->SetVerticalAlignment(VAlign_Center);

    // --- Fill spacer ---
    {
        USpacer* SpFill2 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpEconFill"));
        SpFill2->SetSize(FVector2D(0, 0));
        HBox2->AddChild(SpFill2);
        if (UHorizontalBoxSlot* FillSlot2 = Cast<UHorizontalBoxSlot>(SpFill2->Slot))
            FillSlot2->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    // --- Zone name (right side, regular 10) ---
    ZoneText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("ZoneText"));
    ZoneText->SetText(FText::FromString(TEXT("CLEARINGHOUSE")));
    ZoneText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.8f, 0.6f)));
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 10);
        ZoneText->SetFont(Font);
    }
    HBox2->AddChild(ZoneText);
    if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(ZoneText->Slot))
        Slot->SetVerticalAlignment(VAlign_Center);

    // --- Spacer 12px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpHealth"));
        Sp->SetSize(FVector2D(12, 0));
        HBox2->AddChild(Sp);
    }

    // --- Health bar (100 x 12, red fill) ---
    {
        USizeBox* HpBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName("HealthBarBox"));
        HpBox->SetWidthOverride(100);
        HpBox->SetHeightOverride(12);
        HBox2->AddChild(HpBox);
        if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(HpBox->Slot))
            Slot->SetVerticalAlignment(VAlign_Center);

        HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), FName("HealthBar"));
        HealthBar->SetPercent(1.0f);
        HealthBar->SetFillColorAndOpacity(FLinearColor(0.8f, 0.15f, 0.15f));
        HpBox->AddChild(HealthBar);
    }

    // --- Spacer 6px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpHpText"));
        Sp->SetSize(FVector2D(6, 0));
        HBox2->AddChild(Sp);
    }

    // --- Health text ---
    HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("HealthText"));
    HealthText->SetText(FText::FromString(TEXT("100/100")));
    HealthText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.3f, 0.3f)));
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 10);
        HealthText->SetFont(Font);
    }
    HBox2->AddChild(HealthText);
    if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(HealthText->Slot))
        Slot->SetVerticalAlignment(VAlign_Center);

    // --- Right spacer 12px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpEconRight"));
        Sp->SetSize(FVector2D(12, 0));
        HBox2->AddChild(Sp);
    }

    // =====================================================================
    // Faction bar (Phase 4) — third bar at y=68
    // =====================================================================

    UBorder* BarBg3 = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName("FactionBarBg"));
    BarBg3->SetBrushColor(FLinearColor(0.03f, 0.03f, 0.06f, 0.65f));

    UCanvasPanelSlot* Bar3Slot = Root->AddChildToCanvas(BarBg3);
    if (Bar3Slot)
    {
        Bar3Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 0.0f));
        Bar3Slot->SetOffsets(FMargin(0.0f, 68.0f, 0.0f, 22.0f));
        Bar3Slot->SetAlignment(FVector2D(0.0f, 0.0f));
    }

    UHorizontalBox* HBox3 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName("FactionHBox"));
    BarBg3->AddChild(HBox3);

    // Faction colors: ARGENTUM=silver, AUREATE=gold, FERRUM=iron gray, OBSIDIAN=purple
    struct FFactionDef { FString Tag; FLinearColor Color; };
    FFactionDef FactionDefs[] = {
        { TEXT("ARG"), FLinearColor(0.75f, 0.75f, 0.80f) },
        { TEXT("AUR"), FLinearColor(1.0f, 0.84f, 0.0f)   },
        { TEXT("FER"), FLinearColor(0.6f, 0.6f, 0.65f)    },
        { TEXT("OBS"), FLinearColor(0.6f, 0.3f, 0.8f)     }
    };

    FactionLabels.SetNum(4);
    FactionBars.SetNum(4);
    FactionValues.SetNum(4);

    // Left spacer
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpFacLeft"));
        Sp->SetSize(FVector2D(12, 0));
        HBox3->AddChild(Sp);
    }

    for (int32 I = 0; I < 4; I++)
    {
        if (I > 0)
        {
            USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(
                USpacer::StaticClass(), *FString::Printf(TEXT("SpFac_%d"), I));
            Sp->SetSize(FVector2D(16, 0));
            HBox3->AddChild(Sp);
        }

        // Faction tag label
        FactionLabels[I] = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(), *FString::Printf(TEXT("FacLabel_%d"), I));
        FactionLabels[I]->SetText(FText::FromString(FactionDefs[I].Tag));
        FactionLabels[I]->SetColorAndOpacity(FSlateColor(FactionDefs[I].Color));
        {
            FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 9);
            FactionLabels[I]->SetFont(Font);
        }
        HBox3->AddChild(FactionLabels[I]);
        if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(FactionLabels[I]->Slot))
            Slot->SetVerticalAlignment(VAlign_Center);

        // Spacer 4px
        {
            USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(
                USpacer::StaticClass(), *FString::Printf(TEXT("SpFacBar_%d"), I));
            Sp->SetSize(FVector2D(4, 0));
            HBox3->AddChild(Sp);
        }

        // Mini progress bar (50 x 8)
        USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(
            USizeBox::StaticClass(), *FString::Printf(TEXT("FacBarBox_%d"), I));
        BarBox->SetWidthOverride(50);
        BarBox->SetHeightOverride(8);
        HBox3->AddChild(BarBox);
        if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(BarBox->Slot))
            Slot->SetVerticalAlignment(VAlign_Center);

        FactionBars[I] = WidgetTree->ConstructWidget<UProgressBar>(
            UProgressBar::StaticClass(), *FString::Printf(TEXT("FacBar_%d"), I));
        FactionBars[I]->SetPercent(0.0f);
        FactionBars[I]->SetFillColorAndOpacity(FactionDefs[I].Color);
        BarBox->AddChild(FactionBars[I]);

        // Spacer 4px
        {
            USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(
                USpacer::StaticClass(), *FString::Printf(TEXT("SpFacVal_%d"), I));
            Sp->SetSize(FVector2D(4, 0));
            HBox3->AddChild(Sp);
        }

        // Influence value text
        FactionValues[I] = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(), *FString::Printf(TEXT("FacVal_%d"), I));
        FactionValues[I]->SetText(FText::FromString(TEXT("0")));
        FactionValues[I]->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)));
        {
            FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 8);
            FactionValues[I]->SetFont(Font);
        }
        HBox3->AddChild(FactionValues[I]);
        if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(FactionValues[I]->Slot))
            Slot->SetVerticalAlignment(VAlign_Center);
    }

    // Fill spacer
    {
        USpacer* SpFill3 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpFacFill"));
        SpFill3->SetSize(FVector2D(0, 0));
        HBox3->AddChild(SpFill3);
        if (UHorizontalBoxSlot* FillSlot3 = Cast<UHorizontalBoxSlot>(SpFill3->Slot))
            FillSlot3->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    // Governance hint (right side)
    UTextBlock* GovHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("GovHint"));
    GovHint->SetText(FText::FromString(TEXT("G: Governance")));
    GovHint->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.4f, 0.5f)));
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 8);
        GovHint->SetFont(Font);
    }
    HBox3->AddChild(GovHint);
    if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(GovHint->Slot))
        Slot->SetVerticalAlignment(VAlign_Center);

    // Right spacer
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpFacRight"));
        Sp->SetSize(FVector2D(12, 0));
        HBox3->AddChild(Sp);
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

void UHJHUDWidget::SetGameEconomy(int32 InCredits, const FString& InEra, int32 InDay,
                                   int32 InIron, int32 InStone, int32 InKnowledge, int32 InCrystal)
{
    Credits = InCredits;
    EraDisplay = FString::Printf(TEXT("%s -- Day %d"), *InEra, InDay);
    IronCount = InIron;
    StoneCount = InStone;
    KnowledgeCount = InKnowledge;
    CrystalCount = InCrystal;
    OnGameEconomyUpdated();
}

void UHJHUDWidget::OnGameEconomyUpdated_Implementation()
{
    if (CreditsText)
    {
        // Format credits with comma separators
        FString CrStr = FString::FormatAsNumber(Credits);
        CreditsText->SetText(FText::FromString(FString::Printf(TEXT("%s cr"), *CrStr)));
    }

    if (EraText)
    {
        EraText->SetText(FText::FromString(EraDisplay));
    }

    if (ResourcesText)
    {
        ResourcesText->SetText(FText::FromString(
            FString::Printf(TEXT("Fe:%d  St:%d  Kn:%d  Cr:%d"),
                IronCount, StoneCount, KnowledgeCount, CrystalCount)));
    }
}

void UHJHUDWidget::SetFactionInfo(const TArray<FTartariaFactionInfo>& InFactions)
{
    CachedFactions = InFactions;
    OnFactionInfoUpdated();
}

void UHJHUDWidget::OnFactionInfoUpdated_Implementation()
{
    // Map faction keys to bar indices: ARGENTUM=0, AUREATE=1, FERRUM=2, OBSIDIAN=3
    for (const FTartariaFactionInfo& Fac : CachedFactions)
    {
        int32 Idx = -1;
        if      (Fac.FactionKey == TEXT("ARGENTUM")) Idx = 0;
        else if (Fac.FactionKey == TEXT("AUREATE"))  Idx = 1;
        else if (Fac.FactionKey == TEXT("FERRUM"))    Idx = 2;
        else if (Fac.FactionKey == TEXT("OBSIDIAN"))  Idx = 3;

        if (Idx < 0 || Idx >= 4) continue;

        if (FactionBars.IsValidIndex(Idx) && FactionBars[Idx])
        {
            // Influence range 0-100
            float Pct = FMath::Clamp(Fac.Influence / 100.f, 0.f, 1.f);
            FactionBars[Idx]->SetPercent(Pct);
        }

        if (FactionValues.IsValidIndex(Idx) && FactionValues[Idx])
        {
            FactionValues[Idx]->SetText(FText::FromString(
                FString::Printf(TEXT("%.0f"), Fac.Influence)));
        }
    }
}

void UHJHUDWidget::SetHealthInfo(float InHealth, float InMaxHealth,
                                  const FString& InZone, int32 InDifficulty)
{
    CachedHealth = InHealth;
    CachedMaxHealth = InMaxHealth;
    CachedZoneName = InZone;
    CachedDifficulty = InDifficulty;
    OnHealthInfoUpdated();
}

void UHJHUDWidget::OnHealthInfoUpdated_Implementation()
{
    if (HealthBar)
    {
        float Pct = (CachedMaxHealth > 0.f) ? (CachedHealth / CachedMaxHealth) : 0.f;
        HealthBar->SetPercent(Pct);

        // Color: green > 60%, yellow > 30%, red below
        if (Pct > 0.6f)
            HealthBar->SetFillColorAndOpacity(FLinearColor(0.2f, 0.8f, 0.2f));
        else if (Pct > 0.3f)
            HealthBar->SetFillColorAndOpacity(FLinearColor(0.9f, 0.8f, 0.1f));
        else
            HealthBar->SetFillColorAndOpacity(FLinearColor(0.8f, 0.15f, 0.15f));
    }

    if (HealthText)
    {
        HealthText->SetText(FText::FromString(
            FString::Printf(TEXT("%.0f/%.0f"), CachedHealth, CachedMaxHealth)));
    }

    if (ZoneText)
    {
        // Color zone by difficulty: 1=green, 2=yellow, 3=orange, 4=red, 5=purple
        FLinearColor ZoneColor;
        switch (CachedDifficulty)
        {
        case 1:  ZoneColor = FLinearColor(0.4f, 0.9f, 0.4f); break;
        case 2:  ZoneColor = FLinearColor(0.9f, 0.9f, 0.3f); break;
        case 3:  ZoneColor = FLinearColor(0.9f, 0.6f, 0.2f); break;
        case 4:  ZoneColor = FLinearColor(0.9f, 0.2f, 0.2f); break;
        default: ZoneColor = FLinearColor(0.7f, 0.2f, 0.9f); break;
        }
        ZoneText->SetColorAndOpacity(FSlateColor(ZoneColor));
        ZoneText->SetText(FText::FromString(CachedZoneName));
    }
}
