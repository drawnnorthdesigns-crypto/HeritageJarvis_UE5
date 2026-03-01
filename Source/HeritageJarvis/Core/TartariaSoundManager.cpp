#include "TartariaSoundManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/Engine.h"

// Helper: attempt to load and play an engine sound with fallback
void UTartariaSoundManager::PlayEngineSound(UObject* WorldContextObject, float Volume, float Pitch)
{
	if (!WorldContextObject) return;

	// Try to load a basic engine sound (exists in all UE5 installs)
	static USoundBase* EngineBeep = LoadObject<USoundBase>(nullptr,
		TEXT("/Engine/VREditor/Sounds/UI/Object_PickUp"));

	if (EngineBeep)
	{
		UGameplayStatics::PlaySound2D(WorldContextObject, EngineBeep, Volume, Pitch);
	}
}

void UTartariaSoundManager::PlayUIClick(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.3f, 2.0f);  // High pitch, soft
}

void UTartariaSoundManager::PlaySuccess(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.5f, 1.5f);  // Rising feel
}

void UTartariaSoundManager::PlayError(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.4f, 0.5f);  // Low, buzzy
}

void UTartariaSoundManager::PlayHarvest(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.5f, 1.8f);  // Bright pickup
}

void UTartariaSoundManager::PlayCombatHit(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.7f, 0.6f);  // Heavy thud
}

void UTartariaSoundManager::PlayNPCGreeting(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.4f, 1.2f);  // Warm chime
}

void UTartariaSoundManager::PlayMenuToggle(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.3f, 1.6f);  // Quick click
}

void UTartariaSoundManager::PlayForgeSeal(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.6f, 1.0f);  // Full, resonant
}

void UTartariaSoundManager::PlayFootstep(UObject* WorldContextObject, bool bSprinting)
{
	float Pitch = bSprinting ? 1.3f : 1.0f;
	float Volume = bSprinting ? 0.35f : 0.25f;
	PlayEngineSound(WorldContextObject, Volume, Pitch);
}

void UTartariaSoundManager::PlayDodge(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.4f, 2.5f);  // Quick whoosh
}

void UTartariaSoundManager::PlayLevelUp(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.8f, 1.3f);  // Fanfare
}
