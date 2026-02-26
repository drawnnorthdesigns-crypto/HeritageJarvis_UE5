#include "HJSettingsWidget.h"
#include "Core/HJGameInstance.h"
#include "Core/HJSaveGame.h"
#include "Kismet/GameplayStatics.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Border.h"
#include "Components/Spacer.h"
#include "Styling/CoreStyle.h"

// -----------------------------------------------------------------------------
// Programmatic layout
// -----------------------------------------------------------------------------

void UHJSettingsWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!WidgetTree) return;

    // Skip if a Blueprint child already placed content
    UPanelWidget* ExistingRoot = Cast<UPanelWidget>(WidgetTree->RootWidget);
    if (ExistingRoot && ExistingRoot->GetChildrenCount() > 0) return;

    BuildProgrammaticLayout();
}

void UHJSettingsWidget::BuildProgrammaticLayout()
{
    // --- Root overlay ---
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), FName("SettingsRoot"));
    WidgetTree->RootWidget = Root;

    UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName("SettingsVBox"));
    Root->AddChild(VBox);

    // --- Title ---
    {
        UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("SettingsTitle"));
        Title->SetText(FText::FromString(TEXT("Settings")));
        Title->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f)));
        FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 18);
        Title->SetFont(TitleFont);
        VBox->AddChild(Title);
    }

    // --- Spacer 12px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpTitle"));
        Sp->SetSize(FVector2D(0, 12));
        VBox->AddChild(Sp);
    }

    // --- "Flask Server URL" label ---
    {
        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("FlaskUrlLabel"));
        Label->SetText(FText::FromString(TEXT("Flask Server URL")));
        Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f)));
        FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 14);
        Label->SetFont(LabelFont);
        VBox->AddChild(Label);
    }

    // --- Spacer 4px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpLabel"));
        Sp->SetSize(FVector2D(0, 4));
        VBox->AddChild(Sp);
    }

    // --- Flask URL input ---
    FlaskUrlInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), FName("FlaskUrlInput"));
    FlaskUrlInput->SetHintText(FText::FromString(TEXT("http://127.0.0.1:5000")));
    VBox->AddChild(FlaskUrlInput);

    // --- Spacer 8px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpInput"));
        Sp->SetSize(FVector2D(0, 8));
        VBox->AddChild(Sp);
    }

    // --- Button row ---
    {
        UHorizontalBox* BtnRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName("BtnRow"));
        VBox->AddChild(BtnRow);

        // Test Connection button
        TestBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName("TestBtn"));
        TestBtn->SetBackgroundColor(FLinearColor(0.2f, 0.5f, 0.8f));
        {
            UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("TestBtnLabel"));
            BtnLabel->SetText(FText::FromString(TEXT("Test Connection")));
            BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f)));
            FSlateFontInfo BtnFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
            BtnLabel->SetFont(BtnFont);
            TestBtn->AddChild(BtnLabel);
        }
        BtnRow->AddChild(TestBtn);
        TestBtn->OnClicked.AddDynamic(this, &UHJSettingsWidget::OnTestBtnClicked);

        // Spacer 8px
        USpacer* SpBtn1 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpBtn1"));
        SpBtn1->SetSize(FVector2D(8, 0));
        BtnRow->AddChild(SpBtn1);

        // Apply button
        ApplyBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName("ApplyBtn"));
        ApplyBtn->SetBackgroundColor(FLinearColor(0.2f, 0.6f, 0.3f));
        {
            UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("ApplyBtnLabel"));
            BtnLabel->SetText(FText::FromString(TEXT("Apply")));
            BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f)));
            FSlateFontInfo BtnFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
            BtnLabel->SetFont(BtnFont);
            ApplyBtn->AddChild(BtnLabel);
        }
        BtnRow->AddChild(ApplyBtn);
        ApplyBtn->OnClicked.AddDynamic(this, &UHJSettingsWidget::OnApplyBtnClicked);

        // Spacer 8px
        USpacer* SpBtn2 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpBtn2"));
        SpBtn2->SetSize(FVector2D(8, 0));
        BtnRow->AddChild(SpBtn2);

        // Reset Default button
        ResetBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName("ResetBtn"));
        ResetBtn->SetBackgroundColor(FLinearColor(0.4f, 0.4f, 0.4f));
        {
            UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("ResetBtnLabel"));
            BtnLabel->SetText(FText::FromString(TEXT("Reset Default")));
            BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f)));
            FSlateFontInfo BtnFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
            BtnLabel->SetFont(BtnFont);
            ResetBtn->AddChild(BtnLabel);
        }
        BtnRow->AddChild(ResetBtn);
        ResetBtn->OnClicked.AddDynamic(this, &UHJSettingsWidget::OnResetBtnClicked);
    }

    // --- Spacer 8px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpBtnRow"));
        Sp->SetSize(FVector2D(0, 8));
        VBox->AddChild(Sp);
    }

    // --- Connection status text (initially collapsed) ---
    ConnectionStatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("ConnectionStatusText"));
    ConnectionStatusText->SetText(FText::GetEmpty());
    ConnectionStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 12);
        ConnectionStatusText->SetFont(Font);
    }
    ConnectionStatusText->SetVisibility(ESlateVisibility::Collapsed);
    VBox->AddChild(ConnectionStatusText);

    // --- Spacer 16px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpStatus"));
        Sp->SetSize(FVector2D(0, 16));
        VBox->AddChild(Sp);
    }

    // --- Current URL status text ---
    StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("StatusText"));
    StatusText->SetText(FText::FromString(TEXT("Current: http://127.0.0.1:5000")));
    StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)));
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 11);
        StatusText->SetFont(Font);
    }
    VBox->AddChild(StatusText);
}

// -----------------------------------------------------------------------------
// Button handlers
// -----------------------------------------------------------------------------

void UHJSettingsWidget::OnTestBtnClicked()
{
    TestConnection();
}

void UHJSettingsWidget::OnApplyBtnClicked()
{
    if (FlaskUrlInput)
    {
        ApplyFlaskUrl(FlaskUrlInput->GetText().ToString());
    }
}

void UHJSettingsWidget::OnResetBtnClicked()
{
    ResetToDefaults();

    if (FlaskUrlInput)
    {
        FlaskUrlInput->SetText(FText::FromString(TEXT("http://127.0.0.1:5000")));
    }
}

// -----------------------------------------------------------------------------
// BlueprintNativeEvent _Implementation methods
// -----------------------------------------------------------------------------

void UHJSettingsWidget::OnSettingsLoaded_Implementation(const FString& FlaskUrl)
{
    if (FlaskUrlInput)
    {
        FlaskUrlInput->SetText(FText::FromString(FlaskUrl));
    }

    if (StatusText)
    {
        StatusText->SetText(FText::FromString(FString::Printf(TEXT("Current: %s"), *FlaskUrl)));
    }
}

void UHJSettingsWidget::OnConnectionTested_Implementation(bool bOk, const FString& Message)
{
    if (ConnectionStatusText)
    {
        ConnectionStatusText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        ConnectionStatusText->SetText(FText::FromString(Message));

        FLinearColor Color = bOk
            ? FLinearColor(0.2f, 0.9f, 0.3f)   // green
            : FLinearColor(0.9f, 0.2f, 0.2f);   // red
        ConnectionStatusText->SetColorAndOpacity(FSlateColor(Color));
    }
}

// -----------------------------------------------------------------------------
// Original logic (preserved)
// -----------------------------------------------------------------------------

void UHJSettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    LoadSettings();
}

void UHJSettingsWidget::LoadSettings()
{
    UHJGameInstance* GI = UHJGameInstance::Get(this);
    if (!GI) return;

    CurrentFlaskUrl = GI->ApiClient->BaseUrl;
    OnSettingsLoaded(CurrentFlaskUrl);
}

void UHJSettingsWidget::ApplyFlaskUrl(const FString& NewUrl)
{
    if (NewUrl.IsEmpty()) return;

    UHJGameInstance* GI = UHJGameInstance::Get(this);
    if (!GI) return;

    // Update live client
    GI->ApiClient->BaseUrl = NewUrl;
    CurrentFlaskUrl = NewUrl;

    // Persist to save game
    GI->SaveSession();
}

void UHJSettingsWidget::TestConnection()
{
    UHJGameInstance* GI = UHJGameInstance::Get(this);
    if (!GI) return;

    // Fix: Weak pointer prevents crash if widget is destroyed before callback fires
    TWeakObjectPtr<UHJSettingsWidget> WeakThis(this);

    FOnApiResponse CB;
    CB.BindLambda([WeakThis](bool bOk, const FString& Body)
    {
        UHJSettingsWidget* Self = WeakThis.Get();
        if (!Self) return;
        Self->bLastConnectionOk = bOk;
        FString Msg = bOk ? TEXT("Connected") : TEXT("Cannot reach Flask server");
        Self->OnConnectionTested(bOk, Msg);
    });
    GI->ApiClient->CheckHealth(CB);
}

void UHJSettingsWidget::ResetToDefaults()
{
    ApplyFlaskUrl(TEXT("http://127.0.0.1:5000"));
}
