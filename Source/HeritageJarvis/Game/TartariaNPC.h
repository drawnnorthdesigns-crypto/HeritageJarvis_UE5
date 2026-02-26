#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HJInteractable.h"
#include "TartariaNPC.generated.h"

class UStaticMeshComponent;

/**
 * ATartariaNPC — Faction NPC in the Tartaria world.
 * Implements IHJInteractable for player dialogue.
 * OnInteract sends player message to Flask /api/game/npc/dialogue with NPC persona.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaNPC : public ACharacter, public IHJInteractable
{
	GENERATED_BODY()

public:
	ATartariaNPC();

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

	/** System prompt persona for LLM dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|NPC", meta = (MultiLine = true))
	FString PersonaPrompt = TEXT("You are a wise steward of Tartaria. Speak with authority about engineering and construction.");

	// -------------------------------------------------------
	// Components
	// -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|NPC")
	UStaticMeshComponent* BodyMesh;

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

private:
	void SendDialogueRequest(APlayerController* Interactor, const FString& PlayerMessage);
};
