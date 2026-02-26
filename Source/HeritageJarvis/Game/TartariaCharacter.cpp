#include "TartariaCharacter.h"
#include "TartariaGameMode.h"
#include "HJInteractable.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

ATartariaCharacter::ATartariaCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Spring arm — 3rd person offset
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength         = 400.0f;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bEnableCameraLag        = true;
	SpringArm->CameraLagSpeed          = 8.0f;
	SpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));

	// Camera
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	// Movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate              = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity             = 600.0f;
	GetCharacterMovement()->AirControl                = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed              = WalkSpeed;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw   = false;
	bUseControllerRotationRoll  = false;
}

void ATartariaCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ATartariaCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);

	// Legacy input — bindings come from DefaultInput.ini, no assets needed
	Input->BindAxis("MoveForward", this, &ATartariaCharacter::MoveForward);
	Input->BindAxis("MoveRight",   this, &ATartariaCharacter::MoveRight);
	Input->BindAxis("Turn",        this, &ATartariaCharacter::Turn);
	Input->BindAxis("LookUp",      this, &ATartariaCharacter::LookUp);

	Input->BindAction("Jump",      IE_Pressed,  this, &ATartariaCharacter::Jump);
	Input->BindAction("Jump",      IE_Released, this, &ATartariaCharacter::StopJumping);
	Input->BindAction("Sprint",    IE_Pressed,  this, &ATartariaCharacter::StartSprint);
	Input->BindAction("Sprint",    IE_Released, this, &ATartariaCharacter::StopSprint);
	Input->BindAction("Interact",  IE_Pressed,  this, &ATartariaCharacter::Interact);
	Input->BindAction("OpenMenu",  IE_Pressed,  this, &ATartariaCharacter::ToggleMenu);
	Input->BindAction("DebugToggle", IE_Pressed, this, &ATartariaCharacter::ToggleDebug);
	Input->BindAction("DashboardToggle", IE_Pressed, this, &ATartariaCharacter::ToggleDashboard);
}

void ATartariaCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// -------------------------------------------------------
// Movement
// -------------------------------------------------------

void ATartariaCharacter::MoveForward(float Value)
{
	if (Controller && Value != 0.0f)
	{
		const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
		AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X), Value);
	}
}

void ATartariaCharacter::MoveRight(float Value)
{
	if (Controller && Value != 0.0f)
	{
		const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
		AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y), Value);
	}
}

void ATartariaCharacter::Turn(float Value)
{
	AddControllerYawInput(Value);
}

void ATartariaCharacter::LookUp(float Value)
{
	AddControllerPitchInput(Value);
}

void ATartariaCharacter::StartSprint()
{
	bIsSprinting = true;
	GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void ATartariaCharacter::StopSprint()
{
	bIsSprinting = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

// -------------------------------------------------------
// Interact
// -------------------------------------------------------

void ATartariaCharacter::Interact()
{
	DoInteractTrace();
	OnInteract();
}

void ATartariaCharacter::DoInteractTrace()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FVector Start  = GetActorLocation();
	FVector End    = Start + GetActorForwardVector() * InteractReach;
	float   Radius = 60.0f;

	TArray<FHitResult> Hits;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	World->SweepMultiByChannel(
		Hits, Start, End, FQuat::Identity,
		ECollisionChannel::ECC_Visibility,
		FCollisionShape::MakeSphere(Radius), Params);

#if WITH_EDITOR
	DrawDebugSphere(World, End, Radius, 8, FColor::Yellow, false, 0.5f);
#endif

	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor) continue;
		if (HitActor->GetClass()->ImplementsInterface(UHJInteractable::StaticClass()))
		{
			APlayerController* PC = Cast<APlayerController>(GetController());
			IHJInteractable::Execute_OnInteract(HitActor, PC);
			return;
		}
	}
}

// -------------------------------------------------------
// Menu / Debug
// -------------------------------------------------------

void ATartariaCharacter::ToggleMenu()
{
	if (ATartariaGameMode* GM =
	    Cast<ATartariaGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->TogglePauseMenu();
	}
	OnMenuToggled();
}

void ATartariaCharacter::ToggleDebug()
{
	if (ATartariaGameMode* GM =
	    Cast<ATartariaGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->ToggleDebugOverlay();
	}
}

void ATartariaCharacter::ToggleDashboard()
{
	if (ATartariaGameMode* GM =
	    Cast<ATartariaGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->ToggleDashboardOverlay();
	}
}
