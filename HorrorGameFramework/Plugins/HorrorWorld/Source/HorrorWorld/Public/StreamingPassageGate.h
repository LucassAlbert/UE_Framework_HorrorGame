#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HorrorWorldSubsystem.h"
#include "StreamingPassageGate.generated.h"
class UStaticMeshComponent;
// A visible gate prevents entering an area until its geometry and saved state are ready.
UCLASS()
class HORRORWORLD_API AStreamingPassageGate : public AActor
{
    GENERATED_BODY()
public:
    AStreamingPassageGate();
    UPROPERTY(EditAnywhere, Category="World") FName RequiredArea;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Body;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void HandleAreaChanged(FName AreaId, EHorrorAreaState State);
    void Refresh();
};
