#include "HJMainWidget.h"
#include "Core/HJGameInstance.h"
#include "Core/HJSaveGame.h"
#include "HJNotificationWidget.h"
#include "HJDebugWidget.h"
#include "HJLoadingWidget.h"
#include "HJSettingsWidget.h"
#include "HJProjectsWidget.h"
#include "HJLibraryWidget.h"
#include "HJChatWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Spacer.h"
#include "Components/SizeBox.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

// -------------------------------------------------------
// NativeOnInitialized — build programmatic layout if BP is empty
// -------------------------------------------------------

void UHJMainWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree) return;

	// Only build if Blueprint designer didn't provide a layout
	UPanelWidget* ExistingRoot = Cast<UPanelWidget>(WidgetTree->RootWidget);
	if (ExistingRoot && ExistingRoot->GetChildrenCount() > 0) return;

	BuildProgrammaticLayout();
}

// -------------------------------------------------------
// BuildProgrammaticLayout — sidebar + content switcher
// -------------------------------------------------------

void UHJMainWidget::BuildProgrammaticLayout()
{
	// --- Root Overlay ---
	RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), FName("RootOverlay"));
	WidgetTree->RootWidget = RootOverlay;

	// --- Main horizontal split: sidebar | content ---
	UHorizontalBox* MainHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), FName("MainHBox"));
	UOverlaySlot* HBoxSlot = Cast<UOverlaySlot>(RootOverlay->AddChild(MainHBox));
	if (HBoxSlot)
	{
		HBoxSlot->SetHorizontalAlignment(HAlign_Fill);
		HBoxSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// =====================================================
	// LEFT SIDEBAR (220px fixed width via SizeBox + Border)
	// =====================================================

	USizeBox* SidebarSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), FName("SidebarSizeBox"));
	SidebarSizeBox->SetWidthOverride(220.0f);

	UHorizontalBoxSlot* SidebarSlot = MainHBox->AddChildToHorizontalBox(SidebarSizeBox);
	if (SidebarSlot)
	{
		SidebarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		SidebarSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UBorder* SidebarBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), FName("SidebarBorder"));
	SidebarBorder->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.12f, 1.0f));
	SidebarSizeBox->AddChild(SidebarBorder);

	SidebarBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), FName("SidebarBox"));
	SidebarBorder->AddChild(SidebarBox);

	// --- Title text ---
	TitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("HERITAGE JARVIS")));
	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 20);
	TitleText->SetFont(TitleFont);
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	TitleText->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* TitleSlot = SidebarBox->AddChildToVerticalBox(TitleText);
	if (TitleSlot)
	{
		TitleSlot->SetPadding(FMargin(8.0f, 16.0f, 8.0f, 0.0f));
		TitleSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	// --- Subtitle / version ---
	UTextBlock* SubtitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName("SubtitleText"));
	SubtitleText->SetText(FText::FromString(TEXT("AI Sovereign Interface")));
	FSlateFontInfo SubFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	SubtitleText->SetFont(SubFont);
	SubtitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.45f, 0.55f, 1.0f)));
	SubtitleText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* SubSlot = SidebarBox->AddChildToVerticalBox(SubtitleText);
	if (SubSlot) SubSlot->SetPadding(FMargin(8.0f, 2.0f, 8.0f, 0.0f));

	// --- Spacer below subtitle (16px) ---
	USpacer* TitleSpacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), FName("TitleSpacer"));
	TitleSpacer->SetSize(FVector2D(0.0f, 16.0f));
	SidebarBox->AddChildToVerticalBox(TitleSpacer);

	// --- Divider line ---
	UBorder* SidebarDivider = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), FName("SidebarDivider"));
	SidebarDivider->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.28f, 1.0f));
	USizeBox* DividerSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), FName("DividerSize"));
	DividerSize->SetHeightOverride(1.0f);
	DividerSize->AddChild(SidebarDivider);
	UVerticalBoxSlot* DivSlot = SidebarBox->AddChildToVerticalBox(DividerSize);
	if (DivSlot) DivSlot->SetPadding(FMargin(12.0f, 0.0f, 12.0f, 8.0f));

	// --- Lambda helper: create sidebar button ---
	auto CreateSidebarButton = [this](const FName& Name, const FString& Label) -> UButton*
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);

		FButtonStyle BtnStyle;
		FLinearColor DarkBg(0.12f, 0.12f, 0.18f, 1.0f);
		FLinearColor HoverBg(0.18f, 0.18f, 0.26f, 1.0f);
		FLinearColor PressBg(0.22f, 0.22f, 0.32f, 1.0f);

		BtnStyle.Normal.TintColor = FSlateColor(DarkBg);
		BtnStyle.Hovered.TintColor = FSlateColor(HoverBg);
		BtnStyle.Pressed.TintColor = FSlateColor(PressBg);
		Btn->SetStyle(BtnStyle);

		UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), FName(*(Name.ToString() + TEXT("_Label"))));
		BtnLabel->SetText(FText::FromString(Label));
		FSlateFontInfo BtnFont = FCoreStyle::GetDefaultFontStyle("Regular", 14);
		BtnLabel->SetFont(BtnFont);
		BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.9f, 1.0f)));
		Btn->AddChild(BtnLabel);

		UVerticalBoxSlot* BtnSlot = SidebarBox->AddChildToVerticalBox(Btn);
		if (BtnSlot)
		{
			BtnSlot->SetPadding(FMargin(4.0f, 2.0f));
			BtnSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		return Btn;
	};

	// --- Tab buttons ---
	ProjectsTabBtn = CreateSidebarButton(FName("ProjectsTabBtn"), TEXT("Projects"));
	ProjectsTabBtn->OnClicked.AddDynamic(this, &UHJMainWidget::OnProjectsBtnClicked);

	ChatTabBtn = CreateSidebarButton(FName("ChatTabBtn"), TEXT("Chat"));
	ChatTabBtn->OnClicked.AddDynamic(this, &UHJMainWidget::OnChatBtnClicked);

	LibraryTabBtn = CreateSidebarButton(FName("LibraryTabBtn"), TEXT("Library"));
	LibraryTabBtn->OnClicked.AddDynamic(this, &UHJMainWidget::OnLibraryBtnClicked);

	SettingsTabBtn = CreateSidebarButton(FName("SettingsTabBtn"), TEXT("Settings"));
	SettingsTabBtn->OnClicked.AddDynamic(this, &UHJMainWidget::OnSettingsBtnClicked);

	// --- Fill spacer (pushes bottom items down) ---
	USpacer* FillSpacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), FName("FillSpacer"));
	UVerticalBoxSlot* FillSlot = SidebarBox->AddChildToVerticalBox(FillSpacer);
	if (FillSlot)
	{
		FillSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// --- Flask status text ---
	FlaskStatusText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName("FlaskStatusText"));
	FlaskStatusText->SetText(FText::FromString(TEXT("Flask: Checking...")));
	FSlateFontInfo StatusFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);
	FlaskStatusText->SetFont(StatusFont);
	FlaskStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.65f, 1.0f)));

	UVerticalBoxSlot* StatusSlot = SidebarBox->AddChildToVerticalBox(FlaskStatusText);
	if (StatusSlot)
	{
		StatusSlot->SetPadding(FMargin(12.0f, 4.0f));
		StatusSlot->SetHorizontalAlignment(HAlign_Left);
	}

	// --- Spacer above Enter Tartaria (8px) ---
	USpacer* BottomSpacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), FName("BottomSpacer"));
	BottomSpacer->SetSize(FVector2D(0.0f, 8.0f));
	SidebarBox->AddChildToVerticalBox(BottomSpacer);

	// --- Enter Tartaria button ---
	EnterTartariaBtn = CreateSidebarButton(FName("EnterTartariaBtn"), TEXT("Enter Tartaria"));
	EnterTartariaBtn->OnClicked.AddDynamic(this, &UHJMainWidget::OnEnterTartariaBtnClicked);

	// Add bottom padding below Enter Tartaria
	USpacer* EndSpacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), FName("EndSpacer"));
	EndSpacer->SetSize(FVector2D(0.0f, 12.0f));
	SidebarBox->AddChildToVerticalBox(EndSpacer);

	// =====================================================
	// RIGHT CONTENT AREA (fills remaining width)
	// =====================================================

	// Background border for the content area
	UBorder* ContentBg = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), FName("ContentBg"));
	ContentBg->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.08f, 1.0f));
	ContentBg->SetPadding(FMargin(24.0f, 20.0f));

	ContentSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(
		UWidgetSwitcher::StaticClass(), FName("ContentSwitcher"));
	ContentBg->AddChild(ContentSwitcher);

	UHorizontalBoxSlot* ContentSlot = MainHBox->AddChildToHorizontalBox(ContentBg);
	if (ContentSlot)
	{
		ContentSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	// NOTE: ContentSwitcher children (ProjectsPanel, ChatPanel, LibraryPanel, SettingsPanel)
	// are populated in NativeConstruct using CreateWidget (they need a World context).
}

// -------------------------------------------------------
// NativeConstruct — spawn overlays, populate switcher, restore session
// -------------------------------------------------------

void UHJMainWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Auto-wire widget classes by asset path if not already set in editor
	if (!NotificationWidgetClass)
		NotificationWidgetClass = LoadClass<UHJNotificationWidget>(
			nullptr, TEXT("/Game/UI/WBP_Notifications.WBP_Notifications_C"));

	if (!DebugWidgetClass)
		DebugWidgetClass = LoadClass<UHJDebugWidget>(
			nullptr, TEXT("/Game/UI/WBP_Debug.WBP_Debug_C"));

	if (!LoadingWidgetClass)
		LoadingWidgetClass = LoadClass<UHJLoadingWidget>(
			nullptr, TEXT("/Game/UI/WBP_Loading.WBP_Loading_C"));

	if (!SettingsWidgetClass)
		SettingsWidgetClass = LoadClass<UHJSettingsWidget>(
			nullptr, TEXT("/Game/UI/WBP_Settings.WBP_Settings_C"));

	// -------------------------------------------------------
	// Spawn overlay widgets (notification stack, debug, loading)
	// -------------------------------------------------------

	if (NotificationWidgetClass)
	{
		NotifWidget = CreateWidget<UHJNotificationWidget>(GetWorld(), NotificationWidgetClass);
		if (NotifWidget) NotifWidget->AddToViewport(10);
	}

	if (DebugWidgetClass)
	{
		DebugWidget = CreateWidget<UHJDebugWidget>(GetWorld(), DebugWidgetClass);
		if (DebugWidget)
		{
			DebugWidget->AddToViewport(99);
			DebugWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (LoadingWidgetClass)
	{
		LoadingWidget = CreateWidget<UHJLoadingWidget>(GetWorld(), LoadingWidgetClass);
		if (LoadingWidget)
		{
			LoadingWidget->AddToViewport(50);
			LoadingWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// -------------------------------------------------------
	// Populate ContentSwitcher with tab sub-widgets
	// -------------------------------------------------------

	if (ContentSwitcher)
	{
		// Auto-wire widget classes
		if (!ProjectsWidgetClass)
			ProjectsWidgetClass = LoadClass<UHJProjectsWidget>(
				nullptr, TEXT("/Game/UI/WBP_Projects.WBP_Projects_C"));

		if (!ChatWidgetClass)
			ChatWidgetClass = LoadClass<UHJChatWidget>(
				nullptr, TEXT("/Game/UI/WBP_Chat.WBP_Chat_C"));

		if (!LibraryWidgetClass)
			LibraryWidgetClass = LoadClass<UHJLibraryWidget>(
				nullptr, TEXT("/Game/UI/WBP_Library.WBP_Library_C"));

		// SettingsWidgetClass already auto-wired above

		// Index 0: Projects
		if (ProjectsWidgetClass)
		{
			ProjectsPanel = CreateWidget<UHJProjectsWidget>(GetWorld(), ProjectsWidgetClass);
			if (ProjectsPanel) ContentSwitcher->AddChild(ProjectsPanel);
		}

		// Index 1: Chat
		if (ChatWidgetClass)
		{
			ChatPanel = CreateWidget<UHJChatWidget>(GetWorld(), ChatWidgetClass);
			if (ChatPanel) ContentSwitcher->AddChild(ChatPanel);
		}

		// Index 2: Library
		if (LibraryWidgetClass)
		{
			LibraryPanel = CreateWidget<UHJLibraryWidget>(GetWorld(), LibraryWidgetClass);
			if (LibraryPanel) ContentSwitcher->AddChild(LibraryPanel);
		}

		// Index 3: Settings
		if (SettingsWidgetClass)
		{
			SettingsPanel = CreateWidget<UHJSettingsWidget>(GetWorld(), SettingsWidgetClass);
			if (SettingsPanel) ContentSwitcher->AddChild(SettingsPanel);
		}
	}

	// -------------------------------------------------------
	// Restore session
	// -------------------------------------------------------

	if (UHJGameInstance* GI = UHJGameInstance::Get(this))
	{
		ActiveProjectId   = GI->ActiveProjectId;
		ActiveProjectName = GI->ActiveProjectName;

		// Subscribe to EventPoller for live health updates
		if (GI->EventPoller)
		{
			GI->EventPoller->OnHealthStatus.AddDynamic(
				this, &UHJMainWidget::OnHealthStatusFromPoller);
		}

		// Restore last active tab
		FString LastTab = TEXT("projects");
		// Read from save game via GameInstance (it loaded on Init)
		if (UHJSaveGame* Save = Cast<UHJSaveGame>(
			UGameplayStatics::LoadGameFromSlot(UHJSaveGame::SlotName, UHJSaveGame::UserIndex)))
		{
			LastTab = Save->LastActiveTab;
		}

		if (LastTab == TEXT("chat")) ShowChatTab();
		else if (LastTab == TEXT("library")) ShowLibraryTab();
		else if (LastTab == TEXT("settings")) ShowSettingsTab();
		else ShowProjectsTab();

		// Toast restored project context
		if (!ActiveProjectName.IsEmpty())
		{
			UHJNotificationWidget::Toast(
				FString::Printf(TEXT("Resumed: %s"), *ActiveProjectName),
				EHJNotifType::Info
			);
		}
	}
	else
	{
		ShowProjectsTab();
	}

	// -------------------------------------------------------
	// Health polling (backup to EventPoller)
	// -------------------------------------------------------

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			StatusTimer,
			this,
			&UHJMainWidget::CheckFlaskHealth,
			30.0f,
			true,
			1.0f
		);
	}
	CheckFlaskHealth();
}

// -------------------------------------------------------
// Tab switching
// -------------------------------------------------------

void UHJMainWidget::ShowProjectsTab()
{
	ActiveTab = TEXT("projects");
	if (UHJGameInstance* GI = UHJGameInstance::Get(this)) GI->SetLastTab(ActiveTab);
	BP_ShowProjectsTab();
}

void UHJMainWidget::ShowChatTab()
{
	ActiveTab = TEXT("chat");
	if (UHJGameInstance* GI = UHJGameInstance::Get(this)) GI->SetLastTab(ActiveTab);
	BP_ShowChatTab();
}

void UHJMainWidget::ShowLibraryTab()
{
	ActiveTab = TEXT("library");
	if (UHJGameInstance* GI = UHJGameInstance::Get(this)) GI->SetLastTab(ActiveTab);
	BP_ShowLibraryTab();
}

void UHJMainWidget::ShowSettingsTab()
{
	ActiveTab = TEXT("settings");
	if (UHJGameInstance* GI = UHJGameInstance::Get(this)) GI->SetLastTab(ActiveTab);
	BP_ShowSettingsTab();
}

// -------------------------------------------------------
// BlueprintNativeEvent _Implementation methods
// -------------------------------------------------------

void UHJMainWidget::BP_ShowProjectsTab_Implementation()
{
	if (ContentSwitcher) ContentSwitcher->SetActiveWidgetIndex(0);
	UpdateSidebarHighlight(ProjectsTabBtn);
}

void UHJMainWidget::BP_ShowChatTab_Implementation()
{
	if (ContentSwitcher) ContentSwitcher->SetActiveWidgetIndex(1);
	UpdateSidebarHighlight(ChatTabBtn);
}

void UHJMainWidget::BP_ShowLibraryTab_Implementation()
{
	if (ContentSwitcher) ContentSwitcher->SetActiveWidgetIndex(2);
	UpdateSidebarHighlight(LibraryTabBtn);
}

void UHJMainWidget::BP_ShowSettingsTab_Implementation()
{
	if (ContentSwitcher) ContentSwitcher->SetActiveWidgetIndex(3);
	UpdateSidebarHighlight(SettingsTabBtn);
}

void UHJMainWidget::OnFlaskStatusChanged_Implementation(bool bOnline)
{
	if (!FlaskStatusText) return;

	if (bOnline)
	{
		FlaskStatusText->SetText(FText::FromString(TEXT("Flask: Online")));
		FlaskStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.85f, 0.3f, 1.0f)));
	}
	else
	{
		FlaskStatusText->SetText(FText::FromString(TEXT("Flask: Offline")));
		FlaskStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.2f, 0.2f, 1.0f)));
	}
}

void UHJMainWidget::OnProjectSelected_Implementation(const FString& ProjectId, const FString& ProjectName)
{
	if (TitleText && !ProjectName.IsEmpty())
	{
		TitleText->SetText(FText::FromString(
			FString::Printf(TEXT("HERITAGE JARVIS\n%s"), *ProjectName)));
	}
	else if (TitleText)
	{
		TitleText->SetText(FText::FromString(TEXT("HERITAGE JARVIS")));
	}
}

// -------------------------------------------------------
// Sidebar highlight helper
// -------------------------------------------------------

void UHJMainWidget::UpdateSidebarHighlight(UButton* ActiveBtn)
{
	const FLinearColor ActiveColor(0.20f, 0.20f, 0.35f, 1.0f);
	const FLinearColor InactiveColor(0.12f, 0.12f, 0.18f, 1.0f);

	TArray<UButton*> AllBtns = { ProjectsTabBtn, ChatTabBtn, LibraryTabBtn, SettingsTabBtn };
	for (UButton* Btn : AllBtns)
	{
		if (!Btn) continue;

		FButtonStyle Style = Btn->GetStyle();
		if (Btn == ActiveBtn)
		{
			Style.Normal.TintColor = FSlateColor(ActiveColor);
		}
		else
		{
			Style.Normal.TintColor = FSlateColor(InactiveColor);
		}
		Btn->SetStyle(Style);
	}
}

// -------------------------------------------------------
// Button click handlers
// -------------------------------------------------------

void UHJMainWidget::OnProjectsBtnClicked()
{
	ShowProjectsTab();
}

void UHJMainWidget::OnChatBtnClicked()
{
	ShowChatTab();
}

void UHJMainWidget::OnLibraryBtnClicked()
{
	ShowLibraryTab();
}

void UHJMainWidget::OnSettingsBtnClicked()
{
	ShowSettingsTab();
}

void UHJMainWidget::OnEnterTartariaBtnClicked()
{
	EnterGameMode(ActiveProjectId);
}

// -------------------------------------------------------
// Project selection — keeps GameInstance + chat in sync
// -------------------------------------------------------

void UHJMainWidget::SetActiveProject(const FString& ProjectId, const FString& ProjectName)
{
	ActiveProjectId   = ProjectId;
	ActiveProjectName = ProjectName;

	if (UHJGameInstance* GI = UHJGameInstance::Get(this))
	{
		GI->ActiveProjectId   = ProjectId;
		GI->ActiveProjectName = ProjectName;
		GI->SaveSession();
	}

	OnProjectSelected(ProjectId, ProjectName);
}

// -------------------------------------------------------
// Enter Tartaria
// -------------------------------------------------------

void UHJMainWidget::EnterGameMode(const FString& ProjectId)
{
	if (UHJGameInstance* GI = UHJGameInstance::Get(this))
	{
		if (!ProjectId.IsEmpty())
		{
			GI->ActiveProjectId   = ProjectId;
			GI->ActiveProjectName = ActiveProjectName;
		}
		GI->SaveSession();
	}

	// Show loading overlay before level travel
	if (LoadingWidget) LoadingWidget->ShowLoading(TEXT("Entering Tartaria…"));
	else UHJLoadingWidget::Show(this, TEXT("Entering Tartaria…"));

	UGameplayStatics::OpenLevel(this, FName(TEXT("TartariaWorld")));
}

// -------------------------------------------------------
// Debug overlay
// -------------------------------------------------------

void UHJMainWidget::ToggleDebugOverlay()
{
	if (DebugWidget) DebugWidget->ToggleDebugOverlay();
}

// -------------------------------------------------------
// Health
// -------------------------------------------------------

void UHJMainWidget::CheckFlaskHealth()
{
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI) return;

	if (!GI->ApiClient) return;

	// Fix: Weak pointer prevents crash if widget is destroyed before callback fires
	TWeakObjectPtr<UHJMainWidget> WeakThis(this);

	FOnApiResponse CB;
	CB.BindLambda([WeakThis](bool bOk, const FString&)
	{
		if (UHJMainWidget* Self = WeakThis.Get())
			Self->OnFlaskStatusChanged(bOk);
	});
	GI->ApiClient->CheckHealth(CB);
}

void UHJMainWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StatusTimer);
	}
	Super::NativeDestruct();
}

void UHJMainWidget::OnHealthStatusFromPoller(bool bOnline, const FString& Message)
{
	OnFlaskStatusChanged(bOnline);
	if (!bOnline)
		UHJNotificationWidget::Toast(
			FString::Printf(TEXT("Flask offline: %s"), *Message), EHJNotifType::Warning);
}
