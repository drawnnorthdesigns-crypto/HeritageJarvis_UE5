#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaQuestMarker.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * ATartariaQuestMarker — Quest/event markers spawned by WorldSubsystem.
 * Floating mesh with light indicator and bob animation.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaQuestMarker : public AActor
{
	GENERATED_BODY()

public:
	ATartariaQuestMarker();

	virtual void Tick(float DeltaTime) override;

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

	// -------------------------------------------------------
	// Methods
	// -------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Tartaria|Quest")
	void ActivateQuest();

	UFUNCTION(BlueprintCallable, Category = "Tartaria|Quest")
	void CompleteQuest();

	// -------------------------------------------------------
	// Blueprint events
	// -------------------------------------------------------

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|Quest")
	void OnQuestActivated();

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|Quest")
	void OnQuestCompleted();

	// -------------------------------------------------------
	// Components
	// -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Quest")
	UStaticMeshComponent* MarkerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Quest")
	UPointLightComponent* MarkerLight;

	/** Bob animation amplitude in UE units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Quest")
	float BobAmplitude = 30.f;

	/** Bob animation speed (oscillations per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Quest")
	float BobSpeed = 1.5f;

private:
	float BobTimer = 0.f;
	FVector InitialLocation;
};
