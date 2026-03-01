#pragma once

#include "CoreMinimal.h"
#include "TartariaTypes.generated.h"

/**
 * Shared struct definitions for the Tartaria open world.
 */

/** Biome zone types — used by ambient audio, VFX, and world systems. */
UENUM(BlueprintType)
enum class ETartariaBiome : uint8
{
	Clearinghouse  UMETA(DisplayName = "Clearinghouse"),
	Scriptorium    UMETA(DisplayName = "Scriptorium"),
	MonolithWard   UMETA(DisplayName = "Monolith Ward"),
	ForgeDistrict  UMETA(DisplayName = "Forge District"),
	VoidReach      UMETA(DisplayName = "Void Reach")
};

/** NPC specialist types — each has unique dialogue context. */
UENUM(BlueprintType)
enum class ETartariaSpecialist : uint8
{
	Steward        UMETA(DisplayName = "Steward"),
	ForgeMaster    UMETA(DisplayName = "Forge Master"),
	Scribe         UMETA(DisplayName = "Scribe"),
	Alchemist      UMETA(DisplayName = "Alchemist"),
	WarCaptain     UMETA(DisplayName = "War Captain"),
	Surveyor       UMETA(DisplayName = "Surveyor"),
	Governor       UMETA(DisplayName = "Governor"),
};

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

/** Threat info returned by /api/game/threat/check. */
USTRUCT(BlueprintType)
struct FTartariaThreatInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Combat")
	bool bSafe = true;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Combat")
	FString EnemyKey;  // THE_DUSTBORN, VOID_PIRATES, etc.

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Combat")
	int32 EnemyPower = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Combat")
	int32 EffectivePower = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Combat")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Combat")
	FString BiomeKey;
};

/** Result of resolving a threat encounter via /api/game/threat/resolve. */
USTRUCT(BlueprintType)
struct FTartariaEncounterResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Combat")
	bool bVictory = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Combat")
	int32 RewardCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Combat")
	float FactionRep = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Combat")
	int32 DamageTaken = 0;
};

/** Single ingredient requirement in a crafting recipe. */
USTRUCT(BlueprintType)
struct FTartariaCraftIngredient
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Crafting")
	FString ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Crafting")
	int32 Quantity = 1;
};

/** Crafting recipe — parsed from /api/game/craft/recipes. */
USTRUCT(BlueprintType)
struct FTartariaCraftingRecipe
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Crafting")
	FString RecipeKey;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Crafting")
	FString ResultItem;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Crafting")
	TArray<FTartariaCraftIngredient> Ingredients;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Crafting")
	bool bCanCraft = false;  // Set client-side based on inventory
};

/** Fleet summary — parsed from /api/game/status/full. */
USTRUCT(BlueprintType)
struct FTartariaFleetSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Fleet")
	int32 TotalPower = 500;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Fleet")
	int32 ReservePower = 500;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Fleet")
	int32 DeployedPower = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Fleet")
	int32 DeployedZones = 0;
};

/** Tech tree summary — parsed from /api/game/status/full. */
USTRUCT(BlueprintType)
struct FTartariaTechSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Tech")
	int32 TotalNodes = 8;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Tech")
	int32 UnlockedCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Tech")
	int32 AvailableNext = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Tech")
	FString HighestTech;
};

/** Mining summary — parsed from /api/game/status/full. */
USTRUCT(BlueprintType)
struct FTartariaMiningSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Mining")
	int32 TotalMined = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Mining")
	int32 AsteroidsScanned = 0;
};

// -------------------------------------------------------
// Solar System / Transit types
// -------------------------------------------------------

/** Player flight state for transit between celestial bodies. */
UENUM(BlueprintType)
enum class EFlightState : uint8
{
	OnFoot       UMETA(DisplayName = "On Foot"),
	Docked       UMETA(DisplayName = "Docked at Station"),
	Supercruise  UMETA(DisplayName = "Supercruise"),
	Orbital      UMETA(DisplayName = "Orbital Flight")
};

/** Celestial body data — mirrors Python solar_system.py BODIES. */
USTRUCT(BlueprintType)
struct FTartariaCelestialBody
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Solar")
	FString BodyKey;  // "earth", "mars", "jupiter", etc.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Solar")
	FString BodyType;  // "Star", "Planet", "Moon", "Mining Region"

	/** Mass in kg (double precision stored as two floats for UE4 compat). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Solar")
	double MassKg = 0.0;

	/** Body radius in km. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Solar")
	double RadiusKm = 0.0;

	/** Orbital radius in AU from parent body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Solar")
	double OrbitalRadiusAU = 0.0;

	/** Orbital period in Earth days. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Solar")
	double OrbitalPeriodDays = 0.0;

	/** Surface gravity in m/s^2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Solar")
	float SurfaceGravity = 0.f;

	/** Escape velocity in m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Solar")
	float EscapeVelocity = 0.f;

	/** Whether body has atmosphere. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Solar")
	bool bHasAtmosphere = false;

	/** Current orbital angle in radians (updated by simulation). */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Solar")
	double CurrentAngleRad = 0.0;
};

/** Transit request — sent to Python /api/game/transit/start. */
USTRUCT(BlueprintType)
struct FTartariaTransitRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Tartaria|Transit")
	FString OriginBody;

	UPROPERTY(BlueprintReadWrite, Category = "Tartaria|Transit")
	FString DestinationBody;

	UPROPERTY(BlueprintReadWrite, Category = "Tartaria|Transit")
	FString TransferType = TEXT("hohmann");  // hohmann, direct, brachistochrone
};

/** Transit status — returned by Python /api/game/transit/status. */
USTRUCT(BlueprintType)
struct FTartariaTransitStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Transit")
	bool bInTransit = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Transit")
	FString OriginBody;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Transit")
	FString DestinationBody;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Transit")
	float ProgressFraction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Transit")
	float DeltaVRequired = 0.f;  // m/s

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Transit")
	float TransitTimeSec = 0.f;  // Real-time seconds for game-compressed transit

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Transit")
	float ElapsedSec = 0.f;
};
