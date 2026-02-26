#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "HJApiClient.generated.h"

// -------------------------------------------------------
// Delegate types — used for API response callbacks in C++
// -------------------------------------------------------
DECLARE_DELEGATE_TwoParams(FOnApiResponse, bool /*bSuccess*/, const FString& /*ResponseBody*/);

/** Per-token callback for streaming SSE responses (Fix 1.1) */
DECLARE_DELEGATE_OneParam(FOnStreamToken, const FString& /*Token*/);

/** Final callback for streaming completion (Fix 1.1) */
DECLARE_DELEGATE_TwoParams(FOnStreamComplete, const FString& /*State*/, const FString& /*Flags*/);

/** Fix 2.4: Fired when all retries exhausted — triggers immediate health check */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConnectionExhausted, const FString&, Endpoint);

/**
 * UHJApiClient
 * Handles all HTTP communication with the Flask backend (localhost:5000).
 * Lives on the GameInstance so it persists across level changes.
 * Call any method, pass a callback delegate to receive the response.
 *
 * Resilience features:
 *   - All requests have a configurable timeout (default 30s)
 *   - Failed requests auto-retry with exponential backoff (2s/4s/8s)
 *   - Retries only on connection failure or 5xx server errors (not 4xx)
 */
UCLASS(BlueprintType, Blueprintable)
class HERITAGEJARVIS_API UHJApiClient : public UObject
{
	GENERATED_BODY()

public:

	// Base URL of the Flask server — change port here if needed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FString BaseUrl = TEXT("http://127.0.0.1:5000");

	/** HTTP request timeout in seconds. Prevents indefinite hangs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	float RequestTimeoutSec = 30.0f;

	/** Maximum retry attempts on network failure or 5xx error. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	int32 MaxRetries = 3;

	/** Base delay for exponential backoff (seconds). Actual delay = Base * 2^attempt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	float RetryBaseDelaySec = 2.0f;

	// -------------------------------------------------------
	// Projects
	// -------------------------------------------------------

	void GetProjects(FOnApiResponse OnComplete);

	void CreateProject(const FString& Name, const FString& InputPrimaMateria,
	                   bool bCheckpointMode, FOnApiResponse OnComplete);

	void RunPipeline(const FString& ProjectId, FOnApiResponse OnComplete);

	void StopPipeline(const FString& ProjectId, FOnApiResponse OnComplete);

	void GetPipelineStatus(const FString& ProjectId, FOnApiResponse OnComplete);

	void DeleteProject(const FString& ProjectId, FOnApiResponse OnComplete);

	// -------------------------------------------------------
	// Chat / Steward AI
	// -------------------------------------------------------

	void SendChatMessage(const FString& Message, const FString& ProjectId,
	                     FOnApiResponse OnComplete);

	/**
	 * Send a chat message with SSE token streaming (Fix 1.1).
	 * OnToken fires for each received token. OnStreamDone fires when
	 * the stream completes with state/flags. OnFallback fires if
	 * streaming fails (caller should fall back to non-streaming).
	 */
	void SendStreamingChatMessage(const FString& Message, const FString& ProjectId,
	                              FOnStreamToken OnToken, FOnStreamComplete OnStreamDone,
	                              FOnApiResponse OnFallback);

	// -------------------------------------------------------
	// Library
	// -------------------------------------------------------

	void GetLibrary(FOnApiResponse OnComplete);

	void SearchLibrary(const FString& Query, FOnApiResponse OnComplete);

	// -------------------------------------------------------
	// Health / Status
	// -------------------------------------------------------

	void CheckHealth(FOnApiResponse OnComplete);

	/** Fix 2.4: Fires when a request exhausts all retries (connection failure).
	 *  GameInstance binds this to trigger an immediate health check. */
	UPROPERTY(BlueprintAssignable, Category = "HJ|Events")
	FOnConnectionExhausted OnConnectionExhausted;

	// -------------------------------------------------------
	// Game World (Phase 3F)
	// -------------------------------------------------------

	void GetWorldState(FOnApiResponse OnComplete);

	void PostGameTick(float DeltaTime, FOnApiResponse OnComplete);

	// -------------------------------------------------------
	// Generic helpers
	// -------------------------------------------------------

	void Get(const FString& Endpoint, FOnApiResponse OnComplete);

	void Post(const FString& Endpoint, const FString& JsonBody, FOnApiResponse OnComplete);

private:
	void SendRequest(const FString& Endpoint, const FString& Verb,
	                 const FString& Body, FOnApiResponse OnComplete,
	                 int32 Attempt = 0);

	UWorld* GetWorld() const override;
};
