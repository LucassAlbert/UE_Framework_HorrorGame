#pragma once
#include "CoreMinimal.h"
#include "NavMesh/RecastNavMesh.h"
#include "StreamingNavigationData.generated.h"
// Explicit per-world navigation settings; previous demo maps keep their own nav data.
UCLASS()
class HORRORGAMEFRAMEWORK_API AStreamingNavigationData : public ARecastNavMesh
{
    GENERATED_BODY()
public:
    AStreamingNavigationData();
    // Native navigation actors are not placeable through actor factories.
    UFUNCTION(BlueprintCallable, Category="World|Editor", meta=(DevelopmentOnly, WorldContext="WorldContext"))
    static ARecastNavMesh* CreateForEditor(UObject* WorldContext);
};
