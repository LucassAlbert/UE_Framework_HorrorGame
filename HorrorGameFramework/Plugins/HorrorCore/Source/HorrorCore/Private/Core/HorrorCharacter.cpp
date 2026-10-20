#include "Core/HorrorCharacter.h"
#include "Core/HorrorPlayerController.h"
#include "Components/SaveComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Subsystems/InteractionSubsystem.h"
#include "DrawDebugHelpers.h"
// Sets default values
AHorrorCharacter::AHorrorCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	//Create Cam Component
	Camera = CreateDefaultSubobject<UCameraComponent>("Camera");
	Camera->SetupAttachment(RootComponent);
	Camera->bUsePawnControlRotation = true;
    PlayerSaveComponent = CreateDefaultSubobject<USaveComponent>(TEXT("PlayerSave"));
    PlayerSaveComponent->PersistentKey = TEXT("Player");
}

void AHorrorCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (APlayerController* PlayerController = Cast<APlayerController>(NewController); PlayerController && PlayerController->IsLocalController() && InputMapping)
	{
		//GetCharacterMovement()->GravityScale = 1.f;
		//GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(InputMapping, 0);
            MappingController = PlayerController;
		}
	}
}


// Called when the game starts or when spawned
void AHorrorCharacter::BeginPlay()
{
	Super::BeginPlay();

	DefaultCameraZ = Camera->GetRelativeLocation().Z;
	TargetZ = DefaultCameraZ;

	//Pegar o componente de capsula para controlar a colisao
	Capsule = GetCapsuleComponent();
	Radius = Capsule->GetScaledCapsuleRadius();
    TargetHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();

}

// Called every frame
void AHorrorCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CamCrouchMovement(DeltaTime);
}

void AHorrorCharacter::CamCrouchMovement(float DeltaTime)
{
	// Localização atual
	FVector CurrentPos = Camera->GetRelativeLocation();

	if (InCrounch)
	{
		if (CurrentPos.Z < TargetZ + .005f)
		{
			return;
		}
		//UE_LOG(LogTemp, Warning, TEXT("Height = %f - Z = %f"), TargetZ, CurrentPos.Z);
	}
	else
	{
		if (CurrentPos.Z > TargetZ - .005f)
		{
			return;
		}
		//UE_LOG(LogTemp, Warning, TEXT("Height = %f - Z = %f"), TargetZ, CurrentPos.Z);
	}

	// Interpolação suave apenas no eixo Z
	float NewZ = FMath::FInterpTo(CurrentPos.Z, TargetZ, DeltaTime, CrouchSpeed);

	// Aplica a nova posição
	Camera->SetRelativeLocation(FVector(CurrentPos.X, CurrentPos.Y, NewZ));

	float Current = Capsule->GetUnscaledCapsuleHalfHeight();

	float NewHeight = FMath::FInterpTo(Current, TargetHalfHeight, DeltaTime, 10.f);

	float Delta = NewHeight - Current;

	Capsule->SetCapsuleHalfHeight(NewHeight, true);
	AddActorWorldOffset(FVector(0.f, 0.f, Delta));
}

// Called to bind functionality to input
void AHorrorCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AHorrorCharacter::OnLook_Implementation(const FInputActionValue& InputValue)
{
	FVector2D InputVector = InputValue.Get<FVector2D>();

	if (IsValid(Controller))
	{
		AddControllerYawInput(InputVector.X);
		AddControllerPitchInput(InputVector.Y);
	}
}

void AHorrorCharacter::OnMove_Implementation(const FInputActionValue& InputValue)
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

void AHorrorCharacter::OnCrouch_Implementation(const FInputActionValue& InputValue)
{
	//Se tiver agachado e nao puder levantar retornar o metodo
	if (InCrounch && !CanStandUp())return;

	InCrounch = !InCrounch;

	if (InCrounch)
	{
		TargetZ = CrouchedCameraZ;
		TargetHalfHeight = 45.f;
		GetCharacterMovement()->MaxWalkSpeed =  300.f;
	}
	else
	{
		TargetZ = DefaultCameraZ;
		TargetHalfHeight = 88.f;
		GetCharacterMovement()->MaxWalkSpeed = 600.f;
	}
}

bool AHorrorCharacter::CanStandUp()
{
	FVector Location = GetActorLocation();

	//Adicionar a diferença quando estiver abaixado
	if(InCrounch) Location.Z += 50;

	float StandHalfHeight = 88.f;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	bool bHit = GetWorld()->OverlapBlockingTestByChannel
	(
		Location,FQuat::Identity,ECC_Visibility,
		FCollisionShape::MakeCapsule(Radius, StandHalfHeight),Params
	);

	// DEBUG VISUAL
	//DrawDebugCapsule(GetWorld(),Location,StandHalfHeight,Radius,FQuat::Identity,bHit ? FColor::Red : FColor::Green,false,1.5f);

	return !bHit;
}



void AHorrorCharacter::RemoveInputMapping()
{
    if (APlayerController* PC = MappingController.Get())
    {
        if (ULocalPlayer* Player = PC->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Input = Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                if (InputMapping) Input->RemoveMappingContext(InputMapping);
            }
        }
    }
    MappingController.Reset();
}

void AHorrorCharacter::UnPossessed()
{
    RemoveInputMapping();
    Super::UnPossessed();
}

void AHorrorCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemoveInputMapping();
    Super::EndPlay(EndPlayReason);
}

void AHorrorCharacter::OnSaveStateRestored()
{
    ConsumeMovementInputVector();
    GetCharacterMovement()->StopMovementImmediately();
    if (AHorrorPlayerController* PC = Cast<AHorrorPlayerController>(GetController())) PC->ResetTransientControlState();
}
