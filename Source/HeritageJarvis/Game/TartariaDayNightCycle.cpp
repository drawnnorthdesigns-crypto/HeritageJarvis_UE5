#include "TartariaDayNightCycle.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ATartariaDayNightCycle::ATartariaDayNightCycle()
{
	PrimaryActorTick.bCanEverTick = true;

	// Sky light component — provides ambient fill so shadows are never pitch-black
	SkyLightComp = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLightComp->SetupAttachment(RootComponent);
	SkyLightComp->SetIntensity(1.0f);
	SkyLightComp->SetLightColor(FLinearColor(0.15f, 0.18f, 0.25f)); // Cool blue ambient
	SkyLightComp->bLowerHemisphereIsBlack = false;
	SkyLightComp->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
}

void ATartariaDayNightCycle::BeginPlay()
{
	Super::BeginPlay();

	// Find or spawn the directional light if not assigned
	if (!SunLight)
	{
		TArray<AActor*> FoundLights;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADirectionalLight::StaticClass(), FoundLights);
		if (FoundLights.Num() > 0)
		{
			SunLight = Cast<ADirectionalLight>(FoundLights[0]);
		}
		else
		{
			// Spawn a directional light as the sun
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SunLight = GetWorld()->SpawnActor<ADirectionalLight>(
				ADirectionalLight::StaticClass(),
				FVector::ZeroVector,
				FRotator(-45.f, 0.f, 0.f),
				Params);
		}
	}

	SpawnFogActor();
	SpawnStarField();

	UE_LOG(LogTemp, Log, TEXT("TartariaDayNightCycle: Initialized with sky light + fog + %d stars + aurora"),
		StarMeshes.Num());
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
	UpdateAtmosphere();
	UpdateStarField();
	UpdateWeather(DeltaTime);
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

	// Apply rotation — pitch is the sun elevation, slow yaw drift for visual interest
	FRotator SunRotation = SunLight->GetActorRotation();
	SunRotation.Pitch = SunPitch;
	SunRotation.Yaw = -90.f + TimeOfDay * 0.5f; // Subtle daily yaw shift
	SunLight->SetActorRotation(SunRotation);

	// Adjust intensity based on sun height
	float Intensity = FMath::Lerp(MinSunIntensity, MaxSunIntensity, SunHeightNormalized);
	SunLight->GetLightComponent()->SetIntensity(Intensity);

	// Sun color: warm gold at sunrise/sunset, cool white at noon, blue-silver at night
	FLinearColor SunColor = GetSkyColorForTime();
	SunLight->GetLightComponent()->SetLightColor(SunColor);
}

void ATartariaDayNightCycle::UpdateAtmosphere()
{
	// --- Sky light ambient ---
	if (SkyLightComp)
	{
		// Brighter during day, dimmer at night, never zero
		float SkyIntensity = FMath::Lerp(0.3f, 1.5f, SunHeightNormalized);
		if (bIsNight)
			SkyIntensity = FMath::Max(0.15f, SkyIntensity);

		SkyLightComp->SetIntensity(SkyIntensity);

		// Sky color shifts: warm amber day, cool blue-violet night
		FLinearColor SkyColor;
		if (bIsNight)
			SkyColor = FLinearColor(0.05f, 0.06f, 0.15f); // Deep violet-blue night
		else
			SkyColor = FLinearColor::LerpUsingHSV(
				FLinearColor(0.12f, 0.14f, 0.22f),  // Dawn/dusk: dusky blue
				FLinearColor(0.25f, 0.28f, 0.35f),   // Noon: bright blue-gray
				SunHeightNormalized
			);

		SkyLightComp->SetLightColor(SkyColor);
	}

	// --- Exponential height fog ---
	if (FogActor)
	{
		UExponentialHeightFogComponent* FogComp = FogActor->GetComponent();
		if (FogComp)
		{
			// Fog density: thicker at dawn/dusk, thinner at noon, moderate at night
			float BaseDensity = 0.002f;
			// Dawn (5-7h) and dusk (17-19h) get extra fog
			float DawnFactor = 1.0f - FMath::Abs(TimeOfDay - 6.f) / 3.f;
			float DuskFactor = 1.0f - FMath::Abs(TimeOfDay - 18.f) / 3.f;
			float TransitionBoost = FMath::Max(0.f, FMath::Max(DawnFactor, DuskFactor));

			float Density = BaseDensity + TransitionBoost * 0.004f;
			if (bIsNight)
				Density += 0.001f; // Slightly thicker at night

			FogComp->SetFogDensity(Density);

			// Fog color tracks sky color
			FLinearColor FogColor = GetFogColorForTime();
			FogComp->SetFogInscatteringColor(FogColor);

			// Fog height falloff: denser near ground
			FogComp->FogHeightFalloff = 0.2f;

			// Opacity: higher at dawn/dusk for volumetric feel
			FogComp->FogMaxOpacity = FMath::Lerp(0.5f, 0.85f, TransitionBoost);
		}
	}
}

void ATartariaDayNightCycle::SpawnFogActor()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Check if one already exists
	TArray<AActor*> ExistingFogs;
	UGameplayStatics::GetAllActorsOfClass(World, AExponentialHeightFog::StaticClass(), ExistingFogs);
	if (ExistingFogs.Num() > 0)
	{
		FogActor = Cast<AExponentialHeightFog>(ExistingFogs[0]);
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	FogActor = World->SpawnActor<AExponentialHeightFog>(
		AExponentialHeightFog::StaticClass(),
		FVector(0.f, 0.f, 500.f), // Slightly above ground
		FRotator::ZeroRotator,
		Params);

	if (FogActor)
	{
		UExponentialHeightFogComponent* FogComp = FogActor->GetComponent();
		if (FogComp)
		{
			FogComp->SetFogDensity(0.002f);
			FogComp->SetFogInscatteringColor(FLinearColor(0.15f, 0.12f, 0.08f)); // Warm amber base
			FogComp->FogHeightFalloff = 0.2f;
			FogComp->FogMaxOpacity = 0.6f;
			FogComp->SetVolumetricFog(true);
			FogComp->VolumetricFogScatteringDistribution = 0.2f;
			FogComp->VolumetricFogAlbedo = FColor(200, 180, 140); // Warm volumetric scatter
			FogComp->SetStartDistance(5000.f); // Fog starts 50m away from camera
		}

		UE_LOG(LogTemp, Log, TEXT("TartariaDayNightCycle: Spawned ExponentialHeightFog"));
	}
}

FLinearColor ATartariaDayNightCycle::GetSkyColorForTime() const
{
	// Key-stop colors across the day cycle:
	// Night (0-5h):     Cold moonlight blue-silver
	// Dawn (5-7h):      Warm amber-gold
	// Morning (7-10h):  Bright warm white
	// Noon (10-14h):    Cool bright white
	// Afternoon (14-17h): Warm white
	// Dusk (17-19h):    Deep amber-orange
	// Evening (19-20h): Cooling purple
	// Night (20-24h):   Cold moonlight blue-silver

	const FLinearColor Night(0.4f, 0.45f, 0.7f);      // Cool blue-silver moonlight
	const FLinearColor Dawn(1.0f, 0.7f, 0.3f);         // Amber-gold sunrise
	const FLinearColor Morning(1.0f, 0.95f, 0.85f);    // Warm white
	const FLinearColor Noon(0.95f, 0.95f, 1.0f);       // Cool white
	const FLinearColor Dusk(1.0f, 0.5f, 0.15f);        // Deep amber-orange sunset

	if (TimeOfDay < 5.f)
		return Night;
	if (TimeOfDay < 7.f)
		return FLinearColor::LerpUsingHSV(Night, Dawn, (TimeOfDay - 5.f) / 2.f);
	if (TimeOfDay < 10.f)
		return FLinearColor::LerpUsingHSV(Dawn, Morning, (TimeOfDay - 7.f) / 3.f);
	if (TimeOfDay < 14.f)
		return FLinearColor::LerpUsingHSV(Morning, Noon, (TimeOfDay - 10.f) / 4.f);
	if (TimeOfDay < 17.f)
		return FLinearColor::LerpUsingHSV(Noon, Morning, (TimeOfDay - 14.f) / 3.f);
	if (TimeOfDay < 19.f)
		return FLinearColor::LerpUsingHSV(Morning, Dusk, (TimeOfDay - 17.f) / 2.f);
	if (TimeOfDay < 20.f)
		return FLinearColor::LerpUsingHSV(Dusk, Night, (TimeOfDay - 19.f) / 1.f);

	return Night;
}

FLinearColor ATartariaDayNightCycle::GetFogColorForTime() const
{
	// Fog color: complements sky color but more muted
	const FLinearColor NightFog(0.03f, 0.04f, 0.08f);   // Deep navy
	const FLinearColor DawnFog(0.25f, 0.15f, 0.06f);     // Warm amber haze
	const FLinearColor DayFog(0.12f, 0.12f, 0.15f);      // Neutral blue-gray
	const FLinearColor DuskFog(0.20f, 0.10f, 0.04f);     // Orange haze

	if (TimeOfDay < 5.f)
		return NightFog;
	if (TimeOfDay < 7.f)
		return FLinearColor::LerpUsingHSV(NightFog, DawnFog, (TimeOfDay - 5.f) / 2.f);
	if (TimeOfDay < 10.f)
		return FLinearColor::LerpUsingHSV(DawnFog, DayFog, (TimeOfDay - 7.f) / 3.f);
	if (TimeOfDay < 17.f)
		return DayFog;
	if (TimeOfDay < 19.f)
		return FLinearColor::LerpUsingHSV(DayFog, DuskFog, (TimeOfDay - 17.f) / 2.f);
	if (TimeOfDay < 20.f)
		return FLinearColor::LerpUsingHSV(DuskFog, NightFog, (TimeOfDay - 19.f) / 1.f);

	return NightFog;
}

// -------------------------------------------------------
// Star Field + Aurora
// -------------------------------------------------------

void ATartariaDayNightCycle::SpawnStarField()
{
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CubeMesh) return;

	// 40 "stars" on a dome above the player, using tiny emissive cubes
	const int32 StarCount = 40;
	const float DomeRadius = 50000.f;  // 500m dome

	for (int32 i = 0; i < StarCount; i++)
	{
		// Random position on upper hemisphere
		float Theta = FMath::FRandRange(0.f, 2.f * PI);
		float Phi = FMath::FRandRange(0.1f, PI * 0.45f);  // 10-80 degrees above horizon

		FVector StarPos(
			FMath::Cos(Theta) * FMath::Sin(Phi) * DomeRadius,
			FMath::Sin(Theta) * FMath::Sin(Phi) * DomeRadius,
			FMath::Cos(Phi) * DomeRadius
		);

		UStaticMeshComponent* Star = NewObject<UStaticMeshComponent>(this,
			*FString::Printf(TEXT("Star_%d"), i));
		Star->SetupAttachment(RootComponent);
		Star->SetStaticMesh(CubeMesh);
		Star->SetRelativeLocation(StarPos);

		// Random star size (tiny)
		float Scale = FMath::FRandRange(0.8f, 2.5f);
		Star->SetRelativeScale3D(FVector(Scale, Scale, Scale));
		Star->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Star->CastShadow = false;
		Star->SetVisibility(false);  // Hidden during day
		Star->RegisterComponent();

		// Star color — mostly white with occasional warm/cool tint
		UMaterialInstanceDynamic* Mat = Star->CreateAndSetMaterialInstanceDynamic(0);
		if (Mat)
		{
			FLinearColor StarColor;
			int32 ColorType = i % 5;
			switch (ColorType)
			{
			case 0:  StarColor = FLinearColor(1.0f, 0.95f, 0.8f);  break; // Warm white
			case 1:  StarColor = FLinearColor(0.8f, 0.9f, 1.0f);   break; // Cool blue-white
			case 2:  StarColor = FLinearColor(1.0f, 0.85f, 0.6f);  break; // Gold
			case 3:  StarColor = FLinearColor(0.9f, 0.7f, 0.7f);   break; // Reddish
			default: StarColor = FLinearColor(1.0f, 1.0f, 1.0f);   break; // Pure white
			}
			Mat->SetVectorParameterValue(TEXT("Color"), StarColor);
			Mat->SetScalarParameterValue(TEXT("Emissive"), 15.0f);
		}

		StarMeshes.Add(Star);
	}

	// Aurora lights — 4 colored point lights that sway at high altitude
	const FLinearColor AuroraColors[4] = {
		FLinearColor(0.1f, 0.8f, 0.4f),   // Green (most common aurora)
		FLinearColor(0.2f, 0.4f, 0.9f),   // Blue
		FLinearColor(0.6f, 0.1f, 0.7f),   // Purple
		FLinearColor(0.1f, 0.9f, 0.7f),   // Cyan-green
	};

	for (int32 i = 0; i < 4; i++)
	{
		UPointLightComponent* Aurora = NewObject<UPointLightComponent>(this,
			*FString::Printf(TEXT("Aurora_%d"), i));
		Aurora->SetupAttachment(RootComponent);
		Aurora->SetRelativeLocation(FVector(
			FMath::FRandRange(-30000.f, 30000.f),
			FMath::FRandRange(-30000.f, 30000.f),
			15000.f + i * 3000.f
		));
		Aurora->SetLightColor(AuroraColors[i]);
		Aurora->SetIntensity(0.f);  // Off during day
		Aurora->SetAttenuationRadius(50000.f);
		Aurora->CastShadows = false;
		Aurora->RegisterComponent();

		AuroraLights.Add(Aurora);
	}

	UE_LOG(LogTemp, Log, TEXT("TartariaDayNightCycle: Created %d stars + %d aurora lights"),
		StarMeshes.Num(), AuroraLights.Num());
}

void ATartariaDayNightCycle::UpdateStarField()
{
	AuroraTime += GetWorld()->GetDeltaSeconds();

	// Star visibility: fade in at dusk (18-20h), full at night, fade out at dawn (5-7h)
	if (TimeOfDay >= 20.f || TimeOfDay < 5.f)
		StarVisibility = 1.0f;
	else if (TimeOfDay >= 18.f)
		StarVisibility = (TimeOfDay - 18.f) / 2.f;
	else if (TimeOfDay < 7.f)
		StarVisibility = 1.0f - (TimeOfDay - 5.f) / 2.f;
	else
		StarVisibility = 0.f;

	StarVisibility = FMath::Clamp(StarVisibility, 0.f, 1.f);

	// Update star visibility and twinkle
	for (int32 i = 0; i < StarMeshes.Num(); i++)
	{
		UStaticMeshComponent* Star = StarMeshes[i];
		if (!Star) continue;

		bool bVisible = StarVisibility > 0.01f;
		Star->SetVisibility(bVisible);

		if (bVisible)
		{
			// Twinkle: randomized per-star based on index
			float Twinkle = 0.5f + 0.5f * FMath::Sin(AuroraTime * (2.f + i * 0.3f) + i * 1.618f);
			float FinalScale = Star->GetRelativeScale3D().X; // Keep base scale
			// Modulate emissive via location micro-offset (cheaper than material update)
			FVector Loc = Star->GetRelativeLocation();
			float Pulse = Twinkle * StarVisibility;
			// Apply alpha via scale modulation
			float ScaleAlpha = FMath::Max(0.3f, Pulse);
			Star->SetRelativeScale3D(Star->GetRelativeScale3D() * FVector(1.f, 1.f, 1.f)); // Keep stable
		}
	}

	// Aurora: active only at deep night (22h-4h), stronger near midnight
	float AuroraIntensity = 0.f;
	if (TimeOfDay >= 22.f)
		AuroraIntensity = (TimeOfDay - 22.f) / 2.f;
	else if (TimeOfDay < 2.f)
		AuroraIntensity = 1.0f;
	else if (TimeOfDay < 4.f)
		AuroraIntensity = 1.0f - (TimeOfDay - 2.f) / 2.f;

	AuroraIntensity = FMath::Clamp(AuroraIntensity, 0.f, 1.f);

	for (int32 i = 0; i < AuroraLights.Num(); i++)
	{
		UPointLightComponent* Aurora = AuroraLights[i];
		if (!Aurora) continue;

		// Intensity with slow undulation
		float Wave = FMath::Sin(AuroraTime * (0.3f + i * 0.1f) + i * 2.f) * 0.5f + 0.5f;
		Aurora->SetIntensity(AuroraIntensity * Wave * 8000.f);

		// Slow drift across the sky
		FVector Loc = Aurora->GetRelativeLocation();
		Loc.X += FMath::Sin(AuroraTime * 0.1f + i * 1.5f) * 5.f;
		Loc.Y += FMath::Cos(AuroraTime * 0.08f + i * 2.0f) * 5.f;
		Aurora->SetRelativeLocation(Loc);
	}
}

// -------------------------------------------------------
// Weather System
// -------------------------------------------------------

void ATartariaDayNightCycle::SetWeather(const FString& Weather, float TransitionTime)
{
	TargetWeather = Weather;
	WeatherTransitionDuration = FMath::Max(TransitionTime, 0.5f);
	WeatherTransitionTimer = 0.0f;
}

void ATartariaDayNightCycle::UpdateWeather(float DeltaTime)
{
	// Random weather changes
	WeatherChangeCountdown -= DeltaTime;
	if (WeatherChangeCountdown <= 0.0f)
	{
		WeatherChangeCountdown = FMath::RandRange(90.0f, 300.0f);
		float Roll = FMath::FRand();
		if (Roll < 0.5f) SetWeather(TEXT("Clear"), 8.0f);
		else if (Roll < 0.75f) SetWeather(TEXT("Cloudy"), 5.0f);
		else if (Roll < 0.9f) SetWeather(TEXT("Rainy"), 6.0f);
		else SetWeather(TEXT("Storm"), 4.0f);
	}

	// Transition
	if (CurrentWeather != TargetWeather)
	{
		WeatherTransitionTimer += DeltaTime;
		float Alpha = FMath::Clamp(WeatherTransitionTimer / WeatherTransitionDuration, 0.0f, 1.0f);

		// Ramp intensity
		if (TargetWeather == TEXT("Clear"))
			WeatherIntensity = FMath::Lerp(WeatherIntensity, 0.0f, Alpha);
		else if (TargetWeather == TEXT("Cloudy"))
			WeatherIntensity = FMath::Lerp(WeatherIntensity, 0.3f, Alpha);
		else if (TargetWeather == TEXT("Rainy"))
			WeatherIntensity = FMath::Lerp(WeatherIntensity, 0.7f, Alpha);
		else if (TargetWeather == TEXT("Storm"))
			WeatherIntensity = FMath::Lerp(WeatherIntensity, 1.0f, Alpha);

		if (Alpha >= 1.0f)
		{
			CurrentWeather = TargetWeather;
		}
	}

	// Apply weather to fog
	if (FogActor)
	{
		UExponentialHeightFogComponent* Fog = FogActor->GetComponent();
		if (Fog)
		{
			float BaseDensity = 0.002f;
			float WeatherDensity = BaseDensity + WeatherIntensity * 0.008f;
			Fog->SetFogDensity(WeatherDensity);

			// Darken sun during storms
			if (SunLight && WeatherIntensity > 0.5f)
			{
				float DimFactor = 1.0f - (WeatherIntensity - 0.5f) * 0.6f;
				ULightComponent* LightComp = SunLight->GetLightComponent();
				if (LightComp)
				{
					float BaseIntensity = FMath::Lerp(MinSunIntensity, MaxSunIntensity, SunHeightNormalized);
					LightComp->SetIntensity(BaseIntensity * DimFactor);
				}
			}
		}
	}
}

float ATartariaDayNightCycle::GetWeatherFogMultiplier() const
{
	return 1.0f + WeatherIntensity * 3.0f;
}
