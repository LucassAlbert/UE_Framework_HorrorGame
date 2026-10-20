#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ActorRegistry.generated.h"

class AActor;

UCLASS()
class SAVESYSTEM_API UActorRegistry : public UObject
{
    GENERATED_BODY()
public:
    void RegisterActor(AActor* Actor);
    void UnregisterActor(AActor* Actor);
    TArray<AActor*> GetRegisteredActors() const;
    void Reset();

private:
    TArray<TWeakObjectPtr<AActor>> RegisteredActors;
};
