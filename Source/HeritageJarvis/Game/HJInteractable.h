#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HJInteractable.generated.h"

class APlayerController;

UINTERFACE(Blueprintable)
class HERITAGEJARVIS_API UHJInteractable : public UInterface
{
    GENERATED_BODY()
};

/**
 * IHJInteractable — Implement on any actor that the player can interact with.
 * TartariaCharacter traces for this interface when IA_Interact fires.
 *
 * Minimal required implementation:
 *   - OnInteract_Implementation(APlayerController*)
 *   - GetInteractPrompt_Implementation() -> FString shown in HUD
 */
class HERITAGEJARVIS_API IHJInteractable
{
    GENERATED_BODY()

public:

    /** Called when the player presses the interact key while facing this actor */
    UFUNCTION(BlueprintNativeEvent, Category = "HJ|Interact")
    void OnInteract(APlayerController* Interactor);

    /** Short label shown in the HUD interact prompt, e.g. "View Project Artifact" */
    UFUNCTION(BlueprintNativeEvent, Category = "HJ|Interact")
    FString GetInteractPrompt() const;
};
