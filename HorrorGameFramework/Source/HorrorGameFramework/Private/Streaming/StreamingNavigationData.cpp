#include "Streaming/StreamingNavigationData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
AStreamingNavigationData::AStreamingNavigationData()
{
    RuntimeGeneration = ERuntimeGenerationType::Dynamic;
    bFixedTilePoolSize = true;
    TilePoolSize = 2048;
}
ARecastNavMesh* AStreamingNavigationData::CreateForEditor(UObject* WorldContext)
{
#if WITH_EDITOR
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (World && World->WorldType == EWorldType::Editor)
    {
        FActorSpawnParameters Parameters;
        Parameters.OverrideLevel = World->PersistentLevel;
        // Registration matches the configured agent class exactly, so author the native class.
        ARecastNavMesh* Navigation = World->SpawnActor<ARecastNavMesh>(Parameters);
        if (Navigation) Navigation->SetConfig(UNavigationSystemV1::GetDefaultSupportedAgent());
        return Navigation;
    }
#endif
    return nullptr;
}
