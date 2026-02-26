#include "TartariaGameMode.h"
#include "TartariaCharacter.h"
#include "TartariaWorldSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "UI/HJHUDWidget.h"
#include "UI/HJPauseWidget.h"
#include "UI/HJNotificationWidget.h"
#include "UI/HJDebugWidget.h"
#include "UI/HJLoadingWidget.h"
#include "UI/HJDashboardWidget.h"
#include "TartariaWorldPopulator.h"

ATartariaGameMode::ATartariaGameMode()
{
	DefaultPawnClass = ATartariaCharacter::StaticClass();

	// Auto-wire widget Blueprint classes by asset path
	static ConstructorHelpers::FClassFinder<UHJHUDWidget> HUDFinder(
		TEXT("/Game/UI/WBP_HUD"));
	if (HUDFinder.Succeeded()) HUDWidgetClass = HUDFinder.Class;

	static ConstructorHelpers::FClassFinder<UHJPauseWidget> PauseFinder(
		TEXT("/Game/UI/WBP_Pause"));
	if (PauseFinder.Succeeded()) PauseWidgetClass = PauseFinder.Class;

	static ConstructorHelpers::FClassFinder<UHJNotificationWidget> NotifFinder(
		TEXT("/Game/UI/WBP_Notifications"));
	if (NotifFinder.Succeeded()) NotificationWidgetClass = NotifFinder.Class;

	static ConstructorHelpers::FClassFinder<UHJDebugWidget> DebugFinder(
		TEXT("/Game/UI/WBP_Debug"));
	if (DebugFinder.Succeeded()) DebugWidgetClass = DebugFinder.Class;
}

void ATartariaGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Hide loading screen now that the level is ready
	UHJLoadingWidget::Hide(this);

	// Spawn HUD (always visible while in-game)
	if (HUDWidgetClass)
	{
		HUDWidget = CreateWidget<UHJHUDWidget>(UGameplayStatics::GetPlayerController(this, 0), HUDWidgetClass);
		if (HUDWidget) HUDWidget->AddToViewport(0);
	}

	// Spawn notification toast overlay (above HUD)
	if (NotificationWidgetClass)
	{
		NotifWidget = CreateWidget<UHJNotificationWidget>(UGameplayStatics::GetPlayerController(this, 0), NotificationWidgetClass);
		if (NotifWidget) NotifWidget->AddToViewport(10);
	}

	// Spawn debug overlay (hidden, above everything)
	if (DebugWidgetClass)
	{
		DebugWidget = CreateWidget<UHJDebugWidget>(UGameplayStatics::GetPlayerController(this, 0), DebugWidgetClass);
		if (DebugWidget)
		{
			DebugWidget->AddToViewport(99);
			DebugWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Subscribe to EventPoller so HUD stays in sync
	if (UHJGameInstance* GI = UHJGameInstance::Get(this))
	{
		if (GI->EventPoller)
		{
			GI->EventPoller->OnPipelineStatus.AddDynamic(
				this, &ATartariaGameMode::OnPipelineStatusUpdate);
			GI->EventPoller->OnHealthStatus.AddDynamic(
				this, &ATartariaGameMode::OnHealthStatusUpdate);
		}

		// Fix 3.5: Subscribe to queue count changes for HUD badge
		GI->OnQueueCountChanged.AddDynamic(this, &ATartariaGameMode::OnQueueCountChanged);

		// Seed HUD with current project info immediately
		if (HUDWidget)
		{
			HUDWidget->SetProjectInfo(
				GI->ActiveProjectName,
				GI->EventPoller ? GI->EventPoller->LastKnownStage : TEXT(""),
				0,
				GI->EventPoller ? GI->EventPoller->bLastKnownActive : false
			);
			HUDWidget->SetFlaskStatus(GI->bFlaskOnline);
		}

		// Welcome toast
		UHJNotificationWidget::Toast(
			FString::Printf(TEXT("Entered Tartaria — Project: %s"),
			                GI->ActiveProjectName.IsEmpty()
			                    ? TEXT("None selected") : *GI->ActiveProjectName),
			EHJNotifType::Info
		);
	}

	// Populate world with biomes, resources, buildings, NPCs
	WorldPopulator = NewObject<UTartariaWorldPopulator>(this);
	WorldPopulator->PopulateWorld(GetWorld());

	// Subscribe to economy/tick delegates from WorldSubsystem
	UTartariaWorldSubsystem* WorldSub = GetWorld()->GetSubsystem<UTartariaWorldSubsystem>();
	if (WorldSub)
	{
		WorldSub->OnGameStateUpdated.AddDynamic(this, &ATartariaGameMode::OnGameStateUpdated);
		WorldSub->OnTickCompleted.AddDynamic(this, &ATartariaGameMode::OnTickCompleted);
	}

	// Set game input mode (lock mouse to game)
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC)
	{
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void ATartariaGameMode::ReturnToMainMenu()
{
	// Show loading screen before level swap
	UHJLoadingWidget::Show(this, TEXT("Returning to Heritage Jarvis…"));
	UGameplayStatics::OpenLevel(this, FName(TEXT("MainMenu")));
}

void ATartariaGameMode::TogglePauseMenu()
{
	bPauseMenuOpen = !bPauseMenuOpen;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	if (bPauseMenuOpen)
	{
		// Spawn pause widget if not already created
		if (!PauseWidget && PauseWidgetClass)
		{
			PauseWidget = CreateWidget<UHJPauseWidget>(UGameplayStatics::GetPlayerController(this, 0), PauseWidgetClass);
			if (PauseWidget)
			{
				// Populate current context
				if (UHJGameInstance* GI = UHJGameInstance::Get(this))
				{
					PauseWidget->ActiveProjectName = GI->ActiveProjectName;
					PauseWidget->PipelineStage =
						GI->EventPoller ? GI->EventPoller->LastKnownStage : TEXT("");
					PauseWidget->bPipelineActive =
						GI->EventPoller ? GI->EventPoller->bLastKnownActive : false;
				}
				PauseWidget->AddToViewport(20);
			}
		}
		else if (PauseWidget)
		{
			// Re-show existing widget
			PauseWidget->SetVisibility(ESlateVisibility::Visible);
		}

		UGameplayStatics::SetGamePaused(this, true);
		PC->SetShowMouseCursor(true);
		PC->SetInputMode(FInputModeUIOnly());
	}
	else
	{
		if (PauseWidget)
			PauseWidget->SetVisibility(ESlateVisibility::Collapsed);

		UGameplayStatics::SetGamePaused(this, false);
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void ATartariaGameMode::ToggleDebugOverlay()
{
	if (DebugWidget) DebugWidget->ToggleDebugOverlay();
}

void ATartariaGameMode::ToggleDashboardOverlay()
{
	bDashboardOpen = !bDashboardOpen;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	if (bDashboardOpen)
	{
		if (!DashOverlayWidget)
		{
			DashOverlayWidget = CreateWidget<UHJDashboardWidget>(PC);
			if (DashOverlayWidget)
				DashOverlayWidget->AddToViewport(15);
		}
		else
		{
			DashOverlayWidget->SetVisibility(ESlateVisibility::Visible);
		}

		PC->SetShowMouseCursor(true);
		PC->SetInputMode(FInputModeGameAndUI());
	}
	else
	{
		if (DashOverlayWidget)
			DashOverlayWidget->SetVisibility(ESlateVisibility::Collapsed);

		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
	}
}

// -------------------------------------------------------
// EventPoller callbacks
// -------------------------------------------------------

void ATartariaGameMode::OnPipelineStatusUpdate(const FString& ProjectId,
                                                const FString& Stage,
                                                int32          StageIndex,
                                                bool           bActive)
{
	if (HUDWidget)
	{
		UHJGameInstance* GI = UHJGameInstance::Get(this);
		HUDWidget->SetProjectInfo(
			GI ? GI->ActiveProjectName : TEXT(""), Stage, StageIndex, bActive);
	}

	// Also keep pause widget in sync
	if (PauseWidget && bPauseMenuOpen)
	{
		PauseWidget->PipelineStage  = Stage;
		PauseWidget->bPipelineActive = bActive;
		PauseWidget->OnPipelineStatusUpdated();
	}

	// Toast on completion
	if (!bActive && !Stage.IsEmpty())
	{
		UHJNotificationWidget::Toast(TEXT("Pipeline complete!"), EHJNotifType::Success);
	}
}

void ATartariaGameMode::OnHealthStatusUpdate(bool bOnline, const FString& Message)
{
	if (HUDWidget) HUDWidget->SetFlaskStatus(bOnline);

	if (!bOnline)
		UHJNotificationWidget::Toast(
			FString::Printf(TEXT("Flask offline: %s"), *Message), EHJNotifType::Warning);
}

void ATartariaGameMode::OnQueueCountChanged(int32 Count)
{
	if (HUDWidget) HUDWidget->SetQueueCount(Count);
}

// -------------------------------------------------------
// Phase 1: Economy / tick delegate handlers
// -------------------------------------------------------

void ATartariaGameMode::OnGameStateUpdated()
{
	if (!HUDWidget) return;

	UTartariaWorldSubsystem* WorldSub = GetWorld()->GetSubsystem<UTartariaWorldSubsystem>();
	if (!WorldSub) return;

	// Extract resource counts from inventory
	int32 Iron = 0, Stone = 0, Knowledge = 0, Crystal = 0;
	for (const FTartariaInventoryItem& Item : WorldSub->Inventory)
	{
		if (Item.ItemId == TEXT("iron"))           Iron += Item.Quantity;
		else if (Item.ItemId == TEXT("stone"))     Stone += Item.Quantity;
		else if (Item.ItemId == TEXT("knowledge")) Knowledge += Item.Quantity;
		else if (Item.ItemId == TEXT("crystal"))   Crystal += Item.Quantity;
	}

	HUDWidget->SetGameEconomy(
		WorldSub->Credits,
		WorldSub->CurrentEra,
		WorldSub->CurrentDay,
		Iron, Stone, Knowledge, Crystal
	);
}

void ATartariaGameMode::OnTickCompleted(const TArray<FTartariaTickEvent>& Events)
{
	for (const FTartariaTickEvent& Evt : Events)
	{
		FString Msg;
		EHJNotifType NotifType = EHJNotifType::Info;

		if (Evt.Type == TEXT("RAID_VICTORY"))
		{
			Msg = FString::Printf(TEXT("Raid repelled! +%d credits"), Evt.Value);
			NotifType = EHJNotifType::Success;
		}
		else if (Evt.Type == TEXT("RAID_DEFEAT"))
		{
			Msg = FString::Printf(TEXT("Raid lost territory: %s"), *Evt.Detail);
			NotifType = EHJNotifType::Error;
		}
		else if (Evt.Type == TEXT("BOOK_COMPLETED"))
		{
			Msg = FString::Printf(TEXT("Manuscript completed (+%d cr)"), Evt.Value);
			NotifType = EHJNotifType::Info;
		}
		else if (Evt.Type == TEXT("ANNEXATION"))
		{
			Msg = FString::Printf(TEXT("Territory annexed: %s"), *Evt.Detail);
			NotifType = EHJNotifType::Warning;
		}
		else if (Evt.Type == TEXT("HABITABLE"))
		{
			Msg = FString::Printf(TEXT("Planet now habitable: %s"), *Evt.Detail);
			NotifType = EHJNotifType::Success;
		}
		else
		{
			Msg = FString::Printf(TEXT("[%s] %s"), *Evt.Type, *Evt.Detail);
		}

		UHJNotificationWidget::Toast(Msg, NotifType, 4.0f);
	}
}

void ATartariaGameMode::OpenDashboardToRoute(const FString& Route)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	// Ensure dashboard widget exists
	if (!DashOverlayWidget)
	{
		DashOverlayWidget = CreateWidget<UHJDashboardWidget>(PC);
		if (DashOverlayWidget)
			DashOverlayWidget->AddToViewport(15);
	}

	if (!DashOverlayWidget) return;

	// Navigate to the building-specific route
	FString Url = FString::Printf(TEXT("http://127.0.0.1:5000%s?embedded=1"), *Route);
	DashOverlayWidget->NavigateTo(Url);
	DashOverlayWidget->SetVisibility(ESlateVisibility::Visible);

	bDashboardOpen = true;
	PC->SetShowMouseCursor(true);
	PC->SetInputMode(FInputModeGameAndUI());
}
