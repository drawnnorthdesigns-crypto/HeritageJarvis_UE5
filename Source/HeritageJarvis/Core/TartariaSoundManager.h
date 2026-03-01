#pragma once

#include "CoreMinimal.h"
#include "TartariaSoundManager.generated.h"

class USoundBase;

/**
 * UTartariaSoundManager -- Centralized interaction sound playback.
 * Provides static methods for common game sounds.
 * Uses engine tone sounds as placeholders (no imported assets required).
 * Called from Character, NPC, ForgeBuilding, etc.
 */
UCLASS()
class HERITAGEJARVIS_API UTartariaSoundManager : public UObject
{
	GENERATED_BODY()

public:
	/** Play a UI click sound (short blip). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayUIClick(UObject* WorldContextObject);

	/** Play a success chime (rising tone). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlaySuccess(UObject* WorldContextObject);

	/** Play an error buzz. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayError(UObject* WorldContextObject);

	/** Play a harvest/collect sound. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayHarvest(UObject* WorldContextObject);

	/** Play a combat impact sound. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayCombatHit(UObject* WorldContextObject);

	/** Play an NPC greeting chime. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayNPCGreeting(UObject* WorldContextObject);

	/** Play a menu open/close sound. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayMenuToggle(UObject* WorldContextObject);

	/** Play a forge seal / save chime. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayForgeSeal(UObject* WorldContextObject);

	/** Play a footstep sound. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayFootstep(UObject* WorldContextObject, bool bSprinting);

	/** Play a dodge roll whoosh. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayDodge(UObject* WorldContextObject);

	/** Play a level-up fanfare. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayLevelUp(UObject* WorldContextObject);

private:
	/** Play a built-in engine sound by path, with volume and pitch. */
	static void PlayEngineSound(UObject* WorldContextObject, float Volume = 1.0f, float Pitch = 1.0f);
};
