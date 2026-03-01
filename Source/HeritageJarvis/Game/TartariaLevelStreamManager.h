#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaBaseProfile.h"
#include "TartariaLevelStreamManager.generated.h"

class ATartariaDayNightCycle;
class UStaticMeshComponent;

/**
 * ATartariaLevelStreamManager — Manages seamless transitions between celestial bodies.
 *
 * When the player transits from Earth to Mars (or any other body), this manager:
 *   1. Fades the camera to black
 *   2. Hides all Earth-spawned world actors (biomes, buildings, NPCs, etc.)
 *   3. Spawns a temporary terrain plane with the destination body's ground color
 *   4. Applies the destination body's visual profile (sky, fog, sun, gravity)
 *   5. Updates the day/night cycle speed
 *   6. Fades the camera back in
 *
 * On return to Earth, the process reverses: temporary content is destroyed,
 * original actors are re-shown, and Earth's visual profile is restored.
 *
 * Spawned once by TartariaGameMode. Access via static Get().
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaLevelStreamManager : public AActor
{
	GENERATED_BODY()

public:
	ATartariaLevelStreamManager();
	virtual void BeginPlay() override;

	// -------------------------------------------------------
	// API
	// -------------------------------------------------------

	/** Begin transition to a new celestial body. Handles fade, hide/show, profile apply. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|LevelStream")
	void TransitionToBody(const FString& NewBodyName);

	/** Get the body currently loaded. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tartaria|LevelStream")
	FString GetCurrentLoadedBody() const { return CurrentLoadedBody; }

	/** Is a transition currently in progress? */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tartaria|LevelStream")
	bool IsTransitioning() const { return bTransitioning; }

	// -------------------------------------------------------
	// Singleton
	// -------------------------------------------------------

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tartaria|LevelStream",
	          meta = (WorldContext = "WorldContextObject"))
	static ATartariaLevelStreamManager* Get(const UObject* WorldContextObject);

	// -------------------------------------------------------
	// Delegates
	// -------------------------------------------------------

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBodyLoaded, const FString&, BodyName);

	/** Broadcast after the new body's content is fully loaded and visible. */
	UPROPERTY(BlueprintAssignable, Category = "Tartaria|LevelStream")
	FOnBodyLoaded OnBodyLoaded;

private:
	/** Apply visual profile to world systems (lights, fog, sky, gravity). */
	void ApplyVisualProfile(const FTartariaBaseVisualProfile& Profile);

	/** Hide all actors spawned by WorldPopulator (tag-based). */
	void HideWorldPopulatorActors();

	/** Re-show all hidden WorldPopulator actors. */
	void ShowWorldPopulatorActors();

	/** Spawn minimal terrain plane for non-Earth bodies. */
	void SpawnBodyTerrain(const FTartariaBaseVisualProfile& Profile);

	/** Destroy any temporary body terrain. */
	void DestroyBodyTerrain();

	/** Called after fade-out completes to do the actual swap. */
	void OnFadeOutComplete();

	/** Called after content swap to fade back in. */
	void FadeIn();

	/** The body currently loaded in the world. */
	UPROPERTY()
	FString CurrentLoadedBody = TEXT("earth");

	/** The body we're transitioning TO (set before fade-out). */
	UPROPERTY()
	FString PendingBody;

	/** Is a transition in progress? */
	bool bTransitioning = false;

	/** Temporary terrain actor for non-Earth bodies. */
	UPROPERTY()
	AActor* BodyTerrainActor = nullptr;

	/** Timer for fade-out -> swap -> fade-in sequence. */
	FTimerHandle FadeOutTimerHandle;
	FTimerHandle FadeInTimerHandle;

	/** Actors hidden during body transition (restored on return to Earth). */
	UPROPERTY()
	TArray<AActor*> HiddenActors;

	/** Cached singleton. */
	static TWeakObjectPtr<ATartariaLevelStreamManager> Instance;

	/** Tags used by WorldPopulator for spawned actors. */
	static const TArray<FName>& GetPopulatorTags();
};
