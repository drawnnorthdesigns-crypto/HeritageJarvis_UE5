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

	UE_LOG(LogTemp, Log, TEXT("TartariaWorldSubsystem: Synced — Era=%s, Day=%d, POIs=%d"),
		*CurrentEra, CurrentDay, ActivePOIs.Num());
}
