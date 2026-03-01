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

/** NPC activity state — drives visual animation in ATartariaNPC. */
UENUM(BlueprintType)
enum class ENPCActivityState : uint8
{
	Idle      UMETA(DisplayName = "Idle"),
	Working   UMETA(DisplayName = "Working"),
	Thinking  UMETA(DisplayName = "Thinking"),
	Moving    UMETA(DisplayName = "Moving")
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

/** Physical surface material type -- drives impact audio variation. */
UENUM(BlueprintType)
enum class ETartariaPhysicalMaterial : uint8
{
	Stone    UMETA(DisplayName = "Stone"),
	Metal    UMETA(DisplayName = "Metal"),
	Wood     UMETA(DisplayName = "Wood"),
	Crystal  UMETA(DisplayName = "Crystal"),
	Earth    UMETA(DisplayName = "Earth"),
	Water    UMETA(DisplayName = "Water")
};

/**
 * Biome ambient audio profile (Task #215).
 * Describes the tonal characteristics for each biome zone's ambient loop.
 * TartariaSoundManager uses these to configure UAudioComponents with
 * biome-specific pitch, volume, rhythm (LFO-like oscillation), and reverb.
 */
USTRUCT(BlueprintType)
struct FBiomeAudioProfile
{
	GENERATED_BODY()

	/** Fundamental frequency of the ambient tone (Hz). Maps to pitch multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Audio")
	float BaseFrequencyHz = 80.f;

	/** Oscillation frequency (Hz) for rhythmic volume modulation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Audio")
	float RhythmHz = 1.f;

	/** Base volume of the ambient loop (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Audio")
	float Volume = 0.5f;

	/** Reverb decay time (seconds). Longer = more cavernous. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Audio")
	float ReverbDecay = 1.5f;

	/** Depth of rhythmic volume oscillation (0 = steady, 1 = full swing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Audio")
	float RhythmDepth = 0.3f;
};

/** Audio profile describing how a physical material sounds on impact. */
USTRUCT(BlueprintType)
struct FMaterialAudioProfile
{
	GENERATED_BODY()

	/** Fundamental frequency of the impact sound (Hz). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Audio")
	float BasePitch = 120.f;

	/** Random variance applied to pitch (fraction, e.g. 0.1 = +/-10%). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Audio")
	float PitchVariance = 0.1f;

	/** How quickly the sound fades (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Audio")
	float Decay = 0.15f;

	/** Resonance factor (0 = dead/muffled, 1 = bright/ringing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Audio")
	float Resonance = 0.2f;

	/** Debug visualization color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Audio")
	FLinearColor DebugColor = FLinearColor::White;
};

/**
 * Weapon combat stats derived from CAD mesh geometry (Task #203).
 * Parsed from /api/mesh/metadata or /api/mesh/combat-stats endpoint.
 * UE5 uses these to configure dynamic weapon hitboxes and attack parameters.
 */
USTRUCT(BlueprintType)
struct FWeaponCombatStats
{
	GENERATED_BODY()

	/** Weapon reach in cm (longest mesh axis / 10). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Combat")
	float Reach = 50.0f;

	/** Weapon weight in kg (volume * density). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Combat")
	float WeightKg = 1.0f;

	/** Base damage value (10-100, scales with volume). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Combat")
	int32 BaseDamage = 15;

	/** Attack speed multiplier (0.3 for heavy, 1.0 for light). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Combat")
	float AttackSpeed = 1.0f;

	/** Hitbox half-extents in cm for UE5 box collision (derived from mesh bbox). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Combat")
	FVector HitboxExtents = FVector(5.0f, 5.0f, 25.0f);

	/** Whether these stats have been populated from backend data. */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Combat")
	bool bIsValid = false;
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
