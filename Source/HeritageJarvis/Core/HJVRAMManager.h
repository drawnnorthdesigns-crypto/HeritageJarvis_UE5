#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "HJVRAMManager.generated.h"

/**
 * UHJVRAMManager — Manages UE5 rendering quality during heavy LLM/GPU workloads.
 *
 * When the Flask backend signals that LLM inference or CadQuery generation is
 * happening, this manager drops UE5 rendering quality settings to free GPU
 * VRAM and avoid GPU OOM. When the heavy workload completes, settings restore.
 *
 * Controlled settings:
 *   - Global illumination quality (sg.GlobalIlluminationQuality)
 *   - Volumetric fog (r.VolumetricFog)
 *   - Shadow quality (sg.ShadowQuality)
 *   - Post process quality (sg.PostProcessQuality)
 *   - Texture streaming pool size (r.Streaming.PoolSize)
 *
 * Usage:
 *   UHJVRAMManager::Get()->EnterLowPowerMode();  // Called when LLM starts
 *   UHJVRAMManager::Get()->ExitLowPowerMode();   // Called when LLM finishes
 *
 * Integration: HJEventPoller checks pipeline_status, calls Enter/Exit on
 * generation phase transitions.
 */
UCLASS()
class HERITAGEJARVIS_API UHJVRAMManager : public UObject
{
	GENERATED_BODY()

public:
	/** Get the singleton instance. */
	static UHJVRAMManager* Get();

	/**
	 * Drop rendering quality to free GPU VRAM for LLM inference.
	 * Safe to call multiple times (no-op if already in low-power).
	 */
	UFUNCTION(BlueprintCallable, Category = "HeritageJarvis|Performance")
	void EnterLowPowerMode();

	/**
	 * Restore rendering quality to saved settings.
	 * Safe to call multiple times (no-op if not in low-power).
	 */
	UFUNCTION(BlueprintCallable, Category = "HeritageJarvis|Performance")
	void ExitLowPowerMode();

	/** Whether currently in low-power mode. */
	UFUNCTION(BlueprintCallable, Category = "HeritageJarvis|Performance")
	bool IsLowPowerMode() const { return bLowPowerActive; }

private:
	static UHJVRAMManager* Instance;

	bool bLowPowerActive = false;

	/** Saved original console variable values. */
	int32 SavedGIQuality = 3;
	int32 SavedShadowQuality = 3;
	int32 SavedPostProcessQuality = 3;
	int32 SavedVolumetricFog = 1;
	int32 SavedStreamingPoolSize = 1024;

	/** Save current quality settings. */
	void SaveCurrentSettings();

	/** Apply low-power console variable overrides. */
	void ApplyLowPowerSettings();

	/** Restore saved console variable values. */
	void RestoreSavedSettings();

	/** Helper: get/set console variable by name. */
	static int32 GetCVarInt(const TCHAR* Name);
	static void SetCVarInt(const TCHAR* Name, int32 Value);
};
