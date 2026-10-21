#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SaveComponent.generated.h"

UCLASS(ClassGroup = (Save), meta = (BlueprintSpawnableComponent))
class SAVESYSTEM_API USaveComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USaveComponent();

    // For placed actors this is assigned from the editor actor GUID.
    // Runtime-spawned actors must receive a stable key or explicit GUID before BeginPlay.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, SaveGame, Category = "Save")
    FGuid PersistentID;

    // Stable role, such as Player. Must be unique among actors in the saved world.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save")
    FName PersistentKey;

    // Assigned by the streaming world. None identifies persistent-world participants.
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Save") FName StreamingAreaId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save")
    bool bSaveTransform = true;

    // Runtime actors only: recreated when missing, removed when absent from the loaded snapshot.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save")
    bool bRecreateOnLoad = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save")
    bool bSavePhysics = false;

    virtual void OnComponentCreated() override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
