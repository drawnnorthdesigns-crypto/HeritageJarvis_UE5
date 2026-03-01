#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "HJSaveGame.generated.h"

/**
 * UHJSaveGame
 * Persists user session state across game launches.
 * Slot: "HJPlayerData", UserIndex: 0
 *
 * Saved:
 *   FlaskBaseUrl         — custom Flask server address
 *   LastActiveProjectId  — restore project context on boot
 *   LastActiveProjectName
 *   LastActiveTab        — "projects" | "library" | "game"
 */
UCLASS()
class HERITAGEJARVIS_API UHJSaveGame : public USaveGame
{
    GENERATED_BODY()

public:

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save")
    FString FlaskBaseUrl = TEXT("http://127.0.0.1:5000");

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save")
    FString LastActiveProjectId;

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save")
    FString LastActiveProjectName;

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save")
    FString LastActiveTab = TEXT("projects");

    /** Path to the Heritage Jarvis Python project root (for auto-launch). */
    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save")
    FString FlaskProjectRoot;

    /** Last player location in Tartaria world (restored on return). */
    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save")
    FVector LastTartariaLocation = FVector::ZeroVector;

    /** Last player rotation in Tartaria world. */
    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save")
    FRotator LastTartariaRotation = FRotator::ZeroRotator;

    // -------------------------------------------------------
    // Game state (persists credits, resources, factions, etc.)
    // -------------------------------------------------------

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Economy")
    int32 Credits = 0;

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Economy")
    int32 CurrentDay = 1;

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Economy")
    FString CurrentEra = TEXT("FOUNDATION");

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Economy")
    float TimeOfDay = 8.0f;

    // Resource counts (flattened for fast save/load)
    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Economy")
    int32 Iron = 0;

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Economy")
    int32 Stone = 0;

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Economy")
    int32 Knowledge = 0;

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Economy")
    int32 Crystal = 0;

    // Faction reputation (4 factions, stored as parallel arrays)
    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Factions")
    TArray<FString> FactionKeys;

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Factions")
    TArray<float> FactionInfluences;

    // Fleet state
    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Fleet")
    int32 FleetTotalPower = 500;

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Fleet")
    int32 FleetDeployedZones = 0;

    // Tech tree
    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Tech")
    int32 TechUnlockedCount = 1;

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Tech")
    FString HighestTech;

    // Mining
    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Mining")
    int32 TotalMined = 0;

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Mining")
    int32 AsteroidsScanned = 0;

    // Player combat stats
    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Combat")
    float PlayerHealth = 100.0f;

    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Combat")
    FString PlayerBiome = TEXT("CLEARINGHOUSE");

    // Solar system / transit
    UPROPERTY(BlueprintReadWrite, Category = "HJ|Save|Transit")
    FString CurrentCelestialBody = TEXT("earth");

    // -------------------------------------------------------
    // Statics — use these instead of hard-coding slot name
    // -------------------------------------------------------

    static const FString SlotName;
    static const int32   UserIndex;
};
