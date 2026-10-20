#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HideSpotComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class HORRORINTERACTION_API UHideSpotComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHideSpotComponent();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hiding") FTransform HiddenOffset;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hiding", meta = (ClampMin = "1")) float EntryDistance = 250.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hiding", meta = (ClampMin = "0", ClampMax = "89")) float LookYawLimit = 45.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hiding", meta = (ClampMin = "0", ClampMax = "89")) float LookPitchLimit = 25.f;
    UFUNCTION(BlueprintPure, Category = "Hiding") bool IsOccupied() const;
    FTransform GetHiddenTransform() const;
private:
    friend class UHidingParticipantComponent;
    TWeakObjectPtr<class UHidingParticipantComponent> Occupant;

};
