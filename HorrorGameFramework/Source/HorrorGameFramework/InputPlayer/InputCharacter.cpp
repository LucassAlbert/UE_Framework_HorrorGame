#include "HorrorGameFramework/InputPlayer/InputCharacter.h"
#include "InputMappingContext.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Subsystems/InteractionSubsystem.h"
#include "DrawDebugHelpers.h"


// Sets default values
AInputCharacter::AInputCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//Create Component
	Camera = CreateDefaultSubobject<UCameraComponent>("Camera");
	Camera->SetupAttachment(RootComponent);
	Camera->bUsePawnControlRotation = true;
}

//Usar "AInputCharacter::PossessedBy" no lugar de BeginPlay quando se trata de controlador
void AInputCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (APlayerController* PlayerController = Cast<APlayerController>(NewController))
	{
		//GetCharacterMovement()->GravityScale = 1.f;
		//GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(InputMapping, 0);
		}
	}
}


// Called when the game starts or when spawned
void AInputCharacter::BeginPlay()
{
	Super::BeginPlay();

	DefaultCameraZ = Camera->GetRelativeLocation().Z;
	TargetZ = CrouchedCameraZ;
}

// Called every frame
void AInputCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CamCrouchMovement(DeltaTime);
}

void AInputCharacter::CamCrouchMovement(float DeltaTime)
{
	// Localização atual
	FVector CurrentPos = Camera->GetRelativeLocation();

	if (InCrounch)
	{
		if (CurrentPos.Z < TargetZ + .005f) return;
		//UE_LOG(LogTemp, Warning, TEXT("Height = %f - Z = %f"), TargetZ, CurrentPos.Z);
	}
	else
	{
		if (CurrentPos.Z > TargetZ - .005f ) return;
		//UE_LOG(LogTemp, Warning, TEXT("Height = %f - Z = %f"), TargetZ, CurrentPos.Z);
	}

	// Interpolação suave apenas no eixo Z
	float NewZ = FMath::FInterpTo(CurrentPos.Z, TargetZ, DeltaTime, CrouchSpeed);

	// Aplica a nova posição
	Camera->SetRelativeLocation(FVector(CurrentPos.X, CurrentPos.Y, NewZ));
}

// Called to bind functionality to input
void AInputCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AInputCharacter::OnLook_Implementation(const FInputActionValue& InputValue)
{
	FVector2D InputVector = InputValue.Get<FVector2D>();

	if (IsValid(Controller))
	{
		AddControllerYawInput(InputVector.X);
		AddControllerPitchInput(InputVector.Y);
	}
}

void AInputCharacter::OnMove_Implementation(const FInputActionValue& InputValue)
{
	FVector2D InputVector = InputValue.Get<FVector2D>();

	if (IsValid(Controller))
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		//Add movement input
		AddMovementInput(ForwardDirection, InputVector.Y);
		AddMovementInput(RightDirection, InputVector.X);
	}
}

void AInputCharacter::OnCrouch_Implementation(const FInputActionValue& InputValue)
{
	InCrounch = !InCrounch;

	if (InCrounch)
	{
		TargetZ = CrouchedCameraZ;
	}
	else
	{
		TargetZ = DefaultCameraZ;
	}
}



