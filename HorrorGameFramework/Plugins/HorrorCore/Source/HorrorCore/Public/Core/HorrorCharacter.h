#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "InputActionValue.h"
#include "Interfaces/HorrorInputReceiver.h"
#include "Interfaces/SaveStateListener.h"

#include "HorrorCharacter.generated.h"

UCLASS()
class HORRORCORE_API AHorrorCharacter : public ACharacter, public IHorrorInputReceiver, public ISaveStateListener
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* Camera;

protected:

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputMappingContext* InputMapping;

	//Crounch parameters
	bool  InCrounch = false;
	float TargetZ;
	float DefaultCameraZ;
	UCapsuleComponent* Capsule;
	float TargetHalfHeight;
	float Radius;

	// Altura desejada ao agachar (ajuste no editor ou aqui)
	UPROPERTY(EditAnywhere, Category = "Movement")
	float CrouchedCameraZ = -25.0f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float CrouchSpeed = 8.0f;

public:	

	AHorrorCharacter();
    bool IsHorrorCrouched() const { return InCrounch; }
    virtual void GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const override {}
    virtual void OnSaveStateRestored() override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	void CamCrouchMovement(float DeltaTime);

	bool CanStandUp();

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void PossessedBy(AController* NewController) override;
    virtual void UnPossessed() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void RemoveInputMapping();
    TWeakObjectPtr<class APlayerController> MappingController;

    UPROPERTY(VisibleAnywhere, Category = "Save")
    TObjectPtr<class USaveComponent> PlayerSaveComponent;

protected:

	virtual void OnLook_Implementation(const FInputActionValue& InputValue);
	virtual void OnMove_Implementation(const FInputActionValue& InputValue);
	virtual void OnCrouch_Implementation(const FInputActionValue& InputValue);

};
