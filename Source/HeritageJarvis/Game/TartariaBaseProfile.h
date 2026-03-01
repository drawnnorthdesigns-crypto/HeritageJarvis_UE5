#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TartariaBaseProfile.generated.h"

/**
 * Per-celestial-body visual profile data.
 */
USTRUCT(BlueprintType)
struct FTartariaBaseVisualProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString BodyName;

	/** Sky/background color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor SkyColor = FLinearColor(0.1f, 0.1f, 0.2f);

	/** Ambient light color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor AmbientColor = FLinearColor(0.15f, 0.15f, 0.2f);

	/** Ambient light intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AmbientIntensity = 0.5f;

	/** Directional (sun) light color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor SunColor = FLinearColor(1.f, 0.95f, 0.85f);

	/** Sun intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SunIntensity = 3.0f;

	/** Fog color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor FogColor = FLinearColor(0.3f, 0.3f, 0.4f);

	/** Fog density (0=none, 0.05=thick). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FogDensity = 0.001f;

	/** Ground plane color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor GroundColor = FLinearColor(0.3f, 0.25f, 0.2f);

	/** Gravity multiplier (1.0 = Earth). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GravityMultiplier = 1.0f;

	/** Day cycle speed multiplier (1.0 = normal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DayCycleSpeed = 1.0f;
};

/**
 * UTartariaBaseProfile — Static library of visual profiles per celestial body.
 *
 * Each body (Earth, Moon, Mars, Europa, Titan, Ceres, Venus, Mercury,
 * Deep Space, Tartaria Prime) has distinct sky, fog, lighting, and ground
 * colors that completely reshape the atmosphere on arrival.
 *
 * Usage:
 *   FTartariaBaseVisualProfile Profile = UTartariaBaseProfile::GetProfile("mars");
 *   // Apply Profile.SkyColor, Profile.FogDensity, etc. to world systems
 */
UCLASS()
class HERITAGEJARVIS_API UTartariaBaseProfile : public UObject
{
	GENERATED_BODY()

public:
	/** Get visual profile for a celestial body. Returns Earth profile as fallback. */
	static FTartariaBaseVisualProfile GetProfile(const FString& BodyName);

	/** Get all available profiles (for UI listing). */
	static TArray<FTartariaBaseVisualProfile> GetAllProfiles();

private:
	static void InitProfiles();
	static TArray<FTartariaBaseVisualProfile> Profiles;
	static bool bInitialized;
};
