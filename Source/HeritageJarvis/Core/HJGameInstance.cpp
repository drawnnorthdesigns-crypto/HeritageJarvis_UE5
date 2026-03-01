#include "HJGameInstance.h"
#include "HJWebSocketClient.h"
#include "HJSaveGame.h"
#include "HJFlaskProcess.h"
#include "TartariaSoundManager.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Game/TartariaWorldSubsystem.h"
#include "Game/TartariaCharacter.h"
#include "UI/HJNotificationWidget.h"

void UHJGameInstance::Init()
{
	Super::Init();

	// Create core systems
	ApiClient        = NewObject<UHJApiClient>(this);
	EventPoller      = NewObject<UHJEventPoller>(this);
	FlaskProcess     = NewObject<UHJFlaskProcess>(this);
	GameEventStream  = NewObject<UHJGameEventStream>(this);
	WebSocketClient  = NewObject<UHJWebSocketClient>(this);

	// Load saved session — may override ApiClient->BaseUrl
	LoadSession();

	// Auto-launch Flask backend
	FString ProjectRoot = SaveData ? SaveData->FlaskProjectRoot : TEXT("");
	FlaskProcess->LaunchFlask(TEXT(""), ProjectRoot);

	// Bind EventPoller health events to track Flask state
	EventPoller->OnHealthStatus.AddDynamic(this, &UHJGameInstance::OnHealthResponse_FromPoller);

	// Fix 2.4: Trigger immediate health check when any API call exhausts retries
	ApiClient->OnConnectionExhausted.AddDynamic(this, &UHJGameInstance::OnConnectionExhausted_Trigger);

	// Start health polling only (no project yet)
	// Fix 2.5: EventPoller is the SOLE health checker — no legacy timer
	EventPoller->StartPolling(ApiClient, TEXT(""));

	UE_LOG(LogTemp, Log, TEXT("HJGameInstance ready. Flask: %s | Saved project: %s"),
	       *ApiClient->BaseUrl, *ActiveProjectId);
}

void UHJGameInstance::Shutdown()
{
	// Disconnect WebSocket client
	if (WebSocketClient)
		WebSocketClient->Disconnect();

	// Disconnect SSE stream
	if (GameEventStream)
		GameEventStream->Disconnect();

	// Gracefully shut down Flask child process
	if (FlaskProcess)
		FlaskProcess->ShutdownFlask();

	Super::Shutdown();
}

void UHJGameInstance::RefreshHealth()
{
	if (!ApiClient) return;
	FOnApiResponse CB;
	CB.BindUObject(this, &UHJGameInstance::OnHealthResponse);
	ApiClient->CheckHealth(CB);
}

void UHJGameInstance::OnHealthResponse(bool bSuccess, const FString& Body)
{
	bool bWasOnline = bFlaskOnline;
	bFlaskOnline    = bSuccess;

	if (!bWasOnline && bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("HJGameInstance: Flask came back online — starting gradual drain (%d requests)"),
		       RequestQueue.Num());
		DrainRequestQueue();

		// Fire OnFlaskReady once (for loading screen dismissal)
		if (!bFlaskReadyFired)
		{
			bFlaskReadyFired = true;
			UE_LOG(LogTemp, Log, TEXT("HJGameInstance: Broadcasting OnFlaskReady"));
			OnFlaskReady.Broadcast();

			// Start SSE game event stream
			if (GameEventStream && ApiClient)
				GameEventStream->Connect(ApiClient);

			// Connect WebSocket client for real-time events
			if (WebSocketClient)
			{
				WebSocketClient->Connect();

				// Wire WS data notifications to EventPoller so it can suspend
				// redundant HTTP polling while the WebSocket is delivering data.
				if (EventPoller)
				{
					WebSocketClient->OnDataReceived.AddDynamic(
						EventPoller,
						&UHJEventPoller::OnWebSocketDataReceived);
					UE_LOG(LogTemp, Log,
						TEXT("HJGameInstance: WebSocket OnDataReceived wired to EventPoller"));
				}
			}
		}
	}
}

// Called from EventPoller.OnHealthStatus via AddDynamic
void UHJGameInstance::OnHealthResponse_FromPoller(bool bOnline, const FString& Message)
{
	OnHealthResponse(bOnline, Message);
}

// Fix 2.4: Called when ApiClient exhausts all retries — trigger immediate health check
void UHJGameInstance::OnConnectionExhausted_Trigger(const FString& Endpoint)
{
	UE_LOG(LogTemp, Warning, TEXT("HJGameInstance: Connection exhausted on %s — triggering immediate health check"), *Endpoint);
	RefreshHealth();
}

void UHJGameInstance::StartWatchingPipeline(const FString& ProjectId)
{
	ActiveProjectId = ProjectId;
	SaveSession();
	if (EventPoller)
		EventPoller->StartPolling(ApiClient, ProjectId);
}

// -------------------------------------------------------
// Save / Load
// -------------------------------------------------------

void UHJGameInstance::SaveSession()
{
	if (!SaveData)
		SaveData = Cast<UHJSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UHJSaveGame::StaticClass()));
	if (!SaveData) return;

	SaveData->FlaskBaseUrl         = ApiClient ? ApiClient->BaseUrl : TEXT("http://127.0.0.1:5000");
	SaveData->LastActiveProjectId   = ActiveProjectId;
	SaveData->LastActiveProjectName = ActiveProjectName;

	UGameplayStatics::SaveGameToSlot(SaveData, UHJSaveGame::SlotName, UHJSaveGame::UserIndex);
	UE_LOG(LogTemp, Log, TEXT("HJGameInstance: Session saved."));
}

void UHJGameInstance::LoadSession()
{
	if (UGameplayStatics::DoesSaveGameExist(UHJSaveGame::SlotName, UHJSaveGame::UserIndex))
	{
		SaveData = Cast<UHJSaveGame>(
			UGameplayStatics::LoadGameFromSlot(UHJSaveGame::SlotName, UHJSaveGame::UserIndex));
	}

	if (!SaveData)
	{
		// First launch — create defaults
		SaveData = Cast<UHJSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UHJSaveGame::StaticClass()));
		return;
	}

	// Restore
	if (ApiClient && !SaveData->FlaskBaseUrl.IsEmpty())
		ApiClient->BaseUrl = SaveData->FlaskBaseUrl;

	ActiveProjectId   = SaveData->LastActiveProjectId;
	ActiveProjectName = SaveData->LastActiveProjectName;

	UE_LOG(LogTemp, Log, TEXT("HJGameInstance: Session loaded. Project: %s | Flask: %s"),
	       *ActiveProjectId, ApiClient ? *ApiClient->BaseUrl : TEXT("(no client)"));
}

void UHJGameInstance::SetLastTab(const FString& TabName)
{
	if (SaveData) { SaveData->LastActiveTab = TabName; SaveSession(); }
}

// -------------------------------------------------------
// Full game state save/load
// -------------------------------------------------------

void UHJGameInstance::SaveGameState()
{
	if (!SaveData)
		SaveData = Cast<UHJSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UHJSaveGame::StaticClass()));
	if (!SaveData) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// Capture WorldSubsystem state
	UTartariaWorldSubsystem* WorldSub = World->GetSubsystem<UTartariaWorldSubsystem>();
	if (WorldSub)
	{
		SaveData->Credits    = WorldSub->Credits;
		SaveData->CurrentDay = WorldSub->CurrentDay;
		SaveData->CurrentEra = WorldSub->CurrentEra;
		SaveData->TimeOfDay  = WorldSub->TimeOfDay;

		// Flatten inventory to resource counts
		SaveData->Iron = 0; SaveData->Stone = 0;
		SaveData->Knowledge = 0; SaveData->Crystal = 0;
		for (const FTartariaInventoryItem& Item : WorldSub->Inventory)
		{
			if (Item.ItemId == TEXT("iron"))           SaveData->Iron += Item.Quantity;
			else if (Item.ItemId == TEXT("stone"))     SaveData->Stone += Item.Quantity;
			else if (Item.ItemId == TEXT("knowledge")) SaveData->Knowledge += Item.Quantity;
			else if (Item.ItemId == TEXT("crystal"))   SaveData->Crystal += Item.Quantity;
		}

		// Faction reputations
		SaveData->FactionKeys.Empty();
		SaveData->FactionInfluences.Empty();
		for (const FTartariaFactionInfo& F : WorldSub->Factions)
		{
			SaveData->FactionKeys.Add(F.FactionKey);
			SaveData->FactionInfluences.Add(F.Influence);
		}

		// Fleet
		SaveData->FleetTotalPower   = WorldSub->Fleet.TotalPower;
		SaveData->FleetDeployedZones = WorldSub->Fleet.DeployedZones;

		// Tech
		SaveData->TechUnlockedCount = WorldSub->Tech.UnlockedCount;
		SaveData->HighestTech       = WorldSub->Tech.HighestTech;

		// Mining
		SaveData->TotalMined       = WorldSub->Mining.TotalMined;
		SaveData->AsteroidsScanned = WorldSub->Mining.AsteroidsScanned;
	}

	// Capture player state
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	ATartariaCharacter* PlayerChar = Cast<ATartariaCharacter>(PlayerPawn);
	if (PlayerChar)
	{
		SaveData->LastTartariaLocation = PlayerChar->GetActorLocation();
		SaveData->LastTartariaRotation = PlayerChar->GetActorRotation();
		SaveData->PlayerHealth         = PlayerChar->CurrentHealth;
		SaveData->PlayerBiome          = PlayerChar->CurrentBiome;
		SaveData->CurrentCelestialBody = PlayerChar->CurrentBody;
	}

	// Persist session fields too
	SaveData->FlaskBaseUrl          = ApiClient ? ApiClient->BaseUrl : TEXT("http://127.0.0.1:5000");
	SaveData->LastActiveProjectId   = ActiveProjectId;
	SaveData->LastActiveProjectName = ActiveProjectName;

	bool bSlotSaved = UGameplayStatics::SaveGameToSlot(SaveData, UHJSaveGame::SlotName, UHJSaveGame::UserIndex);

	// Toast + sound feedback for save
	if (bSlotSaved)
	{
		UHJNotificationWidget::Toast(TEXT("Chronicle sealed"), EHJNotifType::Success, 2.0f);
		LastSaveTime = FDateTime::Now();

		// Play seal sound if assigned, otherwise use SoundManager fallback
		if (SealSound)
		{
			UGameplayStatics::PlaySound2D(GetWorld(), SealSound, 0.5f);
		}
		else
		{
			UTartariaSoundManager::PlayForgeSeal(this);
		}
	}

	// Also trigger Python backend save (persists sim state to disk)
	if (ApiClient && bFlaskOnline)
	{
		FOnApiResponse SaveCB;
		SaveCB.BindLambda([](bool bOk, const FString& Resp)
		{
			if (!bOk)
			{
				UHJNotificationWidget::Toast(TEXT("Flask archive unreachable"), EHJNotifType::Warning, 3.0f);
			}
			UE_LOG(LogTemp, Log, TEXT("HJGameInstance: Python save %s"),
				bOk ? TEXT("succeeded") : TEXT("failed (Flask may be offline)"));
		});
		ApiClient->Post(TEXT("/api/game/save"), TEXT("{\"slot\":\"auto\"}"), SaveCB);
	}

	UE_LOG(LogTemp, Log, TEXT("HJGameInstance: Full game state saved (Credits=%d, Day=%d, Era=%s)"),
		SaveData->Credits, SaveData->CurrentDay, *SaveData->CurrentEra);
}

void UHJGameInstance::LoadGameState()
{
	if (!SaveData) LoadSession();
	if (!SaveData)
	{
		UHJNotificationWidget::Toast(TEXT("No chronicle found — starting fresh"), EHJNotifType::Warning, 3.0f);
		return;
	}

	// Validate save data integrity
	if (SaveData->Credits < 0 || SaveData->CurrentEra.IsEmpty())
	{
		UHJNotificationWidget::Toast(TEXT("Chronicle damaged — attempting recovery from backup"), EHJNotifType::Warning, 3.0f);
		// Try loading from Python backup
		if (ApiClient && bFlaskOnline)
		{
			FOnApiResponse RecoverCB;
			RecoverCB.BindLambda([](bool bOk, const FString& Resp)
			{
				if (bOk)
					UHJNotificationWidget::Toast(TEXT("Recovered from backup chronicle"), EHJNotifType::Success, 3.0f);
				else
					UHJNotificationWidget::Toast(TEXT("Recovery failed"), EHJNotifType::Error, 3.0f);
			});
			ApiClient->Post(TEXT("/api/game/recover"), TEXT("{}"), RecoverCB);
		}
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	// Restore WorldSubsystem state
	UTartariaWorldSubsystem* WorldSub = World->GetSubsystem<UTartariaWorldSubsystem>();
	if (WorldSub)
	{
		WorldSub->Credits    = SaveData->Credits;
		WorldSub->CurrentDay = SaveData->CurrentDay;
		WorldSub->CurrentEra = SaveData->CurrentEra;
		WorldSub->TimeOfDay  = SaveData->TimeOfDay;

		// Restore faction reputations
		WorldSub->Factions.Empty();
		int32 FCount = FMath::Min(SaveData->FactionKeys.Num(), SaveData->FactionInfluences.Num());
		for (int32 I = 0; I < FCount; I++)
		{
			FTartariaFactionInfo F;
			F.FactionKey = SaveData->FactionKeys[I];
			F.Influence  = SaveData->FactionInfluences[I];
			WorldSub->Factions.Add(F);
		}

		// Restore fleet/tech/mining
		WorldSub->Fleet.TotalPower   = SaveData->FleetTotalPower;
		WorldSub->Fleet.DeployedZones = SaveData->FleetDeployedZones;
		WorldSub->Tech.UnlockedCount = SaveData->TechUnlockedCount;
		WorldSub->Tech.HighestTech   = SaveData->HighestTech;
		WorldSub->Mining.TotalMined       = SaveData->TotalMined;
		WorldSub->Mining.AsteroidsScanned = SaveData->AsteroidsScanned;

		// Broadcast so HUD refreshes
		WorldSub->OnGameStateUpdated.Broadcast();
	}

	// Restore player state
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	ATartariaCharacter* PlayerChar = Cast<ATartariaCharacter>(PlayerPawn);
	if (PlayerChar)
	{
		if (!SaveData->LastTartariaLocation.IsZero())
			PlayerChar->SetActorLocationAndRotation(
				SaveData->LastTartariaLocation, SaveData->LastTartariaRotation);
		PlayerChar->CurrentHealth = SaveData->PlayerHealth;
		PlayerChar->CurrentBiome  = SaveData->PlayerBiome;
		PlayerChar->CurrentBody   = SaveData->CurrentCelestialBody;
	}

	UE_LOG(LogTemp, Log, TEXT("HJGameInstance: Game state restored (Credits=%d, Day=%d, Era=%s)"),
		SaveData->Credits, SaveData->CurrentDay, *SaveData->CurrentEra);
}

// -------------------------------------------------------
// Offline request queue
// -------------------------------------------------------

void UHJGameInstance::EnqueueRequest(const FString& Endpoint, const FString& Verb,
                                      const FString& Body, FOnApiResponse Callback)
{
	// Fix 2.3: Cap queue at QueueCapacity with FIFO eviction
	while (RequestQueue.Num() >= QueueCapacity)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("HJGameInstance: Queue full (%d/%d) — evicting oldest: %s %s"),
			RequestQueue.Num(), QueueCapacity,
			*RequestQueue[0].Verb, *RequestQueue[0].Endpoint);
		RequestQueue.RemoveAt(0);
	}

	FHJQueuedRequest R;
	R.Endpoint = Endpoint;
	R.Verb     = Verb;
	R.Body     = Body;
	R.Callback = Callback;
	RequestQueue.Add(R);
	UE_LOG(LogTemp, Warning,
	       TEXT("HJGameInstance: Flask offline — queued %s %s (queue size: %d/%d)"),
	       *Verb, *Endpoint, RequestQueue.Num(), QueueCapacity);

	// Fix 3.5: Notify HUD of queue count change
	OnQueueCountChanged.Broadcast(RequestQueue.Num());
}

void UHJGameInstance::DrainRequestQueue()
{
	if (!ApiClient || RequestQueue.IsEmpty()) return;

	// Fix 2.4: Gradual drain — process at most DrainBatchSize per invocation
	int32 BatchCount = FMath::Min(DrainBatchSize, RequestQueue.Num());

	// Take the first batch
	TArray<FHJQueuedRequest> Batch;
	for (int32 i = 0; i < BatchCount; ++i)
	{
		Batch.Add(RequestQueue[0]);
		RequestQueue.RemoveAt(0);
	}

	// Send this batch
	for (auto& R : Batch)
	{
		ApiClient->Post(R.Endpoint, R.Body, R.Callback);
	}
	UE_LOG(LogTemp, Log, TEXT("HJGameInstance: Drained %d requests (%d remaining)."),
	       BatchCount, RequestQueue.Num());

	// Fix 3.5: Notify HUD of queue count change after drain
	OnQueueCountChanged.Broadcast(RequestQueue.Num());

	// If more remain, schedule next batch after DrainIntervalSec
	if (!RequestQueue.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				DrainTimerHandle,
				this,
				&UHJGameInstance::DrainRequestQueue,
				DrainIntervalSec,
				/* bLoop = */ false
			);
		}
	}
}

// -------------------------------------------------------
// Static getter
// -------------------------------------------------------

UHJGameInstance* UHJGameInstance::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject) return nullptr;
	return Cast<UHJGameInstance>(UGameplayStatics::GetGameInstance(WorldContextObject));
}
