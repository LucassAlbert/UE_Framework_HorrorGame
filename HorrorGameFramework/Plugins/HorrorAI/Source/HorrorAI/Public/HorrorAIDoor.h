#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HorrorAIDoor.generated.h"

class APawn;
UINTERFACE(BlueprintType)
class HORRORAI_API UHorrorAIDoor : public UInterface { GENERATED_BODY() };

// Opening is explicit and idempotent: an AI never uses a player's toggle interaction.
class HORRORAI_API IHorrorAIDoor
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="AI|Door") bool IsAIDoorOpen() const;
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="AI|Door") bool IsAIDoorLocked() const;
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="AI|Door") bool CanAIOpenDoor(APawn* Agent) const;
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="AI|Door") bool OpenDoorForAI(APawn* Agent);
};
