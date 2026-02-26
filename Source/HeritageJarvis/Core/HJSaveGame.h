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
    // Statics — use these instead of hard-coding slot name
    // -------------------------------------------------------

    static const FString SlotName;
    static const int32   UserIndex;
};
