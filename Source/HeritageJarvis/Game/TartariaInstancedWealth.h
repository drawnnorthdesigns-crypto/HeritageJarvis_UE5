#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaInstancedWealth.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UPointLightComponent;

/**
 * ATartariaInstancedWealth — Renders gold bar stacks using HISM (single draw call).
 *
 * Placed inside the Treasury building. Instead of spawning N individual actors
 * for gold bars (which would be N draw calls), this uses
 * UHierarchicalInstancedStaticMeshComponent to render hundreds of gold bars
 * in 1-3 draw calls.
 *
 * The gold count is set from the game state (credits/treasury balance).
 * Call UpdateGoldCount() to add/remove instances dynamically.
 *
 * Each bar uses Engine/BasicShapes/Cube scaled to bar proportions (4:2:1).
 * Bars are stacked in a pyramid/vault pattern.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaInstancedWealth : public AActor
{
	GENERATED_BODY()

public:
	ATartariaInstancedWealth();

	virtual void BeginPlay() override;

	// ── Configuration ───────────────────────────────────────
	/** Maximum gold bars to display (performance cap). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Wealth")
	int32 MaxBars = 500;

	/** Current gold bar count (set from economy state). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Wealth")
	int32 CurrentBars = 50;

	/** Gold bar dimensions (cm): X=width, Y=depth, Z=height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Wealth")
	FVector BarSize = FVector(20.f, 10.f, 5.f);

	/** Spacing between bars in a row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Wealth")
	float BarSpacing = 2.f;

	/** Bars per row in the stack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Wealth")
	int32 BarsPerRow = 10;

	// ── Components ──────────────────────────────────────────
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Wealth")
	UHierarchicalInstancedStaticMeshComponent* GoldBarHISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Wealth")
	UPointLightComponent* WealthGlow;

	// ── Runtime ─────────────────────────────────────────────
	/**
	 * Update the displayed gold bar count.
	 * Efficiently adds/removes instances to match the target count.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Wealth")
	void UpdateGoldCount(int32 NewCount);

	/**
	 * Set gold count from economy credits (1 bar per 100 credits).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Wealth")
	void UpdateFromCredits(int32 Credits);

private:
	/** Rebuild all bar instances from scratch. */
	void RebuildBarInstances();

	/** Calculate transform for bar at index. */
	FTransform CalcBarTransform(int32 Index) const;
};
