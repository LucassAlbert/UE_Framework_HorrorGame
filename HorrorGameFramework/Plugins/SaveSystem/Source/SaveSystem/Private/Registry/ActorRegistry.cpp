#include "Registry/ActorRegistry.h"
#include "GameFramework/Actor.h"

void UActorRegistry::RegisterActor(AActor* Actor)
{
    if (IsValid(Actor)) RegisteredActors.AddUnique(Actor);
}

void UActorRegistry::UnregisterActor(AActor* Actor)
{
    RegisteredActors.Remove(Actor);
}

TArray<AActor*> UActorRegistry::GetRegisteredActors() const
{
    TArray<AActor*> Result;
    for (const TWeakObjectPtr<AActor>& Actor : RegisteredActors)
    {
        if (Actor.IsValid()) Result.Add(Actor.Get());
    }
    return Result;
}

void UActorRegistry::Reset()
{
    RegisteredActors.Reset();
}
