#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SaveInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(BlueprintType)
class USaveInterface : public UInterface
{
	GENERATED_BODY()
};


class SAVESYSTEM_API ISaveInterface
{
	GENERATED_BODY()

public:

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void OnActorSaved();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void OnActorLoaded();
};
