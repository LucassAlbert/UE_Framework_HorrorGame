#include "WorldResidentSpawnPoint.h"
#include "AreaResidencyComponent.h"
#include "Components/SaveComponent.h"
#include "Components/SceneComponent.h"
#include "Subsystems/SaveSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
AWorldResidentSpawnPoint::AWorldResidentSpawnPoint()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Save = CreateDefaultSubobject<USaveComponent>(TEXT("Save"));
}
bool AWorldResidentSpawnPoint::EnsureSpawned(FString& Error)
{
    if (bSpawned) return true;
    if (!ResidentClass || ResidentClass->HasAnyClassFlags(CLASS_Abstract) || Save->StreamingAreaId.IsNone() || !Save->PersistentID.IsValid())
    { Error = FString::Printf(TEXT("Configure a runtime-save class and stable ID on %s."), *GetNameSafe(this)); return false; }
    const FGuid Id = FGuid::NewDeterministicGuid(Save->PersistentID.ToString(EGuidFormats::Digits) + TEXT(":Resident"));
    USaveSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
    for (AActor* Actor : Saves->ActorRegistry->GetRegisteredActors())
        if (const USaveComponent* Marker = Actor->FindComponentByClass<USaveComponent>(); Marker && Marker->PersistentID == Id)
        { bSpawned = true; return true; }
    FActorSpawnParameters Params;
    Params.OverrideLevel = GetWorld()->PersistentLevel;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.bDeferConstruction = true;
    AActor* Actor = GetWorld()->SpawnActor<AActor>(ResidentClass, GetActorTransform(), Params);
    USaveComponent* Marker = Actor ? Actor->FindComponentByClass<USaveComponent>() : nullptr;
    if (!Marker || !Marker->bRecreateOnLoad || !Marker->PersistentKey.IsNone())
    {
        if (Actor) Actor->Destroy();
        Error = TEXT("Resident classes require opt-in runtime reconstruction and an empty PersistentKey."); return false;
    }
    Marker->PersistentID = Id;
    Marker->StreamingAreaId = Save->StreamingAreaId;
    if (UAreaResidencyComponent* Residency = Actor->FindComponentByClass<UAreaResidencyComponent>()) Residency->HomeAreaId = Save->StreamingAreaId;
    Actor->FinishSpawning(GetActorTransform());
    bSpawned = true;
    return true;
}
