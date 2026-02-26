#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaTypes.h"
#include "TartariaBiomeVolume.generated.h"

class USphereComponent;
class UPostProcessComponent;

/**
 * ATartariaBiomeVolume — Placed in level to define biome zone boundaries.
 * Each biome has unique post-processing atmosphere and collision detection.
 * On zone entry, calls /api/game/threat/check to determine if an encounter triggers.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaBiomeVolume : public AActor
{
	GENERATED_BODY()

public:
	ATartariaBiomeVolume();

	/** Biome identifier matching backend keys. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Biome")
	FString BiomeKey = TEXT("CLEARINGHOUSE");

	/** Radius of the biome zone in UE units (100 UU = 1m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Biome")
	float ZoneRadius = 150000.f;

	/** Difficulty level (1-5). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Biome")
	int32 Difficulty = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Biome")
	USphereComponent* ZoneBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Biome")
	UPostProcessComponent* BiomePostProcess;

	// -------------------------------------------------------
	// Threat detection (Phase 2)
	// -------------------------------------------------------

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnThreatDetected,
		const FTartariaThreatInfo&, Threat, ATartariaBiomeVolume*, Volume);

	/** Fires when a threat is detected on zone entry. */
	UPROPERTY(BlueprintAssignable, Category = "Tartaria|Combat")
	FOnThreatDetected OnThreatDetected;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnZoneChanged,
		const FString&, NewBiome, int32, NewDifficulty);

	/** Fires when player enters this zone (always, regardless of threat). */
	UPROPERTY(BlueprintAssignable, Category = "Tartaria|Biome")
	FOnZoneChanged OnZoneEntered;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent,
	                         AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                         int32 OtherBodyIndex, bool bFromSweep,
	                         const FHitResult& SweepResult);

	UFUNCTION()
	void OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent,
	                       AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                       int32 OtherBodyIndex);

private:
	void OnThreatCheckResponse(bool bSuccess, const FString& Body);

	/** Cooldown to prevent rapid re-triggering. */
	float LastThreatCheckTime = -999.f;
	static constexpr float ThreatCheckCooldownSec = 10.f;
};
