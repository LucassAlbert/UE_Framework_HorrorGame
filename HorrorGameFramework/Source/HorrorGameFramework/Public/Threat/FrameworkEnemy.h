#pragma once
#include "CoreMinimal.h"
#include "HorrorEnemyCharacter.h"
#include "Interfaces/SaveStateListener.h"
#include "FrameworkEnemy.generated.h"

class USaveComponent;

UCLASS()
class HORRORGAMEFRAMEWORK_API AFrameworkEnemy : public AHorrorEnemyCharacter, public ISaveStateListener
{
    GENERATED_BODY()
public:
    AFrameworkEnemy();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save") TObjectPtr<USaveComponent> Save;
    virtual bool ValidateSaveState(FString& Error) const override;
    virtual void GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const override {}
    virtual void OnSaveStateRestored() override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void HandleCaptured(AActor* Enemy, APawn* Player);
    FTimerHandle RestoreTimer;
};
