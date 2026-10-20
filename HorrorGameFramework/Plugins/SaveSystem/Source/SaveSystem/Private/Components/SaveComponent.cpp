#include "Components/SaveComponent.h"
#include "Subsystems/SaveSubsystem.h"
#include "Registry/ActorRegistry.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogSaveComponent, Log, All);

USaveComponent::USaveComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void USaveComponent::OnComponentCreated()
{
    Super::OnComponentCreated();
#if WITH_EDITOR
    AActor* Owner = GetOwner();
    if (Owner && !Owner->IsTemplate() && Owner->GetWorld() && !Owner->GetWorld()->IsGameWorld()
        && Owner->GetActorGuid().IsValid())
    {
        PersistentID = Owner->GetActorGuid();
    }
#endif
}

void USaveComponent::BeginPlay()
{
    Super::BeginPlay();
    AActor* Owner = GetOwner();
    if (!PersistentKey.IsNone())
    {
        PersistentID = FGuid::NewDeterministicGuid(PersistentKey.ToString());
    }
#if WITH_EDITOR
    else if (!PersistentID.IsValid() && Owner && Owner->GetActorGuid().IsValid())
    {
        // Also repairs existing placed actors while running PIE; persist this ID before packaging.
        PersistentID = Owner->GetActorGuid();
    }
#endif
    if (!PersistentID.IsValid())
    {
        UE_LOG(LogSaveComponent, Error, TEXT("%s has no stable save ID. Assign PersistentKey or an instance GUID."),
            *GetNameSafe(Owner));
    }
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    USaveSubsystem* Save = GI ? GI->GetSubsystem<USaveSubsystem>() : nullptr;
    if (Save && Save->ActorRegistry) Save->ActorRegistry->RegisterActor(Owner);
}

void USaveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    USaveSubsystem* Save = GI ? GI->GetSubsystem<USaveSubsystem>() : nullptr;
    if (Save)
    {
        if (EndPlayReason == EEndPlayReason::Destroyed) Save->RecordDestroyedActor(PersistentID);
        if (Save->ActorRegistry) Save->ActorRegistry->UnregisterActor(GetOwner());
    }
    Super::EndPlay(EndPlayReason);
}
