#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HJProjectsWidget.generated.h"

class UOverlay;
class UVerticalBox;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UButton;
class UEditableTextBox;
class UBorder;
class USpacer;

USTRUCT(BlueprintType)
struct FHJProject
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Description;
	UPROPERTY(BlueprintReadOnly) FString Status;
	UPROPERTY(BlueprintReadOnly) FString CreatedAt;
};

/**
 * UHJProjectsWidget
 * Displays the project grid and handles create/run/delete.
 * NativeOnInitialized builds a default UMG layout programmatically.
 * Blueprint child (WBP_Projects) can still override the visual grid.
 */
UCLASS(Abstract)
class HERITAGEJARVIS_API UHJProjectsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeOnInitialized() override;

	// -------------------------------------------------------
	// Data
	// -------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Projects")
	TArray<FHJProject> Projects;

	// -------------------------------------------------------
	// Actions — bind to UMG buttons in Blueprint
	// -------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "HJ|Projects")
	void RefreshProjects();

	UFUNCTION(BlueprintCallable, Category = "HJ|Projects")
	void CreateProject(const FString& Name, const FString& InputPrimaMateria, bool bCheckpointMode);

	UFUNCTION(BlueprintCallable, Category = "HJ|Projects")
	void RunPipeline(const FString& ProjectId);

	UFUNCTION(BlueprintCallable, Category = "HJ|Projects")
	void StopPipeline(const FString& ProjectId);

	UFUNCTION(BlueprintCallable, Category = "HJ|Projects")
	void DeleteProject(const FString& ProjectId);

	UFUNCTION(BlueprintCallable, Category = "HJ|Projects")
	void SelectProject(const FString& ProjectId, const FString& ProjectName);

	// -------------------------------------------------------
	// Blueprint events — override visually in WBP_Projects
	// -------------------------------------------------------

	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Projects")
	void OnProjectsLoaded(const TArray<FHJProject>& LoadedProjects);

	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Projects")
	void OnPipelineStarted(const FString& ProjectId);

	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Projects")
	void OnError(const FString& Message);

private:
	void ParseProjects(const FString& Json);
	void BuildProgrammaticLayout();

	UFUNCTION()
	void OnCreateBtnClicked();

	// --- Programmatic widget references (GC-safe) ---

	UPROPERTY()
	UOverlay* RootOverlay = nullptr;

	UPROPERTY()
	UVerticalBox* MainVBox = nullptr;

	UPROPERTY()
	UScrollBox* ProjectsScrollBox = nullptr;

	UPROPERTY()
	UEditableTextBox* ProjectNameInput = nullptr;

	UPROPERTY()
	UEditableTextBox* PrimaInput = nullptr;

	UPROPERTY()
	UButton* CreateBtn = nullptr;

	UPROPERTY()
	UTextBlock* ErrorText = nullptr;
};
