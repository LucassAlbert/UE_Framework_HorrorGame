#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SaveStateListener.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class USaveStateListener : public UInterface
{
    GENERATED_BODY()
};

// Components can refresh derived state without making their reusable systems depend on save.
class SAVESYSTEM_API ISaveStateListener
{
    GENERATED_BODY()
public:
    virtual bool ValidateSaveState(FString& Error) const { return true; }
    virtual void GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const = 0;
    virtual void OnSaveStateRestored() = 0;
};
