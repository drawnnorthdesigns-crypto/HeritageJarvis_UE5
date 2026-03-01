#include "TartariaBaseProfile.h"

TArray<FTartariaBaseVisualProfile> UTartariaBaseProfile::Profiles;
bool UTartariaBaseProfile::bInitialized = false;

void UTartariaBaseProfile::InitProfiles()
{
	if (bInitialized) return;
	bInitialized = true;

	// ── EARTH (Tartaria homeworld) ──────────────────────────
	{
		FTartariaBaseVisualProfile P;
		P.BodyName = TEXT("earth");
		P.SkyColor = FLinearColor(0.1f, 0.12f, 0.22f);      // Deep blue-indigo
		P.AmbientColor = FLinearColor(0.15f, 0.18f, 0.25f);
		P.AmbientIntensity = 0.6f;
		P.SunColor = FLinearColor(1.f, 0.92f, 0.78f);        // Warm daylight
		P.SunIntensity = 3.5f;
		P.FogColor = FLinearColor(0.35f, 0.35f, 0.45f);
		P.FogDensity = 0.0008f;
		P.GroundColor = FLinearColor(0.35f, 0.3f, 0.2f);     // Earthy brown
		P.GravityMultiplier = 1.0f;
		P.DayCycleSpeed = 1.0f;
		Profiles.Add(P);
	}

	// ── MOON ────────────────────────────────────────────────
	{
		FTartariaBaseVisualProfile P;
		P.BodyName = TEXT("moon");
		P.SkyColor = FLinearColor(0.01f, 0.01f, 0.02f);      // Near-black void
		P.AmbientColor = FLinearColor(0.08f, 0.08f, 0.12f);
		P.AmbientIntensity = 0.2f;
		P.SunColor = FLinearColor(1.f, 1.f, 1.f);             // Pure white (no atmosphere)
		P.SunIntensity = 5.0f;                                 // Harsh, unfiltered
		P.FogColor = FLinearColor(0.02f, 0.02f, 0.03f);
		P.FogDensity = 0.0001f;                                // Almost none
		P.GroundColor = FLinearColor(0.25f, 0.25f, 0.27f);    // Gray regolith
		P.GravityMultiplier = 0.166f;
		P.DayCycleSpeed = 0.034f;                               // 29.5 Earth days
		Profiles.Add(P);
	}

	// ── MARS ────────────────────────────────────────────────
	{
		FTartariaBaseVisualProfile P;
		P.BodyName = TEXT("mars");
		P.SkyColor = FLinearColor(0.25f, 0.12f, 0.08f);       // Butterscotch sky
		P.AmbientColor = FLinearColor(0.2f, 0.12f, 0.08f);
		P.AmbientIntensity = 0.4f;
		P.SunColor = FLinearColor(0.9f, 0.75f, 0.55f);        // Warm but dimmer
		P.SunIntensity = 2.0f;
		P.FogColor = FLinearColor(0.45f, 0.25f, 0.12f);       // Red dust haze
		P.FogDensity = 0.003f;                                  // Dusty atmosphere
		P.GroundColor = FLinearColor(0.5f, 0.22f, 0.08f);     // Iron oxide red
		P.GravityMultiplier = 0.38f;
		P.DayCycleSpeed = 0.97f;                                // ~24.6h day
		Profiles.Add(P);
	}

	// ── EUROPA ──────────────────────────────────────────────
	{
		FTartariaBaseVisualProfile P;
		P.BodyName = TEXT("europa");
		P.SkyColor = FLinearColor(0.02f, 0.03f, 0.06f);       // Dark blue-black
		P.AmbientColor = FLinearColor(0.05f, 0.08f, 0.15f);
		P.AmbientIntensity = 0.15f;
		P.SunColor = FLinearColor(0.7f, 0.8f, 1.0f);          // Pale distant sun
		P.SunIntensity = 0.8f;                                  // Very dim
		P.FogColor = FLinearColor(0.1f, 0.15f, 0.25f);
		P.FogDensity = 0.002f;                                  // Ice crystal haze
		P.GroundColor = FLinearColor(0.55f, 0.6f, 0.65f);     // Ice-white
		P.GravityMultiplier = 0.134f;
		P.DayCycleSpeed = 0.28f;                                // 3.5 Earth days
		Profiles.Add(P);
	}

	// ── TITAN ───────────────────────────────────────────────
	{
		FTartariaBaseVisualProfile P;
		P.BodyName = TEXT("titan");
		P.SkyColor = FLinearColor(0.15f, 0.1f, 0.02f);        // Hazy orange
		P.AmbientColor = FLinearColor(0.12f, 0.08f, 0.03f);
		P.AmbientIntensity = 0.25f;
		P.SunColor = FLinearColor(0.6f, 0.45f, 0.25f);        // Dim orange
		P.SunIntensity = 0.5f;
		P.FogColor = FLinearColor(0.25f, 0.18f, 0.06f);       // Thick methane fog
		P.FogDensity = 0.01f;                                   // Very thick
		P.GroundColor = FLinearColor(0.3f, 0.2f, 0.08f);      // Dark organic
		P.GravityMultiplier = 0.14f;
		P.DayCycleSpeed = 0.0625f;                              // 16 Earth days
		Profiles.Add(P);
	}

	// ── CERES ───────────────────────────────────────────────
	{
		FTartariaBaseVisualProfile P;
		P.BodyName = TEXT("ceres");
		P.SkyColor = FLinearColor(0.02f, 0.02f, 0.04f);       // Near-void
		P.AmbientColor = FLinearColor(0.06f, 0.06f, 0.08f);
		P.AmbientIntensity = 0.2f;
		P.SunColor = FLinearColor(0.9f, 0.9f, 0.95f);
		P.SunIntensity = 1.5f;
		P.FogColor = FLinearColor(0.05f, 0.05f, 0.07f);
		P.FogDensity = 0.0003f;
		P.GroundColor = FLinearColor(0.18f, 0.18f, 0.2f);     // Dark rocky
		P.GravityMultiplier = 0.029f;
		P.DayCycleSpeed = 2.67f;                                // 9h day
		Profiles.Add(P);
	}

	// ── VENUS ───────────────────────────────────────────────
	{
		FTartariaBaseVisualProfile P;
		P.BodyName = TEXT("venus");
		P.SkyColor = FLinearColor(0.35f, 0.25f, 0.1f);        // Sulfuric amber
		P.AmbientColor = FLinearColor(0.25f, 0.18f, 0.08f);
		P.AmbientIntensity = 0.6f;                              // Scattered light
		P.SunColor = FLinearColor(0.8f, 0.6f, 0.3f);          // Filtered through clouds
		P.SunIntensity = 1.5f;
		P.FogColor = FLinearColor(0.5f, 0.35f, 0.15f);        // Dense sulfuric
		P.FogDensity = 0.02f;                                   // Extremely thick
		P.GroundColor = FLinearColor(0.4f, 0.3f, 0.15f);      // Basalt with sulfur
		P.GravityMultiplier = 0.9f;
		P.DayCycleSpeed = 0.0042f;                              // 243 Earth days (retrograde)
		Profiles.Add(P);
	}

	// ── MERCURY ─────────────────────────────────────────────
	{
		FTartariaBaseVisualProfile P;
		P.BodyName = TEXT("mercury");
		P.SkyColor = FLinearColor(0.01f, 0.01f, 0.02f);       // Void
		P.AmbientColor = FLinearColor(0.04f, 0.04f, 0.05f);
		P.AmbientIntensity = 0.1f;
		P.SunColor = FLinearColor(1.f, 0.95f, 0.85f);         // Blazing close sun
		P.SunIntensity = 8.0f;                                  // Extremely bright
		P.FogColor = FLinearColor(0.02f, 0.02f, 0.02f);
		P.FogDensity = 0.0001f;                                 // No atmosphere
		P.GroundColor = FLinearColor(0.3f, 0.28f, 0.25f);     // Gray-brown regolith
		P.GravityMultiplier = 0.378f;
		P.DayCycleSpeed = 0.017f;                               // 58.6 Earth days
		Profiles.Add(P);
	}

	// ── DEEP_SPACE ──────────────────────────────────────────
	{
		FTartariaBaseVisualProfile P;
		P.BodyName = TEXT("deep_space");
		P.SkyColor = FLinearColor(0.005f, 0.005f, 0.01f);     // Nearly black
		P.AmbientColor = FLinearColor(0.02f, 0.02f, 0.04f);
		P.AmbientIntensity = 0.05f;                             // Minimal
		P.SunColor = FLinearColor(0.5f, 0.5f, 0.6f);          // Distant starlight
		P.SunIntensity = 0.3f;
		P.FogColor = FLinearColor(0.01f, 0.01f, 0.02f);
		P.FogDensity = 0.f;                                     // No fog
		P.GroundColor = FLinearColor(0.05f, 0.05f, 0.07f);    // Void platform
		P.GravityMultiplier = 0.0f;                              // Microgravity
		P.DayCycleSpeed = 0.0f;                                  // No day/night
		Profiles.Add(P);
	}

	// ── TARTARIA_PRIME ──────────────────────────────────────
	{
		FTartariaBaseVisualProfile P;
		P.BodyName = TEXT("tartaria_prime");
		P.SkyColor = FLinearColor(0.12f, 0.08f, 0.18f);       // Deep royal purple
		P.AmbientColor = FLinearColor(0.18f, 0.12f, 0.25f);
		P.AmbientIntensity = 0.7f;
		P.SunColor = FLinearColor(0.83f, 0.66f, 0.22f);       // Golden sun
		P.SunIntensity = 4.0f;
		P.FogColor = FLinearColor(0.2f, 0.15f, 0.28f);        // Purple mist
		P.FogDensity = 0.0015f;
		P.GroundColor = FLinearColor(0.22f, 0.15f, 0.08f);    // Ancient terra
		P.GravityMultiplier = 0.85f;                             // Slightly lower
		P.DayCycleSpeed = 1.5f;                                  // Faster cycle
		Profiles.Add(P);
	}

	UE_LOG(LogTemp, Log, TEXT("TartariaBaseProfile: Initialized %d body profiles"), Profiles.Num());
}

FTartariaBaseVisualProfile UTartariaBaseProfile::GetProfile(const FString& BodyName)
{
	InitProfiles();

	for (const FTartariaBaseVisualProfile& P : Profiles)
	{
		if (P.BodyName.Equals(BodyName, ESearchCase::IgnoreCase))
		{
			return P;
		}
	}

	// Fallback to Earth
	if (Profiles.Num() > 0) return Profiles[0];
	return FTartariaBaseVisualProfile();
}

TArray<FTartariaBaseVisualProfile> UTartariaBaseProfile::GetAllProfiles()
{
	InitProfiles();
	return Profiles;
}
