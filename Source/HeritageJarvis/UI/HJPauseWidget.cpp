#include "HJPauseWidget.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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
#include "Components/SizeBox.h"
#include "Styling/CoreStyle.h"

// -----------------------------------------------------------------------------
// Programmatic layout
// -----------------------------------------------------------------------------

void UHJPauseWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!WidgetTree) return;

    // Skip if a Blueprint child already placed content
    UPanelWidget* ExistingRoot = Cast<UPanelWidget>(WidgetTree->RootWidget);
    if (ExistingRoot && ExistingRoot->GetChildrenCount() > 0) return;

    BuildProgrammaticLayout();
}

void UHJPauseWidget::BuildProgrammaticLayout()
{
    // --- Root overlay (full screen) ---
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), FName("PauseRoot"));
    WidgetTree->RootWidget = Root;

    // --- Dark semi-transparent background (fills entire screen) ---
    UBorder* DarkBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName("PauseDarkBg"));
    DarkBg->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
    Root->AddChild(DarkBg);

    // --- Centered panel via SizeBox ---
    USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName("PausePanelSize"));
    PanelSize->SetWidthOverride(500);
    PanelSize->SetHeightOverride(450);
    Root->AddChild(PanelSize);
    UOverlaySlot* PanelSlot = Cast<UOverlaySlot>(PanelSize->Slot);
    if (PanelSlot)
    {
        PanelSlot->SetHorizontalAlignment(HAlign_Center);
        PanelSlot->SetVerticalAlignment(VAlign_Center);
    }

    // --- Panel background ---
    UBorder* PanelBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName("PausePanelBg"));
    PanelBg->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.15f, 1.0f));
    PanelBg->SetPadding(FMargin(20.0f));
    PanelSize->AddChild(PanelBg);

    // --- Main vertical box ---
    UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName("PauseVBox"));
    PanelBg->AddChild(VBox);

    // --- "PAUSED" title ---
    {
        UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("PauseTitle"));
        Title->SetText(FText::FromString(TEXT("PAUSED")));
        Title->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f)));
        FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 22);
        Title->SetFont(TitleFont);
        Title->SetJustification(ETextJustify::Center);
        VBox->AddChild(Title);
        UVerticalBoxSlot* TitleSlot = Cast<UVerticalBoxSlot>(Title->Slot);
        if (TitleSlot)
        {
            TitleSlot->SetHorizontalAlignment(HAlign_Center);
        }
    }

    // --- Spacer 12px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpPauseTitle"));
        Sp->SetSize(FVector2D(0, 12));
        VBox->AddChild(Sp);
    }

    // --- Project info text ---
    ProjectInfoText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("ProjectInfoText"));
    ProjectInfoText->SetText(FText::FromString(TEXT("Project: None")));
    ProjectInfoText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f)));
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 14);
        ProjectInfoText->SetFont(Font);
    }
    VBox->AddChild(ProjectInfoText);

    // --- Pipeline status text ---
    PipelineStatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("PipelineStatusText"));
    PipelineStatusText->SetText(FText::FromString(TEXT("Pipeline: Idle")));
    PipelineStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)));
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 12);
        PipelineStatusText->SetFont(Font);
    }
    VBox->AddChild(PipelineStatusText);

    // --- Spacer 16px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpPipelineInfo"));
        Sp->SetSize(FVector2D(0, 16));
        VBox->AddChild(Sp);
    }

    // --- "Quick Chat" label ---
    {
        UTextBlock* ChatLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("ChatLabel"));
        ChatLabel->SetText(FText::FromString(TEXT("Quick Chat")));
        ChatLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.7f, 1.0f)));
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 13);
        ChatLabel->SetFont(Font);
        VBox->AddChild(ChatLabel);
    }

    // --- Spacer 4px ---
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpChatLabel"));
        Sp->SetSize(FVector2D(0, 4));
        VBox->AddChild(Sp);
    }

    // --- Chat input row ---
    {
        UHorizontalBox* ChatRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName("ChatRow"));
        VBox->AddChild(ChatRow);

        // Chat input (fills available space)
        ChatInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), FName("ChatInput"));
        ChatInput->SetHintText(FText::FromString(TEXT("Ask Jarvis...")));
        ChatRow->AddChild(ChatInput);
        UHorizontalBoxSlot* InputSlot = Cast<UHorizontalBoxSlot>(ChatInput->Slot);
        if (InputSlot)
        {
            InputSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        }

        // Send button
        ChatSendBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName("ChatSendBtn"));
        ChatSendBtn->SetBackgroundColor(FLinearColor(0.2f, 0.5f, 0.8f));
        {
            UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("ChatSendBtnLabel"));
            BtnLabel->SetText(FText::FromString(TEXT("Send")));
            BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f)));
            FSlateFontInfo BtnFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
            BtnLabel->SetFont(BtnFont);
            ChatSendBtn->AddChild(BtnLabel);
        }
        ChatRow->AddChild(ChatSendBtn);
        ChatSendBtn->OnClicked.AddDynamic(this, &UHJPauseWidget::OnChatSendBtnClicked);
    }

    // --- Chat response text (initially collapsed) ---
    ChatResponseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("ChatResponseText"));
    ChatResponseText->SetText(FText::GetEmpty());
    ChatResponseText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f)));
    ChatResponseText->SetAutoWrapText(true);
    {
        FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 12);
        ChatResponseText->SetFont(Font);
    }
    ChatResponseText->SetVisibility(ESlateVisibility::Collapsed);
    VBox->AddChild(ChatResponseText);

    // --- Fill spacer (pushes buttons to bottom) ---
    {
        USpacer* SpFill = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpFill"));
        SpFill->SetSize(FVector2D(0, 0));
        VBox->AddChild(SpFill);
        UVerticalBoxSlot* FillSlot = Cast<UVerticalBoxSlot>(SpFill->Slot);
        if (FillSlot)
        {
            FillSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        }
    }

    // --- Bottom button row ---
    {
        UHorizontalBox* BtnRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName("PauseBtnRow"));
        VBox->AddChild(BtnRow);

        // Resume button (accent blue)
        ResumeBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName("ResumeBtn"));
        ResumeBtn->SetBackgroundColor(FLinearColor(0.2f, 0.6f, 1.0f));
        {
            UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("ResumeBtnLabel"));
            BtnLabel->SetText(FText::FromString(TEXT("Resume")));
            BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f)));
            FSlateFontInfo BtnFont = FCoreStyle::GetDefaultFontStyle("Bold", 14);
            BtnLabel->SetFont(BtnFont);
            ResumeBtn->AddChild(BtnLabel);
        }
        BtnRow->AddChild(ResumeBtn);
        ResumeBtn->OnClicked.AddDynamic(this, &UHJPauseWidget::OnResumeBtnClicked);

        // Spacer 12px
        USpacer* SpBtns = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpBtns"));
        SpBtns->SetSize(FVector2D(12, 0));
        BtnRow->AddChild(SpBtns);

        // Dashboard button (opens CEF overlay)
        DashboardBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName("DashboardBtn"));
        DashboardBtn->SetBackgroundColor(FLinearColor(0.15f, 0.35f, 0.55f));
        {
            UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("DashboardBtnLabel"));
            BtnLabel->SetText(FText::FromString(TEXT("Dashboard")));
            BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f)));
            FSlateFontInfo BtnFont = FCoreStyle::GetDefaultFontStyle("Regular", 14);
            BtnLabel->SetFont(BtnFont);
            DashboardBtn->AddChild(BtnLabel);
        }
        BtnRow->AddChild(DashboardBtn);
        DashboardBtn->OnClicked.AddDynamic(this, &UHJPauseWidget::OnDashboardBtnClicked);

        // Spacer 12px
        USpacer* SpBtns2 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("SpBtns2"));
        SpBtns2->SetSize(FVector2D(12, 0));
        BtnRow->AddChild(SpBtns2);

        // Main Menu button (dark background)
        MainMenuBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName("MainMenuBtn"));
        MainMenuBtn->SetBackgroundColor(FLinearColor(0.25f, 0.25f, 0.3f));
        {
            UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("MainMenuBtnLabel"));
            BtnLabel->SetText(FText::FromString(TEXT("Main Menu")));
            BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f)));
            FSlateFontInfo BtnFont = FCoreStyle::GetDefaultFontStyle("Regular", 14);
            BtnLabel->SetFont(BtnFont);
            MainMenuBtn->AddChild(BtnLabel);
        }
        BtnRow->AddChild(MainMenuBtn);
        MainMenuBtn->OnClicked.AddDynamic(this, &UHJPauseWidget::OnMainMenuBtnClicked);
    }
}

// -----------------------------------------------------------------------------
// Button handlers
// -----------------------------------------------------------------------------

void UHJPauseWidget::OnResumeBtnClicked()
{
    ResumeGame();
}

void UHJPauseWidget::OnMainMenuBtnClicked()
{
    ReturnToMainMenu();
}

void UHJPauseWidget::OnChatSendBtnClicked()
{
    if (ChatInput)
    {
        FString Message = ChatInput->GetText().ToString();
        SendQuickChat(Message);
        ChatInput->SetText(FText::GetEmpty());
    }
}

void UHJPauseWidget::OnDashboardBtnClicked()
{
    // Resume game first, then toggle dashboard overlay
    ResumeGame();

    if (AGameModeBase* GM = UGameplayStatics::GetGameMode(this))
    {
        if (UFunction* Fn = GM->FindFunction(TEXT("ToggleDashboardOverlay")))
            GM->ProcessEvent(Fn, nullptr);
    }
}

// -----------------------------------------------------------------------------
// BlueprintNativeEvent _Implementation methods
// -----------------------------------------------------------------------------

void UHJPauseWidget::OnPipelineStatusUpdated_Implementation()
{
    if (ProjectInfoText)
    {
        FString ProjectLabel = ActiveProjectName.IsEmpty()
            ? TEXT("Project: None")
            : FString::Printf(TEXT("Project: %s"), *ActiveProjectName);
        ProjectInfoText->SetText(FText::FromString(ProjectLabel));
    }

    if (PipelineStatusText)
    {
        if (bPipelineActive && !PipelineStage.IsEmpty())
        {
            PipelineStatusText->SetText(FText::FromString(
                FString::Printf(TEXT("Pipeline: %s"), *PipelineStage)));
        }
        else
        {
            PipelineStatusText->SetText(FText::FromString(TEXT("Pipeline: Idle")));
        }
    }
}

void UHJPauseWidget::OnChatResponse_Implementation(const FString& Response)
{
    if (ChatResponseText)
    {
        ChatResponseText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        ChatResponseText->SetText(FText::FromString(FString::Printf(TEXT("Jarvis: %s"), *Response)));
        ChatResponseText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f)));
    }
}

void UHJPauseWidget::OnChatError_Implementation()
{
    if (ChatResponseText)
    {
        ChatResponseText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        ChatResponseText->SetText(FText::FromString(TEXT("Error: Could not reach Flask")));
        ChatResponseText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.2f, 0.2f)));
    }
}

// -----------------------------------------------------------------------------
// Original logic (preserved)
// -----------------------------------------------------------------------------

void UHJPauseWidget::ResumeGame()
{
    // Tell the game mode to un-pause -- find TartariaGameMode
    if (AGameModeBase* GM = UGameplayStatics::GetGameMode(this))
    {
        // TartariaGameMode::TogglePauseMenu() will close this widget
        if (UFunction* Fn = GM->FindFunction(TEXT("TogglePauseMenu")))
            GM->ProcessEvent(Fn, nullptr);
    }
}

void UHJPauseWidget::ReturnToMainMenu()
{
    UGameplayStatics::OpenLevel(this, FName(TEXT("MainMenu")));
}

void UHJPauseWidget::SendQuickChat(const FString& Message)
{
    if (Message.IsEmpty()) return;

    UHJGameInstance* GI = UHJGameInstance::Get(this);
    if (!GI) { OnChatError(); return; }

    // Fix: Weak pointer prevents crash if widget is destroyed before callback fires
    TWeakObjectPtr<UHJPauseWidget> WeakThis(this);

    FOnApiResponse CB;
    CB.BindLambda([WeakThis](bool bOk, const FString& Body)
    {
        UHJPauseWidget* Self = WeakThis.Get();
        if (!Self) return;

        if (!bOk) { Self->OnChatError(); return; }

        TSharedPtr<FJsonObject> Json;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
        if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
        {
            FString Response;
            if (Json->TryGetStringField(TEXT("response"), Response))
            {
                Self->OnChatResponse(Response);
            }
        }
        else Self->OnChatError();
    });

    GI->ApiClient->SendChatMessage(Message, GI->ActiveProjectId, CB);
}
