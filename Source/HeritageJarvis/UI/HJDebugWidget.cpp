#include "HJDebugWidget.h"
#include "Core/HJGameInstance.h"
#include "Engine/Engine.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Styling/CoreStyle.h"

// -------------------------------------------------------
// Programmatic layout
// -------------------------------------------------------

void UHJDebugWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!WidgetTree) return;

    // If a Blueprint child already has a layout, don't overwrite it
    UPanelWidget* ExistingRoot = Cast<UPanelWidget>(WidgetTree->RootWidget);
    if (ExistingRoot && ExistingRoot->GetChildrenCount() > 0) return;

    BuildProgrammaticLayout();
}

void UHJDebugWidget::BuildProgrammaticLayout()
{
    // Root: CanvasPanel for absolute positioning
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), FName("Root"));
    WidgetTree->RootWidget = Root;

    // Semi-transparent dark background border
    UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(
        UBorder::StaticClass(), FName("DebugBg"));
    Bg->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.05f, 0.8f));
    Bg->SetPadding(FMargin(8.f));

    UCanvasPanelSlot* BgSlot = Root->AddChildToCanvas(Bg);
    BgSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
    BgSlot->SetAutoSize(true);
    BgSlot->SetOffsets(FMargin(10.f, 50.f, 0.f, 0.f));

    // Vertical box to hold all debug lines
    DebugInfoBox = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), FName("DebugInfoBox"));
    Bg->AddChild(DebugInfoBox);

    // Helper lambda: create a labeled text block with light-green color
    auto CreateDebugLine = [&](FName Name, const FString& DefaultText, int32 FontSize = 10) -> UTextBlock*
    {
        UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(), Name);
        T->SetText(FText::FromString(DefaultText));
        T->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 1.0f, 0.5f)));
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", FontSize);
        T->SetFont(Font);
        DebugInfoBox->AddChild(T);
        return T;
    };

    // Title line (bold, yellow-green)
    UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), FName("DebugTitle"));
    TitleText->SetText(FText::FromString(TEXT("DEBUG INFO")));
    TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 1.0f, 0.3f)));
    FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);
    TitleText->SetFont(TitleFont);
    DebugInfoBox->AddChild(TitleText);

    // Individual debug info lines
    FlaskUrlText       = CreateDebugLine(FName("FlaskUrlText"),       TEXT("Flask: ---"));
    FlaskOnlineText    = CreateDebugLine(FName("FlaskOnlineText"),    TEXT("Status: ---"));
    ProjectIdText      = CreateDebugLine(FName("ProjectIdText"),      TEXT("Project ID: ---"));
    ProjectNameText    = CreateDebugLine(FName("ProjectNameText"),    TEXT("Project: ---"));
    QueuedText         = CreateDebugLine(FName("QueuedText"),         TEXT("Queued Requests: 0"));
    PollTimeText       = CreateDebugLine(FName("PollTimeText"),       TEXT("Last Poll: ---"));
    PollIntervalText   = CreateDebugLine(FName("PollIntervalText"),   TEXT("Poll Interval: 0s"));
    FPSText            = CreateDebugLine(FName("FPSText"),            TEXT("FPS: 0"));
    PipelineStageText  = CreateDebugLine(FName("PipelineStageText"),  TEXT("Stage: ---"));
    PipelineActiveText = CreateDebugLine(FName("PipelineActiveText"), TEXT("Pipeline: Idle"));
}

// -------------------------------------------------------
// Tick & control (unchanged)
// -------------------------------------------------------

void UHJDebugWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!bDebugVisible) return;

    TickAccum += InDeltaTime;
    if (TickAccum >= 0.5f)  // refresh twice per second
    {
        TickAccum = 0.0f;
        RefreshDebugInfo();
    }
}

void UHJDebugWidget::ToggleDebugOverlay()
{
    bDebugVisible = !bDebugVisible;
    SetVisibility(bDebugVisible ? ESlateVisibility::SelfHitTestInvisible
                                : ESlateVisibility::Collapsed);
    if (bDebugVisible) RefreshDebugInfo();
}

void UHJDebugWidget::RefreshDebugInfo()
{
    UHJGameInstance* GI = UHJGameInstance::Get(this);
    if (!GI) return;

    DebugInfo.FlaskUrl          = GI->ApiClient ? GI->ApiClient->BaseUrl : TEXT("\u2014");
    DebugInfo.bFlaskOnline      = GI->bFlaskOnline;
    DebugInfo.ActiveProjectId   = GI->ActiveProjectId;
    DebugInfo.ActiveProjectName = GI->ActiveProjectName;
    DebugInfo.QueuedRequests    = GI->GetQueuedRequestCount();
    DebugInfo.PollInterval      = GI->EventPoller ? GI->EventPoller->PipelinePollInterval : 0.0f;

    if (GI->EventPoller)
    {
        DebugInfo.PipelineStage  = GI->EventPoller->LastKnownStage;
        DebugInfo.bPipelineActive = GI->EventPoller->bLastKnownActive;
    }

    // Frame rate
    DebugInfo.FPS = GEngine ? (1.0f / FMath::Max(FApp::GetDeltaTime(), SMALL_NUMBER)) : 0.0f;

    // Timestamp
    DebugInfo.LastPollTime = FDateTime::Now().ToString(TEXT("%H:%M:%S"));

    OnDebugInfoRefreshed(DebugInfo);
}

// -------------------------------------------------------
// BlueprintNativeEvent default implementation
// -------------------------------------------------------

void UHJDebugWidget::OnDebugInfoRefreshed_Implementation(const FHJDebugInfo& Info)
{
    if (FlaskUrlText)
    {
        FlaskUrlText->SetText(FText::FromString(FString::Printf(TEXT("Flask: %s"), *Info.FlaskUrl)));
    }

    if (FlaskOnlineText)
    {
        FlaskOnlineText->SetText(FText::FromString(
            FString::Printf(TEXT("Status: %s"), Info.bFlaskOnline ? TEXT("Online") : TEXT("Offline"))));
        FlaskOnlineText->SetColorAndOpacity(FSlateColor(
            Info.bFlaskOnline ? FLinearColor(0.3f, 1.0f, 0.3f) : FLinearColor(1.0f, 0.3f, 0.3f)));
    }

    if (ProjectIdText)
    {
        ProjectIdText->SetText(FText::FromString(
            FString::Printf(TEXT("Project ID: %s"), *Info.ActiveProjectId)));
    }

    if (ProjectNameText)
    {
        ProjectNameText->SetText(FText::FromString(
            FString::Printf(TEXT("Project: %s"), *Info.ActiveProjectName)));
    }

    if (QueuedText)
    {
        QueuedText->SetText(FText::FromString(
            FString::Printf(TEXT("Queued Requests: %d"), Info.QueuedRequests)));
    }

    if (PollTimeText)
    {
        PollTimeText->SetText(FText::FromString(
            FString::Printf(TEXT("Last Poll: %s"), *Info.LastPollTime)));
    }

    if (PollIntervalText)
    {
        PollIntervalText->SetText(FText::FromString(
            FString::Printf(TEXT("Poll Interval: %.1fs"), Info.PollInterval)));
    }

    if (FPSText)
    {
        FPSText->SetText(FText::FromString(
            FString::Printf(TEXT("FPS: %.0f"), Info.FPS)));

        // Color code: green > 30, yellow > 15, red otherwise
        FLinearColor FpsColor;
        if (Info.FPS > 30.0f)
            FpsColor = FLinearColor(0.3f, 1.0f, 0.3f);
        else if (Info.FPS > 15.0f)
            FpsColor = FLinearColor(1.0f, 1.0f, 0.3f);
        else
            FpsColor = FLinearColor(1.0f, 0.3f, 0.3f);
        FPSText->SetColorAndOpacity(FSlateColor(FpsColor));
    }

    if (PipelineStageText)
    {
        PipelineStageText->SetText(FText::FromString(
            FString::Printf(TEXT("Stage: %s"), *Info.PipelineStage)));
    }

    if (PipelineActiveText)
    {
        PipelineActiveText->SetText(FText::FromString(
            FString::Printf(TEXT("Pipeline: %s"), Info.bPipelineActive ? TEXT("Active") : TEXT("Idle"))));
        PipelineActiveText->SetColorAndOpacity(FSlateColor(
            Info.bPipelineActive ? FLinearColor(0.3f, 1.0f, 0.3f) : FLinearColor(0.6f, 0.6f, 0.6f)));
    }
}
