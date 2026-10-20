#include "HorrorEnemyCharacter.h"
#include "HorrorEnemyController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AHorrorEnemyCharacter::AHorrorEnemyCharacter()
{
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AHorrorEnemyController::StaticClass();
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 360.f, 0.f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnemyBody"));
    Body->SetupAttachment(GetCapsuleComponent());
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AHorrorEnemyCharacter::ResumeAfterRestore()
{
    GetCharacterMovement()->StopMovementImmediately();
    if (AHorrorEnemyController* AI = Cast<AHorrorEnemyController>(GetController())) AI->ResumeAfterRestore();
}
