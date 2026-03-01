#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HJInteractable.h"
#include "TartariaTypes.h"
#include "TartariaTerminal.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;
class UPointLightComponent;
class UHJDashboardWidget;

/**
 * ATartariaTerminal — Diegetic in-world terminal that displays the CEF dashboard
 * on a physical screen mesh using UWidgetComponent.
 *
 * Different terminal types show different dashboard tabs:
 *   Forge terminal    → /game#forge
 *   Command terminal  → /game#governance
 *   Library terminal  → /game#scriptorium
 *   etc.
 *
 * The widget renders directly on the 3D mesh, creating an immersive
 * "brass-and-glass" terminal feel rather than a flat 2D overlay.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaTerminal : public AActor, public IHJInteractable
{
	GENERATED_BODY()

public:
	ATartariaTerminal();

	// -------------------------------------------------------
	// IHJInteractable
	// -------------------------------------------------------

	virtual void OnInteract_Implementation(APlayerController* Interactor) override;
	virtual FString GetInteractPrompt_Implementation() const override;

	// -------------------------------------------------------
	// Properties
	// -------------------------------------------------------

	/** Display name shown in interact prompt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Terminal")
	FString TerminalName = TEXT("Terminal");

	/** Which building type this terminal serves (determines the dashboard route). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Terminal")
	ETartariaBuildingType TerminalType = ETartariaBuildingType::Forge;

	/** Resolution of the widget component render target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Terminal")
	FVector2D ScreenResolution = FVector2D(1280.f, 720.f);

	/** Whether this terminal is currently active (screen on). */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Terminal")
	bool bScreenActive = false;

	// -------------------------------------------------------
	// Components
	// -------------------------------------------------------

	/** Physical terminal frame (tall slab). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Terminal")
	UStaticMeshComponent* FrameMesh;

	/** The screen mesh (flat plane facing the player). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Terminal")
	UStaticMeshComponent* ScreenMesh;

	/** Widget component that renders the CEF dashboard in 3D space. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Terminal")
	UWidgetComponent* ScreenWidget;

	/** Ambient glow light around the terminal screen. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Terminal")
	UPointLightComponent* ScreenGlow;

	// -------------------------------------------------------
	// Methods
	// -------------------------------------------------------

	/** Activate the terminal screen — creates and shows dashboard widget. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Terminal")
	void ActivateScreen(APlayerController* PC);

	/** Deactivate — hides the widget, returns to game input. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Terminal")
	void DeactivateScreen(APlayerController* PC);

	/** Get the Flask dashboard route for this terminal type. */
	FString GetDashboardRoute() const;

	// -------------------------------------------------------
	// Blueprint events
	// -------------------------------------------------------

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|Terminal")
	void OnTerminalActivated(APlayerController* Interactor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|Terminal")
	void OnTerminalDeactivated();

private:
	/** The dashboard widget created for this terminal's screen. */
	UPROPERTY()
	UHJDashboardWidget* DashWidget = nullptr;
};
