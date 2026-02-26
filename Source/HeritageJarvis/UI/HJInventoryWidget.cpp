#include "HJInventoryWidget.h"
#include "HJNotificationWidget.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/SizeBox.h"
#include "Styling/CoreStyle.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

// Helper: create a text block inline
static UTextBlock* MakeText(UWidgetTree* Tree, const FName& Name,
	const FString& Content, FLinearColor Color, const FString& Style, int32 Size)
{
	UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	T->SetText(FText::FromString(Content));
	T->SetColorAndOpacity(FSlateColor(Color));
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(*Style, Size);
	T->SetFont(Font);
	return T;
}

// -----------------------------------------------------------------------------
void UHJInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree) return;

	UPanelWidget* ExistingRoot = Cast<UPanelWidget>(WidgetTree->RootWidget);
	if (ExistingRoot && ExistingRoot->GetChildrenCount() > 0) return;

	BuildProgrammaticLayout();
	SetVisibility(ESlateVisibility::Collapsed);
}

// -----------------------------------------------------------------------------
void UHJInventoryWidget::BuildProgrammaticLayout()
{
	// Root canvas
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), FName("InvRoot"));
	WidgetTree->RootWidget = Root;

	// Dimmer
	UBorder* Dimmer = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), FName("InvDimmer"));
	Dimmer->SetBrushColor(FLinearColor(0, 0, 0, 0.4f));
	UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dimmer);
	if (DimSlot) { DimSlot->SetAnchors(FAnchors(0, 0, 1, 1)); DimSlot->SetOffsets(FMargin(0)); }

	// Panel background — centered 700x500
	PanelBg = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), FName("InvPanelBg"));
	PanelBg->SetBrushColor(FLinearColor(0.06f, 0.06f, 0.10f, 0.95f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(PanelBg);
	if (PanelSlot)
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetOffsets(FMargin(0, 0, 700, 500));
	}

	// Main vertical layout
	UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), FName("InvMainVBox"));
	PanelBg->AddChild(MainVBox);

	// --- Title bar ---
	{
		UHorizontalBox* TitleBar = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), FName("InvTitleBar"));
		MainVBox->AddChild(TitleBar);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(TitleBar->Slot))
			Slot->SetPadding(FMargin(16, 12, 16, 8));

		TitleText = MakeText(WidgetTree, FName("InvTitle"),
			TEXT("INVENTORY & CRAFTING"), FLinearColor(0.9f, 0.85f, 0.6f), TEXT("Bold"), 15);
		TitleBar->AddChild(TitleText);
		if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(TitleText->Slot))
			Slot->SetVerticalAlignment(VAlign_Center);

		// Fill spacer
		USpacer* SpFill = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("InvTitleFill"));
		SpFill->SetSize(FVector2D(0, 0));
		TitleBar->AddChild(SpFill);
		if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(SpFill->Slot))
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		// Item count badge
		ItemCountText = MakeText(WidgetTree, FName("ItemCountText"),
			TEXT("0 items"), FLinearColor(0.6f, 0.6f, 0.6f), TEXT("Regular"), 10);
		TitleBar->AddChild(ItemCountText);
		if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(ItemCountText->Slot))
			Slot->SetVerticalAlignment(VAlign_Center);

		// Spacer
		USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("InvSpClose"));
		Sp->SetSize(FVector2D(16, 0));
		TitleBar->AddChild(Sp);

		// Close button
		CloseButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), FName("InvCloseBtn"));
		CloseButton->SetBackgroundColor(FLinearColor(0.6f, 0.15f, 0.15f));
		CloseButton->OnClicked.AddDynamic(this, &UHJInventoryWidget::OnCloseClicked);
		TitleBar->AddChild(CloseButton);
		if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(CloseButton->Slot))
			Slot->SetVerticalAlignment(VAlign_Center);

		UTextBlock* CloseLabel = MakeText(WidgetTree, FName("InvCloseLabel"),
			TEXT(" X "), FLinearColor::White, TEXT("Bold"), 12);
		CloseButton->AddChild(CloseLabel);
	}

	// --- Two-column content area ---
	UHorizontalBox* ContentRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), FName("InvContentRow"));
	MainVBox->AddChild(ContentRow);
	if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(ContentRow->Slot))
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// === LEFT PANEL: Inventory ===
	{
		UBorder* LeftBg = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), FName("InvLeftBg"));
		LeftBg->SetBrushColor(FLinearColor(0.04f, 0.04f, 0.07f, 0.8f));
		ContentRow->AddChild(LeftBg);
		if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(LeftBg->Slot))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(8, 0, 4, 8));
		}

		UVerticalBox* LeftVBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), FName("InvLeftVBox"));
		LeftBg->AddChild(LeftVBox);

		// Section header
		UTextBlock* InvHeader = MakeText(WidgetTree, FName("InvHeader"),
			TEXT("Items"), FLinearColor(0.7f, 0.85f, 1.0f), TEXT("Bold"), 12);
		LeftVBox->AddChild(InvHeader);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(InvHeader->Slot))
			Slot->SetPadding(FMargin(8, 8, 0, 4));

		// Scrollable item list
		ItemScroll = WidgetTree->ConstructWidget<UScrollBox>(
			UScrollBox::StaticClass(), FName("InvItemScroll"));
		LeftVBox->AddChild(ItemScroll);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(ItemScroll->Slot))
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		ItemListBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), FName("InvItemList"));
		ItemScroll->AddChild(ItemListBox);
	}

	// === RIGHT PANEL: Crafting ===
	{
		UBorder* RightBg = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), FName("InvRightBg"));
		RightBg->SetBrushColor(FLinearColor(0.04f, 0.04f, 0.07f, 0.8f));
		ContentRow->AddChild(RightBg);
		if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(RightBg->Slot))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(4, 0, 8, 8));
		}

		UVerticalBox* RightVBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), FName("InvRightVBox"));
		RightBg->AddChild(RightVBox);

		// Section header
		UTextBlock* CraftHeader = MakeText(WidgetTree, FName("CraftHeader"),
			TEXT("Crafting"), FLinearColor(1.0f, 0.85f, 0.5f), TEXT("Bold"), 12);
		RightVBox->AddChild(CraftHeader);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(CraftHeader->Slot))
			Slot->SetPadding(FMargin(8, 8, 0, 4));

		// Scrollable recipe list
		RecipeScroll = WidgetTree->ConstructWidget<UScrollBox>(
			UScrollBox::StaticClass(), FName("InvRecipeScroll"));
		RightVBox->AddChild(RecipeScroll);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(RecipeScroll->Slot))
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		RecipeListBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), FName("InvRecipeList"));
		RecipeScroll->AddChild(RecipeListBox);
	}

	// --- Bottom hint bar ---
	{
		UTextBlock* HintText = MakeText(WidgetTree, FName("InvHintText"),
			TEXT("Press I to close"), FLinearColor(0.4f, 0.4f, 0.4f), TEXT("Regular"), 9);
		HintText->SetJustification(ETextJustify::Center);
		MainVBox->AddChild(HintText);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(HintText->Slot))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetPadding(FMargin(0, 4, 0, 8));
		}
	}
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

void UHJInventoryWidget::ShowInventory()
{
	bIsOpen = true;
	SetVisibility(ESlateVisibility::Visible);
	FetchInventory();
	if (!bRecipesFetched)
		FetchRecipes();
	else
		CheckCraftability();
}

void UHJInventoryWidget::HideInventory()
{
	bIsOpen = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

void UHJInventoryWidget::OnCloseClicked()
{
	HideInventory();
}

// -----------------------------------------------------------------------------
// Network
// -----------------------------------------------------------------------------

void UHJInventoryWidget::FetchInventory()
{
	UHJGameInstance* GI = UHJGameInstance::Get(GetWorld());
	if (!GI || !GI->ApiClient) return;

	FOnApiResponse CB;
	CB.BindUObject(this, &UHJInventoryWidget::OnInventoryResponse);
	GI->ApiClient->Get(TEXT("/api/game/inventory"), CB);
}

void UHJInventoryWidget::OnInventoryResponse(bool bSuccess, const FString& Body)
{
	if (!bSuccess) return;

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

	Items.Empty();
	const TArray<TSharedPtr<FJsonValue>>* ItemsArr;
	if (Root->TryGetArrayField(TEXT("items"), ItemsArr))
	{
		for (const TSharedPtr<FJsonValue>& Val : *ItemsArr)
		{
			const TSharedPtr<FJsonObject>& Obj = Val->AsObject();
			if (!Obj.IsValid()) continue;

			FTartariaInventoryItem Item;
			// Try both "item_id" and "id" keys
			if (!Obj->TryGetStringField(TEXT("item_id"), Item.ItemId))
				Obj->TryGetStringField(TEXT("id"), Item.ItemId);

			double Qty = 0;
			Obj->TryGetNumberField(TEXT("quantity"), Qty);
			Item.Quantity = static_cast<int32>(Qty);
			Obj->TryGetBoolField(TEXT("equipped"), Item.bEquipped);

			if (!Item.ItemId.IsEmpty() && Item.Quantity > 0)
				Items.Add(Item);
		}
	}

	RefreshItemList();
	CheckCraftability();
	OnInventoryRefreshed();
}

void UHJInventoryWidget::FetchRecipes()
{
	UHJGameInstance* GI = UHJGameInstance::Get(GetWorld());
	if (!GI || !GI->ApiClient) return;

	FOnApiResponse CB;
	CB.BindUObject(this, &UHJInventoryWidget::OnRecipesResponse);
	GI->ApiClient->Get(TEXT("/api/game/craft/recipes"), CB);
}

void UHJInventoryWidget::OnRecipesResponse(bool bSuccess, const FString& Body)
{
	if (!bSuccess) return;

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

	Recipes.Empty();

	// Recipes come as { "recipes": { "key": { "ingredients": {...}, "result": "..." } } }
	const TSharedPtr<FJsonObject>* RecipesObj;
	if (Root->TryGetObjectField(TEXT("recipes"), RecipesObj))
	{
		for (const auto& Pair : (*RecipesObj)->Values)
		{
			const TSharedPtr<FJsonObject>* RObj;
			if (!Pair.Value->TryGetObject(RObj)) continue;

			FTartariaCraftingRecipe Recipe;
			Recipe.RecipeKey = Pair.Key;
			(*RObj)->TryGetStringField(TEXT("result"), Recipe.ResultItem);

			const TSharedPtr<FJsonObject>* IngrObj;
			if ((*RObj)->TryGetObjectField(TEXT("ingredients"), IngrObj))
			{
				for (const auto& IPair : (*IngrObj)->Values)
				{
					FTartariaCraftIngredient Ingr;
					Ingr.ItemId = IPair.Key;
					double Qty = 1;
					IPair.Value->TryGetNumber(Qty);
					Ingr.Quantity = static_cast<int32>(Qty);
					Recipe.Ingredients.Add(Ingr);
				}
			}

			Recipes.Add(Recipe);
		}
	}

	bRecipesFetched = true;
	CheckCraftability();
	RefreshRecipeList();
}

// -----------------------------------------------------------------------------
// Crafting
// -----------------------------------------------------------------------------

void UHJInventoryWidget::CheckCraftability()
{
	// Build lookup of owned quantities
	TMap<FString, int32> Owned;
	for (const FTartariaInventoryItem& Item : Items)
		Owned.FindOrAdd(Item.ItemId) += Item.Quantity;

	for (FTartariaCraftingRecipe& Recipe : Recipes)
	{
		bool bCan = true;
		for (const FTartariaCraftIngredient& Ingr : Recipe.Ingredients)
		{
			int32* Have = Owned.Find(Ingr.ItemId);
			if (!Have || *Have < Ingr.Quantity)
			{
				bCan = false;
				break;
			}
		}
		Recipe.bCanCraft = bCan;
	}
}

void UHJInventoryWidget::CraftRecipe(const FString& RecipeKey)
{
	UHJGameInstance* GI = UHJGameInstance::Get(GetWorld());
	if (!GI || !GI->ApiClient) return;

	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject());
	Body->SetStringField(TEXT("recipe"), RecipeKey);

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	FOnApiResponse CB;
	CB.BindUObject(this, &UHJInventoryWidget::OnCraftResponse);
	GI->ApiClient->Post(TEXT("/api/game/craft"), BodyStr, CB);
}

void UHJInventoryWidget::OnCraftResponse(bool bSuccess, const FString& Body)
{
	if (!bSuccess)
	{
		OnCraftResult(false, TEXT(""));
		UHJNotificationWidget::Toast(TEXT("Crafting failed!"), EHJNotifType::Error, 2.0f);
		return;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	FString CraftedItem;
	bool bOk = false;

	if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
	{
		Root->TryGetBoolField(TEXT("success"), bOk);
		Root->TryGetStringField(TEXT("crafted"), CraftedItem);
	}

	if (bOk)
	{
		FString DisplayName = CraftedItem.Replace(TEXT("_"), TEXT(" "));
		UHJNotificationWidget::Toast(
			FString::Printf(TEXT("Crafted: %s"), *DisplayName),
			EHJNotifType::Success, 2.0f);

		// Refresh inventory to reflect changes
		FetchInventory();
	}
	else
	{
		FString Err;
		if (Root.IsValid()) Root->TryGetStringField(TEXT("error"), Err);
		UHJNotificationWidget::Toast(
			Err.IsEmpty() ? TEXT("Cannot craft — missing ingredients") : Err,
			EHJNotifType::Warning, 3.0f);
	}

	OnCraftResult(bOk, CraftedItem);
}

// -----------------------------------------------------------------------------
// UI Refresh
// -----------------------------------------------------------------------------

void UHJInventoryWidget::RefreshItemList()
{
	if (!ItemListBox || !WidgetTree) return;

	// Clear old rows
	for (UWidget* W : DynamicItemWidgets)
	{
		if (W) W->RemoveFromParent();
	}
	DynamicItemWidgets.Empty();

	if (Items.Num() == 0)
	{
		UTextBlock* Empty = MakeText(WidgetTree, FName("InvEmpty"),
			TEXT("  No items yet"), FLinearColor(0.5f, 0.5f, 0.5f), TEXT("Regular"), 10);
		ItemListBox->AddChild(Empty);
		DynamicItemWidgets.Add(Empty);
	}
	else
	{
		int32 Idx = 0;
		for (const FTartariaInventoryItem& Item : Items)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(
				UHorizontalBox::StaticClass(),
				*FString::Printf(TEXT("ItemRow_%d"), Idx));
			ItemListBox->AddChild(Row);
			DynamicItemWidgets.Add(Row);

			// Item name
			FString DisplayName = Item.ItemId.Replace(TEXT("_"), TEXT(" "));
			UTextBlock* NameText = MakeText(WidgetTree,
				*FString::Printf(TEXT("ItemName_%d"), Idx),
				FString::Printf(TEXT("  %s"), *DisplayName),
				Item.bEquipped ? FLinearColor(0.3f, 0.9f, 0.5f) : FLinearColor(0.85f, 0.85f, 0.85f),
				TEXT("Regular"), 10);
			Row->AddChild(NameText);
			if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(NameText->Slot))
			{
				Slot->SetVerticalAlignment(VAlign_Center);
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}

			// Quantity
			UTextBlock* QtyText = MakeText(WidgetTree,
				*FString::Printf(TEXT("ItemQty_%d"), Idx),
				FString::Printf(TEXT("x%d  "), Item.Quantity),
				FLinearColor(0.6f, 0.7f, 0.8f),
				TEXT("Bold"), 10);
			Row->AddChild(QtyText);
			if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(QtyText->Slot))
				Slot->SetVerticalAlignment(VAlign_Center);

			Idx++;
		}
	}

	// Update count badge
	if (ItemCountText)
	{
		int32 TotalQty = 0;
		for (const FTartariaInventoryItem& Item : Items)
			TotalQty += Item.Quantity;
		ItemCountText->SetText(FText::FromString(
			FString::Printf(TEXT("%d items"), TotalQty)));
	}
}

void UHJInventoryWidget::RefreshRecipeList()
{
	if (!RecipeListBox || !WidgetTree) return;

	// Clear old rows
	for (UWidget* W : DynamicRecipeWidgets)
	{
		if (W) W->RemoveFromParent();
	}
	DynamicRecipeWidgets.Empty();

	if (Recipes.Num() == 0)
	{
		UTextBlock* Empty = MakeText(WidgetTree, FName("RecipeEmpty"),
			TEXT("  Loading recipes..."), FLinearColor(0.5f, 0.5f, 0.5f), TEXT("Regular"), 10);
		RecipeListBox->AddChild(Empty);
		DynamicRecipeWidgets.Add(Empty);
		return;
	}

	// Clear previous button mappings
	CraftButtons.Empty();
	CraftButtonKeys.Empty();

	int32 Idx = 0;
	for (const FTartariaCraftingRecipe& Recipe : Recipes)
	{
		// Recipe container
		UVerticalBox* RecipeBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			*FString::Printf(TEXT("RecipeBox_%d"), Idx));
		RecipeListBox->AddChild(RecipeBox);
		DynamicRecipeWidgets.Add(RecipeBox);

		// Header row: recipe name + craft button
		UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			*FString::Printf(TEXT("RecipeHeader_%d"), Idx));
		RecipeBox->AddChild(HeaderRow);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(HeaderRow->Slot))
			Slot->SetPadding(FMargin(8, 6, 8, 0));

		FString ResultDisplay = Recipe.ResultItem.Replace(TEXT("_"), TEXT(" "));
		FLinearColor NameColor = Recipe.bCanCraft
			? FLinearColor(0.9f, 0.9f, 0.7f)
			: FLinearColor(0.5f, 0.5f, 0.5f);
		UTextBlock* RecipeName = MakeText(WidgetTree,
			*FString::Printf(TEXT("RecipeName_%d"), Idx),
			ResultDisplay, NameColor, TEXT("Bold"), 11);
		HeaderRow->AddChild(RecipeName);
		if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(RecipeName->Slot))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		// Craft button — uses OnAnyCraftClicked + IsHovered dispatch
		UButton* CraftBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			*FString::Printf(TEXT("CraftBtn_%d"), Idx));
		CraftBtn->SetBackgroundColor(Recipe.bCanCraft
			? FLinearColor(0.15f, 0.5f, 0.2f)
			: FLinearColor(0.3f, 0.3f, 0.3f));
		CraftBtn->SetIsEnabled(Recipe.bCanCraft);
		CraftBtn->OnClicked.AddDynamic(this, &UHJInventoryWidget::OnAnyCraftClicked);
		HeaderRow->AddChild(CraftBtn);
		if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(CraftBtn->Slot))
			Slot->SetVerticalAlignment(VAlign_Center);

		UTextBlock* CraftLabel = MakeText(WidgetTree,
			*FString::Printf(TEXT("CraftLabel_%d"), Idx),
			TEXT(" Craft "), FLinearColor::White, TEXT("Bold"), 10);
		CraftBtn->AddChild(CraftLabel);

		// Track button → recipe key mapping
		CraftButtons.Add(CraftBtn);
		CraftButtonKeys.Add(Recipe.RecipeKey);

		// Ingredients line
		FString IngrStr;
		for (int32 I = 0; I < Recipe.Ingredients.Num(); I++)
		{
			const FTartariaCraftIngredient& Ingr = Recipe.Ingredients[I];
			FString IngName = Ingr.ItemId.Replace(TEXT("_"), TEXT(" "));
			if (I > 0) IngrStr += TEXT(", ");
			IngrStr += FString::Printf(TEXT("%s x%d"), *IngName, Ingr.Quantity);
		}

		UTextBlock* IngrText = MakeText(WidgetTree,
			*FString::Printf(TEXT("RecipeIngr_%d"), Idx),
			FString::Printf(TEXT("  Needs: %s"), *IngrStr),
			FLinearColor(0.5f, 0.55f, 0.6f), TEXT("Regular"), 9);
		RecipeBox->AddChild(IngrText);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(IngrText->Slot))
			Slot->SetPadding(FMargin(8, 0, 8, 4));

		Idx++;
	}
}

// -----------------------------------------------------------------------------
// BlueprintNativeEvent defaults
// -----------------------------------------------------------------------------

void UHJInventoryWidget::OnAnyCraftClicked()
{
	// Dispatch: find the hovered button and craft its recipe
	for (int32 I = 0; I < CraftButtons.Num(); I++)
	{
		if (CraftButtons[I] && CraftButtons[I]->IsHovered())
		{
			CraftRecipe(CraftButtonKeys[I]);
			return;
		}
	}
}

void UHJInventoryWidget::OnInventoryRefreshed_Implementation() {}
void UHJInventoryWidget::OnCraftResult_Implementation(bool bSuccess, const FString& ItemName) {}
