#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HJInteractable.h"
#include "HJArtifactActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;

/**
 * AHJArtifactActor — A glowing, bobbing world object linked to a Heritage Jarvis project.
 * Place in TartariaWorld via the editor. Player presses IA_Interact nearby
 * to open the linked project's Library entry (fires OnArtifactInteracted in Blueprint).
 *
 * Blueprint child: BP_Artifact in Content/Blueprints/
 *
 * Visual: heritage gold glow, sinusoidal vertical bob, sphere overlap prompt.
 */
UCLASS()
class HERITAGEJARVIS_API AHJArtifactActor : public AActor, public IHJInteractable
{
    GENERATED_BODY()

public:
    AHJArtifactActor();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

public:

    // -------------------------------------------------------
    // Configuration — set in editor per-artifact
    // -------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|Artifact")
    FString LinkedProjectId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|Artifact")
    FString LinkedProjectName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|Artifact")
    FString ArtifactTitle = TEXT("Project Artifact");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|Artifact")
    FLinearColor GlowColor = FLinearColor(0.9f, 0.75f, 0.2f, 1.0f);  // Heritage gold

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|Artifact")
    float BobAmplitude = 20.0f;   // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|Artifact")
    float BobFrequency = 0.8f;    // Hz

    // -------------------------------------------------------
    // IHJInteractable
    // -------------------------------------------------------

    virtual void    OnInteract_Implementation(APlayerController* Interactor) override;
    virtual FString GetInteractPrompt_Implementation()                   const override;

    // -------------------------------------------------------
    // Blueprint event — open Library entry / HUD panel
    // -------------------------------------------------------

    UFUNCTION(BlueprintImplementableEvent, Category = "HJ|Artifact")
    void OnArtifactInteracted(APlayerController* Interactor,
                               const FString&     ProjectId,
                               const FString&     ProjectName);

    // -------------------------------------------------------
    // Components
    // -------------------------------------------------------

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* InteractSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UPointLightComponent* GlowLight;

private:
    FVector BaseLocation;
    float   BobTime = 0.0f;

    UFUNCTION()
    void OnBeginOverlap(UPrimitiveComponent* Overlapped, AActor* Other,
                        UPrimitiveComponent* OtherComp, int32 OtherBodyIdx,
                        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnEndOverlap(UPrimitiveComponent* Overlapped, AActor* Other,
                      UPrimitiveComponent* OtherComp, int32 OtherBodyIdx);
};
