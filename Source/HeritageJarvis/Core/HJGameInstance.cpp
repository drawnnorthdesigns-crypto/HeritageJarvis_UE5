#include "HJGameInstance.h"
#include "HJSaveGame.h"
#include "HJFlaskProcess.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void UHJGameInstance::Init()
{
	Super::Init();

	// Create core systems
	ApiClient    = NewObject<UHJApiClient>(this);
	EventPoller  = NewObject<UHJEventPoller>(this);
	FlaskProcess = NewObject<UHJFlaskProcess>(this);

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
	// Gracefully shut down Flask child process
	if (FlaskProcess)
	{
		FlaskProcess->ShutdownFlask();
	}

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
