// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InputActionValue.h"
#include "HorrorInputReceiver.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UHorrorInputReceiver : public UInterface
{
	GENERATED_BODY()
};

class HORRORCORE_API IHorrorInputReceiver
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Input")
    void OnInventory();


	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnMove(const FInputActionValue& InputValue);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnLook(const FInputActionValue& InputValue);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnCrouch(const FInputActionValue& InputValue);
};
