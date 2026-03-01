#include "TartariaWeatherManager.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"

ATartariaWeatherManager::ATartariaWeatherManager()
{
	PrimaryActorTick.bCanEverTick = true;

	// Root
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Fog
	FogComponent = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("WeatherFog"));
	FogComponent->SetupAttachment(Root);
	FogComponent->SetFogDensity(0.005f);
	FogComponent->SetFogHeightFalloff(0.2f);
	FogComponent->SetFogInscatteringColor(FLinearColor(0.15f, 0.12f, 0.18f));
	FogComponent->SetVolumetricFog(true);

	// Lightning flash light (hidden by default)
	LightningLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("LightningLight"));
	LightningLight->SetupAttachment(Root);
	LightningLight->SetRelativeLocation(FVector(0.f, 0.f, 5000.f));
	LightningLight->SetIntensity(0.f);
	LightningLight->SetAttenuationRadius(50000.f);
	LightningLight->SetLightColor(FLinearColor(0.9f, 0.85f, 1.0f));
	LightningLight->SetCastShadows(false);
}

void ATartariaWeatherManager::SetWeather(ETartariaWeather NewWeather)
{
	if (NewWeather == TargetWeather) return;
	TargetWeather = NewWeather;
	WeatherTransition = 0.f;
	UE_LOG(LogTemp, Log, TEXT("TartariaWeather: Transitioning to %d"), static_cast<int32>(NewWeather));
}

void ATartariaWeatherManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// -- Smooth weather transition --
	if (WeatherTransition < 1.f)
	{
		WeatherTransition = FMath::Clamp(WeatherTransition + DeltaTime * 0.2f, 0.f, 1.f);
		if (WeatherTransition >= 1.f)
		{
			CurrentWeather = TargetWeather;
		}
	}

	ApplyWeatherState(WeatherTransition);

	// -- Lightning flashes during rain --
	if (CurrentWeather == ETartariaWeather::Rain && LightningLight)
	{
		LightningTimer += DeltaTime;
		if (LightningTimer >= NextLightningTime)
		{
			LightningTimer = 0.f;
			NextLightningTime = FMath::RandRange(5.f, 15.f);
			LightningLight->SetIntensity(200000.f);
		}
		// Fade lightning
		float CurrentIntensity = LightningLight->Intensity;
		if (CurrentIntensity > 0.f)
		{
			LightningLight->SetIntensity(FMath::FInterpTo(CurrentIntensity, 0.f, DeltaTime, 10.f));
		}
	}
	else if (LightningLight)
	{
		LightningLight->SetIntensity(0.f);
	}

	// Clamp post-process after all weather modifications
	ClampPostProcessValues();
}

void ATartariaWeatherManager::ApplyWeatherState(float Alpha)
{
	if (!FogComponent) return;

	// Target values per weather type
	float TargetDensity = 0.005f;
	FLinearColor TargetFogColor(0.15f, 0.12f, 0.18f);
	float TargetFalloff = 0.2f;

	switch (TargetWeather)
	{
	case ETartariaWeather::Clear:
		TargetDensity = 0.003f;
		TargetFogColor = FLinearColor(0.18f, 0.15f, 0.22f);
		TargetFalloff = 0.3f;
		break;

	case ETartariaWeather::Overcast:
		TargetDensity = 0.015f;
		TargetFogColor = FLinearColor(0.12f, 0.12f, 0.15f);
		TargetFalloff = 0.15f;
		break;

	case ETartariaWeather::Rain:
		TargetDensity = 0.035f;
		TargetFogColor = FLinearColor(0.08f, 0.10f, 0.15f);
		TargetFalloff = 0.1f;
		break;

	case ETartariaWeather::DustStorm:
		TargetDensity = 0.05f;
		TargetFogColor = FLinearColor(0.25f, 0.18f, 0.10f);
		TargetFalloff = 0.08f;
		break;

	case ETartariaWeather::AetherMist:
		TargetDensity = 0.025f;
		TargetFogColor = FLinearColor(0.10f, 0.05f, 0.20f);
		TargetFalloff = 0.12f;
		break;
	}

	// Interpolate current to target
	float CurrentDensity = FogComponent->FogDensity;
	float NewDensity = FMath::FInterpTo(CurrentDensity, TargetDensity, Alpha, 2.f);
	FogComponent->SetFogDensity(NewDensity);
	FogComponent->SetFogHeightFalloff(FMath::FInterpTo(FogComponent->FogHeightFalloff, TargetFalloff, Alpha, 2.f));

	FLinearColor CurrentColor = FogComponent->FogInscatteringLuminance;
	FLinearColor NewColor = FLinearColor::LerpUsingHSV(CurrentColor, TargetFogColor, Alpha * 0.1f);
	FogComponent->SetFogInscatteringColor(NewColor);
}

void ATartariaWeatherManager::ClampPostProcessValues()
{
	if (!FogComponent) return;

	// Clamp fog density to prevent blackout (too dense) or whiteout (too thin)
	float Density = FogComponent->FogDensity;
	float ClampedDensity = FMath::Clamp(Density, MinFogDensity, MaxFogDensity);
	if (!FMath::IsNearlyEqual(Density, ClampedDensity))
	{
		FogComponent->SetFogDensity(ClampedDensity);
	}

	// Clamp fog height falloff -- too low = fog everywhere, too high = no fog
	float Falloff = FogComponent->FogHeightFalloff;
	float ClampedFalloff = FMath::Clamp(Falloff, 0.02f, 1.0f);
	if (!FMath::IsNearlyEqual(Falloff, ClampedFalloff))
	{
		FogComponent->SetFogHeightFalloff(ClampedFalloff);
	}
}
