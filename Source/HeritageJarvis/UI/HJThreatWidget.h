#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/TartariaTypes.h"
#include "HJThreatWidget.generated.h"

class UCanvasPanel;
class UBorder;
class UTextBlock;
class UButton;
class UHorizontalBox;
class USpacer;
class USizeBox;
class UProgressBar;

/**
 * UHJThreatWidget — Modal encounter overlay.
 * Shows when a threat is detected on zone entry.
 * Displays enemy info and Fight/Evade options.
 * Calls /api/game/threat/resolve on player action.
 */
UCLASS(Abstract)
class HERITAGEJARVIS_API UHJThreatWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	// -------------------------------------------------------
	// State
	// -------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Threat")
	FTartariaThreatInfo ActiveThreat;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Threat")
	bool bResolving = false;

	// -------------------------------------------------------
	// API
	// -------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "HJ|Threat")
	void ShowEncounter(const FTartariaThreatInfo& Threat);

	UFUNCTION(BlueprintCallable, Category = "HJ|Threat")
	void HideEncounter();

	// -------------------------------------------------------
	// Delegates
	// -------------------------------------------------------

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEncounterResolved,
		const FTartariaEncounterResult&, Result);

	UPROPERTY(BlueprintAssignable, Category = "HJ|Threat")
	FOnEncounterResolved OnEncounterResolved;

	// -------------------------------------------------------
	// Blueprint events
	// -------------------------------------------------------

	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Threat")
	void OnEncounterShown();

	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Threat")
	void OnEncounterHidden();

	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Threat")
	void OnResultShown(const FTartariaEncounterResult& Result);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** Fade animation state. */
	float FadeAlpha = 0.f;
	int32 FadeDir = 0;  // 0=idle, +1=fading in, -1=fading out

	void BuildProgrammaticLayout();

	UFUNCTION()
	void OnFightClicked();

	UFUNCTION()
	void OnEvadeClicked();

	void ResolveEncounter(const FString& Action);
	void OnResolveResponse(bool bSuccess, const FString& Body);

	// Widget refs
	UPROPERTY() UBorder* ModalBg = nullptr;
	UPROPERTY() UTextBlock* TitleText = nullptr;
	UPROPERTY() UTextBlock* EnemyNameText = nullptr;
	UPROPERTY() UTextBlock* EnemyPowerText = nullptr;
	UPROPERTY() UTextBlock* MessageText = nullptr;
	UPROPERTY() UTextBlock* ResultText = nullptr;
	UPROPERTY() UButton* FightButton = nullptr;
	UPROPERTY() UButton* EvadeButton = nullptr;
	UPROPERTY() UTextBlock* FightLabel = nullptr;
	UPROPERTY() UTextBlock* EvadeLabel = nullptr;
};
