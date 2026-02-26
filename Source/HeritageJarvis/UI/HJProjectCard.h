#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HJProjectCard.generated.h"

class UButton;
class UTextBlock;
class UBorder;
class UVerticalBox;
class UHorizontalBox;
class UHJProjectsWidget;

/**
 * UHJProjectCard — A self-contained project card widget.
 * Stores the ProjectId so Run/Stop/Delete buttons can pass it
 * to UHJProjectsWidget methods (solving the FOnButtonClickedEvent
 * zero-parameter limitation).
 */
UCLASS()
class HERITAGEJARVIS_API UHJProjectCard : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "HJ|ProjectCard")
	FString ProjectId;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|ProjectCard")
	FString ProjectName;

	/** Initialize the card with project data and build its layout. */
	void Init(const FString& Id, const FString& Name, const FString& Desc,
	          const FString& Status, UHJProjectsWidget* Owner);

	UFUNCTION()
	void OnRunClicked();

	UFUNCTION()
	void OnStopClicked();

	UFUNCTION()
	void OnDeleteClicked();

private:
	UPROPERTY()
	UButton* RunBtn = nullptr;

	UPROPERTY()
	UButton* StopBtn = nullptr;

	UPROPERTY()
	UButton* DelBtn = nullptr;

	UPROPERTY()
	UTextBlock* ConfirmText = nullptr;

	TWeakObjectPtr<UHJProjectsWidget> OwnerWidget;

	bool bDeleteConfirmPending = false;
};
