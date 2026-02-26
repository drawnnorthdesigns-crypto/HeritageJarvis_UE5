#include "HJMainGameMode.h"
#include "HJDashboardWidget.h"
#include "HJLoadingWidget.h"
#include "Core/HJGameInstance.h"
#include "Kismet/GameplayStatics.h"

AHJMainGameMode::AHJMainGameMode()
{
	DefaultPawnClass = nullptr;
}

void AHJMainGameMode::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	// Show mouse cursor, UI-only input mode
	PC->SetShowMouseCursor(true);
	PC->SetInputMode(FInputModeUIOnly());

	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI) return;

	// If Flask is already online (e.g. returning from Tartaria), skip loading
	if (GI->bFlaskOnline)
	{
		OnFlaskReady();
		return;
	}

	// Show loading screen while Flask starts up
	UHJLoadingWidget::Show(this, TEXT("Starting Heritage Jarvis backend..."));

	// Subscribe to OnFlaskReady to dismiss loading and show dashboard
	GI->OnFlaskReady.AddDynamic(this, &AHJMainGameMode::OnFlaskReady);
}

void AHJMainGameMode::OnFlaskReady()
{
	// Hide loading screen
	UHJLoadingWidget::Hide(this);

	// Spawn the CEF dashboard widget
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	DashboardWidget = CreateWidget<UHJDashboardWidget>(PC);
	if (DashboardWidget)
	{
		DashboardWidget->AddToViewport(0);
	}

	UE_LOG(LogTemp, Log, TEXT("HJMainGameMode: Flask ready — dashboard spawned"));
}
