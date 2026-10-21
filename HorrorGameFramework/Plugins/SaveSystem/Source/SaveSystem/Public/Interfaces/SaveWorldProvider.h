#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SaveWorldProvider.generated.h"

class USystemSaveGame;
DECLARE_DELEGATE_TwoParams(FSaveWorldPrepared, bool, FText);

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class USaveWorldProvider : public UInterface { GENERATED_BODY() };

// World preparation belongs to its owner; persistence waits without a streaming-plugin dependency.
class SAVESYSTEM_API ISaveWorldProvider
{
    GENERATED_BODY()
public:
    virtual bool IsSaveWorldReady() const = 0;
    virtual void RefreshSaveAreaBindings() {}
    virtual bool CaptureWorldMetadata(USystemSaveGame& Snapshot, FString& Error) const = 0;
    virtual void PrepareSaveWorld(const USystemSaveGame& Snapshot, FSaveWorldPrepared Completion) = 0;
    virtual bool IsSaveAreaResident(FName AreaId) const = 0;
};
