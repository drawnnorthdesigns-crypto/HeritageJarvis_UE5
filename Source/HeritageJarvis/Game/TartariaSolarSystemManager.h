#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaTypes.h"
#include "TartariaSolarSystemManager.generated.h"

/**
 * ATartariaSolarSystemManager — Singleton actor managing the solar system.
 *
 * Holds double-precision orbital data for 10 celestial bodies (mirroring
 * Python solar_system.py). Provides:
 *   - Real-time orbital position updates (scaled time)
 *   - Coordinate conversion: solar system (AU) <-> UE5 world space (cm)
 *   - Origin rebasing support for player >50km from world origin
 *   - Transit calculations (delta-V, travel time via Hohmann transfer)
 *
 * Spawned once by TartariaGameMode. Access via static Get().
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaSolarSystemManager : public AActor
{
	GENERATED_BODY()

public:
	ATartariaSolarSystemManager();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------
	// Constants
	// -------------------------------------------------------

	/** 1 AU in meters. */
	static constexpr double AU_METERS = 1.496e11;

	/** Gravitational constant. */
	static constexpr double G_CONST = 6.674e-11;

	/** Sun mass in kg. */
	static constexpr double SUN_MASS = 1.989e30;

	/** Scale factor: 1 AU = this many UE5 centimeters.
	 *  At 1:1e6, 1 AU = 149,600 km = 14,960,000,000 cm -> 14,960 cm at 1e-6.
	 *  We use 1 AU = 100,000 cm (1 km) for navigable game scale.
	 */
	static constexpr double AU_TO_UE_CM = 100000.0;

	/** Body radius scale: real km -> UE cm.
	 *  Earth (6371 km) -> 637.1 cm = 6.37m at 1:1000.
	 */
	static constexpr double KM_TO_UE_CM = 0.1;

	/** Game time scale: how many "sim seconds" per real second.
	 *  At 86400x, 1 real second = 1 sim day.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Solar")
	float TimeScale = 86400.f;

	// -------------------------------------------------------
	// Celestial bodies
	// -------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Solar")
	TArray<FTartariaCelestialBody> Bodies;

	/** Get a body by key (case-insensitive). Returns nullptr if not found. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Solar")
	FTartariaCelestialBody GetBody(const FString& BodyKey) const;

	/** Get the UE5 world position of a celestial body (after scale + origin offset). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Solar")
	FVector GetBodyWorldPosition(const FString& BodyKey) const;

	/** Get the position of the player's current celestial body. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Solar")
	FString GetPlayerCurrentBody() const { return PlayerCurrentBody; }

	/** Set which body the player is at (updated on transit arrival). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Solar")
	void SetPlayerCurrentBody(const FString& BodyKey);

	// -------------------------------------------------------
	// Transit calculations
	// -------------------------------------------------------

	/** Calculate Hohmann transfer delta-V between two bodies (m/s). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Transit")
	float CalculateHohmannDeltaV(const FString& FromBody, const FString& ToBody) const;

	/** Calculate travel time in real seconds (compressed by TimeScale). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Transit")
	float CalculateTransitTime(const FString& FromBody, const FString& ToBody) const;

	// -------------------------------------------------------
	// Origin rebasing
	// -------------------------------------------------------

	/** Current world origin offset (added to all body positions). */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Solar")
	FVector OriginOffset = FVector::ZeroVector;

	/** Threshold distance from origin before rebasing triggers (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Solar")
	double RebaseThresholdCm = 5000000.0;  // 50km

	/** Rebase world origin to center on the given body. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Solar")
	void RebaseOriginToBody(const FString& BodyKey);

	// -------------------------------------------------------
	// Singleton access
	// -------------------------------------------------------

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tartaria|Solar",
	          meta = (WorldContext = "WorldContextObject"))
	static ATartariaSolarSystemManager* Get(const UObject* WorldContextObject);

private:
	void InitializeBodies();
	void UpdateOrbitalPositions(float DeltaTime);

	/** Accumulated simulation time in seconds. */
	double SimTimeSec = 0.0;

	/** Body the player is currently at. */
	UPROPERTY()
	FString PlayerCurrentBody = TEXT("earth");

	/** Cached singleton reference. */
	static TWeakObjectPtr<ATartariaSolarSystemManager> Instance;
};
