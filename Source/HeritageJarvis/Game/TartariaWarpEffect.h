#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaWarpEffect.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * ATartariaWarpEffect — Aetheric warp tunnel VFX during interplanetary transit.
 *
 * Spawned on the player character when supercruise begins, destroyed on arrival.
 * Creates a spinning compound-primitive warp tunnel from basic engine shapes:
 *   - 4 concentric rings rotating at different speeds
 *   - Streak bars simulating motion parallax
 *   - Pulsing glow light
 *   - Camera FOV manipulation (widen on entry, normalize on exit)
 *
 * All primitives use LOCAL SPACE for origin rebasing safety.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaWarpEffect : public AActor
{
	GENERATED_BODY()

public:
	ATartariaWarpEffect();
	virtual void Tick(float DeltaTime) override;

	/** Activate the warp effect. Call when transit begins. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Warp")
	void ActivateWarp(const FString& DestinationBody);

	/** Deactivate and begin the exit sequence. Call when transit ends. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Warp")
	void DeactivateWarp();

	/** Is the warp effect currently active? */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Warp")
	bool bWarpActive = false;

	/** Destination body name (used for color theming). */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Warp")
	FString Destination;

private:
	/** Build the compound primitive warp tunnel. */
	void BuildWarpGeometry();

	/** Update ring rotations and streak animations. */
	void AnimateWarp(float DeltaTime);

	/** Smoothly adjust camera FOV. */
	void UpdateCameraFOV(float DeltaTime);

	// -------------------------------------------------------
	// Components
	// -------------------------------------------------------

	UPROPERTY(VisibleAnywhere)
	USceneComponent* WarpRoot;

	/** 4 concentric rings (torus approximated by stretched cubes on circle path). */
	UPROPERTY()
	TArray<UStaticMeshComponent*> RingSegments;

	/** 8 streak bars (long thin cubes simulating speed lines). */
	UPROPERTY()
	TArray<UStaticMeshComponent*> StreakBars;

	/** Central warp glow. */
	UPROPERTY(VisibleAnywhere)
	UPointLightComponent* WarpGlow;

	/** Secondary glow at warp "exit" point. */
	UPROPERTY(VisibleAnywhere)
	UPointLightComponent* ExitGlow;

	// -------------------------------------------------------
	// Animation state
	// -------------------------------------------------------

	float WarpTime = 0.f;
	float TargetFOV = 90.f;
	float CurrentFOVAdjust = 0.f;

	/** Ring rotation speeds (degrees per second). */
	static constexpr float RING_SPEEDS[4] = { 120.f, -90.f, 150.f, -60.f };

	/** Ring radii. */
	static constexpr float RING_RADII[4] = { 200.f, 280.f, 360.f, 440.f };

	/** Number of segments per ring. */
	static constexpr int32 SEGMENTS_PER_RING = 8;

	/** Warp color palette. */
	FLinearColor WarpColor = FLinearColor(0.3f, 0.5f, 1.0f);  // Default: blue-white
};
