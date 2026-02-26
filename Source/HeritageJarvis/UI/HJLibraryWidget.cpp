#include "HJLibraryWidget.h"
#include "Core/HJGameInstance.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Border.h"
#include "Components/Spacer.h"
#include "Styling/CoreStyle.h"

// -------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------

void UHJLibraryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildProgrammaticLayout();
}

void UHJLibraryWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshLibrary();
}

// -------------------------------------------------------------------
// Programmatic UMG layout
// -------------------------------------------------------------------

void UHJLibraryWidget::BuildProgrammaticLayout()
{
    if (!WidgetTree) return;

    // Guard: if Blueprint already placed children, skip programmatic build
    UPanelWidget* ExistingRoot = Cast<UPanelWidget>(WidgetTree->RootWidget);
    if (ExistingRoot && ExistingRoot->GetChildrenCount() > 0) return;

    // --- Root Overlay ---
    UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), FName("RootOverlay"));
    WidgetTree->RootWidget = RootOverlay;

    // --- Main VBox ---
    UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName("MainVBox"));
    RootOverlay->AddChild(MainVBox);

    // --- Title ---
    UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("TitleText"));
    TitleText->SetText(FText::FromString(TEXT("Knowledge Library")));
    TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 18);
    TitleText->SetFont(TitleFont);
    MainVBox->AddChild(TitleText);

    // --- Spacer 8px ---
    USpacer* Spacer1 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("Spacer1"));
    Spacer1->SetSize(FVector2D(0.f, 8.f));
    MainVBox->AddChild(Spacer1);

    // --- Search bar row ---
    UHorizontalBox* SearchRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName("SearchRow"));
    MainVBox->AddChild(SearchRow);

    // Search input (Fill)
    SearchInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), FName("SearchInput"));
    SearchInput->SetHintText(FText::FromString(TEXT("Search library...")));
    SearchRow->AddChild(SearchInput);
    if (UHorizontalBoxSlot* InputSlot = Cast<UHorizontalBoxSlot>(SearchInput->Slot))
    {
        InputSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    // Search button
    SearchBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName("SearchBtn"));
    SearchBtn->SetBackgroundColor(FLinearColor(0.15f, 0.55f, 0.95f));
    UTextBlock* SearchBtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("SearchBtnLabel"));
    SearchBtnLabel->SetText(FText::FromString(TEXT("Search")));
    SearchBtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    SearchBtn->AddChild(SearchBtnLabel);
    SearchRow->AddChild(SearchBtn);
    SearchBtn->OnClicked.AddDynamic(this, &UHJLibraryWidget::OnSearchBtnClicked);

    // Clear button
    ClearBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName("ClearBtn"));
    ClearBtn->SetBackgroundColor(FLinearColor(0.35f, 0.35f, 0.35f));
    UTextBlock* ClearBtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("ClearBtnLabel"));
    ClearBtnLabel->SetText(FText::FromString(TEXT("Clear")));
    ClearBtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    ClearBtn->AddChild(ClearBtnLabel);
    SearchRow->AddChild(ClearBtn);
    ClearBtn->OnClicked.AddDynamic(this, &UHJLibraryWidget::OnClearBtnClicked);

    // --- Spacer 4px ---
    USpacer* Spacer2 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("Spacer2"));
    Spacer2->SetSize(FVector2D(0.f, 4.f));
    MainVBox->AddChild(Spacer2);

    // --- Result count text ---
    ResultCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("ResultCountText"));
    ResultCountText->SetText(FText::FromString(TEXT("0 entries")));
    ResultCountText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)));
    FSlateFontInfo CountFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);
    ResultCountText->SetFont(CountFont);
    MainVBox->AddChild(ResultCountText);

    // --- Spacer 4px ---
    USpacer* Spacer3 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("Spacer3"));
    Spacer3->SetSize(FVector2D(0.f, 4.f));
    MainVBox->AddChild(Spacer3);

    // --- Library ScrollBox (Fill) ---
    LibraryScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), FName("LibraryScrollBox"));
    MainVBox->AddChild(LibraryScrollBox);
    if (UVerticalBoxSlot* ScrollSlot = Cast<UVerticalBoxSlot>(LibraryScrollBox->Slot))
    {
        ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    // --- Error text (red, collapsed by default) ---
    ErrorText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("ErrorText"));
    ErrorText->SetText(FText::GetEmpty());
    ErrorText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.25f, 0.25f)));
    FSlateFontInfo ErrFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
    ErrorText->SetFont(ErrFont);
    ErrorText->SetVisibility(ESlateVisibility::Collapsed);
    MainVBox->AddChild(ErrorText);
}

// -------------------------------------------------------------------
// Button handlers
// -------------------------------------------------------------------

void UHJLibraryWidget::OnSearchBtnClicked()
{
    if (!SearchInput) return;
    FString Query = SearchInput->GetText().ToString();
    SearchLibrary(Query);
}

void UHJLibraryWidget::OnClearBtnClicked()
{
    if (SearchInput) SearchInput->SetText(FText::GetEmpty());
    ClearSearch();
}

// -------------------------------------------------------------------
// Helper: populate scroll box with entry cards
// -------------------------------------------------------------------

static void PopulateLibraryCards(UScrollBox* ScrollBox, const TArray<FHJLibraryEntry>& InEntries, UObject* Outer)
{
    if (!ScrollBox) return;
    ScrollBox->ClearChildren();

    for (const FHJLibraryEntry& Entry : InEntries)
    {
        // Card border
        UBorder* Card = NewObject<UBorder>(Outer);
        Card->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.16f, 1.f));
        Card->SetPadding(FMargin(10.f));

        UVerticalBox* CardVBox = NewObject<UVerticalBox>(Outer);
        Card->AddChild(CardVBox);

        // Title row: Title + ContentType
        UHorizontalBox* TopRow = NewObject<UHorizontalBox>(Outer);
        CardVBox->AddChild(TopRow);

        UTextBlock* TitleLabel = NewObject<UTextBlock>(Outer);
        TitleLabel->SetText(FText::FromString(Entry.Title));
        TitleLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
        TitleLabel->SetFont(TitleFont);
        TopRow->AddChild(TitleLabel);
        if (UHorizontalBoxSlot* TitleSlot = Cast<UHorizontalBoxSlot>(TitleLabel->Slot))
        {
            TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        }

        UTextBlock* TypeLabel = NewObject<UTextBlock>(Outer);
        TypeLabel->SetText(FText::FromString(Entry.ContentType));
        TypeLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.7f, 1.f)));
        FSlateFontInfo TypeFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);
        TypeLabel->SetFont(TypeFont);
        TopRow->AddChild(TypeLabel);

        // Summary
        UTextBlock* SummaryLabel = NewObject<UTextBlock>(Outer);
        FString SummaryTrunc = Entry.Summary.Left(200);
        if (Entry.Summary.Len() > 200) SummaryTrunc += TEXT("...");
        SummaryLabel->SetText(FText::FromString(SummaryTrunc));
        SummaryLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)));
        FSlateFontInfo SumFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);
        SummaryLabel->SetFont(SumFont);
        SummaryLabel->SetAutoWrapText(true);
        CardVBox->AddChild(SummaryLabel);

        // Created at
        UTextBlock* DateLabel = NewObject<UTextBlock>(Outer);
        DateLabel->SetText(FText::FromString(Entry.CreatedAt));
        DateLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.45f, 0.45f)));
        FSlateFontInfo DateFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
        DateLabel->SetFont(DateFont);
        CardVBox->AddChild(DateLabel);

        ScrollBox->AddChild(Card);

        // Spacer between cards
        USpacer* CardSpacer = NewObject<USpacer>(Outer);
        CardSpacer->SetSize(FVector2D(0.f, 4.f));
        ScrollBox->AddChild(CardSpacer);
    }
}

// -------------------------------------------------------------------
// BlueprintNativeEvent _Implementation defaults
// -------------------------------------------------------------------

void UHJLibraryWidget::OnEntriesLoaded_Implementation(const TArray<FHJLibraryEntry>& LoadedEntries)
{
    PopulateLibraryCards(LibraryScrollBox, LoadedEntries, this);

    if (ResultCountText)
    {
        ResultCountText->SetText(FText::FromString(
            FString::Printf(TEXT("%d entries"), LoadedEntries.Num())));
    }
}

void UHJLibraryWidget::OnSearchComplete_Implementation(const TArray<FHJLibraryEntry>& Results, const FString& Query)
{
    PopulateLibraryCards(LibraryScrollBox, Results, this);

    if (ResultCountText)
    {
        ResultCountText->SetText(FText::FromString(
            FString::Printf(TEXT("%d results for '%s'"), Results.Num(), *Query)));
    }
}

void UHJLibraryWidget::OnError_Implementation(const FString& Message)
{
    if (ErrorText)
    {
        ErrorText->SetText(FText::FromString(Message));
        ErrorText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.25f, 0.25f)));
        ErrorText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

        // Auto-hide after 5 seconds — weak pointer prevents crash if widget destroyed
        TWeakObjectPtr<UHJLibraryWidget> WeakThis(this);
        FTimerHandle TimerHandle;
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().SetTimer(TimerHandle, [WeakThis]()
            {
                UHJLibraryWidget* Self = WeakThis.Get();
                if (Self && Self->ErrorText)
                {
                    Self->ErrorText->SetVisibility(ESlateVisibility::Collapsed);
                }
            }, 5.f, false);
        }
    }
}

// -------------------------------------------------------------------
// Existing logic (preserved)
// -------------------------------------------------------------------

void UHJLibraryWidget::RefreshLibrary()
{
    UHJGameInstance* GI = UHJGameInstance::Get(this);
    if (!GI) return;

    // Fix: Weak pointer prevents crash if widget is destroyed before callback fires
    TWeakObjectPtr<UHJLibraryWidget> WeakThis(this);

    FOnApiResponse CB;
    CB.BindLambda([WeakThis](bool bOk, const FString& Body)
    {
        UHJLibraryWidget* Self = WeakThis.Get();
        if (!Self) return;
        if (bOk) Self->ParseEntries(Body, false);
        else Self->OnError(TEXT("Failed to load library. Is Flask running?"));
    });
    if (!GI->ApiClient) return;
    GI->ApiClient->GetLibrary(CB);
}

void UHJLibraryWidget::SearchLibrary(const FString& Query)
{
    SearchQuery = Query;

    if (Query.IsEmpty())
    {
        RefreshLibrary();
        return;
    }

    UHJGameInstance* GI = UHJGameInstance::Get(this);
    if (!GI) return;

    // Fix: Weak pointer prevents crash if widget is destroyed before callback fires
    TWeakObjectPtr<UHJLibraryWidget> WeakThis(this);

    FOnApiResponse CB;
    CB.BindLambda([WeakThis](bool bOk, const FString& Body)
    {
        UHJLibraryWidget* Self = WeakThis.Get();
        if (!Self) return;
        if (bOk) Self->ParseEntries(Body, true);
        else Self->OnError(TEXT("Search failed."));
    });
    if (!GI->ApiClient) return;
    GI->ApiClient->SearchLibrary(Query, CB);
}

void UHJLibraryWidget::ClearSearch()
{
    SearchQuery.Empty();
    RefreshLibrary();
}

void UHJLibraryWidget::ParseEntries(const FString& Json, bool bIsSearch)
{
    Entries.Empty();

    TArray<TSharedPtr<FJsonValue>> JsonArray;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, JsonArray)) return;

    for (auto& Val : JsonArray)
    {
        TSharedPtr<FJsonObject> Obj = Val->AsObject();
        if (!Obj.IsValid()) continue;

        FHJLibraryEntry E;
        if (!Obj->TryGetStringField(TEXT("id"), E.Id)) E.Id = TEXT("");
        if (!Obj->TryGetStringField(TEXT("title"), E.Title)) E.Title = TEXT("Untitled");
        if (!Obj->TryGetStringField(TEXT("content_type"), E.ContentType)) E.ContentType = TEXT("");
        if (!Obj->TryGetStringField(TEXT("summary"), E.Summary)) E.Summary = TEXT("");
        if (!Obj->TryGetStringField(TEXT("created_at"), E.CreatedAt)) E.CreatedAt = TEXT("");
        if (!Obj->TryGetStringField(TEXT("project_id"), E.ProjectId)) E.ProjectId = TEXT("");
        Entries.Add(E);
    }

    if (bIsSearch) OnSearchComplete(Entries, SearchQuery);
    else OnEntriesLoaded(Entries);
}
