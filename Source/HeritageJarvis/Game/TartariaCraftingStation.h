#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HJInteractable.h"
#include "TartariaCraftingStation.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UTextRenderComponent;
class UWidgetComponent;

/**
 * ATartariaCraftingStation — In-world crafting station that lets the player
 * craft items without opening the CEF dashboard.
 *
 * Three station types (set via StationType property):
 *   "anvil"     — weapon and armour recipes, dark iron look, orange glow.
 *   "workbench" — tools and equipment recipes, warm wood look, white glow.
 *   "alembic"   — potion and charm recipes, glass-like look, green/purple glow.
 *
 * Interaction flow:
 *   1. Player presses E → OnInteract_Implementation() fires.
 *   2. FetchRecipes() HTTP-GETs /api/game/crafting/recipes?station_type=X
 *      and caches the recipe list locally.
 *   3. Player selects a recipe → SubmitCraft() HTTP-POSTs
 *      /api/game/crafting/craft-at-station with recipe_id + station_type.
 *   4. OnCraftComplete() plays success/fail sound via TartariaSoundManager
 *      and shows a toast notification.
 *
 * Visual differentiation is applied in BeginPlay() based on StationType.
 * The NameTag TextRender component billboards to face the camera in Tick().
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaCraftingStation : public AActor, public IHJInteractable
{
	GENERATED_BODY()

public:
	ATartariaCraftingStation();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------
	// IHJInteractable
	// -------------------------------------------------------

	virtual void OnInteract_Implementation(APlayerController* Interactor) override;
	virtual FString GetInteractPrompt_Implementation() const override;

	// -------------------------------------------------------
	// Designer-facing properties
	// -------------------------------------------------------

	/** Station archetype: "anvil", "workbench", or "alembic". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Crafting")
	FString StationType = TEXT("workbench");

	/** Display name shown on the name tag and in the interact prompt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Crafting")
	FString StationName = TEXT("Crafting Station");

	// -------------------------------------------------------
	// Components
	// -------------------------------------------------------

	/** Primary visual mesh for the station body. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Crafting")
	UStaticMeshComponent* StationMesh;

	/** Ambient point light coloured per station type. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Crafting")
	UPointLightComponent* StationGlow;

	/** Floating billboard label showing StationName. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Crafting")
	UTextRenderComponent* NameTag;

	/** World-space widget component for the recipe selection panel.
	 *  Populated by FetchRecipes() and shown on interact. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Crafting")
	UWidgetComponent* RecipePanel;

	// -------------------------------------------------------
	// Runtime API methods
	// -------------------------------------------------------

	/**
	 * Fetch available recipes from the backend for this station type.
	 * HTTP GET /api/game/crafting/recipes?station_type=<StationType>
	 * Populates CachedRecipes and logs the count.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Crafting")
	void FetchRecipes();

	/**
	 * Submit a craft request for the given recipe ID at this station.
	 * HTTP POST /api/game/crafting/craft-at-station
	 *   { "recipe_id": <RecipeId>, "station_type": <StationType> }
	 * On response calls OnCraftComplete().
	 *
	 * @param RecipeId  The recipe key, e.g. "master_blade".
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Crafting")
	void SubmitCraft(const FString& RecipeId);

	/**
	 * Called when a craft HTTP response is received.
	 * Plays sound, shows toast, and optionally spawns a VFX burst.
	 *
	 * @param bSuccess  True when the backend confirms the craft succeeded.
	 * @param ItemName  Human-readable name of the crafted item (or error msg).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Crafting")
	void OnCraftComplete(bool bSuccess, const FString& ItemName);

	// -------------------------------------------------------
	// Blueprint events
	// -------------------------------------------------------

	/** Fired after FetchRecipes() completes — Blueprint can populate UI. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|Crafting")
	void OnRecipesFetched(const TArray<FString>& RecipeIds);

	/** Fired after a craft completes — Blueprint can play custom VFX. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|Crafting")
	void OnCraftFinished(bool bSuccess, const FString& ItemName, const FString& Rarity);

private:
	// ── Cached state ─────────────────────────────────────────

	/** Recipe IDs last returned by FetchRecipes(). */
	TArray<FString> CachedRecipes;

	/** Whether the recipe list has been fetched at least once this session. */
	bool bRecipesFetched = false;

	/** Whether the recipe panel is currently visible. */
	bool bPanelOpen = false;

	// ── Glow pulse animation ─────────────────────────────────

	/** Accumulated time for the idle glow pulse. */
	float GlowPulseAccumulator = 0.f;

	/** Base intensity established in BeginPlay() per station type. */
	float BaseGlowIntensity = 3000.f;

	/** Craft success flash state. */
	bool bCraftFlashActive = false;
	float CraftFlashElapsed = 0.f;
	static constexpr float CraftFlashDuration = 1.2f;

	// ── Helpers ──────────────────────────────────────────────

	/** Configure mesh scale, material colour, and glow colour for StationType. */
	void ApplyStationTypeVisuals();

	/** Spawn upward-drifting sparkle orbs to celebrate a craft completion. */
	void SpawnCraftVFX();
};
