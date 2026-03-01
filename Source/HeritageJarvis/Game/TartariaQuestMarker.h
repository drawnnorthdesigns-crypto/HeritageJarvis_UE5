#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HJInteractable.h"
#include "TartariaQuestMarker.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UCapsuleComponent;

/**
 * ATartariaQuestMarker — Quest/event markers spawned by WorldPopulator.
 * Floating mesh with light indicator and bob animation.
 * Implements IHJInteractable — on interact, advances quest via Flask backend.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaQuestMarker : public AActor, public IHJInteractable
{
	GENERATED_BODY()

public:
	ATartariaQuestMarker();

	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------
	// IHJInteractable
	// -------------------------------------------------------

	virtual void OnInteract_Implementation(APlayerController* Interactor) override;
	virtual FString GetInteractPrompt_Implementation() const override;

	// -------------------------------------------------------
	// Properties
	// -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Quest")
	FString QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Quest")
	FString QuestTitle;

	/** Quest type: threat, event, faction, exploration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Quest")
	FString QuestType = TEXT("exploration");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Quest")
	FString LinkedBiome;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Quest")
	bool bActive = true;

	/** Current quest step (0 = not started, increments on interact). */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Quest")
	int32 CurrentStep = 0;

	// -------------------------------------------------------
	// Methods
	// -------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Tartaria|Quest")
	void ActivateQuest();

	UFUNCTION(BlueprintCallable, Category = "Tartaria|Quest")
	void CompleteQuest();

	// -------------------------------------------------------
	// Delegates
	// -------------------------------------------------------

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestInteracted,
		const FString&, QuestId, int32, Step);

	UPROPERTY(BlueprintAssignable, Category = "Tartaria|Quest")
	FOnQuestInteracted OnQuestInteractedDelegate;

	// -------------------------------------------------------
	// Blueprint events
	// -------------------------------------------------------

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|Quest")
	void OnQuestActivated();

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|Quest")
	void OnQuestCompleted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|Quest")
	void OnQuestAdvanced(int32 NewStep);

	// -------------------------------------------------------
	// Components
	// -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Quest")
	UStaticMeshComponent* MarkerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Quest")
	UPointLightComponent* MarkerLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Quest")
	UCapsuleComponent* InteractCapsule;

	/** Bob animation amplitude in UE units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Quest")
	float BobAmplitude = 30.f;

	/** Bob animation speed (oscillations per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Quest")
	float BobSpeed = 1.5f;

private:
	void SendAdvanceRequest();
	void OnAdvanceResponse(bool bSuccess, const FString& Body);

	float BobTimer = 0.f;
	FVector InitialLocation;
};
