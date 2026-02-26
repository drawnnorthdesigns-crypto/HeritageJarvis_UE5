#include "TartariaWorldSubsystem.h"
#include "TartariaQuestMarker.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void UTartariaWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("TartariaWorldSubsystem: Initialized"));

	// Set up default biome zones (level-design driven, override via backend)
	FTartariaBiomeZone Clearinghouse;
	Clearinghouse.BiomeKey = TEXT("CLEARINGHOUSE");
	Clearinghouse.Theme = TEXT("Safe Haven");
	Clearinghouse.Difficulty = 1;
	Clearinghouse.WorldCenter = FVector(0.f, 0.f, 0.f);
	Clearinghouse.Radius = 150000.f; // 1500m in UE units
	BiomeZones.Add(Clearinghouse);

	FTartariaBiomeZone Scriptorium;
	Scriptorium.BiomeKey = TEXT("SCRIPTORIUM");
	Scriptorium.Theme = TEXT("Knowledge");
	Scriptorium.Difficulty = 2;
	Scriptorium.WorldCenter = FVector(250000.f, 0.f, 0.f);
	Scriptorium.Radius = 150000.f;
	BiomeZones.Add(Scriptorium);

	FTartariaBiomeZone MonolithWard;
	MonolithWard.BiomeKey = TEXT("MONOLITH_WARD");
	MonolithWard.Theme = TEXT("Construction");
	MonolithWard.Difficulty = 3;
	MonolithWard.WorldCenter = FVector(0.f, 300000.f, 0.f);
	MonolithWard.Radius = 200000.f;
	BiomeZones.Add(MonolithWard);

	FTartariaBiomeZone ForgeDistrict;
	ForgeDistrict.BiomeKey = TEXT("FORGE_DISTRICT");
	ForgeDistrict.Theme = TEXT("Industrial");
	ForgeDistrict.Difficulty = 4;
	ForgeDistrict.WorldCenter = FVector(-250000.f, 0.f, 0.f);
	ForgeDistrict.Radius = 200000.f;
	BiomeZones.Add(ForgeDistrict);

	FTartariaBiomeZone VoidReach;
	VoidReach.BiomeKey = TEXT("VOID_REACH");
	VoidReach.Theme = TEXT("Endgame");
	VoidReach.Difficulty = 5;
	VoidReach.WorldCenter = FVector(0.f, -350000.f, 0.f);
	VoidReach.Radius = 200000.f;
	BiomeZones.Add(VoidReach);

	// Initial sync
	SyncWithBackend();
}

void UTartariaWorldSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("TartariaWorldSubsystem: Deinitialized"));
	Super::Deinitialize();
}

void UTartariaWorldSubsystem::Tick(float DeltaTime)
{
	// Periodic backend sync
	SyncTimer += DeltaTime;
	if (SyncTimer >= SyncIntervalSec)
	{
		SyncTimer = 0.f;
		SyncWithBackend();
	}

	// Periodic inventory poll
	InventoryTimer += DeltaTime;
	if (InventoryTimer >= InventoryPollSec)
	{
		InventoryTimer = 0.f;
		PollInventory();
	}

	// Periodic game tick
	TickTimer += DeltaTime;
	if (TickTimer >= TickIntervalSec)
	{
		TickTimer = 0.f;
		ExecuteGameTick();
	}

	// Periodic strategic status poll (fleet, tech, mining)
	StrategicTimer += DeltaTime;
	if (StrategicTimer >= StrategicPollSec)
	{
		StrategicTimer = 0.f;
		PollStrategicStatus();
	}
}

void UTartariaWorldSubsystem::SyncWithBackend()
{
	UHJGameInstance* GI = UHJGameInstance::Get(GetWorld());
	if (!GI || !GI->ApiClient) return;

	FOnApiResponse CB;
	CB.BindUObject(this, &UTartariaWorldSubsystem::OnWorldStateResponse);
	GI->ApiClient->GetWorldState(CB);
}

void UTartariaWorldSubsystem::OnWorldStateResponse(bool bSuccess, const FString& Body)
{
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("TartariaWorldSubsystem: Backend sync failed"));
		return;
	}

	ParseWorldState(Body);
}

void UTartariaWorldSubsystem::ParseWorldState(const FString& JsonBody)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

	// Parse era
	Root->TryGetStringField(TEXT("era"), CurrentEra);

	// Parse temporal clock
	const TSharedPtr<FJsonObject>* ClockObj;
	if (Root->TryGetObjectField(TEXT("temporal_clock"), ClockObj))
	{
		double Tod = 8.0;
		if ((*ClockObj)->TryGetNumberField(TEXT("time_of_day"), Tod))
			TimeOfDay = static_cast<float>(Tod);

		int32 Day = 1;
		(*ClockObj)->TryGetNumberField(TEXT("current_day"), Day);
		CurrentDay = Day;
	}

	// Parse active events/threats as POIs
	const TArray<TSharedPtr<FJsonValue>>* EventsArr;
	if (Root->TryGetArrayField(TEXT("active_events"), EventsArr))
	{
		ActivePOIs.Empty();
		for (const TSharedPtr<FJsonValue>& Val : *EventsArr)
		{
			const TSharedPtr<FJsonObject>& Obj = Val->AsObject();
			if (!Obj.IsValid()) continue;

			FTartariaPOI POI;
			Obj->TryGetStringField(TEXT("id"), POI.POIId);
			Obj->TryGetStringField(TEXT("type"), POI.POIType);
			Obj->TryGetStringField(TEXT("biome"), POI.BiomeKey);
			Obj->TryGetStringField(TEXT("name"), POI.DisplayName);
			POI.bActive = true;

			// Parse location if available
			double X = 0, Y = 0, Z = 0;
			const TSharedPtr<FJsonObject>* LocObj;
			if (Obj->TryGetObjectField(TEXT("location"), LocObj))
			{
				(*LocObj)->TryGetNumberField(TEXT("x"), X);
				(*LocObj)->TryGetNumberField(TEXT("y"), Y);
				(*LocObj)->TryGetNumberField(TEXT("z"), Z);
			}
			POI.WorldLocation = FVector(X, Y, Z);

			ActivePOIs.Add(POI);
		}
	}

	// Parse credits
	double Cr = 0;
	if (Root->TryGetNumberField(TEXT("credits"), Cr))
		Credits = static_cast<int32>(Cr);

	// Parse phase
	Root->TryGetStringField(TEXT("phase"), Phase);

	// Parse factions
	const TSharedPtr<FJsonObject>* FactionsObj;
	if (Root->TryGetObjectField(TEXT("factions"), FactionsObj))
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

	OnGameStateUpdated.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("TartariaWorldSubsystem: Synced — Era=%s, Day=%d, POIs=%d, Credits=%d"),
		*CurrentEra, CurrentDay, ActivePOIs.Num(), Credits);
}

// -------------------------------------------------------
// Inventory polling
// -------------------------------------------------------

void UTartariaWorldSubsystem::PollInventory()
{
	UHJGameInstance* GI = UHJGameInstance::Get(GetWorld());
	if (!GI || !GI->ApiClient) return;

	FOnApiResponse CB;
	CB.BindUObject(this, &UTartariaWorldSubsystem::OnInventoryResponse);
	GI->ApiClient->Get(TEXT("/api/game/inventory"), CB);
}

void UTartariaWorldSubsystem::OnInventoryResponse(bool bSuccess, const FString& Body)
{
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("TartariaWorldSubsystem: Inventory poll failed"));
		return;
	}
	ParseInventory(Body);
}

void UTartariaWorldSubsystem::ParseInventory(const FString& JsonBody)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

	const TArray<TSharedPtr<FJsonValue>>* ItemsArr;
	if (Root->TryGetArrayField(TEXT("items"), ItemsArr))
	{
		Inventory.Empty();
		for (const TSharedPtr<FJsonValue>& Val : *ItemsArr)
		{
			const TSharedPtr<FJsonObject>& Obj = Val->AsObject();
			if (!Obj.IsValid()) continue;

			FTartariaInventoryItem Item;
			Obj->TryGetStringField(TEXT("item_id"), Item.ItemId);

			double Qty = 0;
			Obj->TryGetNumberField(TEXT("quantity"), Qty);
			Item.Quantity = static_cast<int32>(Qty);

			Obj->TryGetBoolField(TEXT("equipped"), Item.bEquipped);
			Inventory.Add(Item);
		}
	}

	OnGameStateUpdated.Broadcast();
}

// -------------------------------------------------------
// Game tick
// -------------------------------------------------------

void UTartariaWorldSubsystem::ExecuteGameTick()
{
	UHJGameInstance* GI = UHJGameInstance::Get(GetWorld());
	if (!GI || !GI->ApiClient) return;

	FOnApiResponse CB;
	CB.BindUObject(this, &UTartariaWorldSubsystem::OnTickResponse);
	GI->ApiClient->PostGameTick(1.0f, CB);
}

void UTartariaWorldSubsystem::OnTickResponse(bool bSuccess, const FString& Body)
{
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("TartariaWorldSubsystem: Game tick failed"));
		return;
	}
	ParseTickResponse(Body);
}

void UTartariaWorldSubsystem::ParseTickResponse(const FString& JsonBody)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

	// Update credits if provided
	double CreditsEarned = 0;
	if (Root->TryGetNumberField(TEXT("credits_earned"), CreditsEarned))
		Credits += static_cast<int32>(CreditsEarned);

	// Parse events
	TArray<FTartariaTickEvent> Events;
	const TArray<TSharedPtr<FJsonValue>>* EventsArr;
	if (Root->TryGetArrayField(TEXT("events"), EventsArr))
	{
		for (const TSharedPtr<FJsonValue>& Val : *EventsArr)
		{
			const TSharedPtr<FJsonObject>& Obj = Val->AsObject();
			if (!Obj.IsValid()) continue;

			FTartariaTickEvent Evt;
			Obj->TryGetStringField(TEXT("type"), Evt.Type);
			Obj->TryGetStringField(TEXT("detail"), Evt.Detail);

			double V = 0;
			Obj->TryGetNumberField(TEXT("value"), V);
			Evt.Value = static_cast<int32>(V);

			Events.Add(Evt);
		}
	}

	if (Events.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("TartariaWorldSubsystem: Tick produced %d events"), Events.Num());
		OnTickCompleted.Broadcast(Events);
	}
}

// -------------------------------------------------------
// Strategic status (fleet, tech, mining)
// -------------------------------------------------------

void UTartariaWorldSubsystem::PollStrategicStatus()
{
	UHJGameInstance* GI = UHJGameInstance::Get(GetWorld());
	if (!GI || !GI->ApiClient) return;

	FOnApiResponse CB;
	CB.BindUObject(this, &UTartariaWorldSubsystem::OnStrategicResponse);
	GI->ApiClient->Get(TEXT("/api/game/status/full"), CB);
}

void UTartariaWorldSubsystem::OnStrategicResponse(bool bSuccess, const FString& Body)
{
	if (!bSuccess) return;
	ParseStrategicStatus(Body);
}

void UTartariaWorldSubsystem::ParseStrategicStatus(const FString& JsonBody)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

	// Parse fleet
	const TSharedPtr<FJsonObject>* FleetObj;
	if (Root->TryGetObjectField(TEXT("fleet"), FleetObj))
	{
		double Total = 0, Reserve = 0, Deployed = 0;
		(*FleetObj)->TryGetNumberField(TEXT("total_fleet_power"), Total);
		(*FleetObj)->TryGetNumberField(TEXT("reserve_power"), Reserve);
		(*FleetObj)->TryGetNumberField(TEXT("deployed_power"), Deployed);
		Fleet.TotalPower = static_cast<int32>(Total);
		Fleet.ReservePower = static_cast<int32>(Reserve);
		Fleet.DeployedPower = static_cast<int32>(Deployed);

		// Count deployed zones
		const TSharedPtr<FJsonObject>* DeployObj;
		if ((*FleetObj)->TryGetObjectField(TEXT("deployments"), DeployObj))
		{
			int32 Zones = 0;
			for (const auto& Pair : (*DeployObj)->Values)
			{
				const TSharedPtr<FJsonObject>* ZObj;
				if (Pair.Value->TryGetObject(ZObj))
				{
					double Pwr = 0;
					(*ZObj)->TryGetNumberField(TEXT("power"), Pwr);
					if (Pwr > 0 && Pair.Key != TEXT("RESERVE"))
						Zones++;
				}
			}
			Fleet.DeployedZones = Zones;
		}
	}

	// Parse tech tree
	const TSharedPtr<FJsonObject>* TechObj;
	if (Root->TryGetObjectField(TEXT("tech_tree"), TechObj))
	{
		double TotalN = 0, Unlocked = 0;
		(*TechObj)->TryGetNumberField(TEXT("total_nodes"), TotalN);
		(*TechObj)->TryGetNumberField(TEXT("unlocked_count"), Unlocked);
		Tech.TotalNodes = static_cast<int32>(TotalN);
		Tech.UnlockedCount = static_cast<int32>(Unlocked);

		// Get available next count
		const TArray<TSharedPtr<FJsonValue>>* AvailArr;
		if ((*TechObj)->TryGetArrayField(TEXT("available_next"), AvailArr))
			Tech.AvailableNext = AvailArr->Num();

		// Get highest unlocked tech
		const TArray<TSharedPtr<FJsonValue>>* UnlockedArr;
		if ((*TechObj)->TryGetArrayField(TEXT("unlocked"), UnlockedArr) && UnlockedArr->Num() > 0)
			Tech.HighestTech = (*UnlockedArr)[UnlockedArr->Num() - 1]->AsString();
	}

	// Parse mining
	const TSharedPtr<FJsonObject>* MiningObj;
	if (Root->TryGetObjectField(TEXT("mining"), MiningObj))
	{
		double Mined = 0, Scanned = 0;
		(*MiningObj)->TryGetNumberField(TEXT("total_mined"), Mined);
		(*MiningObj)->TryGetNumberField(TEXT("asteroids_scanned"), Scanned);
		Mining.TotalMined = static_cast<int32>(Mined);
		Mining.AsteroidsScanned = static_cast<int32>(Scanned);
	}

	OnGameStateUpdated.Broadcast();
}
