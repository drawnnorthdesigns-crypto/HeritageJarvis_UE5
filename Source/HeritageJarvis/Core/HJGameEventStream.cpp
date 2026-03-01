#include "HJGameEventStream.h"
#include "HJApiClient.h"
#include "Http.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void UHJGameEventStream::Connect(UHJApiClient* InApiClient)
{
	ApiClient = InApiClient;
	bIntentionalDisconnect = false;
	StartSSERequest();
}

void UHJGameEventStream::Disconnect()
{
	bIntentionalDisconnect = true;
	bConnected = false;

	// Cancel any pending reconnect
	if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(GetOuter(), EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		World->GetTimerManager().ClearTimer(ReconnectTimerHandle);
	}
}

void UHJGameEventStream::StartSSERequest()
{
	if (!ApiClient || bIntentionalDisconnect) return;

	FString Url = ApiClient->BaseUrl + TEXT("/api/game/events/stream");

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("text/event-stream"));
	Request->SetHeader(TEXT("Cache-Control"), TEXT("no-cache"));
	Request->SetTimeout(0);  // No timeout for SSE

	SSEBuffer.Empty();
	CurrentEventType.Empty();

	// Use streaming delegate for chunked response
	TWeakObjectPtr<UHJGameEventStream> WeakThis(this);

	Request->OnRequestProgress().BindLambda(
		[WeakThis](FHttpRequestPtr Req, int32 BytesSent, int32 BytesReceived)
	{
		UHJGameEventStream* Self = WeakThis.Get();
		if (!Self) return;

		const FHttpResponsePtr Response = Req->GetResponse();
		if (!Response.IsValid()) return;

		FString Content = Response->GetContentAsString();

		// Process only new data since last callback
		if (Content.Len() > Self->SSEBuffer.Len())
		{
			FString NewData = Content.Mid(Self->SSEBuffer.Len());
			Self->SSEBuffer = Content;

			// Parse SSE lines
			AsyncTask(ENamedThreads::GameThread, [WeakThis, NewData]()
			{
				UHJGameEventStream* Self = WeakThis.Get();
				if (!Self) return;

				TArray<FString> Lines;
				NewData.ParseIntoArray(Lines, TEXT("\n"), false);

				for (const FString& Line : Lines)
				{
					Self->OnSSEData(Line);
				}
			});
		}
	});

	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnectedOk)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bConnectedOk]()
		{
			UHJGameEventStream* Self = WeakThis.Get();
			if (!Self) return;

			Self->bConnected = false;
			UE_LOG(LogTemp, Warning,
				TEXT("HJGameEventStream: SSE connection %s — scheduling reconnect"),
				bConnectedOk ? TEXT("closed") : TEXT("failed"));

			Self->ScheduleReconnect();
		});
	});

	Request->ProcessRequest();
	bConnected = true;
	UE_LOG(LogTemp, Log, TEXT("HJGameEventStream: Connected to game event stream"));
}

void UHJGameEventStream::OnSSEData(const FString& Line)
{
	// SSE format:
	//   event: state_snapshot
	//   data: {"credits": 100, ...}
	//   (empty line = end of message)
	//   : keepalive

	if (Line.StartsWith(TEXT("event: ")))
	{
		CurrentEventType = Line.Mid(7).TrimStartAndEnd();
		return;
	}

	if (Line.StartsWith(TEXT(": ")))
	{
		// Comment / keepalive — ignore
		return;
	}

	if (Line.StartsWith(TEXT("data: ")))
	{
		FString JsonStr = Line.Mid(6);

		TSharedPtr<FJsonObject> Json;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
		if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
			return;

		// Route by event type or embedded event field
		if (CurrentEventType == TEXT("state_snapshot"))
		{
			ParseStateSnapshot(Json);
		}
		else if (CurrentEventType == TEXT("error"))
		{
			FString Error;
			Json->TryGetStringField(TEXT("error"), Error);
			UE_LOG(LogTemp, Warning, TEXT("HJGameEventStream: Server error: %s"), *Error);
		}
		else
		{
			// Non-typed data lines contain {"event": "...", "data": {...}}
			FString EventType;
			if (Json->TryGetStringField(TEXT("event"), EventType))
			{
				const TSharedPtr<FJsonObject>* DataObj;
				if (Json->TryGetObjectField(TEXT("data"), DataObj))
				{
					if (EventType == TEXT("tick_events"))
						ParseTickEvents(*DataObj);
				}
			}
		}

		CurrentEventType.Empty();
	}
}

void UHJGameEventStream::ParseStateSnapshot(const TSharedPtr<FJsonObject>& Json)
{
	// Credits
	double Cr = 0;
	if (Json->TryGetNumberField(TEXT("credits"), Cr))
		Credits = static_cast<int32>(Cr);

	Json->TryGetStringField(TEXT("phase"), Phase);

	// Temporal
	const TSharedPtr<FJsonObject>* TemporalObj;
	if (Json->TryGetObjectField(TEXT("temporal"), TemporalObj))
	{
		double Tod = 0, Day = 0;
		if ((*TemporalObj)->TryGetNumberField(TEXT("time_of_day"), Tod))
			TimeOfDay = static_cast<float>(Tod);
		if ((*TemporalObj)->TryGetNumberField(TEXT("current_day"), Day))
			CurrentDay = static_cast<int32>(Day);
		(*TemporalObj)->TryGetStringField(TEXT("era"), Era);
	}

	// Factions
	const TSharedPtr<FJsonObject>* FactionsObj;
	if (Json->TryGetObjectField(TEXT("factions"), FactionsObj))
	{
		Factions.Empty();
		for (const auto& Pair : (*FactionsObj)->Values)
		{
			const TSharedPtr<FJsonObject>* FObj;
			if (Pair.Value->TryGetObject(FObj))
			{
				FTartariaFactionInfo Info;
				Info.FactionKey = Pair.Key;
				double Inf = 0;
				(*FObj)->TryGetNumberField(TEXT("influence"), Inf);
				Info.Influence = static_cast<float>(Inf);
				(*FObj)->TryGetStringField(TEXT("domain"), Info.Domain);
				Factions.Add(Info);
			}
		}
	}

	// Fleet
	const TSharedPtr<FJsonObject>* FleetObj;
	if (Json->TryGetObjectField(TEXT("fleet"), FleetObj))
	{
		double Val = 0;
		if ((*FleetObj)->TryGetNumberField(TEXT("total_power"), Val))
			Fleet.TotalPower = static_cast<int32>(Val);
		if ((*FleetObj)->TryGetNumberField(TEXT("reserve_power"), Val))
			Fleet.ReservePower = static_cast<int32>(Val);
		if ((*FleetObj)->TryGetNumberField(TEXT("deployed_power"), Val))
			Fleet.DeployedPower = static_cast<int32>(Val);
		if ((*FleetObj)->TryGetNumberField(TEXT("deployed_zones"), Val))
			Fleet.DeployedZones = static_cast<int32>(Val);
	}

	// Tech
	const TSharedPtr<FJsonObject>* TechObj;
	if (Json->TryGetObjectField(TEXT("tech"), TechObj))
	{
		double Val = 0;
		if ((*TechObj)->TryGetNumberField(TEXT("total_nodes"), Val))
			Tech.TotalNodes = static_cast<int32>(Val);
		if ((*TechObj)->TryGetNumberField(TEXT("unlocked"), Val))
			Tech.UnlockedCount = static_cast<int32>(Val);
		(*TechObj)->TryGetStringField(TEXT("highest"), Tech.HighestTech);
	}

	// Mining
	const TSharedPtr<FJsonObject>* MiningObj;
	if (Json->TryGetObjectField(TEXT("mining"), MiningObj))
	{
		double Val = 0;
		if ((*MiningObj)->TryGetNumberField(TEXT("total_mined"), Val))
			Mining.TotalMined = static_cast<int32>(Val);
		if ((*MiningObj)->TryGetNumberField(TEXT("asteroids_scanned"), Val))
			Mining.AsteroidsScanned = static_cast<int32>(Val);
	}

	// Inventory
	const TArray<TSharedPtr<FJsonValue>>* InvArray;
	if (Json->TryGetArrayField(TEXT("inventory"), InvArray))
	{
		Inventory.Empty();
		for (const auto& Val : *InvArray)
		{
			const TSharedPtr<FJsonObject>* ItemObj;
			if (Val->TryGetObject(ItemObj))
			{
				FTartariaInventoryItem Item;
				(*ItemObj)->TryGetStringField(TEXT("item_id"), Item.ItemId);
				double Qty = 0;
				if ((*ItemObj)->TryGetNumberField(TEXT("quantity"), Qty))
					Item.Quantity = static_cast<int32>(Qty);
				bool Eq = false;
				(*ItemObj)->TryGetBoolField(TEXT("equipped"), Eq);
				Item.bEquipped = Eq;
				Inventory.Add(Item);
			}
		}
	}

	OnSnapshotReceived.Broadcast();
}

void UHJGameEventStream::ParseTickEvents(const TSharedPtr<FJsonObject>& Json)
{
	const TArray<TSharedPtr<FJsonValue>>* EventsArray;
	if (!Json->TryGetArrayField(TEXT("events"), EventsArray))
		return;

	TArray<FTartariaTickEvent> Events;
	for (const auto& Val : *EventsArray)
	{
		const TSharedPtr<FJsonObject>* EvtObj;
		if (Val->TryGetObject(EvtObj))
		{
			FTartariaTickEvent Evt;
			(*EvtObj)->TryGetStringField(TEXT("type"), Evt.Type);
			(*EvtObj)->TryGetStringField(TEXT("detail"), Evt.Detail);
			double V = 0;
			if ((*EvtObj)->TryGetNumberField(TEXT("value"), V))
				Evt.Value = static_cast<int32>(V);
			Events.Add(Evt);
		}
	}

	if (Events.Num() > 0)
		OnTickEventsReceived.Broadcast(Events);
}

void UHJGameEventStream::ScheduleReconnect()
{
	if (bIntentionalDisconnect) return;

	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(GetOuter(), EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (!World) return;

	UE_LOG(LogTemp, Log, TEXT("HJGameEventStream: Reconnecting in 5 seconds..."));
	World->GetTimerManager().SetTimer(
		ReconnectTimerHandle,
		[this]() { StartSSERequest(); },
		5.0f,
		false
	);
}
