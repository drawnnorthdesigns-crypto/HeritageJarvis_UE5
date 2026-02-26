#include "HJEventPoller.h"
#include "UI/HJLoadingWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"

// -------------------------------------------------------
// Public
// -------------------------------------------------------

void UHJEventPoller::StartPolling(UHJApiClient* Client, const FString& ProjectId)
{
    ApiClient = Client;
    ActiveProjectId = ProjectId;

    UWorld* World = GetWorld();
    if (!World) return;

    // Pipeline timer — only run if we have a project to watch
    World->GetTimerManager().ClearTimer(PipelineTimerHandle);
    if (!ActiveProjectId.IsEmpty())
    {
        // Kick off immediately, then repeat
        PollPipelineStatus();
        World->GetTimerManager().SetTimer(
            PipelineTimerHandle,
            this,
            &UHJEventPoller::PollPipelineStatus,
            PipelinePollInterval,
            /* bLoop = */ true
        );
    }

    // Health timer — always run
    World->GetTimerManager().ClearTimer(HealthTimerHandle);
    PollHealth();
    World->GetTimerManager().SetTimer(
        HealthTimerHandle,
        this,
        &UHJEventPoller::PollHealth,
        HealthPollInterval,
        /* bLoop = */ true
    );
}

void UHJEventPoller::StopPolling()
{
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(PipelineTimerHandle);
        World->GetTimerManager().ClearTimer(HealthTimerHandle);
    }
    ActiveProjectId.Empty();
    ApiClient = nullptr;
}

void UHJEventPoller::PollNow()
{
    if (!ActiveProjectId.IsEmpty()) PollPipelineStatus();
    PollHealth();
}

// -------------------------------------------------------
// Private — poll implementations
// -------------------------------------------------------

void UHJEventPoller::PollPipelineStatus()
{
    if (!ApiClient || ActiveProjectId.IsEmpty()) return;

    FString ProjectId = ActiveProjectId;  // capture by value for lambda

    // Fix: Weak pointer prevents crash if EventPoller is GC'd before callback fires
    TWeakObjectPtr<UHJEventPoller> WeakThis(this);

    FOnApiResponse CB;
    CB.BindLambda([WeakThis, ProjectId](bool bOk, const FString& Body)
    {
        if (UHJEventPoller* Self = WeakThis.Get())
            Self->ParsePipelineResponse(bOk, Body, ProjectId);
    });
    ApiClient->GetPipelineStatus(ProjectId, CB);
}

void UHJEventPoller::PollHealth()
{
    if (!ApiClient) return;

    // Fix: Weak pointer prevents crash if EventPoller is GC'd before callback fires
    TWeakObjectPtr<UHJEventPoller> WeakThis(this);

    FOnApiResponse CB;
    CB.BindLambda([WeakThis](bool bOk, const FString& Body)
    {
        if (UHJEventPoller* Self = WeakThis.Get())
            Self->ParseHealthResponse(bOk, Body);
    });
    ApiClient->CheckHealth(CB);
}

void UHJEventPoller::ParsePipelineResponse(bool bOk, const FString& Body, const FString& ProjectId)
{
    if (!bOk) return;

    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid()) return;

    // Hardening: reject error responses from backend
    if (Json->HasField(TEXT("error"))) return;

    FString Stage;
    if (!Json->TryGetStringField(TEXT("current_stage"), Stage)) return;
    double StageIndexD = 0;
    Json->TryGetNumberField(TEXT("current_stage_index"), StageIndexD);
    int32 StageIndex = static_cast<int32>(StageIndexD);
    bool bActive = false;
    Json->TryGetBoolField(TEXT("active"), bActive);
    // Fix 1.2: Extract total_stages from response (default 7)
    double TotalStagesD = 7.0;
    Json->TryGetNumberField(TEXT("total_stages"), TotalStagesD);
    int32 TotalStages = static_cast<int32>(TotalStagesD);

    LastKnownStage  = Stage;
    bLastKnownActive = bActive;

    // Fix 2.6: Ensure delegate broadcasts execute on the game thread
    // HTTP callbacks may fire off-thread in some UE5 configurations.
    // Fix: Use weak pointer to prevent crash if EventPoller is destroyed mid-callback
    TWeakObjectPtr<UHJEventPoller> WeakThis(this);

    auto BroadcastAndUpdateProgress = [WeakThis, ProjectId, Stage, StageIndex, TotalStages, bActive]()
    {
        UHJEventPoller* Self = WeakThis.Get();
        if (!Self) return;

        Self->OnPipelineStatus.Broadcast(ProjectId, Stage, StageIndex, bActive);

        // Fix 1.2: Update loading widget with pipeline progress
        if (bActive)
        {
            if (UHJLoadingWidget::Instance)
            {
                UHJLoadingWidget::Instance->ShowPipelineProgress(Stage, StageIndex, TotalStages);
            }
        }
        else
        {
            UHJLoadingWidget::Hide(nullptr);
        }
    };

    if (IsInGameThread())
    {
        BroadcastAndUpdateProgress();
    }
    else
    {
        AsyncTask(ENamedThreads::GameThread, BroadcastAndUpdateProgress);
    }

    // If pipeline finished, stop the pipeline timer
    if (!bActive)
    {
        UWorld* World = GetWorld();
        if (World) World->GetTimerManager().ClearTimer(PipelineTimerHandle);
    }
}

void UHJEventPoller::ParseHealthResponse(bool bOk, const FString& Body)
{
    // Fix 2.6: Parse JSON first, then broadcast on game thread
    // Fix: Use weak pointer to prevent crash if EventPoller is destroyed mid-callback
    TWeakObjectPtr<UHJEventPoller> WeakThis(this);

    if (!bOk)
    {
        if (IsInGameThread())
        {
            OnHealthStatus.Broadcast(false, TEXT("Flask server unreachable"));
        }
        else
        {
            AsyncTask(ENamedThreads::GameThread, [WeakThis]()
            {
                if (UHJEventPoller* Self = WeakThis.Get())
                    Self->OnHealthStatus.Broadcast(false, TEXT("Flask server unreachable"));
            });
        }
        return;
    }

    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
    {
        if (IsInGameThread())
        {
            OnHealthStatus.Broadcast(false, TEXT("Invalid health response"));
        }
        else
        {
            AsyncTask(ENamedThreads::GameThread, [WeakThis]()
            {
                if (UHJEventPoller* Self = WeakThis.Get())
                    Self->OnHealthStatus.Broadcast(false, TEXT("Invalid health response"));
            });
        }
        return;
    }

    // Hardening: reject error responses from backend
    if (Json->HasField(TEXT("error"))) return;

    bool bOnline = false;
    Json->TryGetBoolField(TEXT("ok"), bOnline);
    FString Message;
    if (!Json->TryGetStringField(TEXT("message"), Message))
    {
        Message = bOnline ? TEXT("OK") : TEXT("Unknown status");
    }

    if (IsInGameThread())
    {
        OnHealthStatus.Broadcast(bOnline, Message);
    }
    else
    {
        AsyncTask(ENamedThreads::GameThread, [WeakThis, bOnline, Message]()
        {
            if (UHJEventPoller* Self = WeakThis.Get())
                Self->OnHealthStatus.Broadcast(bOnline, Message);
        });
    }
}

UWorld* UHJEventPoller::GetWorld() const
{
    // Walk outer chain to find a world context
    UObject* Outer = GetOuter();
    while (Outer)
    {
        UWorld* World = Cast<UWorld>(Outer);
        if (World) return World;
        if (Outer->GetWorld()) return Outer->GetWorld();
        Outer = Outer->GetOuter();
    }
    return nullptr;
}
