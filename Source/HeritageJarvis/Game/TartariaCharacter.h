#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TartariaCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;

/**
 * ATartariaCharacter — Player character for the Tartaria game world.
 * Uses legacy input system (configured via DefaultInput.ini — no assets needed).
 * WASD move, mouse look, Space jump, Shift sprint, E interact, Escape menu, F3 debug.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATartariaCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;

public:
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------
	// Camera
	// -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* Camera;

	// -------------------------------------------------------
	// Stats
	// -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float WalkSpeed = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float SprintSpeed = 800.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	bool bIsSprinting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	float InteractReach = 250.0f;

	// -------------------------------------------------------
	// Blueprint events
	// -------------------------------------------------------

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria")
	void OnInteract();

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria")
	void OnMenuToggled();

	/** Toggle CEF dashboard overlay (bound to Tab key). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria")
	void ToggleDashboard();

private:
	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);
	void StartSprint();
	void StopSprint();
	void Interact();
	void ToggleMenu();
	void ToggleDebug();
	void DoInteractTrace();
};
