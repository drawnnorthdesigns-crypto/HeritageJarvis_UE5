#include "HJProjectsWidget.h"
#include "HJProjectCard.h"
#include "HJLoadingWidget.h"
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

void UHJProjectsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildProgrammaticLayout();
}

void UHJProjectsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshProjects();
}

// -------------------------------------------------------------------
// Programmatic UMG layout
// -------------------------------------------------------------------

void UHJProjectsWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree) return;

	// Guard: if Blueprint already placed children, skip programmatic build
	UPanelWidget* ExistingRoot = Cast<UPanelWidget>(WidgetTree->RootWidget);
	if (ExistingRoot && ExistingRoot->GetChildrenCount() > 0) return;

	// --- Root Overlay ---
	RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), FName("RootOverlay"));
	WidgetTree->RootWidget = RootOverlay;

	// --- Main VBox ---
	MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName("MainVBox"));
	RootOverlay->AddChild(MainVBox);

	// --- Title ---
	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("Projects")));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 18);
	TitleText->SetFont(TitleFont);
	MainVBox->AddChild(TitleText);

	// --- Spacer 8px ---
	USpacer* Spacer1 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("Spacer1"));
	Spacer1->SetSize(FVector2D(0.f, 8.f));
	MainVBox->AddChild(Spacer1);

	// --- Projects ScrollBox (Fill) ---
	ProjectsScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), FName("ProjectsScrollBox"));
	MainVBox->AddChild(ProjectsScrollBox);
	if (UVerticalBoxSlot* ScrollSlot = Cast<UVerticalBoxSlot>(ProjectsScrollBox->Slot))
	{
		ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// --- Spacer 8px ---
	USpacer* Spacer2 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("Spacer2"));
	Spacer2->SetSize(FVector2D(0.f, 8.f));
	MainVBox->AddChild(Spacer2);

	// --- Form area (dark background) ---
	UBorder* FormBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName("FormBorder"));
	FormBorder->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.08f, 1.f));
	FormBorder->SetPadding(FMargin(12.f));
	MainVBox->AddChild(FormBorder);

	UVerticalBox* FormVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName("FormVBox"));
	FormBorder->AddChild(FormVBox);

	// Form title
	UTextBlock* FormTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("FormTitle"));
	FormTitle->SetText(FText::FromString(TEXT("Create New Project")));
	FormTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.9f, 0.9f)));
	FSlateFontInfo FormFont = FCoreStyle::GetDefaultFontStyle("Regular", 14);
	FormTitle->SetFont(FormFont);
	FormVBox->AddChild(FormTitle);

	// Project name input
	ProjectNameInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), FName("ProjectNameInput"));
	ProjectNameInput->SetHintText(FText::FromString(TEXT("Project name...")));
	FormVBox->AddChild(ProjectNameInput);

	// Prima materia input
	PrimaInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), FName("PrimaInput"));
	PrimaInput->SetHintText(FText::FromString(TEXT("Prima materia / description...")));
	FormVBox->AddChild(PrimaInput);

	// Create button
	CreateBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName("CreateBtn"));
	CreateBtn->SetBackgroundColor(FLinearColor(0.15f, 0.55f, 0.95f));
	UTextBlock* CreateBtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName("CreateBtnLabel"));
	CreateBtnLabel->SetText(FText::FromString(TEXT("Create Project")));
	CreateBtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	CreateBtn->AddChild(CreateBtnLabel);
	FormVBox->AddChild(CreateBtn);
	CreateBtn->OnClicked.AddDynamic(this, &UHJProjectsWidget::OnCreateBtnClicked);

	// --- Spacer 4px ---
	USpacer* Spacer3 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName("Spacer3"));
	Spacer3->SetSize(FVector2D(0.f, 4.f));
	MainVBox->AddChild(Spacer3);

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

void UHJProjectsWidget::OnCreateBtnClicked()
{
	FString Name = ProjectNameInput ? ProjectNameInput->GetText().ToString() : FString();
	FString Prima = PrimaInput ? PrimaInput->GetText().ToString() : FString();
	CreateProject(Name, Prima, false);

	// Clear fields after submission
	if (ProjectNameInput) ProjectNameInput->SetText(FText::GetEmpty());
	if (PrimaInput) PrimaInput->SetText(FText::GetEmpty());
}

// -------------------------------------------------------------------
// BlueprintNativeEvent _Implementation defaults
// -------------------------------------------------------------------

void UHJProjectsWidget::OnProjectsLoaded_Implementation(const TArray<FHJProject>& LoadedProjects)
{
	if (!ProjectsScrollBox) return;

	ProjectsScrollBox->ClearChildren();

	// Empty state
	if (LoadedProjects.Num() == 0)
	{
		UVerticalBox* EmptyBox = NewObject<UVerticalBox>(this);
		ProjectsScrollBox->AddChild(EmptyBox);

		USpacer* TopSpace = NewObject<USpacer>(this);
		TopSpace->SetSize(FVector2D(0.f, 40.f));
		EmptyBox->AddChild(TopSpace);

		UTextBlock* EmptyTitle = NewObject<UTextBlock>(this);
		EmptyTitle->SetText(FText::FromString(TEXT("No projects yet")));
		EmptyTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.7f)));
		FSlateFontInfo EmpFont = FCoreStyle::GetDefaultFontStyle("Bold", 16);
		EmptyTitle->SetFont(EmpFont);
		EmptyTitle->SetJustification(ETextJustify::Center);
		EmptyBox->AddChild(EmptyTitle);

		USpacer* MidSpace = NewObject<USpacer>(this);
		MidSpace->SetSize(FVector2D(0.f, 8.f));
		EmptyBox->AddChild(MidSpace);

		UTextBlock* EmptyHint = NewObject<UTextBlock>(this);
		EmptyHint->SetText(FText::FromString(TEXT("Create a new project below to get started.\nDescribe what you want to build and the AI pipeline\nwill generate designs, materials, and deliverables.")));
		EmptyHint->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.45f, 0.55f)));
		FSlateFontInfo HintFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
		EmptyHint->SetFont(HintFont);
		EmptyHint->SetJustification(ETextJustify::Center);
		EmptyHint->SetAutoWrapText(true);
		EmptyBox->AddChild(EmptyHint);

		return;
	}

	for (const FHJProject& Proj : LoadedProjects)
	{
		// Fix 1.3: Use UHJProjectCard subclass that stores ProjectId
		// so Run/Stop/Delete buttons can pass it to handler methods.
		UHJProjectCard* Card = CreateWidget<UHJProjectCard>(GetOwningPlayer());
		if (Card)
		{
			Card->Init(Proj.Id, Proj.Name, Proj.Description, Proj.Status, this);
			ProjectsScrollBox->AddChild(Card);
		}

		USpacer* CardSpacer = NewObject<USpacer>(this);
		CardSpacer->SetSize(FVector2D(0.f, 4.f));
		ProjectsScrollBox->AddChild(CardSpacer);
	}
}

void UHJProjectsWidget::OnPipelineStarted_Implementation(const FString& ProjectId)
{
	// Default: briefly show a status message in the error text area (green instead of red)
	if (ErrorText)
	{
		ErrorText->SetText(FText::FromString(FString::Printf(TEXT("Pipeline started for %s"), *ProjectId)));
		ErrorText->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.9f, 0.4f)));
		ErrorText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		// Auto-hide after 4 seconds — weak pointer prevents crash if widget destroyed
		TWeakObjectPtr<UHJProjectsWidget> WeakThis(this);
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, [WeakThis]()
		{
			UHJProjectsWidget* Self = WeakThis.Get();
			if (Self && Self->ErrorText)
			{
				Self->ErrorText->SetVisibility(ESlateVisibility::Collapsed);
			}
		}, 4.f, false);
	}
}

void UHJProjectsWidget::OnError_Implementation(const FString& Message)
{
	if (ErrorText)
	{
		ErrorText->SetText(FText::FromString(Message));
		ErrorText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.25f, 0.25f)));
		ErrorText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		// Auto-hide after 5 seconds — weak pointer prevents crash if widget destroyed
		TWeakObjectPtr<UHJProjectsWidget> WeakThis(this);
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, [WeakThis]()
		{
			UHJProjectsWidget* Self = WeakThis.Get();
			if (Self && Self->ErrorText)
			{
				Self->ErrorText->SetVisibility(ESlateVisibility::Collapsed);
			}
		}, 5.f, false);
	}
}

// -------------------------------------------------------------------
// Existing logic (preserved)
// -------------------------------------------------------------------

void UHJProjectsWidget::RefreshProjects()
{
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI) return;

	// Fix: Weak pointer prevents crash if widget is destroyed before callback fires
	TWeakObjectPtr<UHJProjectsWidget> WeakThis(this);

	FOnApiResponse CB;
	CB.BindLambda([WeakThis](bool bOk, const FString& Body)
	{
		UHJProjectsWidget* Self = WeakThis.Get();
		if (!Self) return;
		if (bOk) Self->ParseProjects(Body);
		else Self->OnError(TEXT("Failed to load projects. Is Flask running?"));
	});
	GI->ApiClient->GetProjects(CB);
}

void UHJProjectsWidget::CreateProject(const FString& Name, const FString& InputPrimaMateria,
                                        bool bCheckpointMode)
{
	if (Name.IsEmpty())
	{
		OnError(TEXT("Project name cannot be empty."));
		return;
	}

	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI) return;

	// Fix: Weak pointer prevents crash if widget is destroyed before callback fires
	TWeakObjectPtr<UHJProjectsWidget> WeakThis(this);

	FOnApiResponse CB;
	CB.BindLambda([WeakThis](bool bOk, const FString& Body)
	{
		UHJProjectsWidget* Self = WeakThis.Get();
		if (!Self) return;
		if (bOk) Self->RefreshProjects();
		else Self->OnError(TEXT("Failed to create project."));
	});
	GI->ApiClient->CreateProject(Name, InputPrimaMateria, bCheckpointMode, CB);
}

void UHJProjectsWidget::RunPipeline(const FString& ProjectId)
{
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI) return;

	// Fix 1.2: Show loading overlay immediately before API call
	UHJLoadingWidget::Show(this, TEXT("Launching pipeline..."));

	// Fix: Weak pointer prevents crash if widget is destroyed before callback fires
	TWeakObjectPtr<UHJProjectsWidget> WeakThis(this);

	FOnApiResponse CB;
	CB.BindLambda([WeakThis, ProjectId, GI](bool bOk, const FString&)
	{
		UHJProjectsWidget* Self = WeakThis.Get();
		if (bOk)
		{
			GI->StartWatchingPipeline(ProjectId);
			if (Self) Self->OnPipelineStarted(ProjectId);
			// Don't hide loading — EventPoller will take over with progress updates
		}
		else
		{
			UHJLoadingWidget::Hide(Self);
			if (Self) Self->OnError(TEXT("Failed to start pipeline."));
		}
	});
	GI->ApiClient->RunPipeline(ProjectId, CB);
}

void UHJProjectsWidget::StopPipeline(const FString& ProjectId)
{
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI) return;

	// Fix: Weak pointer prevents crash if widget is destroyed before callback fires
	TWeakObjectPtr<UHJProjectsWidget> WeakThis(this);

	FOnApiResponse CB;
	CB.BindLambda([WeakThis](bool bOk, const FString&)
	{
		if (UHJProjectsWidget* Self = WeakThis.Get())
			Self->RefreshProjects();
	});
	GI->ApiClient->StopPipeline(ProjectId, CB);
}

void UHJProjectsWidget::DeleteProject(const FString& ProjectId)
{
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI) return;

	// Fix: Weak pointer prevents crash if widget is destroyed before callback fires
	TWeakObjectPtr<UHJProjectsWidget> WeakThis(this);

	FOnApiResponse CB;
	CB.BindLambda([WeakThis](bool bOk, const FString&)
	{
		if (UHJProjectsWidget* Self = WeakThis.Get())
			Self->RefreshProjects();
	});
	GI->ApiClient->DeleteProject(ProjectId, CB);
}

void UHJProjectsWidget::SelectProject(const FString& ProjectId, const FString& ProjectName)
{
	if (UHJGameInstance* GI = UHJGameInstance::Get(this))
	{
		GI->ActiveProjectId = ProjectId;
		GI->ActiveProjectName = ProjectName;
		// Start polling status for this project
		if (GI->EventPoller)
			GI->EventPoller->StartPolling(GI->ApiClient, ProjectId);
	}
}

void UHJProjectsWidget::ParseProjects(const FString& Json)
{
	Projects.Empty();

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);

	if (!FJsonSerializer::Deserialize(Reader, JsonArray)) return;

	for (auto& Val : JsonArray)
	{
		TSharedPtr<FJsonObject> Obj = Val->AsObject();
		if (!Obj.IsValid()) continue;

		FHJProject P;
		if (!Obj->TryGetStringField(TEXT("id"), P.Id)) P.Id = TEXT("");
		if (!Obj->TryGetStringField(TEXT("name"), P.Name)) P.Name = TEXT("Untitled");
		if (!Obj->TryGetStringField(TEXT("input_prima_materia"), P.Description)) P.Description = TEXT("");
		if (!Obj->TryGetStringField(TEXT("status"), P.Status)) P.Status = TEXT("unknown");
		if (!Obj->TryGetStringField(TEXT("created_at"), P.CreatedAt)) P.CreatedAt = TEXT("");

		if (P.Status != TEXT("Deleted"))
			Projects.Add(P);
	}

	OnProjectsLoaded(Projects);
}
