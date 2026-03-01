#include "TartariaWorldSubsystem.h"
#include "TartariaInstancedWealth.h"
#include "TartariaQuestMarker.h"
#include "TartariaEnemyActor.h"
#include "TartariaBiomeVolume.h"
#include "TartariaForgeBuilding.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "Core/HJWebSocketClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"

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

	// Subscribe to WebSocket events for real-time updates
	UHJGameInstance* GI = UHJGameInstance::Get(GetWorld());
	if (GI && GI->WebSocketClient)
	{
		GI->WebSocketClient->OnMessage.AddDynamic(this, &UTartariaWorldSubsystem::OnWebSocketMessage);
	}
}

void UTartariaWorldSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("TartariaWorldSubsystem: Deinitialized"));
	Super::Deinitialize();
}

void UTartariaWorldSubsystem::Tick(float DeltaTime)
{
	// When SSE stream is active, skip polling — state comes via push.
	// Only the game tick still fires (it's a write operation, not a read).
	if (bUseSSE)
	{
		TickTimer += DeltaTime;
		if (TickTimer >= TickIntervalSec)
		{
			TickTimer = 0.f;
			ExecuteGameTick();
		}
		return;
	}

	// Fallback: poll-based updates when SSE is not connected
	SyncTimer += DeltaTime;
	if (SyncTimer >= SyncIntervalSec)
	{
		SyncTimer = 0.f;
		SyncWithBackend();
	}

	InventoryTimer += DeltaTime;
	if (InventoryTimer >= InventoryPollSec)
	{
		InventoryTimer = 0.f;
		PollInventory();
	}

	TickTimer += DeltaTime;
	if (TickTimer >= TickIntervalSec)
	{
		TickTimer = 0.f;
		ExecuteGameTick();
	}

	StrategicTimer += DeltaTime;
	if (StrategicTimer >= StrategicPollSec)
	{
		StrategicTimer = 0.f;
		PollStrategicStatus();
	}

	ConsequencePollTimer += DeltaTime;
	if (ConsequencePollTimer >= ConsequencePollSec)
	{
		ConsequencePollTimer = 0.f;
		PollWorldConsequences();
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

	// ── Push live credit balance to all TartariaInstancedWealth vault actors ──
	UWorld* World = GetWorld();
	if (World)
	{
		for (TActorIterator<ATartariaInstancedWealth> It(World); It; ++It)
		{
			ATartariaInstancedWealth* Vault = *It;
			if (Vault)
			{
				Vault->UpdateFromWorldState(Credits);
			}
		}
	}

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
	{
		Credits += static_cast<int32>(CreditsEarned);

		// Push updated balance to vault visualizers immediately
		UWorld* TickWorld = GetWorld();
		if (TickWorld)
		{
			for (TActorIterator<ATartariaInstancedWealth> It(TickWorld); It; ++It)
			{
				ATartariaInstancedWealth* Vault = *It;
				if (Vault)
				{
					Vault->UpdateFromWorldState(Credits);
				}
			}
		}
	}

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

	// Process threat data for enemy spawning
	const TSharedPtr<FJsonObject>* ThreatObj;
	if (Root->TryGetObjectField(TEXT("threat_engine"), ThreatObj))
	{
		ProcessThreatData(*ThreatObj);
	}

	OnGameStateUpdated.Broadcast();
}

// -------------------------------------------------------
// Enemy Spawning & Combat
// -------------------------------------------------------

void UTartariaWorldSubsystem::SpawnEnemyFromThreat(const FString& InEnemyName, int32 Power, int32 InDifficulty, int32 Reward, const FString& InScenarioKey)
{
	// Clean up dead weak pointers first
	ActiveEnemies.RemoveAll([](const TWeakObjectPtr<ATartariaEnemyActor>& E) { return !E.IsValid(); });

	// Don't spawn too many enemies at once
	if (ActiveEnemies.Num() >= 3) return;

	UWorld* World = GetWorld();
	if (!World) return;

	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
	if (!Player) return;

	// Spawn 800-1200cm in front of the player, on the ground
	FVector PlayerLoc = Player->GetActorLocation();
	FVector PlayerFwd = Player->GetActorForwardVector();
	float SpawnDist = FMath::RandRange(800.f, 1200.f);
	float SpawnAngle = FMath::RandRange(-30.f, 30.f);
	FVector SpawnOffset = PlayerFwd.RotateAngleAxis(SpawnAngle, FVector::UpVector) * SpawnDist;
	FVector SpawnLoc = PlayerLoc + SpawnOffset;
	SpawnLoc.Z = PlayerLoc.Z;  // same ground level

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ATartariaEnemyActor* Enemy = World->SpawnActor<ATartariaEnemyActor>(
		ATartariaEnemyActor::StaticClass(), SpawnLoc, FRotator::ZeroRotator, Params);

	if (Enemy)
	{
		Enemy->EnemyName = InEnemyName;
		Enemy->EnemyPower = Power;
		Enemy->Difficulty = InDifficulty;
		Enemy->RewardCredits = Reward;
		Enemy->ScenarioKey = InScenarioKey;
		ActiveEnemies.Add(Enemy);

		UE_LOG(LogTemp, Log, TEXT("TartariaWorld: Spawned enemy '%s' (Power=%d, Diff=%d) near player"),
			*InEnemyName, Power, InDifficulty);
	}
}

void UTartariaWorldSubsystem::ProcessThreatData(const TSharedPtr<FJsonObject>& ThreatJson)
{
	if (!ThreatJson.IsValid()) return;

	// Check if there's an active threat level
	int32 ThreatLevel = 0;
	ThreatJson->TryGetNumberField(TEXT("threat_level"), ThreatLevel);
	if (ThreatLevel <= 0) return;

	FString EnemyName = TEXT("Unknown Threat");
	ThreatJson->TryGetStringField(TEXT("active_threat"), EnemyName);

	int32 Difficulty = FMath::Clamp(ThreatLevel, 1, 10);
	int32 Power = Difficulty * 15;
	int32 Reward = Difficulty * 100;

	SpawnEnemyFromThreat(EnemyName, Power, Difficulty, Reward, TEXT(""));
}

void UTartariaWorldSubsystem::ResolveCombatWithNearestEnemy(bool bPlayerWins)
{
	UWorld* World = GetWorld();
	if (!World) return;

	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
	if (!Player) return;

	// Find nearest active enemy
	ATartariaEnemyActor* Nearest = nullptr;
	float MinDist = MAX_FLT;

	for (auto& WeakEnemy : ActiveEnemies)
	{
		ATartariaEnemyActor* Enemy = WeakEnemy.Get();
		if (Enemy && !Enemy->bInCombat && !Enemy->bDefeated)
		{
			float Dist = FVector::Dist(Player->GetActorLocation(), Enemy->GetActorLocation());
			if (Dist < MinDist)
			{
				MinDist = Dist;
				Nearest = Enemy;
			}
		}
	}

	if (Nearest && MinDist < 1500.f)
	{
		Nearest->BeginCombatChoreography(bPlayerWins);
	}
}

// -------------------------------------------------------
// WebSocket Message Handler
// -------------------------------------------------------

void UTartariaWorldSubsystem::OnWebSocketMessage(const FString& Channel, const FString& Payload)
{
	if (Channel == TEXT("game"))
	{
		// Parse game state update immediately (no 30s wait)
		TSharedPtr<FJsonObject> Json;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
		if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
		{
			// Check for combat results
			FString EventType;
			if (Json->TryGetStringField(TEXT("event"), EventType))
			{
				if (EventType == TEXT("combat_result"))
				{
					FString Outcome;
					if (Json->TryGetStringField(TEXT("outcome"), Outcome))
					{
						bool bWin = (Outcome == TEXT("VICTORY"));
						ResolveCombatWithNearestEnemy(bWin);
					}
				}
			}
		}
	}
	else if (Channel == TEXT("pipeline"))
	{
		// Pipeline status -- update forge building progress
		UE_LOG(LogTemp, Verbose, TEXT("WorldSubsystem: Pipeline WS update received"));
	}
}

// -------------------------------------------------------
// World Consequences — Visual Zone Modifiers
// -------------------------------------------------------

void UTartariaWorldSubsystem::PollWorldConsequences()
{
	UHJGameInstance* GI = UHJGameInstance::Get(GetWorld());
	if (!GI || !GI->ApiClient) return;

	TWeakObjectPtr<UTartariaWorldSubsystem> WeakThis(this);
	FOnApiResponse CB;
	CB.BindLambda([WeakThis](bool bOk, const FString& Body)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bOk, Body]()
		{
			UTartariaWorldSubsystem* Self = WeakThis.Get();
			if (!Self || !bOk) return;

			TSharedPtr<FJsonObject> Json;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
			if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
			{
				Self->ApplyZoneVisualModifiers(Json);
			}
		});
	});

	GI->ApiClient->Get(TEXT("/api/game/world-consequences/visual"), CB);
}

void UTartariaWorldSubsystem::ApplyZoneVisualModifiers(const TSharedPtr<FJsonObject>& ModifiersJson)
{
	if (!ModifiersJson.IsValid()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// Iterate all biome volumes and apply visual modifiers per zone
	for (TActorIterator<ATartariaBiomeVolume> It(World); It; ++It)
	{
		ATartariaBiomeVolume* Volume = *It;
		if (!Volume) continue;

		FString ZoneKey = Volume->BiomeKey;

		const TSharedPtr<FJsonObject>* ZoneMods = nullptr;
		if (!ModifiersJson->TryGetObjectField(ZoneKey, ZoneMods))
		{
			// Try lowercase variant
			if (!ModifiersJson->TryGetObjectField(ZoneKey.ToLower(), ZoneMods))
			{
				continue;
			}
		}

		if (!ZoneMods || !(*ZoneMods).IsValid()) continue;
		if (!Volume->BiomePostProcess) continue;

		FPostProcessSettings& PP = Volume->BiomePostProcess->Settings;

		// ── Fog modifications ──
		double FogReduction = 0.0;
		double FogIncrease = 0.0;
		(*ZoneMods)->TryGetNumberField(TEXT("fog_reduction"), FogReduction);
		(*ZoneMods)->TryGetNumberField(TEXT("fog_increase"), FogIncrease);

		if (FogReduction > 0)
		{
			// Reduce bloom intensity to simulate clearer air
			PP.bOverride_BloomIntensity = true;
			PP.BloomIntensity = FMath::Max(0.1f, PP.BloomIntensity * (1.0f - static_cast<float>(FogReduction)));

			// Reduce vignette for more open feel
			PP.bOverride_VignetteIntensity = true;
			PP.VignetteIntensity = FMath::Max(0.0f, PP.VignetteIntensity * (1.0f - static_cast<float>(FogReduction)));
		}
		if (FogIncrease > 0)
		{
			// Increase bloom to simulate fog/haze
			PP.bOverride_BloomIntensity = true;
			PP.BloomIntensity = FMath::Min(2.0f, PP.BloomIntensity + static_cast<float>(FogIncrease));

			// Darken exposure slightly for oppressive atmosphere
			PP.bOverride_AutoExposureBias = true;
			PP.AutoExposureBias -= static_cast<float>(FogIncrease) * 0.5f;
		}

		// ── Tint shift via ColorGain ──
		FString TintShift;
		if ((*ZoneMods)->TryGetStringField(TEXT("tint_shift"), TintShift))
		{
			PP.bOverride_ColorGain = true;

			if (TintShift == TEXT("warm_gold"))
				PP.ColorGain = FVector4(1.1f, 1.05f, 0.85f, 1.0f);
			else if (TintShift == TEXT("sickly_green"))
				PP.ColorGain = FVector4(0.8f, 1.1f, 0.7f, 1.0f);
			else if (TintShift == TEXT("market_amber"))
				PP.ColorGain = FVector4(1.15f, 1.0f, 0.8f, 1.0f);
			else if (TintShift == TEXT("steel_blue"))
				PP.ColorGain = FVector4(0.85f, 0.9f, 1.15f, 1.0f);
			else if (TintShift == TEXT("ashen_grey"))
				PP.ColorGain = FVector4(0.9f, 0.9f, 0.9f, 1.0f);
		}

		// ── Chromatic aberration ──
		double ChromAb = 0.0;
		if ((*ZoneMods)->TryGetNumberField(TEXT("chromatic_aberration"), ChromAb) && ChromAb > 0)
		{
			PP.bOverride_SceneFringeIntensity = true;
			PP.SceneFringeIntensity = static_cast<float>(ChromAb) * 5.0f;
		}

		// ── Desaturation ──
		double Desat = 0.0;
		if ((*ZoneMods)->TryGetNumberField(TEXT("desaturation"), Desat) && Desat > 0)
		{
			float DesatF = static_cast<float>(Desat);
			PP.bOverride_ColorSaturation = true;
			PP.ColorSaturation = FVector4(1.0f - DesatF, 1.0f - DesatF, 1.0f - DesatF, 1.0f);
		}

		UE_LOG(LogTemp, Verbose, TEXT("WorldSubsystem: Applied visual mods to zone %s"), *ZoneKey);
	}

	// ── Update forge production visibility based on zone flags ──
	// Check if FORGE_DISTRICT has any consequence flags that indicate
	// active production (TRADE_HUB or FORTIFIED suggest economic activity)
	for (TActorIterator<ATartariaForgeBuilding> ForgeIt(World); ForgeIt; ++ForgeIt)
	{
		ATartariaForgeBuilding* Forge = *ForgeIt;
		if (!Forge) continue;

		// Forge production state is already driven by pipeline polling
		// via SetForgeActive(). Here we additionally activate forges
		// in zones that have the TRADE_HUB consequence (economic activity).
		const TSharedPtr<FJsonObject>* ForgeMods = nullptr;
		if (ModifiersJson->TryGetObjectField(TEXT("FORGE_DISTRICT"), ForgeMods) ||
			ModifiersJson->TryGetObjectField(TEXT("forge_district"), ForgeMods))
		{
			if (ForgeMods && (*ForgeMods).IsValid())
			{
				// If zone has trade hub or other activity indicators,
				// boost forge production visibility
				int32 AmbientNpcs = 0;
				(*ForgeMods)->TryGetNumberField(TEXT("ambient_npcs"), AmbientNpcs);
				if (AmbientNpcs > 0)
				{
					Forge->SetProductionActive(true);
				}
			}
		}
	}
}
