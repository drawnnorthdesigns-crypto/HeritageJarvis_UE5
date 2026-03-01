#include "TartariaAmbientZone.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"

ATartariaAmbientZone::ATartariaAmbientZone()
{
	PrimaryActorTick.bCanEverTick = true;

	AudioRoot = CreateDefaultSubobject<USceneComponent>(TEXT("AudioRoot"));
	RootComponent = AudioRoot;

	AmbientAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("AmbientAudio"));
	AmbientAudio->SetupAttachment(AudioRoot);
	AmbientAudio->bAutoActivate = false;
	AmbientAudio->bIsUISound = false;
	AmbientAudio->VolumeMultiplier = 0.f;  // Start silent, fade in
}

void ATartariaAmbientZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateSpatialVolume(DeltaTime);
}

void ATartariaAmbientZone::UpdateSpatialVolume(float DeltaTime)
{
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!Player) return;

	float Distance = FVector::Dist(GetActorLocation(), Player->GetActorLocation());

	// Smooth distance-based falloff
	float TargetVolume = 0.f;
	if (Distance < AudibleRadius)
	{
		// Linear falloff with inner radius at 30% for full volume
		float InnerRadius = AudibleRadius * 0.3f;
		if (Distance <= InnerRadius)
			TargetVolume = MasterVolume;
		else
			TargetVolume = MasterVolume * (1.0f - (Distance - InnerRadius) / (AudibleRadius - InnerRadius));
	}

	// Smooth interpolation for gradual volume changes
	CurrentVolume = FMath::FInterpTo(CurrentVolume, TargetVolume, DeltaTime, 2.0f);

	if (AmbientAudio)
	{
		AmbientAudio->SetVolumeMultiplier(FMath::Max(0.f, CurrentVolume));

		// Auto-activate/deactivate to save resources
		if (CurrentVolume > 0.01f && !AmbientAudio->IsPlaying())
		{
			AmbientAudio->Play();
		}
		else if (CurrentVolume <= 0.01f && AmbientAudio->IsPlaying())
		{
			AmbientAudio->Stop();
		}
	}
}
