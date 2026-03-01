#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaAlchemicalScales.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * ATartariaAlchemicalScales — Physical trade balance indicator.
 *
 * A pair of weighing pans on a fulcrum, tilting based on trade balance:
 *   - Positive balance (exports > imports): left pan tips down (gold side)
 *   - Negative balance: right pan tips down (debt side)
 *   - Balanced: level
 *
 * The tilt angle and glow color change dynamically based on the economy.
 * Compound-primitive construction: cylinder fulcrum, cube beam, sphere pans.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaAlchemicalScales : public AActor
{
	GENERATED_BODY()

public:
	ATartariaAlchemicalScales();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Update the trade balance (-1.0 to +1.0, 0 = balanced). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Economy")
	void UpdateTradeBalance(float NormalizedBalance);

	/** Update from raw credit/debt values. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Economy")
	void UpdateFromCredits(int32 Credits, int32 Debt);

	// Components

	/** Central fulcrum pillar. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Economy")
	UStaticMeshComponent* FulcrumPillar;

	/** Horizontal beam that tilts. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Economy")
	UStaticMeshComponent* BeamMesh;

	/** Left pan (prosperity side). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Economy")
	UStaticMeshComponent* LeftPan;

	/** Right pan (debt side). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Economy")
	UStaticMeshComponent* RightPan;

	/** Left pan chain (thin cylinder). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Economy")
	UStaticMeshComponent* LeftChain;

	/** Right pan chain (thin cylinder). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Economy")
	UStaticMeshComponent* RightChain;

	/** Status glow. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Economy")
	UPointLightComponent* StatusGlow;

private:
	/** Current balance target (-1 to +1). */
	float TargetBalance = 0.f;

	/** Current animated balance (lerps toward target). */
	float CurrentBalance = 0.f;

	/** Maximum tilt angle in degrees. */
	static constexpr float MAX_TILT_DEG = 25.f;

	/** Beam half-length (distance from center to pan). */
	static constexpr float BEAM_HALF_LEN = 80.f;

	/** Beam height above ground. */
	static constexpr float BEAM_HEIGHT = 120.f;

	/** Animation time. */
	float AnimTime = 0.f;

	/** Update beam tilt and pan positions. */
	void UpdateScaleVisuals();
};
