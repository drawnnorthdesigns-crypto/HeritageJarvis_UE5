#include "TartariaDayNightCycle.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"

ATartariaDayNightCycle::ATartariaDayNightCycle()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ATartariaDayNightCycle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Advance time of day
	float HoursPerSecond = 24.f / DayLengthSeconds;
	TimeOfDay += HoursPerSecond * DeltaTime;

	// Wrap at 24
	if (TimeOfDay >= 24.f)
		TimeOfDay -= 24.f;

	// Night check (20:00 - 05:00)
	bIsNight = (TimeOfDay >= 20.f || TimeOfDay < 5.f);

	UpdateSunPosition();
}

void ATartariaDayNightCycle::UpdateSunPosition()
{
	if (!SunLight) return;

	// Convert time of day to sun angle
	// 6h = sunrise (0 pitch), 12h = noon (90 pitch), 18h = sunset (0), 0h = midnight (-90)
	float TimeRadians = (TimeOfDay - 6.f) / 24.f * 2.f * PI;
	float SunPitch = FMath::Sin(TimeRadians) * 90.f;

	// Clamp below horizon
	SunHeightNormalized = FMath::Max(0.f, FMath::Sin(TimeRadians));

	// Apply rotation — pitch is the sun elevation
	FRotator SunRotation = SunLight->GetActorRotation();
	SunRotation.Pitch = SunPitch;
	SunLight->SetActorRotation(SunRotation);

	// Adjust intensity based on sun height
	float Intensity = FMath::Lerp(MinSunIntensity, MaxSunIntensity, SunHeightNormalized);
	SunLight->GetLightComponent()->SetIntensity(Intensity);
}
