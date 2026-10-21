#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StreamingWorldCoordinator.generated.h"
class UHorrorWorldDefinition;
UCLASS()
class HORRORWORLD_API AStreamingWorldCoordinator : public AActor
{
    GENERATED_BODY()
public:
    AStreamingWorldCoordinator();
    UPROPERTY(EditAnywhere, Category="World") TObjectPtr<UHorrorWorldDefinition> Definition;
protected:
    virtual void BeginPlay() override;
};
