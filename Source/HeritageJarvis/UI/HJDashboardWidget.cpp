#include "HJDashboardWidget.h"
#include "WebBrowser.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Spacer.h"
#include "Styling/CoreStyle.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "TimerManager.h"
#include "Async/Async.h"

void UHJDashboardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree) return;

	UPanelWidget* ExistingRoot = Cast<UPanelWidget>(WidgetTree->RootWidget);
	if (ExistingRoot && ExistingRoot->GetChildrenCount() > 0) return;

	BuildLayout();
}

void UHJDashboardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Start health polling
	CheckFlaskHealth();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			HealthTimerHandle,
			FTimerDelegate::CreateUObject(this, &UHJDashboardWidget::CheckFlaskHealth),
			5.0f,
			true
		);

		// Fallback timer: if browser hasn't loaded in 10s, show fallback button
		World->GetTimerManager().SetTimer(
			FallbackTimerHandle,
			FTimerDelegate::CreateUObject(this, &UHJDashboardWidget::OnFallbackTimeout),
			10.0f,
			false
		);
	}
}

void UHJDashboardWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealthTimerHandle);
		World->GetTimerManager().ClearTimer(FallbackTimerHandle);
	}
	Super::NativeDestruct();
}

void UHJDashboardWidget::NavigateTo(const FString& Url)
{
	if (WebBrowserWidget)
	{
		WebBrowserWidget->LoadURL(Url);
	}
}

void UHJDashboardWidget::BuildLayout()
{
	// --- Root overlay (fills screen) ---
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(), FName("DashRoot"));
	WidgetTree->RootWidget = Root;

	// =====================================================================
	// Layer 1: UWebBrowser — DIRECT child of UOverlay (critical CEF fix)
	// =====================================================================
	WebBrowserWidget = WidgetTree->ConstructWidget<UWebBrowser>(
		UWebBrowser::StaticClass(), FName("DashWebBrowser"));
	WebBrowserWidget->LoadURL(TEXT("http://127.0.0.1:5000?embedded=1"));
	Root->AddChild(WebBrowserWidget);

	if (UOverlaySlot* BrowserSlot = Cast<UOverlaySlot>(WebBrowserWidget->Slot))
	{
		BrowserSlot->SetHorizontalAlignment(HAlign_Fill);
		BrowserSlot->SetVerticalAlignment(VAlign_Fill);
		BrowserSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 40.f)); // 40px bottom for status bar
	}

	// =====================================================================
	// Layer 2: Status bar (40px tall, anchored to bottom)
	// =====================================================================
	UBorder* StatusBar = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), FName("StatusBar"));
	StatusBar->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.12f, 0.95f));
	StatusBar->SetPadding(FMargin(12.f, 6.f, 12.f, 6.f));
	Root->AddChild(StatusBar);

	if (UOverlaySlot* BarSlot = Cast<UOverlaySlot>(StatusBar->Slot))
	{
		BarSlot->SetHorizontalAlignment(HAlign_Fill);
		BarSlot->SetVerticalAlignment(VAlign_Bottom);
	}

	// HBox inside status bar
	UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), FName("StatusHBox"));
	StatusBar->AddChild(HBox);

	// --- Status dot (colored indicator) ---
	StatusDot = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), FName("StatusDot"));
	StatusDot->SetBrushColor(FLinearColor(0.9f, 0.4f, 0.3f)); // red = offline initially
	HBox->AddChild(StatusDot);
	if (UHorizontalBoxSlot* DotSlot = Cast<UHorizontalBoxSlot>(StatusDot->Slot))
	{
		DotSlot->SetVerticalAlignment(VAlign_Center);
		DotSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		DotSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	// --- Status label ---
	StatusLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), FName("StatusLabel"));
	StatusLabel->SetText(FText::FromString(TEXT("Connecting to Flask...")));
	StatusLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.7f)));
	StatusLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 12));
	HBox->AddChild(StatusLabel);
	if (UHorizontalBoxSlot* LblSlot = Cast<UHorizontalBoxSlot>(StatusLabel->Slot))
	{
		LblSlot->SetVerticalAlignment(VAlign_Center);
		LblSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	// --- Fill spacer ---
	USpacer* FillSpacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), FName("FillSpacer"));
	HBox->AddChild(FillSpacer);
	if (UHorizontalBoxSlot* SpSlot = Cast<UHorizontalBoxSlot>(FillSpacer->Slot))
	{
		SpSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// --- Fallback "Open in Browser" button (hidden initially) ---
	FallbackButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), FName("FallbackBtn"));
	FallbackButton->SetBackgroundColor(FLinearColor(0.15f, 0.2f, 0.4f, 0.9f));
	FallbackButton->SetVisibility(ESlateVisibility::Collapsed);
	{
		UTextBlock* FbLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), FName("FallbackLabel"));
		FbLabel->SetText(FText::FromString(TEXT("Open in Browser")));
		FbLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.8f, 1.0f)));
		FbLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 11));
		FallbackButton->AddChild(FbLabel);
	}
	HBox->AddChild(FallbackButton);
	FallbackButton->OnClicked.AddDynamic(this, &UHJDashboardWidget::OnOpenInBrowserClicked);
	if (UHorizontalBoxSlot* FbSlot = Cast<UHorizontalBoxSlot>(FallbackButton->Slot))
	{
		FbSlot->SetVerticalAlignment(VAlign_Center);
		FbSlot->SetPadding(FMargin(8.f, 0.f, 8.f, 0.f));
		FbSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	// --- Enter Tartaria button ---
	UButton* TartariaBtn = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), FName("TartariaBtn"));
	TartariaBtn->SetBackgroundColor(FLinearColor(0.1f, 0.35f, 0.15f, 0.9f));
	{
		UTextBlock* TLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), FName("TartariaLabel"));
		TLabel->SetText(FText::FromString(TEXT("  Enter Tartaria  ")));
		TLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		TLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 13));
		TartariaBtn->AddChild(TLabel);
	}
	HBox->AddChild(TartariaBtn);
	TartariaBtn->OnClicked.AddDynamic(this, &UHJDashboardWidget::OnEnterTartariaClicked);
	if (UHorizontalBoxSlot* TSlot = Cast<UHorizontalBoxSlot>(TartariaBtn->Slot))
	{
		TSlot->SetVerticalAlignment(VAlign_Center);
		TSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
}

void UHJDashboardWidget::CheckFlaskHealth()
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(TEXT("http://127.0.0.1:5000/health"));
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetTimeout(2.0f);

	TWeakObjectPtr<UHJDashboardWidget> WeakThis(this);

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[WeakThis](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected)
		{
			AsyncTask(ENamedThreads::GameThread, [WeakThis, bConnected, Response]()
			{
				UHJDashboardWidget* Self = WeakThis.Get();
				if (!Self) return;

				bool bOk = bConnected && Response.IsValid() && Response->GetResponseCode() == 200;
				Self->bFlaskOnline = bOk;

				if (bOk)
				{
					// Cancel fallback timer on successful connection
					if (!Self->bBrowserLoaded)
					{
						Self->bBrowserLoaded = true;
						if (UWorld* World = Self->GetWorld())
						{
							World->GetTimerManager().ClearTimer(Self->FallbackTimerHandle);
						}
					}

					if (Self->StatusLabel)
					{
						Self->StatusLabel->SetText(FText::FromString(TEXT("Flask: Online")));
						Self->StatusLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.9f, 0.4f)));
					}
					if (Self->StatusDot)
						Self->StatusDot->SetBrushColor(FLinearColor(0.3f, 0.9f, 0.4f));
				}
				else
				{
					if (Self->StatusLabel)
					{
						Self->StatusLabel->SetText(FText::FromString(TEXT("Flask: Offline")));
						Self->StatusLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.4f, 0.3f)));
					}
					if (Self->StatusDot)
						Self->StatusDot->SetBrushColor(FLinearColor(0.9f, 0.4f, 0.3f));
				}
			});
		});

	HttpRequest->ProcessRequest();
}

void UHJDashboardWidget::OnFallbackTimeout()
{
	if (bBrowserLoaded) return;

	UE_LOG(LogTemp, Warning, TEXT("HJDashboardWidget: Browser load timeout — showing fallback"));

	if (FallbackButton)
		FallbackButton->SetVisibility(ESlateVisibility::Visible);

	if (StatusLabel)
		StatusLabel->SetText(FText::FromString(TEXT("CEF failed to load — use fallback")));
}

void UHJDashboardWidget::OnEnterTartariaClicked()
{
	UGameplayStatics::OpenLevel(this, FName(TEXT("TartariaWorld")));
}

void UHJDashboardWidget::OnOpenInBrowserClicked()
{
	FPlatformProcess::LaunchURL(TEXT("http://127.0.0.1:5000"), nullptr, nullptr);
}
