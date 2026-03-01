#include "TartariaTerminal.h"
#include "TartariaGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/PointLightComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "UI/HJDashboardWidget.h"

ATartariaTerminal::ATartariaTerminal()
{
	PrimaryActorTick.bCanEverTick = false;

	// Frame mesh — tall slab acting as the terminal body
	FrameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameMesh"));
	RootComponent = FrameMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube"));
	if (CubeFinder.Succeeded())
		FrameMesh->SetStaticMesh(CubeFinder.Object);
	FrameMesh->SetWorldScale3D(FVector(0.3f, 1.2f, 2.0f));

	// Screen mesh — flat plane in front of the frame
	ScreenMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScreenMesh"));
	ScreenMesh->SetupAttachment(RootComponent);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
		TEXT("/Engine/BasicShapes/Plane"));
	if (PlaneFinder.Succeeded())
		ScreenMesh->SetStaticMesh(PlaneFinder.Object);
	ScreenMesh->SetRelativeLocation(FVector(16.f, 0.f, 30.f));
	ScreenMesh->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	ScreenMesh->SetRelativeScale3D(FVector(0.9f, 0.55f, 1.f));

	// Widget component — renders CEF dashboard on the screen mesh surface
	ScreenWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("ScreenWidget"));
	ScreenWidget->SetupAttachment(RootComponent);
	ScreenWidget->SetRelativeLocation(FVector(17.f, 0.f, 30.f));
	ScreenWidget->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	ScreenWidget->SetDrawSize(FVector2D(1280.f, 720.f));
	ScreenWidget->SetPivot(FVector2D(0.5f, 0.5f));
	ScreenWidget->SetGeometryMode(EWidgetGeometryMode::Plane);
	ScreenWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ScreenWidget->SetVisibility(false); // Hidden until activated

	// Screen glow — amber light emanating from the terminal
	ScreenGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("ScreenGlow"));
	ScreenGlow->SetupAttachment(RootComponent);
	ScreenGlow->SetRelativeLocation(FVector(40.f, 0.f, 30.f));
	ScreenGlow->SetIntensity(2000.f);
	ScreenGlow->SetLightColor(FLinearColor(0.79f, 0.66f, 0.30f)); // Gold #c9a84c
	ScreenGlow->SetAttenuationRadius(400.f);
}

void ATartariaTerminal::OnInteract_Implementation(APlayerController* Interactor)
{
	UE_LOG(LogTemp, Log, TEXT("TartariaTerminal: Player interacted with '%s'"), *TerminalName);

	if (bScreenActive)
	{
		DeactivateScreen(Interactor);
	}
	else
	{
		ActivateScreen(Interactor);
	}
}

FString ATartariaTerminal::GetInteractPrompt_Implementation() const
{
	if (bScreenActive)
		return FString::Printf(TEXT("Close %s"), *TerminalName);

	return FString::Printf(TEXT("Use %s"), *TerminalName);
}

FString ATartariaTerminal::GetDashboardRoute() const
{
	switch (TerminalType)
	{
	case ETartariaBuildingType::Forge:       return TEXT("/game#forge");
	case ETartariaBuildingType::Scriptorium: return TEXT("/game#scriptorium");
	case ETartariaBuildingType::Treasury:    return TEXT("/game#treasury");
	case ETartariaBuildingType::Barracks:    return TEXT("/game#barracks");
	case ETartariaBuildingType::Lab:         return TEXT("/game#lab");
	default: return TEXT("/game");
	}
}

void ATartariaTerminal::ActivateScreen(APlayerController* PC)
{
	if (bScreenActive) return;
	bScreenActive = true;

	// Create the dashboard widget if needed
	if (!DashWidget && PC)
	{
		DashWidget = CreateWidget<UHJDashboardWidget>(PC);
	}

	// Assign widget to the widget component for 3D rendering
	if (DashWidget && ScreenWidget)
	{
		ScreenWidget->SetWidget(DashWidget);
		ScreenWidget->SetDrawSize(ScreenResolution);
		ScreenWidget->SetVisibility(true);

		// Navigate to the terminal-specific route
		FString Route = GetDashboardRoute();
		FString Url = FString::Printf(TEXT("http://127.0.0.1:5000%s?embedded=1"), *Route);
		DashWidget->NavigateTo(Url);
	}

	// Boost screen glow when active
	ScreenGlow->SetIntensity(5000.f);

	// Enable mouse interaction with the 3D widget
	if (PC)
	{
		PC->SetShowMouseCursor(true);
		PC->SetInputMode(FInputModeGameAndUI());
	}

	OnTerminalActivated(PC);

	UE_LOG(LogTemp, Log, TEXT("TartariaTerminal: Screen activated — route: %s"), *GetDashboardRoute());
}

void ATartariaTerminal::DeactivateScreen(APlayerController* PC)
{
	if (!bScreenActive) return;
	bScreenActive = false;

	// Hide the widget component
	if (ScreenWidget)
		ScreenWidget->SetVisibility(false);

	// Dim screen glow
	ScreenGlow->SetIntensity(2000.f);

	// Return to game-only input
	if (PC)
	{
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
	}

	OnTerminalDeactivated();

	UE_LOG(LogTemp, Log, TEXT("TartariaTerminal: Screen deactivated"));
}
