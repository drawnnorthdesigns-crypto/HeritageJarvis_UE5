#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "HJApiClient.h"
#include "HJEventPoller.generated.h"

/**
 * UHJEventPoller
 *
 * Since UE5 cannot consume SSE streams natively, this object polls
 * the Flask backend at a regular interval to simulate real-time events.
 *
 * Polling targets:
 *   GET /api/project/<id>/pipeline_status   → OnPipelineStatus
 *   GET /health                             → OnHealthStatus
 *
 * Usage:
 *   1. Create via NewObject<UHJEventPoller>
 *   2. Call StartPolling(ApiClient, ProjectId)
 *   3. Bind BlueprintAssignable delegates to react to updates
 *   4. Call StopPolling() when done
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
    FOnPipelineStatus,
    const FString&, ProjectId,
    const FString&, CurrentStage,
    int32, StageIndex,
    bool, bActive
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnHealthStatus,
    bool, bOnline,
    const FString&, Message
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnStageProgress,
    const FString&, StageName,
    int32, Percent,
    int32, TokensGenerated
);

UCLASS(BlueprintType, Blueprintable)
class HERITAGEJARVIS_API UHJEventPoller : public UObject
{
    GENERATED_BODY()

public:

    // -------------------------------------------------------
    // Configuration
    // -------------------------------------------------------

    /** Seconds between pipeline status polls while a pipeline is active */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|Polling")
    float PipelinePollInterval = 3.0f;

    /** Seconds between health polls */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|Polling")
    float HealthPollInterval = 30.0f;

    // -------------------------------------------------------
    // Delegates — bind in Blueprint or C++
    // -------------------------------------------------------

    UPROPERTY(BlueprintAssignable, Category = "HJ|Polling")
    FOnPipelineStatus OnPipelineStatus;

    UPROPERTY(BlueprintAssignable, Category = "HJ|Polling")
    FOnHealthStatus OnHealthStatus;

    /** Fires when intra-stage token progress is reported */
    UPROPERTY(BlueprintAssignable, Category = "HJ|Polling")
    FOnStageProgress OnStageProgress;

    // -------------------------------------------------------
    // Last known state (read by HJDebugWidget)
    // -------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "HJ|Polling")
    FString LastKnownStage;

    UPROPERTY(BlueprintReadOnly, Category = "HJ|Polling")
    bool bLastKnownActive = false;

    /** Last known intra-stage progress percent (0-95) */
    UPROPERTY(BlueprintReadOnly, Category = "HJ|Polling")
    int32 LastStageProgressPct = 0;

    // -------------------------------------------------------
    // Control
    // -------------------------------------------------------

    /**
     * Start polling for the given project's pipeline status.
     * Call again with a new ProjectId to switch projects.
     * Pass empty string to stop pipeline polling only.
     */
    UFUNCTION(BlueprintCallable, Category = "HJ|Polling")
    void StartPolling(UHJApiClient* Client, const FString& ProjectId);

    UFUNCTION(BlueprintCallable, Category = "HJ|Polling")
    void StopPolling();

    /** Immediately query pipeline status for the current project */
    UFUNCTION(BlueprintCallable, Category = "HJ|Polling")
    void PollNow();

private:
    UPROPERTY()
    UHJApiClient* ApiClient = nullptr;

    FString ActiveProjectId;

    FTimerHandle PipelineTimerHandle;
    FTimerHandle HealthTimerHandle;

    void PollPipelineStatus();
    void PollHealth();
    void ParsePipelineResponse(bool bOk, const FString& Body, const FString& ProjectId);
    void ParseHealthResponse(bool bOk, const FString& Body);

    UWorld* GetWorld() const override;
};
