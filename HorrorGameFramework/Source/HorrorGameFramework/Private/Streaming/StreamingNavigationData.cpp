#include "Streaming/StreamingNavigationData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
AStreamingNavigationData::AStreamingNavigationData()
{
    RuntimeGeneration = ERuntimeGenerationType::Dynamic;
    bFixedTilePoolSize = true;
    TilePoolSize = 2048;
}
AStreamingNavigationData* AStreamingNavigationData::CreateForEditor(UObject* WorldContext)
{
#if WITH_EDITOR
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (World && World->WorldType == EWorldType::Editor)
    {
        FActorSpawnParameters Parameters;
        Parameters.OverrideLevel = World->PersistentLevel;
        return World->SpawnActor<AStreamingNavigationData>(Parameters);
    }
#endif
    return nullptr;
}
