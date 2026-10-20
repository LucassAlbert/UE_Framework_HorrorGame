#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "Interfaces/HorrorInputReceiver.h"
#include "InteractionScannerComponent.h"

#include "HorrorPlayerController.generated.h"

UENUM(BlueprintType)
enum class EPlayerControlState : uint8
{
	Gameplay,
	Hidden,
	UI,
	Paused
};

UCLASS()
class HORRORCORE_API AHorrorPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHorrorPlayerController();

	void SetControlState(EPlayerControlState NewState);
    void ResetTransientControlState();

protected:
	virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void SetupInputComponent() override;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputMappingContext* InputMapping;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* InteractAction;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* LookAction;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* CrouchAction;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* PauseGameAction;
    UPROPERTY(EditAnywhere, Category = "EnhancedInput")
    class UInputAction* InventoryAction;
    void CallInventory(const FInputActionValue& InputValue);


	void HandleInteract(const FInputActionValue& InputValue);

	void Move(const FInputActionValue& InputValue);

	void Look(const FInputActionValue& InputValue);

	void Crouch(const FInputActionValue& InputValue);

	void CallPauseGame(const FInputActionValue& InputValue);
	 
	//Hide System
	void CheckHideSpot(AActor* InteractiveActor);
	void SetHideSpotCamera(AActor* HideSpotActor);
	void HideSpotEnter( );
	void HideSpotExit( );
	UPROPERTY(EditAnywhere, Category = "HideSpot")
	bool InAnimation = false;

private:
    friend class FHorrorFrameworkLifecycleTest;
    bool IsGameplayFlowActive() const;
	EPlayerControlState CurrentState = EPlayerControlState::Gameplay;
    EPlayerControlState StateBeforeUI = EPlayerControlState::Gameplay;
    FTimerHandle HideTransitionTimer;
    void FinishHideEnter();
    void FinishHideExit();

    UFUNCTION()
    void HandleScreenStackChanged(bool bHasModalScreen);

    UFUNCTION()
    void HandleHidingChanged(bool bNowHidden, class UHideSpotComponent* Spot, FVector EntryLocation);

};
