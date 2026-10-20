#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HorrorEnemyDefinition.h"
#include "HorrorEnemyCharacter.generated.h"

class UStaticMeshComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHorrorEnemyCaptured, AActor*, Enemy, APawn*, Player);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHorrorEnemyStateChanged, EHorrorEnemyState, State);

UCLASS()
class HORRORAI_API AHorrorEnemyCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AHorrorEnemyCharacter();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI") TObjectPtr<UHorrorEnemyDefinition> Definition;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (MakeEditWidget = "true")) TArray<FVector> PatrolLocations;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "AI") FHorrorEnemyMemory Memory;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Presentation") TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(BlueprintAssignable, Category = "AI") FHorrorEnemyCaptured OnCaptured;
    UPROPERTY(BlueprintAssignable, Category = "AI") FHorrorEnemyStateChanged OnStateChanged;
    void ResumeAfterRestore();
};
