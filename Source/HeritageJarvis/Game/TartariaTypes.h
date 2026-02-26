#pragma once

#include "CoreMinimal.h"
#include "TartariaTypes.generated.h"

/**
 * Shared struct definitions for the Tartaria open world.
 */

/** Biome zone definition — synced from Flask backend. */
USTRUCT(BlueprintType)
struct FTartariaBiomeZone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria")
	FString BiomeKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria")
	FString Theme;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria")
	int32 Difficulty = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria")
	FVector WorldCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria")
	float Radius = 10000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria")
	TArray<FString> Resources;
};

/** Point of interest in the world — quests, events, NPCs, etc. */
USTRUCT(BlueprintType)
struct FTartariaPOI
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria")
	FString POIId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria")
	FString POIType;  // "quest", "event", "threat", "npc", "resource"

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria")
	FString BiomeKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria")
	bool bActive = true;
};

/** Resource type enum for harvestable nodes. */
UENUM(BlueprintType)
enum class ETartariaResourceType : uint8
{
	Iron        UMETA(DisplayName = "Iron"),
	Stone       UMETA(DisplayName = "Stone"),
	Knowledge   UMETA(DisplayName = "Knowledge"),
	Crystal     UMETA(DisplayName = "Crystal")
};

/** Building type enum for forge buildings. */
UENUM(BlueprintType)
enum class ETartariaBuildingType : uint8
{
	Forge        UMETA(DisplayName = "Forge"),
	Scriptorium  UMETA(DisplayName = "Scriptorium"),
	Treasury     UMETA(DisplayName = "Treasury"),
	Barracks     UMETA(DisplayName = "Barracks"),
	Lab          UMETA(DisplayName = "Laboratory")
};

/** Faction reputation info — parsed from world_state. */
USTRUCT(BlueprintType)
struct FTartariaFactionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Economy")
	FString FactionKey;   // ARGENTUM, AUREATE, FERRUM, OBSIDIAN

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Economy")
	float Influence = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Economy")
	FString Domain;
};

/** Single inventory item — parsed from /api/game/inventory. */
USTRUCT(BlueprintType)
struct FTartariaInventoryItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Economy")
	FString ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Economy")
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Economy")
	bool bEquipped = false;
};

/** Event emitted by a game tick — raids, completions, annexations, etc. */
USTRUCT(BlueprintType)
struct FTartariaTickEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Economy")
	FString Type;    // RAID_VICTORY, BOOK_COMPLETED, ANNEXATION, HABITABLE, etc.

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Economy")
	FString Detail;  // Human-readable detail string

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Economy")
	int32 Value = 0;
};
