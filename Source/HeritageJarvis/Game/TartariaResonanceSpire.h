#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaResonanceSpire.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UParticleSystemComponent;

/**
 * ATartariaResonanceSpire — The world's central landmark.
 * 500m tall compound primitive tower visible from all biomes.
 * Pulsing light intensity keyed to hermetic cycle phase from Python backend.
 * Serves as the world's orienting beacon — the Tartarian Erdtree.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaResonanceSpire : public AActor
{
	GENERATED_BODY()

public:
	ATartariaResonanceSpire();

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

	// -------------------------------------------------------
	// Structure Components
	// -------------------------------------------------------

	/** Root scene component for hierarchy. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Spire")
	USceneComponent* SpireRoot;

	/** Base platform — large octagonal approximation (flattened cylinder). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Spire")
	UStaticMeshComponent* BasePlatform;

	/** Lower shaft — thick cylinder. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Spire")
	UStaticMeshComponent* LowerShaft;

	/** Upper shaft — thinner cylinder tapering upward. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Spire")
	UStaticMeshComponent* UpperShaft;

	/** Crown crystal — sphere at the peak. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Spire")
	UStaticMeshComponent* CrownCrystal;

	/** Crown spikes — 4 cone accents around the crown. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Spire")
	UStaticMeshComponent* CrownSpike1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Spire")
	UStaticMeshComponent* CrownSpike2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Spire")
	UStaticMeshComponent* CrownSpike3;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Spire")
	UStaticMeshComponent* CrownSpike4;

	/** Ring accent midway up the spire. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Spire")
	UStaticMeshComponent* MidRing;

	// -------------------------------------------------------
	// Lighting
	// -------------------------------------------------------

	/** Crown beacon — main light visible from far away. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Spire")
	UPointLightComponent* CrownBeacon;

	/** Base glow — warm light at the foundation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Spire")
	UPointLightComponent* BaseGlow;

	/** Mid-level accent light. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Spire")
	UPointLightComponent* MidGlow;

	// -------------------------------------------------------
	// Configuration
	// -------------------------------------------------------

	/** Current hermetic phase (0.0 = dormant, 1.0 = peak resonance). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Spire")
	float HermeticPhase = 0.5f;

	/** Base intensity for the crown beacon at full resonance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Spire")
	float MaxBeaconIntensity = 500000.f;

	/** Minimum beacon intensity at dormant phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Spire")
	float MinBeaconIntensity = 50000.f;

	/** How often to poll the backend for hermetic phase (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Spire")
	float PollInterval = 30.f;

private:
	void BuildSpireStructure();
	void UpdateBeaconFromPhase(float DeltaTime);
	void PollHermeticPhase();

	UStaticMeshComponent* CreateSpirePart(const FName& Name, const TCHAR* ShapePath,
		USceneComponent* Parent, const FVector& RelLoc, const FVector& Scale,
		const FLinearColor& Color);

	float PollTimer = 0.f;
	float PulsePhase = 0.f;
	float CurrentBeaconIntensity = 100000.f;
	float TargetBeaconIntensity = 100000.f;

	// Crown rotation
	float CrownRotation = 0.f;
};
