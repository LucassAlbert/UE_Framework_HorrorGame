#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProgressionArea.generated.h"

class UBoxComponent;
class USaveComponent;
class UPrimitiveComponent;

UCLASS()
class HORRORGAMEFRAMEWORK_API AProgressionArea : public AActor
{
    GENERATED_BODY()
public:
    AProgressionArea();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression") FName EntryFact;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Progression") bool bVisited = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progression") TObjectPtr<UBoxComponent> Volume;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save") TObjectPtr<USaveComponent> SaveComponent;
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void HandleEntry(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
        int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);
};
