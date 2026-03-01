#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/TartariaTypes.h"
#include "HJInventoryWidget.generated.h"

class UCanvasPanel;
class UBorder;
class UTextBlock;
class UButton;
class UVerticalBox;
class UHorizontalBox;
class UScrollBox;
class USpacer;
class USizeBox;

/**
 * UHJInventoryWidget — Native inventory + crafting panel.
 * Toggle with I key. Left side: item list. Right side: crafting recipes.
 * Fetches live data from Flask backend on open.
 */
UCLASS(Abstract)
class HERITAGEJARVIS_API UHJInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	/** Open/refresh the inventory panel. */
	UFUNCTION(BlueprintCallable, Category = "HJ|Inventory")
	void ShowInventory();

	/** Close the inventory panel. */
	UFUNCTION(BlueprintCallable, Category = "HJ|Inventory")
	void HideInventory();

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Inventory")
	bool bIsOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Inventory")
	TArray<FTartariaInventoryItem> Items;

	UPROPERTY(BlueprintReadOnly, Category = "HJ|Inventory")
	TArray<FTartariaCraftingRecipe> Recipes;

	// -------------------------------------------------------
	// Blueprint events
	// -------------------------------------------------------

	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Inventory")
	void OnInventoryRefreshed();

	UFUNCTION(BlueprintNativeEvent, Category = "HJ|Inventory")
	void OnCraftResult(bool bSuccess, const FString& ItemName);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** Fade animation state. */
	float FadeAlpha = 0.f;
	int32 FadeDir = 0;  // 0=idle, +1=fading in, -1=fading out

	void BuildProgrammaticLayout();
	void FetchInventory();
	void FetchRecipes();
	void OnInventoryResponse(bool bSuccess, const FString& Body);
	void OnRecipesResponse(bool bSuccess, const FString& Body);
	void RefreshItemList();
	void RefreshRecipeList();
	void CraftRecipe(const FString& RecipeKey);
	void OnCraftResponse(bool bSuccess, const FString& Body);
	void CheckCraftability();

	// Widget refs
	UPROPERTY() UBorder* PanelBg = nullptr;
	UPROPERTY() UTextBlock* TitleText = nullptr;
	UPROPERTY() UVerticalBox* ItemListBox = nullptr;
	UPROPERTY() UVerticalBox* RecipeListBox = nullptr;
	UPROPERTY() UScrollBox* ItemScroll = nullptr;
	UPROPERTY() UScrollBox* RecipeScroll = nullptr;
	UPROPERTY() UTextBlock* ItemCountText = nullptr;
	UPROPERTY() UButton* CloseButton = nullptr;

	/** Dynamically created item row widgets (cleared on refresh). */
	UPROPERTY()
	TArray<UWidget*> DynamicItemWidgets;

	UPROPERTY()
	TArray<UWidget*> DynamicRecipeWidgets;

	/** Craft button → recipe key mapping for click dispatch. */
	UPROPERTY()
	TArray<UButton*> CraftButtons;

	TArray<FString> CraftButtonKeys;

	UFUNCTION()
	void OnCloseClicked();

	UFUNCTION()
	void OnAnyCraftClicked();

	bool bRecipesFetched = false;
};
