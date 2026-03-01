#include "HJVRAMManager.h"
#include "HAL/IConsoleManager.h"

UHJVRAMManager* UHJVRAMManager::Instance = nullptr;

UHJVRAMManager* UHJVRAMManager::Get()
{
	if (!Instance)
	{
		Instance = NewObject<UHJVRAMManager>();
		Instance->AddToRoot(); // Prevent GC
		UE_LOG(LogTemp, Log, TEXT("HJVRAMManager: Initialized"));
	}
	return Instance;
}

void UHJVRAMManager::EnterLowPowerMode()
{
	if (bLowPowerActive) return;

	UE_LOG(LogTemp, Log, TEXT("HJVRAMManager: Entering low-power mode (LLM generation in progress)"));

	SaveCurrentSettings();
	ApplyLowPowerSettings();

	bLowPowerActive = true;
}

void UHJVRAMManager::ExitLowPowerMode()
{
	if (!bLowPowerActive) return;

	UE_LOG(LogTemp, Log, TEXT("HJVRAMManager: Restoring full rendering quality"));

	RestoreSavedSettings();

	bLowPowerActive = false;
}

void UHJVRAMManager::SaveCurrentSettings()
{
	SavedGIQuality = GetCVarInt(TEXT("sg.GlobalIlluminationQuality"));
	SavedShadowQuality = GetCVarInt(TEXT("sg.ShadowQuality"));
	SavedPostProcessQuality = GetCVarInt(TEXT("sg.PostProcessQuality"));
	SavedVolumetricFog = GetCVarInt(TEXT("r.VolumetricFog"));
	SavedStreamingPoolSize = GetCVarInt(TEXT("r.Streaming.PoolSize"));

	UE_LOG(LogTemp, Log,
		TEXT("HJVRAMManager: Saved settings — GI=%d, Shadow=%d, PP=%d, VFog=%d, Pool=%d"),
		SavedGIQuality, SavedShadowQuality, SavedPostProcessQuality,
		SavedVolumetricFog, SavedStreamingPoolSize);
}

void UHJVRAMManager::ApplyLowPowerSettings()
{
	/*
	 * Low-power overrides:
	 * - GI quality 1 (low) — saves significant VRAM from Lumen probes
	 * - Shadow quality 1 (low) — reduces shadow map VRAM
	 * - Post process quality 1 — reduces RT post-process cost
	 * - Volumetric fog OFF — removes 3D fog texture (~50-100MB VRAM)
	 * - Streaming pool reduced to 512MB — caps texture VRAM
	 */
	SetCVarInt(TEXT("sg.GlobalIlluminationQuality"), 1);
	SetCVarInt(TEXT("sg.ShadowQuality"), 1);
	SetCVarInt(TEXT("sg.PostProcessQuality"), 1);
	SetCVarInt(TEXT("r.VolumetricFog"), 0);
	SetCVarInt(TEXT("r.Streaming.PoolSize"), 512);

	UE_LOG(LogTemp, Log, TEXT("HJVRAMManager: Low-power settings applied"));
}

void UHJVRAMManager::RestoreSavedSettings()
{
	SetCVarInt(TEXT("sg.GlobalIlluminationQuality"), SavedGIQuality);
	SetCVarInt(TEXT("sg.ShadowQuality"), SavedShadowQuality);
	SetCVarInt(TEXT("sg.PostProcessQuality"), SavedPostProcessQuality);
	SetCVarInt(TEXT("r.VolumetricFog"), SavedVolumetricFog);
	SetCVarInt(TEXT("r.Streaming.PoolSize"), SavedStreamingPoolSize);

	UE_LOG(LogTemp, Log, TEXT("HJVRAMManager: Original settings restored"));
}

int32 UHJVRAMManager::GetCVarInt(const TCHAR* Name)
{
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name);
	if (CVar)
	{
		return CVar->GetInt();
	}
	return 0;
}

void UHJVRAMManager::SetCVarInt(const TCHAR* Name, int32 Value)
{
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name);
	if (CVar)
	{
		CVar->Set(Value, ECVF_SetByCode);
	}
}
