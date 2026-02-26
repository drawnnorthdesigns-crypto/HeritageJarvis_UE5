#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HJLibraryWidget.generated.h"

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
struct FHJLibraryEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString Id;
    UPROPERTY(BlueprintReadOnly) FString Title;
    UPROPERTY(BlueprintReadOnly) FString ContentType;
    UPROPERTY(BlueprintReadOnly) FString Summary;
    UPROPERTY(BlueprintReadOnly) FString CreatedAt;
    UPROPERTY(BlueprintReadOnly) FString ProjectId;
};

/**
 * UHJLibraryWidget -- Library Tab
 * Displays artifacts / documents produced by completed pipelines.
 * NativeOnInitialized builds a default UMG layout programmatically.
 * Blueprint child (WBP_Library) can still override the visual.
 *
 * Flask endpoints used:
 *   GET /api/library                  -> full listing
 *   GET /api/library/search?q=<query> -> filtered listing
 */
UCLASS(Abstract)
class HERITAGEJARVIS_API UHJLibraryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeOnInitialized() override;

    // All loaded entries
    UPROPERTY(BlueprintReadOnly, Category = "HJ|Library")
    TArray<FHJLibraryEntry> Entries;

    // Current search query (empty = show all)
    UPROPERTY(BlueprintReadOnly, Category = "HJ|Library")
    FString SearchQuery;

    // -------------------------------------------------------
    // Actions -- call from Blueprint buttons
    // -------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "HJ|Library")
    void RefreshLibrary();

    UFUNCTION(BlueprintCallable, Category = "HJ|Library")
    void SearchLibrary(const FString& Query);

    UFUNCTION(BlueprintCallable, Category = "HJ|Library")
    void ClearSearch();

    // -------------------------------------------------------
    // Blueprint events -- implement visually in WBP_Library
    // -------------------------------------------------------

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|Library")
    void OnEntriesLoaded(const TArray<FHJLibraryEntry>& LoadedEntries);

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|Library")
    void OnSearchComplete(const TArray<FHJLibraryEntry>& Results, const FString& Query);

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|Library")
    void OnError(const FString& Message);

private:
    void ParseEntries(const FString& Json, bool bIsSearch);
    void BuildProgrammaticLayout();

    UFUNCTION()
    void OnSearchBtnClicked();

    UFUNCTION()
    void OnClearBtnClicked();

    // --- Programmatic widget references (GC-safe) ---

    UPROPERTY()
    UScrollBox* LibraryScrollBox = nullptr;

    UPROPERTY()
    UEditableTextBox* SearchInput = nullptr;

    UPROPERTY()
    UButton* SearchBtn = nullptr;

    UPROPERTY()
    UButton* ClearBtn = nullptr;

    UPROPERTY()
    UTextBlock* ErrorText = nullptr;

    UPROPERTY()
    UTextBlock* ResultCountText = nullptr;
};
