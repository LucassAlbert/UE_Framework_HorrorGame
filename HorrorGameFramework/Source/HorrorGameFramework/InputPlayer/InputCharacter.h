#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Interfaces/HorrorInputReceiver.h"

#include "InputCharacter.generated.h"

UCLASS()
class HORRORGAMEFRAMEWORK_API AInputCharacter : public ACharacter, public IHorrorInputReceiver
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* Camera;

protected:

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputMappingContext* InputMapping;

protected:

	//Crounch parameters
	bool  InCrounch = false;
	float TargetZ;
	float DefaultCameraZ; 

	// Altura desejada ao agachar (ajuste no editor ou aqui)
	UPROPERTY(EditAnywhere, Category = "Movement")
	float CrouchedCameraZ = -25.0f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float CrouchSpeed = 8.0f;

public:	

	// Sets default values for this character's properties
	AInputCharacter();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	void CamCrouchMovement(float DeltaTime) ;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void PossessedBy(AController* NewController) override;



protected:

	virtual void OnLook_Implementation(const FInputActionValue& InputValue);
	virtual void OnMove_Implementation(const FInputActionValue& InputValue);
	virtual void OnCrouch_Implementation(const FInputActionValue& InputValue);

};
