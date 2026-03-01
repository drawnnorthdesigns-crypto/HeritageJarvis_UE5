#include "HJHUDWidget.h"
#include "Core/HJGameInstance.h"
#include "Game/HJInteractable.h"
#include "EngineUtils.h"

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
#include "Kismet/GameplayStatics.h"

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
    EscHintText->SetText(FText::FromString(TEXT("ESC: Menu | I: Inv | G: Gov | M: Star Map | F3: Debug")));
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

    // --- Spacer 12px (between health and stamina) ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpHpStam"));
        Sp->SetSize(FVector2D(12, 0));
        HBox2->AddChild(Sp);
    }

    // --- Stamina bar (100 x 12, green fill) ---
    {
        USizeBox* StamBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName("StaminaBarBox"));
        StamBox->SetWidthOverride(100);
        StamBox->SetHeightOverride(12);
        HBox2->AddChild(StamBox);
        if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(StamBox->Slot))
            Slot->SetVerticalAlignment(VAlign_Center);

        StaminaBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), FName("StaminaBar"));
        StaminaBar->SetPercent(1.0f);
        StaminaBar->SetFillColorAndOpacity(FLinearColor(0.2f, 0.8f, 0.3f));  // green
        StamBox->AddChild(StaminaBar);
    }

    // --- Spacer 6px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpStamText"));
        Sp->SetSize(FVector2D(6, 0));
        HBox2->AddChild(Sp);
    }

    // --- Stamina text ---
    StaminaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("StaminaText"));
    StaminaText->SetText(FText::FromString(TEXT("100/100")));
    StaminaText->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.9f, 0.4f)));  // green
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 10);
        StaminaText->SetFont(Font);
    }
    HBox2->AddChild(StaminaText);
    if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(StaminaText->Slot))
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

    // Separator spacer
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpStrategicSep"));
        Sp->SetSize(FVector2D(24, 0));
        HBox3->AddChild(Sp);
    }

    // Fleet text (cyan)
    FleetText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("FleetText"));
    FleetText->SetText(FText::FromString(TEXT("Fleet: 500")));
    FleetText->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.7f, 0.9f)));
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 8);
        FleetText->SetFont(Font);
    }
    HBox3->AddChild(FleetText);
    if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(FleetText->Slot))
        Slot->SetVerticalAlignment(VAlign_Center);

    // Spacer
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpFleetTech"));
        Sp->SetSize(FVector2D(12, 0));
        HBox3->AddChild(Sp);
    }

    // Tech text (green)
    TechText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("TechText"));
    TechText->SetText(FText::FromString(TEXT("Tech: 1/8")));
    TechText->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.9f, 0.4f)));
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 8);
        TechText->SetFont(Font);
    }
    HBox3->AddChild(TechText);
    if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(TechText->Slot))
        Slot->SetVerticalAlignment(VAlign_Center);

    // Spacer
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpTechMine"));
        Sp->SetSize(FVector2D(12, 0));
        HBox3->AddChild(Sp);
    }

    // Mining text (amber)
    MiningText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("MiningText"));
    MiningText->SetText(FText::FromString(TEXT("Mined: 0")));
    MiningText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.7f, 0.2f)));
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 8);
        MiningText->SetFont(Font);
    }
    HBox3->AddChild(MiningText);
    if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(MiningText->Slot))
        Slot->SetVerticalAlignment(VAlign_Center);

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

    // =====================================================================
    // Last Saved indicator — bottom-right corner
    // =====================================================================

    LastSavedText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("LastSavedText"));
    LastSavedText->SetText(FText::FromString(TEXT("Last sealed: --")));
    LastSavedText->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f, 0.6f)));
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 10);
        LastSavedText->SetFont(Font);
    }

    UCanvasPanelSlot* SavedSlot = Root->AddChildToCanvas(LastSavedText);
    if (SavedSlot)
    {
        // Bottom-right anchor
        SavedSlot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
        SavedSlot->SetOffsets(FMargin(-200.0f, -30.0f, 0.0f, 0.0f));
        SavedSlot->SetAlignment(FVector2D(0.0f, 0.0f));
    }

    // =====================================================================
    // Compass bar (Phase 3) — top center
    // =====================================================================
    CompassText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("CompassText"));
    CompassText->SetText(FText::FromString(TEXT("N --- NE --- E --- SE --- S --- SW --- W --- NW --- N")));
    CompassText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f, 0.7f)));
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 11);
        CompassText->SetFont(Font);
    }

    UCanvasPanelSlot* CompassSlot = Root->AddChildToCanvas(CompassText);
    if (CompassSlot)
    {
        // Top center
        CompassSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
        CompassSlot->SetOffsets(FMargin(-150.0f, 4.0f, 300.0f, 20.0f));
        CompassSlot->SetAlignment(FVector2D(0.0f, 0.0f));
    }

    // =====================================================================
    // Quest Tracker (Phase 3) — top right
    // =====================================================================
    {
        UVerticalBox* QuestBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName("QuestBox"));

        QuestTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("QuestTitleText"));
        QuestTitleText->SetText(FText::GetEmpty());
        QuestTitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.63f, 0.30f)));  // gold
        {
            FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 11);
            QuestTitleText->SetFont(Font);
        }
        QuestBox->AddChild(QuestTitleText);

        QuestObjectiveText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("QuestObjectiveText"));
        QuestObjectiveText->SetText(FText::GetEmpty());
        QuestObjectiveText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
        {
            FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 10);
            QuestObjectiveText->SetFont(Font);
        }
        QuestBox->AddChild(QuestObjectiveText);

        UCanvasPanelSlot* QuestSlot = Root->AddChildToCanvas(QuestBox);
        if (QuestSlot)
        {
            // Top right corner
            QuestSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
            QuestSlot->SetOffsets(FMargin(-250.0f, 95.0f, 240.0f, 40.0f));
            QuestSlot->SetAlignment(FVector2D(0.0f, 0.0f));
        }
    }

    // =====================================================================
    // Zone Toast (Phase 3) — center screen, fades out
    // =====================================================================
    ZoneToastText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("ZoneToastText"));
    ZoneToastText->SetText(FText::GetEmpty());
    ZoneToastText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.63f, 0.30f, 0.0f)));  // starts invisible
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 18);
        ZoneToastText->SetFont(Font);
    }
    ZoneToastText->SetVisibility(ESlateVisibility::Collapsed);

    UCanvasPanelSlot* ZoneSlot = Root->AddChildToCanvas(ZoneToastText);
    if (ZoneSlot)
    {
        // Center screen
        ZoneSlot->SetAnchors(FAnchors(0.5f, 0.35f, 0.5f, 0.35f));
        ZoneSlot->SetOffsets(FMargin(-200.0f, 0.0f, 400.0f, 30.0f));
        ZoneSlot->SetAlignment(FVector2D(0.0f, 0.0f));
    }

    // =====================================================================
    // Minimap (top-right)
    // =====================================================================
    {
        MinimapBg = WidgetTree->ConstructWidget<UBorder>(
            UBorder::StaticClass(), FName("MinimapBg"));
        MinimapBg->SetBrushColor(FLinearColor(0.03f, 0.03f, 0.06f, 0.85f));
        MinimapBg->SetPadding(FMargin(2));
        UCanvasPanelSlot* MmSlot = Root->AddChildToCanvas(MinimapBg);
        if (MmSlot)
        {
            MmSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
            MmSlot->SetAlignment(FVector2D(1.0f, 0.0f));
            MmSlot->SetOffsets(FMargin(-10, 10, 150, 150));
        }

        // Canvas inside minimap for positioning dots
        MinimapCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
            UCanvasPanel::StaticClass(), FName("MinimapCanvas"));
        MinimapBg->AddChild(MinimapCanvas);

        // North indicator
        MinimapNorth = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(), FName("MinimapN"));
        MinimapNorth->SetText(FText::FromString(TEXT("N")));
        MinimapNorth->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.85f, 0.6f)));
        FSlateFontInfo NFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10);
        MinimapNorth->SetFont(NFont);
        UCanvasPanelSlot* NSlot = MinimapCanvas->AddChildToCanvas(MinimapNorth);
        if (NSlot)
        {
            NSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
            NSlot->SetAlignment(FVector2D(0.5f, 0.0f));
            NSlot->SetOffsets(FMargin(0, 2, 0, 0));
        }

        // Player arrow (center dot, green)
        MinimapPlayerArrow = WidgetTree->ConstructWidget<UBorder>(
            UBorder::StaticClass(), FName("MinimapPlayer"));
        MinimapPlayerArrow->SetBrushColor(FLinearColor(0.2f, 0.9f, 0.3f));
        UCanvasPanelSlot* PSlot = MinimapCanvas->AddChildToCanvas(MinimapPlayerArrow);
        if (PSlot)
        {
            PSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
            PSlot->SetAlignment(FVector2D(0.5f, 0.5f));
            PSlot->SetOffsets(FMargin(0, 0, 8, 8));
        }

        // Pre-create dots for NPCs/buildings (up to 20)
        for (int32 i = 0; i < 20; ++i)
        {
            FName DotName(*FString::Printf(TEXT("MmDot_%d"), i));
            UBorder* Dot = WidgetTree->ConstructWidget<UBorder>(
                UBorder::StaticClass(), DotName);
            Dot->SetBrushColor(FLinearColor(0.5f, 0.5f, 0.5f, 0.0f));  // Hidden
            UCanvasPanelSlot* DSlot = MinimapCanvas->AddChildToCanvas(Dot);
            if (DSlot)
            {
                DSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
                DSlot->SetAlignment(FVector2D(0.5f, 0.5f));
                DSlot->SetOffsets(FMargin(0, 0, 5, 5));
            }
            Dot->SetVisibility(ESlateVisibility::Collapsed);
            MinimapDots.Add(Dot);
        }

        // Biome zone colored border
        UBorder* MinimapBorder = WidgetTree->ConstructWidget<UBorder>(
            UBorder::StaticClass(), FName("MinimapBorder"));
        MinimapBorder->SetBrushColor(FLinearColor(0.3f, 0.25f, 0.15f, 0.6f));
        MinimapBorder->SetPadding(FMargin(1));
    }

    // =====================================================================
    // Interaction Prompt (center-bottom)
    // =====================================================================
    {
        InteractPromptBg = WidgetTree->ConstructWidget<UBorder>(
            UBorder::StaticClass(), FName("InteractBg"));
        InteractPromptBg->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.08f, 0.85f));
        InteractPromptBg->SetPadding(FMargin(16, 8, 16, 8));
        UCanvasPanelSlot* IPSlot = Root->AddChildToCanvas(InteractPromptBg);
        if (IPSlot)
        {
            IPSlot->SetAnchors(FAnchors(0.5f, 0.75f, 0.5f, 0.75f));
            IPSlot->SetAlignment(FVector2D(0.5f, 0.5f));
            IPSlot->SetAutoSize(true);
        }

        InteractPromptText = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(), FName("InteractText"));
        InteractPromptText->SetText(FText::FromString(TEXT("[E] Interact")));
        InteractPromptText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.85f, 0.6f)));
        FSlateFontInfo IFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14);
        InteractPromptText->SetFont(IFont);
        InteractPromptBg->AddChild(InteractPromptText);
        InteractPromptBg->SetVisibility(ESlateVisibility::Collapsed);
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

void UHJHUDWidget::SetStrategicInfo(const FTartariaFleetSummary& InFleet,
                                     const FTartariaTechSummary& InTech,
                                     const FTartariaMiningSummary& InMining)
{
    CachedFleet = InFleet;
    CachedTech = InTech;
    CachedMining = InMining;
    OnStrategicInfoUpdated();
}

void UHJHUDWidget::OnStrategicInfoUpdated_Implementation()
{
    if (FleetText)
    {
        FString Deployed = CachedFleet.DeployedZones > 0
            ? FString::Printf(TEXT(" (%d zones)"), CachedFleet.DeployedZones)
            : TEXT("");
        FleetText->SetText(FText::FromString(
            FString::Printf(TEXT("Fleet: %d%s"), CachedFleet.TotalPower, *Deployed)));
    }

    if (TechText)
    {
        TechText->SetText(FText::FromString(
            FString::Printf(TEXT("Tech: %d/%d"), CachedTech.UnlockedCount, CachedTech.TotalNodes)));
    }

    if (MiningText)
    {
        MiningText->SetText(FText::FromString(
            FString::Printf(TEXT("Mined: %s"), *FString::FormatAsNumber(CachedMining.TotalMined))));
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

void UHJHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Update "Last sealed: Xs ago" display from GameInstance's LastSaveTime
    if (LastSavedText)
    {
        UHJGameInstance* GI = UHJGameInstance::Get(this);
        if (GI && GI->LastSaveTime.GetTicks() > 0)
        {
            LastSaveTime = GI->LastSaveTime;
        }

        if (LastSaveTime.GetTicks() > 0)
        {
            FTimespan Elapsed = FDateTime::Now() - LastSaveTime;
            int32 Secs = FMath::FloorToInt(Elapsed.GetTotalSeconds());
            FString TimeStr;
            if (Secs < 60)
                TimeStr = FString::Printf(TEXT("%ds ago"), Secs);
            else if (Secs < 3600)
                TimeStr = FString::Printf(TEXT("%dm ago"), Secs / 60);
            else
                TimeStr = FString::Printf(TEXT("%dh ago"), Secs / 3600);

            LastSavedText->SetText(FText::FromString(FString::Printf(TEXT("Last sealed: %s"), *TimeStr)));

            // Fade: bright after save, dim after 10s
            float Alpha = (Secs < 10) ? 1.0f : 0.5f;
            LastSavedText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, Alpha)));
        }
    }

    // ── Zone Toast Fade ──────────────────────────────────────
    if (ZoneToastText && ZoneToastTimer > 0.f)
    {
        ZoneToastTimer -= InDeltaTime;
        float Alpha = (ZoneToastTimer > 0.5f) ? 1.0f : (ZoneToastTimer / 0.5f);
        ZoneToastText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.63f, 0.30f, Alpha)));
        if (ZoneToastTimer <= 0.f)
        {
            ZoneToastText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // ── Compass Update from Player Yaw ──────────────────────
    if (CompassText)
    {
        APlayerController* PC = GetOwningPlayer();
        if (PC)
        {
            SetCompassHeading(PC->GetControlRotation().Yaw);
        }
    }

    // ── Minimap update (every 0.5s) ─────────────────────────
    static float MinimapTimer = 0.f;
    MinimapTimer += InDeltaTime;
    if (MinimapTimer >= 0.5f)
    {
        MinimapTimer = 0.f;
        UpdateMinimap();
    }

    // ── Interaction prompt (check for nearby interactables) ──
    {
        ACharacter* PC = UGameplayStatics::GetPlayerCharacter(this, 0);
        if (PC)
        {
            bool bFoundInteractable = false;
            UWorld* W = GetWorld();
            if (W)
            {
                for (TActorIterator<AActor> It(W); It; ++It)
                {
                    if (It->GetClass()->ImplementsInterface(UHJInteractable::StaticClass()))
                    {
                        float D = FVector::Dist(PC->GetActorLocation(), It->GetActorLocation());
                        if (D < 400.f)
                        {
                            FString Prompt = FString::Printf(TEXT("[E] %s"),
                                *IHJInteractable::Execute_GetInteractPrompt(*It));
                            ShowInteractPrompt(Prompt);
                            bFoundInteractable = true;
                            break;
                        }
                    }
                }
            }
            if (!bFoundInteractable)
                HideInteractPrompt();
        }
    }
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

// -----------------------------------------------------------------------------
// Stamina HUD
// -----------------------------------------------------------------------------

void UHJHUDWidget::SetStaminaInfo(float InStamina, float InMaxStamina)
{
    CachedStamina = InStamina;
    CachedMaxStamina = InMaxStamina;
    OnStaminaInfoUpdated();
}

void UHJHUDWidget::OnStaminaInfoUpdated_Implementation()
{
    if (StaminaBar)
    {
        float Pct = (CachedMaxStamina > 0.f) ? (CachedStamina / CachedMaxStamina) : 0.f;
        StaminaBar->SetPercent(Pct);

        // Color: green > 50%, yellow > 25%, red below
        if (Pct > 0.5f)
            StaminaBar->SetFillColorAndOpacity(FLinearColor(0.2f, 0.8f, 0.3f));
        else if (Pct > 0.25f)
            StaminaBar->SetFillColorAndOpacity(FLinearColor(0.9f, 0.8f, 0.1f));
        else
            StaminaBar->SetFillColorAndOpacity(FLinearColor(0.9f, 0.3f, 0.1f));
    }

    if (StaminaText)
    {
        StaminaText->SetText(FText::FromString(
            FString::Printf(TEXT("%.0f/%.0f"), CachedStamina, CachedMaxStamina)));
    }
}

// -----------------------------------------------------------------------------
// Phase 3: Compass
// -----------------------------------------------------------------------------

void UHJHUDWidget::SetCompassHeading(float Yaw)
{
    CurrentCompassYaw = Yaw;
    if (!CompassText) return;

    // Build compass string centered on current heading
    static const TCHAR* Directions[] = {
        TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"),
        TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW")
    };
    // Normalize yaw to 0-360
    float NormYaw = FMath::Fmod(Yaw + 360.f, 360.f);
    int32 CenterIdx = FMath::RoundToInt(NormYaw / 45.f) % 8;
    // Show 5 directions centered on current heading
    FString CompassStr;
    for (int32 I = -2; I <= 2; I++)
    {
        int32 Idx = (CenterIdx + I + 8) % 8;
        if (I != -2) CompassStr += TEXT("  ---  ");
        CompassStr += Directions[Idx];
    }
    CompassText->SetText(FText::FromString(CompassStr));
}

// -----------------------------------------------------------------------------
// Phase 3: Quest Tracker
// -----------------------------------------------------------------------------

void UHJHUDWidget::SetActiveQuest(const FString& QuestTitle, const FString& ObjectiveText, int32 Step, int32 TotalSteps)
{
    if (QuestTitleText)
    {
        QuestTitleText->SetText(FText::FromString(QuestTitle));
        QuestTitleText->SetVisibility(QuestTitle.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    }
    if (QuestObjectiveText)
    {
        FString ObjStr = FString::Printf(TEXT("[%d/%d] %s"), Step, TotalSteps, *ObjectiveText);
        QuestObjectiveText->SetText(FText::FromString(ObjStr));
        QuestObjectiveText->SetVisibility(ObjectiveText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    }
}

// -----------------------------------------------------------------------------
// Phase 3: Zone Toast
// -----------------------------------------------------------------------------

void UHJHUDWidget::ShowZoneToast(const FString& ZoneName, int32 Difficulty)
{
    if (!ZoneToastText) return;
    // Difficulty stars
    FString Stars;
    for (int32 I = 0; I < Difficulty; I++) Stars += TEXT("*");
    ZoneToastText->SetText(FText::FromString(FString::Printf(TEXT("- %s %s -"), *ZoneName, *Stars)));
    ZoneToastText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    ZoneToastText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.63f, 0.30f, 1.0f)));
    ZoneToastTimer = 3.0f;  // Show for 3 seconds
}

// -----------------------------------------------------------------------------
// Phase 2: Damage Direction
// -----------------------------------------------------------------------------

void UHJHUDWidget::ShowDamageDirection(float DirectionAngle, float Intensity)
{
    DamageDirectionAngle = DirectionAngle;
    DamageDirectionIntensity = FMath::Clamp(Intensity, 0.2f, 1.0f);
    DamageDirectionTimer = 1.5f;
}

// -----------------------------------------------------------------------------
// Phase 3: Victory / Level-Up
// -----------------------------------------------------------------------------

void UHJHUDWidget::ShowVictoryFanfare(const FString& Message, int32 XPGained)
{
    VictoryMessage = FString::Printf(TEXT("VICTORY -- %s (+%d XP)"), *Message, XPGained);
    VictoryTimer = 3.0f;
}

void UHJHUDWidget::ShowLevelUp(int32 NewLevel, const FString& Title)
{
    LevelUpLevel = NewLevel;
    LevelUpTitle = Title;
    LevelUpTimer = 4.0f;
}

// -----------------------------------------------------------------------------
// Minimap
// -----------------------------------------------------------------------------

void UHJHUDWidget::UpdateMinimap()
{
    if (!MinimapCanvas) return;

    ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
    if (!Player) return;

    FVector PlayerLoc = Player->GetActorLocation();
    float MapRange = 5000.f;  // 50m radius shown on minimap

    // Find all NPCs and buildings in range
    int32 DotIndex = 0;
    UWorld* World = GetWorld();
    if (!World) return;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (DotIndex >= MinimapDots.Num()) break;

        AActor* Actor = *It;
        if (Actor == Player) continue;

        float Dist = FVector::Dist(Actor->GetActorLocation(), PlayerLoc);
        if (Dist > MapRange) continue;

        // Determine dot color by actor class name
        FLinearColor DotColor(0.5f, 0.5f, 0.5f);
        bool bShow = false;

        FString ClassName = Actor->GetClass()->GetName();
        if (ClassName.Contains(TEXT("NPC")))
        {
            DotColor = FLinearColor(0.9f, 0.85f, 0.3f);  // Gold for NPCs
            bShow = true;
        }
        else if (ClassName.Contains(TEXT("Forge")) || ClassName.Contains(TEXT("Building")))
        {
            DotColor = FLinearColor(0.4f, 0.6f, 0.9f);  // Blue for buildings
            bShow = true;
        }
        else if (ClassName.Contains(TEXT("Resource")))
        {
            DotColor = FLinearColor(0.3f, 0.8f, 0.4f);  // Green for resources
            bShow = true;
        }
        else if (ClassName.Contains(TEXT("Enemy")))
        {
            DotColor = FLinearColor(0.9f, 0.2f, 0.2f);  // Red for enemies
            bShow = true;
        }

        if (!bShow) continue;

        UBorder* Dot = MinimapDots[DotIndex];
        Dot->SetBrushColor(DotColor);
        Dot->SetVisibility(ESlateVisibility::Visible);

        // Position relative to player
        FVector Delta = Actor->GetActorLocation() - PlayerLoc;
        float NormX = FMath::Clamp(Delta.X / MapRange, -1.f, 1.f) * 0.4f + 0.5f;
        float NormY = FMath::Clamp(Delta.Y / MapRange, -1.f, 1.f) * 0.4f + 0.5f;

        UCanvasPanelSlot* DSlot = Cast<UCanvasPanelSlot>(Dot->Slot);
        if (DSlot)
        {
            DSlot->SetAnchors(FAnchors(NormX, NormY, NormX, NormY));
        }

        DotIndex++;
    }

    // Hide unused dots
    for (int32 i = DotIndex; i < MinimapDots.Num(); ++i)
    {
        MinimapDots[i]->SetVisibility(ESlateVisibility::Collapsed);
    }
}

// -----------------------------------------------------------------------------
// Interaction Prompt
// -----------------------------------------------------------------------------

void UHJHUDWidget::ShowInteractPrompt(const FString& Prompt)
{
    if (InteractPromptBg)
        InteractPromptBg->SetVisibility(ESlateVisibility::Visible);
    if (InteractPromptText)
        InteractPromptText->SetText(FText::FromString(Prompt));
}

void UHJHUDWidget::HideInteractPrompt()
{
    if (InteractPromptBg)
        InteractPromptBg->SetVisibility(ESlateVisibility::Collapsed);
}
