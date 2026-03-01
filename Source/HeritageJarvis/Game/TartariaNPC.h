#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/TextRenderComponent.h"
#include "HJInteractable.h"
#include "TartariaTypes.h"
#include "TartariaNPC.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * ATartariaNPC — Faction NPC in the Tartaria world.
 * Implements IHJInteractable for player dialogue.
 * OnInteract sends player message to Flask /api/game/npc/dialogue with NPC persona.
 * Supports streaming dialogue via SSE for typewriter effect.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaNPC : public ACharacter, public IHJInteractable
{
	GENERATED_BODY()

public:
	ATartariaNPC();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------
	// IHJInteractable
	// -------------------------------------------------------

	virtual void OnInteract_Implementation(APlayerController* Interactor) override;
	virtual FString GetInteractPrompt_Implementation() const override;

	// -------------------------------------------------------
	// Properties
	// -------------------------------------------------------

	/** Display name shown in HUD prompt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|NPC")
	FString NPCName = TEXT("The Steward");

	/** Faction this NPC belongs to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|NPC")
	FString FactionKey = TEXT("STEWARDS");

	/** Flask endpoint for dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|NPC")
	FString DialogueEndpoint = TEXT("/api/game/npc/dialogue");

	/** NPC specialist type — determines dialogue context and sim module routing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|NPC")
	ETartariaSpecialist SpecialistType = ETartariaSpecialist::Steward;

	/** System prompt persona for LLM dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|NPC", meta = (MultiLine = true))
	FString PersonaPrompt = TEXT("You are a wise steward of Tartaria. Speak with authority about engineering and construction.");

	// -------------------------------------------------------
	// Components — Compound Primitive Humanoid
	// -------------------------------------------------------

	/** Torso — box body (primary attachment point). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|NPC")
	UStaticMeshComponent* BodyTorso;

	/** Head — sphere on top of torso. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|NPC")
	UStaticMeshComponent* BodyHead;

	/** Left arm — cylinder. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|NPC")
	UStaticMeshComponent* BodyArmL;

	/** Right arm — cylinder. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|NPC")
	UStaticMeshComponent* BodyArmR;

	/** Left leg — cylinder. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|NPC")
	UStaticMeshComponent* BodyLegL;

	/** Right leg — cylinder. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|NPC")
	UStaticMeshComponent* BodyLegR;

	/** Type-specific accessory (staff, flask, tome, etc.). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|NPC")
	UStaticMeshComponent* Accessory;

	/** Floating name tag rendered above the NPC head. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|NPC")
	UTextRenderComponent* NameTag;

	// -------------------------------------------------------
	// Blueprint events
	// -------------------------------------------------------

	/** Fired when dialogue starts — pass JSON response to UI. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|NPC")
	void OnDialogueStarted(APlayerController* Interactor, const FString& DialogueJson);

	/** Fired when dialogue response received from backend. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|NPC")
	void OnDialogueResponse(const FString& Response);

	/** Fired on dialogue error. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|NPC")
	void OnDialogueError();

	// -------------------------------------------------------
	// C++ delegates (for GameMode to subscribe)
	// -------------------------------------------------------

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNPCDialogueStarted, const FString&, NPCName, const FString&, Faction);

	UPROPERTY(BlueprintAssignable, Category = "Tartaria|NPC")
	FOnNPCDialogueStarted OnDialogueStartedDelegate;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnNPCDialogueReceived, const FString&, NPCName, const FString&, ResponseText, const TArray<FString>&, Actions);

	UPROPERTY(BlueprintAssignable, Category = "Tartaria|NPC")
	FOnNPCDialogueReceived OnDialogueReceivedDelegate;

	/** Per-token streaming delegate for typewriter effect. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNPCDialogueToken, const FString&, NPCName, const FString&, Token);

	UPROPERTY(BlueprintAssignable, Category = "Tartaria|NPC")
	FOnNPCDialogueToken OnDialogueTokenDelegate;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCDialogueErrored, const FString&, NPCName);

	UPROPERTY(BlueprintAssignable, Category = "Tartaria|NPC")
	FOnNPCDialogueErrored OnDialogueErroredDelegate;

	/** Map specialist enum to Flask API string key. */
	FString GetSpecialistString() const;

	/** Apply specialist-specific colors and spawn accessory.
	 *  Called automatically in BeginPlay, but can also be called
	 *  externally (e.g. by WorldPopulator) after setting SpecialistType post-spawn. */
	void ApplySpecialistAppearance();

	/** Spawn environmental props around the NPC. */
	void SpawnWorkshopProps();

	/** Workshop prop actors (for cleanup). */
	TArray<TWeakObjectPtr<AActor>> WorkshopProps;

	/** Play a gesture animation for an NPC action (e.g., forging, reading, analyzing). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|NPC")
	void PlayActionGesture(const FString& ActionType);

	/** Is currently playing a gesture? */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|NPC")
	bool bPlayingGesture = false;

	// ── Mood System ─────────────────────────────────────────
	/** Current mood level: 0=neutral, 1=happy, 2=busy, -1=displeased */
	UPROPERTY(BlueprintReadWrite, Category = "Tartaria|NPC")
	int32 MoodLevel = 0;

	/** Set NPC mood (called after dialogue/action results). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|NPC")
	void SetMood(int32 NewMood);

	// ── Dialogue Eye Contact ────────────────────────────────
	/** True while NPC is engaged in dialogue — enables eye-contact head tracking. */
	UPROPERTY(BlueprintReadWrite, Category = "Tartaria|NPC")
	bool bInDialogue = false;

	/** Begin dialogue mode — called by OnInteract. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|NPC")
	void StartDialogue();

	/** End dialogue mode — lerps head back to forward-facing. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|NPC")
	void EndDialogue();

	// ── Activity State Reflection ────────────────────────────
	/** Current activity string received from Python backend (e.g. "researching", "idle"). */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|NPC")
	FString CurrentActivity = TEXT("idle");

	/** Derived activity state enum for animation selection. */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|NPC")
	ENPCActivityState ActivityState = ENPCActivityState::Idle;

	/** Timer accumulator for activity-based animations (wraps at 2*PI). */
	float ActivityAnimTimer = 0.f;

	/** Apply visual behavior (head tilt, sway, arm motion) based on CurrentActivity. */
	void UpdateActivityVisuals(float DeltaTime);

	/** Time of last activity fetch from backend. */
	float LastActivityFetchTime = -999.f;

	/** Fetch current_activity from the NPC profile endpoint. */
	void FetchCurrentActivity();

	// ── Reputation Aura ─────────────────────────────────────
	/** Reputation tier: 0=STRANGER, 1=ACQUAINTANCE, 2=ALLY, 3=CONFIDANT, 4=TRUSTED */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|NPC")
	int32 ReputationTier = 0;

	/** Update the reputation aura from a relationship_score (0-100). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|NPC")
	void SetReputationFromScore(int32 Score);

	/** Force-fetch reputation from the Python backend. */
	void FetchReputationTier();

private:
	void SendDialogueRequest(APlayerController* Interactor, const FString& PlayerMessage);

	/** Apply a solid color to a static mesh component via dynamic material instance. */
	static void ApplyColorToMesh(UStaticMeshComponent* Mesh, const FLinearColor& Color);

	/** Idle animation accumulators. */
	float BreathPhase = 0.f;
	float GestureTimer = 0.f;

	/** Action gesture state. */
	float ActionGestureTimer = 0.f;
	float ActionGestureDuration = 2.0f;
	FString CurrentGestureType;

	/** Mood indicator light above head (color changes with mood). */
	UPROPERTY()
	UPointLightComponent* MoodLight = nullptr;

	// ── Dialogue Eye Contact (private state) ────────────────
	/** Target head rotation during dialogue (toward player camera). */
	FRotator DialogueTargetHeadRotation = FRotator::ZeroRotator;

	/** Current interpolated head rotation for smooth eye contact. */
	FRotator DialogueCurrentHeadRotation = FRotator::ZeroRotator;

	// ── Reputation Aura (private state) ─────────────────────
	/** Point light component for the reputation aura glow. */
	UPROPERTY()
	UPointLightComponent* ReputationAura = nullptr;

	/** Base intensity for the current reputation tier (before pulse). */
	float ReputationBaseIntensity = 200.f;

	/** Aura pulse phase accumulator. */
	float AuraPulsePhase = 0.f;

	/** Time of last reputation fetch from backend. */
	float LastReputationFetchTime = -999.f;

	/** Idle variation timer. */
	float IdleVariationTimer = 0.f;

	/** Current idle variant index (0=default, 1-3=variants). */
	int32 CurrentIdleVariant = 0;

	/** Time until next idle switch. */
	float NextIdleSwitchTime = 15.f;

	/** Apply color and intensity to the reputation aura based on current tier. */
	void ApplyReputationAuraVisuals();

	/** Get tier-specific aura color. */
	static FLinearColor GetReputationTierColor(int32 Tier);

	/** Get tier-specific aura base intensity. */
	static float GetReputationTierIntensity(int32 Tier);

	/** Map an activity string (from Python) to the ENPCActivityState enum. */
	static ENPCActivityState MapActivityStringToState(const FString& Activity);
};
