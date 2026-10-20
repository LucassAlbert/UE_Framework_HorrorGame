#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/SaveComponent.h"
#include "SaveTestStateComponent.generated.h"

// Small serialization fixture: distinguishes persistent state from transient gameplay state.
UCLASS(Transient, NotBlueprintable)
class USaveTestStateComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame)
    int32 PuzzleState = 0;
    UPROPERTY(Transient)
    int32 RuntimeCounter = 0;
};

UCLASS(Transient, NotBlueprintable)
class ASaveTestRuntimeActor : public AActor
{
    GENERATED_BODY()
public:
    ASaveTestRuntimeActor()
    {
        SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
        Marker = CreateDefaultSubobject<USaveComponent>(TEXT("Save"));
        Marker->bRecreateOnLoad = true;
        State = CreateDefaultSubobject<USaveTestStateComponent>(TEXT("Puzzle"));
    }
    UPROPERTY() TObjectPtr<USaveComponent> Marker;
    UPROPERTY() TObjectPtr<USaveTestStateComponent> State;
};
