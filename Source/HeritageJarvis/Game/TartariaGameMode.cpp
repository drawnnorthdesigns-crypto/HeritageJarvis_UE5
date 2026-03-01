#include "TartariaGameMode.h"
#include "TartariaCharacter.h"
#include "TartariaWorldSubsystem.h"
#include "TartariaBiomeVolume.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "Core/HJGameEventStream.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"
#include "UI/HJHUDWidget.h"
#include "UI/HJPauseWidget.h"
#include "UI/HJNotificationWidget.h"
#include "UI/HJDebugWidget.h"
#include "UI/HJLoadingWidget.h"
#include "UI/HJDashboardWidget.h"
#include "UI/HJThreatWidget.h"
#include "UI/HJInventoryWidget.h"
#include "UI/HJDialogueWidget.h"
#include "TartariaNPC.h"
#include "TartariaForgeBuilding.h"
#include "TartariaWorldPopulator.h"
#include "TartariaQuestMarker.h"
#include "TartariaSolarSystemManager.h"
#include "TartariaLevelStreamManager.h"
#include "TartariaRewardVFX.h"

ATartariaGameMode::ATartariaGameMode()
{
	DefaultPawnClass = ATartariaCharacter::StaticClass();

	// Auto-wire widget Blueprint classes by asset path.
	// If BP asset doesn't exist, fall back to native C++ class so
	// widgets still spawn without requiring editor-created assets.

	static ConstructorHelpers::FClassFinder<UHJHUDWidget> HUDFinder(
		TEXT("/Game/UI/WBP_HUD"));
	HUDWidgetClass = HUDFinder.Succeeded()
		? HUDFinder.Class : UHJHUDWidget::StaticClass();

	static ConstructorHelpers::FClassFinder<UHJPauseWidget> PauseFinder(
		TEXT("/Game/UI/WBP_Pause"));
	PauseWidgetClass = PauseFinder.Succeeded()
		? PauseFinder.Class : UHJPauseWidget::StaticClass();

	static ConstructorHelpers::FClassFinder<UHJNotificationWidget> NotifFinder(
		TEXT("/Game/UI/WBP_Notifications"));
	NotificationWidgetClass = NotifFinder.Succeeded()
		? NotifFinder.Class : UHJNotificationWidget::StaticClass();

	static ConstructorHelpers::FClassFinder<UHJDebugWidget> DebugFinder(
		TEXT("/Game/UI/WBP_Debug"));
	DebugWidgetClass = DebugFinder.Succeeded()
		? DebugFinder.Class : UHJDebugWidget::StaticClass();

	static ConstructorHelpers::FClassFinder<UHJThreatWidget> ThreatFinder(
		TEXT("/Game/UI/WBP_Threat"));
	ThreatWidgetClass = ThreatFinder.Succeeded()
		? ThreatFinder.Class : UHJThreatWidget::StaticClass();

	static ConstructorHelpers::FClassFinder<UHJInventoryWidget> InvFinder(
		TEXT("/Game/UI/WBP_Inventory"));
	InventoryWidgetClass = InvFinder.Succeeded()
		? InvFinder.Class : UHJInventoryWidget::StaticClass();

	static ConstructorHelpers::FClassFinder<UHJDialogueWidget> DlgFinder(
		TEXT("/Game/UI/WBP_Dialogue"));
	DialogueWidgetClass = DlgFinder.Succeeded()
		? DlgFinder.Class : UHJDialogueWidget::StaticClass();
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

	// Spawn solar system manager
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SolarSystem = GetWorld()->SpawnActor<ATartariaSolarSystemManager>(
		ATartariaSolarSystemManager::StaticClass(),
		FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	// Spawn level stream manager (handles body transitions, visual profiles)
	FActorSpawnParameters LSMParams;
	LSMParams.Owner = this;
	LevelStreamManager = GetWorld()->SpawnActor<ATartariaLevelStreamManager>(
		ATartariaLevelStreamManager::StaticClass(),
		FVector::ZeroVector, FRotator::ZeroRotator, LSMParams);

	// Populate world with biomes, resources, buildings, NPCs
	WorldPopulator = NewObject<UTartariaWorldPopulator>(this);
	WorldPopulator->PopulateWorld(GetWorld());

	// Load latest forged model on all forge building pedestals
	for (TActorIterator<ATartariaForgeBuilding> FIt(GetWorld()); FIt; ++FIt)
	{
		ATartariaForgeBuilding* ForgeB = *FIt;
		if (ForgeB->BuildingType == ETartariaBuildingType::Forge)
			ForgeB->LoadLatestForgedModel();
	}

	// Restore saved game state (credits, factions, fleet, etc.)
	if (UHJGameInstance* GI2 = UHJGameInstance::Get(this))
		GI2->LoadGameState();

	// Subscribe to economy/tick delegates from WorldSubsystem
	UTartariaWorldSubsystem* WorldSub = GetWorld()->GetSubsystem<UTartariaWorldSubsystem>();
	if (WorldSub)
	{
		WorldSub->OnGameStateUpdated.AddDynamic(this, &ATartariaGameMode::OnGameStateUpdated);
		WorldSub->OnTickCompleted.AddDynamic(this, &ATartariaGameMode::OnTickCompleted);
	}

	// Subscribe to SSE game event stream (replaces most polling)
	if (UHJGameInstance* GI3 = UHJGameInstance::Get(this))
	{
		if (GI3->GameEventStream)
		{
			GI3->GameEventStream->OnSnapshotReceived.AddDynamic(
				this, &ATartariaGameMode::OnSSESnapshotReceived);
			GI3->GameEventStream->OnTickEventsReceived.AddDynamic(
				this, &ATartariaGameMode::OnSSETickEvents);

			// Enable SSE mode on WorldSubsystem if stream is connected
			if (GI3->GameEventStream->bConnected && WorldSub)
				WorldSub->bUseSSE = true;
		}
	}

	// Spawn threat encounter widget (hidden, above HUD)
	if (ThreatWidgetClass)
	{
		ThreatWidget = CreateWidget<UHJThreatWidget>(
			UGameplayStatics::GetPlayerController(this, 0), ThreatWidgetClass);
		if (ThreatWidget)
		{
			ThreatWidget->AddToViewport(25);
			ThreatWidget->OnEncounterResolved.AddDynamic(
				this, &ATartariaGameMode::OnEncounterResolved);
		}
	}

	// Spawn inventory widget (hidden, toggle with I key)
	if (InventoryWidgetClass)
	{
		InventoryWidget = CreateWidget<UHJInventoryWidget>(
			UGameplayStatics::GetPlayerController(this, 0), InventoryWidgetClass);
		if (InventoryWidget)
			InventoryWidget->AddToViewport(20);
	}

	// Spawn dialogue widget (hidden, shown on NPC interaction)
	if (DialogueWidgetClass)
	{
		DialogueWidget = CreateWidget<UHJDialogueWidget>(
			UGameplayStatics::GetPlayerController(this, 0), DialogueWidgetClass);
		if (DialogueWidget)
		{
			DialogueWidget->AddToViewport(22);

			// Subscribe to action button clicks
			DialogueWidget->OnActionSelected.AddDynamic(
				this, &ATartariaGameMode::OnDialogueActionSelected);
		}
	}

	// Subscribe to all BiomeVolume threat/zone delegates
	SubscribeToBiomeVolumes();

	// Subscribe to all NPC dialogue delegates
	SubscribeToNPCs();

	// Subscribe to quest marker delegates
	SubscribeToQuestMarkers();

	// Subscribe to player health changes
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (ATartariaCharacter* PlayerChar = Cast<ATartariaCharacter>(PlayerPawn))
	{
		PlayerChar->OnHealthChanged.AddDynamic(this, &ATartariaGameMode::OnPlayerHealthChanged);
		PlayerChar->OnPlayerDeath.AddDynamic(this, &ATartariaGameMode::OnPlayerDeath);

		// Seed HUD with initial health
		if (HUDWidget)
		{
			HUDWidget->SetHealthInfo(
				PlayerChar->CurrentHealth,
				PlayerChar->MaxHealth,
				PlayerChar->CurrentBiome,
				1);
		}
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
	// Auto-save full game state before leaving
	if (UHJGameInstance* GI = UHJGameInstance::Get(this))
		GI->SaveGameState();

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
		// Auto-save game state when pausing
		if (UHJGameInstance* GI = UHJGameInstance::Get(this))
			GI->SaveGameState();

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

		// Holographic tilt — slight 3D perspective effect for immersion
		// This gives the Tab-overlay a diegetic "projected hologram" feel
		// rather than a flat 2D overlay that breaks immersion
		if (DashOverlayWidget)
		{
			FWidgetTransform HoloTransform;
			HoloTransform.Scale = FVector2D(0.92f, 0.92f);
			HoloTransform.Shear = FVector2D(0.02f, 0.0f);
			DashOverlayWidget->SetRenderTransform(HoloTransform);
			DashOverlayWidget->SetRenderOpacity(0.92f);
		}

		PC->SetShowMouseCursor(true);
		PC->SetInputMode(FInputModeGameAndUI());
	}
	else
	{
		if (DashOverlayWidget)
		{
			DashOverlayWidget->SetVisibility(ESlateVisibility::Collapsed);

			// Reset holographic transform
			DashOverlayWidget->SetRenderTransform(FWidgetTransform());
			DashOverlayWidget->SetRenderOpacity(1.0f);
		}

		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void ATartariaGameMode::ToggleInventory()
{
	if (!InventoryWidget) return;

	bInventoryOpen = !bInventoryOpen;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	if (bInventoryOpen)
	{
		InventoryWidget->ShowInventory();
		PC->SetShowMouseCursor(true);
		PC->SetInputMode(FInputModeGameAndUI());
	}
	else
	{
		InventoryWidget->HideInventory();
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

	// ── Forge Building: sync activity state + fire stage transitions ──
	UWorld* World = GetWorld();
	if (World)
	{
		for (TActorIterator<ATartariaForgeBuilding> It(World); It; ++It)
		{
			ATartariaForgeBuilding* Forge = *It;
			if (Forge->BuildingType == ETartariaBuildingType::Forge)
			{
				Forge->SetForgeActive(bActive, Stage);

				// Detect stage transition
				if (bActive && !Stage.IsEmpty() && Stage != Forge->CurrentForgeStage)
				{
					Forge->OnStageTransition(Stage, StageIndex);
				}
			}
		}
	}

	// Toast on completion + materialization VFX + load model on pedestal
	if (!bActive && !Stage.IsEmpty())
	{
		UHJNotificationWidget::Toast(TEXT("Pipeline complete!"), EHJNotifType::Success);

		// Spawn materialization VFX at player location (CAD model ready)
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
		if (PlayerPawn)
		{
			ATartariaRewardVFX::SpawnRewardVFX(
				this, PlayerPawn->GetActorLocation(),
				ERewardVFXType::Materialization, 2.5f);
		}

		// Load the forged model onto all Forge building pedestals
		if (World)
		{
			for (TActorIterator<ATartariaForgeBuilding> FIt(World); FIt; ++FIt)
			{
				ATartariaForgeBuilding* ForgeB = *FIt;
				if (ForgeB->BuildingType == ETartariaBuildingType::Forge)
				{
					ForgeB->LoadLatestForgedModel();
				}
			}
		}
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

	// Push faction data to HUD
	if (WorldSub->Factions.Num() > 0)
	{
		HUDWidget->SetFactionInfo(WorldSub->Factions);
	}

	// Push fleet/tech/mining data to HUD
	HUDWidget->SetStrategicInfo(WorldSub->Fleet, WorldSub->Tech, WorldSub->Mining);
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

		// Spawn celebration VFX for significant events
		APawn* EventPawn = UGameplayStatics::GetPlayerPawn(this, 0);
		FVector VFXLoc = EventPawn ? EventPawn->GetActorLocation() : FVector::ZeroVector;

		if (Evt.Type == TEXT("RAID_VICTORY"))
		{
			ATartariaRewardVFX::SpawnRewardVFX(this, VFXLoc, ERewardVFXType::CombatVictory, 2.5f);
		}
		else if (Evt.Type == TEXT("HABITABLE"))
		{
			ATartariaRewardVFX::SpawnRewardVFX(this, VFXLoc, ERewardVFXType::LevelUp, 3.0f);
		}
		else if (Evt.Type == TEXT("BOOK_COMPLETED"))
		{
			ATartariaRewardVFX::SpawnRewardVFX(this, VFXLoc, ERewardVFXType::QuestComplete, 2.0f);
		}
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

// -------------------------------------------------------
// Phase 2: Combat / threat handlers
// -------------------------------------------------------

void ATartariaGameMode::SubscribeToBiomeVolumes()
{
	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<ATartariaBiomeVolume> It(World); It; ++It)
	{
		ATartariaBiomeVolume* Volume = *It;
		Volume->OnThreatDetected.AddDynamic(this, &ATartariaGameMode::OnThreatDetected);
		Volume->OnZoneEntered.AddDynamic(this, &ATartariaGameMode::OnZoneChanged);
	}
}

void ATartariaGameMode::OnThreatDetected(const FTartariaThreatInfo& Threat,
                                          ATartariaBiomeVolume* Volume)
{
	if (ThreatWidget)
	{
		ThreatWidget->ShowEncounter(Threat);

		// Switch to GameAndUI so player can click Fight/Evade
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		if (PC)
		{
			PC->SetShowMouseCursor(true);
			PC->SetInputMode(FInputModeGameAndUI());
		}
	}

	UHJNotificationWidget::Toast(
		FString::Printf(TEXT("Ambush! %s in %s zone"),
			*Threat.EnemyKey.Replace(TEXT("_"), TEXT(" ")),
			*Threat.BiomeKey),
		EHJNotifType::Warning, 3.0f);
}

void ATartariaGameMode::OnZoneChanged(const FString& NewBiome, int32 NewDifficulty)
{
	CurrentZoneDifficulty = NewDifficulty;

	if (HUDWidget)
	{
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
		ATartariaCharacter* PlayerChar = Cast<ATartariaCharacter>(PlayerPawn);
		if (PlayerChar)
		{
			HUDWidget->SetHealthInfo(
				PlayerChar->CurrentHealth,
				PlayerChar->MaxHealth,
				NewBiome,
				NewDifficulty);
		}
	}

	// Toast zone entry for non-safe zones
	if (NewDifficulty > 1)
	{
		UHJNotificationWidget::Toast(
			FString::Printf(TEXT("Entering %s (Danger: %d/5)"), *NewBiome, NewDifficulty),
			NewDifficulty >= 4 ? EHJNotifType::Error : EHJNotifType::Warning,
			2.0f);
	}
}

void ATartariaGameMode::OnEncounterResolved(const FTartariaEncounterResult& Result)
{
	// Apply damage to player
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	ATartariaCharacter* PlayerChar = Cast<ATartariaCharacter>(PlayerPawn);
	if (PlayerChar && Result.DamageTaken > 0)
	{
		PlayerChar->ApplyDamage(static_cast<float>(Result.DamageTaken));
	}

	// Return to game-only input
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC)
	{
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
	}

	// Victory toast with credit reward + VFX celebration
	if (Result.bVictory)
	{
		if (Result.RewardCredits > 0)
		{
			UHJNotificationWidget::Toast(
				FString::Printf(TEXT("Victory! +%d credits"), Result.RewardCredits),
				EHJNotifType::Success, 3.0f);
		}

		// Spawn combat victory VFX at player location
		if (PlayerChar)
		{
			ATartariaRewardVFX::SpawnRewardVFX(
				this, PlayerChar->GetActorLocation(),
				ERewardVFXType::CombatVictory, 2.5f);
		}
	}
}

void ATartariaGameMode::OnPlayerHealthChanged(float NewHealth, float MaxHealthVal)
{
	if (HUDWidget)
	{
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
		ATartariaCharacter* PlayerChar = Cast<ATartariaCharacter>(PlayerPawn);
		FString Zone = PlayerChar ? PlayerChar->CurrentBiome : TEXT("UNKNOWN");
		HUDWidget->SetHealthInfo(NewHealth, MaxHealthVal, Zone, CurrentZoneDifficulty);
	}
}

void ATartariaGameMode::OnPlayerDeath()
{
	UHJNotificationWidget::Toast(
		TEXT("You have fallen! Respawning at Clearinghouse..."),
		EHJNotifType::Error, 5.0f);

	// Respawn: heal to full, teleport to origin (Clearinghouse)
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	ATartariaCharacter* PlayerChar = Cast<ATartariaCharacter>(PlayerPawn);
	if (PlayerChar)
	{
		PlayerChar->bIsDead = false;
		PlayerChar->Heal(PlayerChar->MaxHealth);
		PlayerChar->SetActorLocation(FVector(0.f, 0.f, 200.f));
		PlayerChar->CurrentBiome = TEXT("CLEARINGHOUSE");
	}
}

// -------------------------------------------------------
// Phase 6: NPC dialogue handlers
// -------------------------------------------------------

void ATartariaGameMode::SubscribeToNPCs()
{
	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<ATartariaNPC> It(World); It; ++It)
	{
		ATartariaNPC* NPC = *It;
		NPC->OnDialogueStartedDelegate.AddDynamic(this, &ATartariaGameMode::OnNPCDialogueStarted);
		NPC->OnDialogueReceivedDelegate.AddDynamic(this, &ATartariaGameMode::OnNPCDialogueReceived);
		NPC->OnDialogueErroredDelegate.AddDynamic(this, &ATartariaGameMode::OnNPCDialogueErrored);
	}
}

void ATartariaGameMode::OnNPCDialogueStarted(const FString& NPCName, const FString& Faction)
{
	if (DialogueWidget)
	{
		DialogueWidget->ShowDialogue(NPCName, Faction);

		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		if (PC)
		{
			PC->SetShowMouseCursor(true);
			PC->SetInputMode(FInputModeGameAndUI());
		}
	}
}

void ATartariaGameMode::OnNPCDialogueReceived(const FString& NPCName,
	const FString& ResponseText, const TArray<FString>& Actions)
{
	if (DialogueWidget && DialogueWidget->bIsOpen)
	{
		if (Actions.Num() > 0)
			DialogueWidget->SetResponseWithActions(ResponseText, Actions);
		else
			DialogueWidget->SetResponse(ResponseText);
	}
}

void ATartariaGameMode::OnDialogueActionSelected(const FString& ActionKey)
{
	UE_LOG(LogTemp, Log, TEXT("TartariaGameMode: Dialogue action selected: %s"), *ActionKey);

	// Toast the action for player feedback
	UHJNotificationWidget::Toast(
		FString::Printf(TEXT("Executing: %s"), *ActionKey),
		EHJNotifType::Info, 2.0f);

	// Execute action via Flask API
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI || !GI->ApiClient) return;

	// Find the current NPC's specialist type
	FString SpecialistType = TEXT("steward");
	if (DialogueWidget)
	{
		// Look up the NPC that is currently in dialogue
		for (TActorIterator<ATartariaNPC> It(GetWorld()); It; ++It)
		{
			ATartariaNPC* NPC = *It;
			if (NPC->NPCName == DialogueWidget->CurrentNPCName)
			{
				SpecialistType = NPC->GetSpecialistString();
				break;
			}
		}
	}

	// Build action request JSON
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject());
	Body->SetStringField(TEXT("specialist_type"), SpecialistType);
	Body->SetStringField(TEXT("action"), ActionKey);
	// Params left empty — backend uses defaults

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	TWeakObjectPtr<ATartariaGameMode> WeakThis(this);
	FOnApiResponse CB;
	CB.BindLambda([WeakThis, ActionKey](bool bOk, const FString& RespBody)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bOk, ActionKey]()
		{
			ATartariaGameMode* Self = WeakThis.Get();
			if (!Self) return;

			if (bOk)
			{
				UHJNotificationWidget::Toast(
					FString::Printf(TEXT("Action complete: %s"), *ActionKey),
					EHJNotifType::Success, 2.0f);
			}
			else
			{
				UHJNotificationWidget::Toast(
					FString::Printf(TEXT("Action failed: %s"), *ActionKey),
					EHJNotifType::Error, 3.0f);
			}
		});
	});

	GI->ApiClient->Post(TEXT("/api/game/npc/action"), BodyStr, CB);
}

void ATartariaGameMode::OnNPCDialogueErrored(const FString& NPCName)
{
	if (DialogueWidget && DialogueWidget->bIsOpen)
	{
		DialogueWidget->ShowError(TEXT("Could not reach the NPC. Flask may be offline."));
	}

	// Return to game input after error
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC)
	{
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
	}
}

// -------------------------------------------------------
// Quest marker handlers
// -------------------------------------------------------

void ATartariaGameMode::SubscribeToQuestMarkers()
{
	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<ATartariaQuestMarker> It(World); It; ++It)
	{
		ATartariaQuestMarker* Marker = *It;
		Marker->OnQuestInteractedDelegate.AddDynamic(this, &ATartariaGameMode::OnQuestInteracted);
	}
}

void ATartariaGameMode::OnQuestInteracted(const FString& QuestId, int32 Step)
{
	UE_LOG(LogTemp, Log, TEXT("TartariaGameMode: Quest '%s' interacted, step %d"), *QuestId, Step);

	// Spawn quest-complete VFX at player location
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (PlayerPawn)
	{
		ATartariaRewardVFX::SpawnRewardVFX(
			this, PlayerPawn->GetActorLocation(),
			ERewardVFXType::QuestComplete, 2.0f);
	}

	// Refresh world state after quest interaction to pick up any changes
	UTartariaWorldSubsystem* WorldSub = GetWorld()->GetSubsystem<UTartariaWorldSubsystem>();
	if (WorldSub)
		WorldSub->SyncWithBackend();
}

// -------------------------------------------------------
// SSE game event stream handlers
// -------------------------------------------------------

void ATartariaGameMode::OnSSESnapshotReceived()
{
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI || !GI->GameEventStream || !HUDWidget) return;

	UHJGameEventStream* Stream = GI->GameEventStream;

	// Enable SSE mode on WorldSubsystem (disable polling)
	UTartariaWorldSubsystem* WorldSub = GetWorld()->GetSubsystem<UTartariaWorldSubsystem>();
	if (WorldSub && !WorldSub->bUseSSE)
		WorldSub->bUseSSE = true;

	// Push SSE data into WorldSubsystem so save/load still works
	if (WorldSub)
	{
		WorldSub->Credits    = Stream->Credits;
		WorldSub->CurrentEra = Stream->Era;
		WorldSub->CurrentDay = Stream->CurrentDay;
		WorldSub->TimeOfDay  = Stream->TimeOfDay;
		WorldSub->Phase      = Stream->Phase;
		WorldSub->Factions   = Stream->Factions;
		WorldSub->Inventory  = Stream->Inventory;
		WorldSub->Fleet      = Stream->Fleet;
		WorldSub->Tech       = Stream->Tech;
		WorldSub->Mining     = Stream->Mining;
	}

	// Extract resource counts from SSE inventory
	int32 Iron = 0, Stone = 0, Knowledge = 0, Crystal = 0;
	for (const FTartariaInventoryItem& Item : Stream->Inventory)
	{
		if (Item.ItemId == TEXT("iron"))           Iron += Item.Quantity;
		else if (Item.ItemId == TEXT("stone"))     Stone += Item.Quantity;
		else if (Item.ItemId == TEXT("knowledge")) Knowledge += Item.Quantity;
		else if (Item.ItemId == TEXT("crystal"))   Crystal += Item.Quantity;
	}

	HUDWidget->SetGameEconomy(
		Stream->Credits, Stream->Era, Stream->CurrentDay,
		Iron, Stone, Knowledge, Crystal);

	if (Stream->Factions.Num() > 0)
		HUDWidget->SetFactionInfo(Stream->Factions);

	HUDWidget->SetStrategicInfo(Stream->Fleet, Stream->Tech, Stream->Mining);
}

void ATartariaGameMode::OnSSETickEvents(const TArray<FTartariaTickEvent>& Events)
{
	// Reuse existing tick event handler
	OnTickCompleted(Events);
}
