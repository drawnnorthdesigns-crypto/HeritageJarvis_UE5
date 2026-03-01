#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaInstancedWealth.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UPointLightComponent;
class UParticleSystemComponent;

/**
 * ATartariaInstancedWealth — Renders gold bar stacks using HISM (single draw call).
 *
 * Placed inside the Treasury building. Instead of spawning N individual actors
 * for gold bars (which would be N draw calls), this uses
 * UHierarchicalInstancedStaticMeshComponent to render hundreds of gold bars
 * in 1-3 draw calls.
 *
 * Wired to live economy data via UpdateFromWorldState(). The
 * TartariaWorldSubsystem calls this method after every world_state sync,
 * feeding the current credit balance. Bars animate smoothly toward the
 * target count and a golden particle burst fires on large transactions.
 *
 * Each bar uses Engine/BasicShapes/Cube scaled to ingot proportions (20x8x8 cm).
 * Bars are laid out in a 10-wide grid with slight random rotation.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaInstancedWealth : public AActor
{
	GENERATED_BODY()

public:
	ATartariaInstancedWealth();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ── Configuration ───────────────────────────────────────

	/** Maximum gold bars to display (performance cap). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Wealth")
	int32 MaxBars = 200;

	/** Current gold bar count (set from economy state). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Wealth")
	int32 CurrentBars = 0;

	/** Gold bar dimensions (cm): X=width, Y=depth, Z=height — ingot proportions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Wealth")
	FVector BarSize = FVector(20.f, 8.f, 8.f);

	/** Spacing between bars in a row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Wealth")
	float BarSpacing = 2.f;

	/** Bars per row in the grid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Wealth")
	int32 BarsPerRow = 10;

	/** How many gold bars per 1000 credits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Wealth")
	int32 BarsPerThousandCredits = 1;

	// ── Components ──────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Wealth")
	UHierarchicalInstancedStaticMeshComponent* GoldBarHISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Wealth")
	UPointLightComponent* WealthGlow;

	/** Golden particle burst — fires on large transactions. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Wealth")
	UParticleSystemComponent* TransactionBurstPSC;

	// ── Live Economy Interface ──────────────────────────────

	/**
	 * Primary entry point for live economy data.
	 * Called by TartariaWorldSubsystem after every world_state sync.
	 * Calculates target bar count = CreditBalance / 1000 * BarsPerThousandCredits,
	 * clamped to MaxBars. Triggers particle burst on large deltas (>500 credits).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Wealth")
	void UpdateFromWorldState(int32 CreditBalance);

	/**
	 * Update the displayed gold bar count directly.
	 * Efficiently rebuilds instances to match the target count.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Wealth")
	void UpdateGoldCount(int32 NewCount);

	/**
	 * Legacy convenience — set gold count from credits (1 bar per 100 credits).
	 * Prefer UpdateFromWorldState() for live data wiring.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Wealth")
	void UpdateFromCredits(int32 Credits);

private:
	/** Rebuild all bar instances from scratch in a grid layout. */
	void RebuildBarInstances();

	/** Calculate transform for bar at index (grid layout with random rotation). */
	FTransform CalcBarTransform(int32 Index) const;

	// ── Smooth transition state ─────────────────────────────

	/** The credit value we are currently displaying bars for. */
	int32 CurrentDisplayedCredits = 0;

	/** The credit value we are animating toward. */
	int32 TargetCredits = 0;

	/** Previous target for detecting large transaction deltas. */
	int32 PreviousTargetCredits = 0;

	/** Timer accumulator for periodic subsystem polling (fallback). */
	float WealthUpdateTimer = 0.f;

	/** Interval in seconds for fallback polling from subsystem. */
	static constexpr float WealthPollIntervalSec = 10.f;

	/** Speed of credit lerp (credits per second). */
	static constexpr float CreditLerpSpeed = 2000.f;

	/** Threshold (in credits) for triggering the transaction burst VFX. */
	static constexpr int32 LargeTransactionThreshold = 500;

	// ── Transaction burst state ─────────────────────────────

	/** True while the glow flash is active. */
	bool bGlowFlashActive = false;

	/** Remaining time for the glow flash effect. */
	float GlowFlashTimer = 0.f;

	/** Duration of the glow flash in seconds. */
	static constexpr float GlowFlashDuration = 0.5f;

	/** Base glow intensity (before flash). */
	float BaseGlowIntensity = 8000.f;

	/** Flash multiplier for the glow. */
	static constexpr float GlowFlashMultiplier = 4.0f;

	/** Fire the golden particle burst and glow flash. */
	void TriggerTransactionBurst();
};
