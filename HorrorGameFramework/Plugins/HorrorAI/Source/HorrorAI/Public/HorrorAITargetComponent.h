#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HorrorAITargetComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FHorrorConcealmentChanged, AActor*, Target, bool, bConcealed, FVector, EntryLocation);

UCLASS(ClassGroup = (Horror), meta = (BlueprintSpawnableComponent))
class HORRORAI_API UHorrorAITargetComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHorrorAITargetComponent();
    virtual bool IsAvailable() const;
    virtual bool CanBeCaptured() const { return IsAvailable(); }
    UFUNCTION(BlueprintPure, Category = "AI") bool IsConcealed() const { return bConcealed; }
    UFUNCTION(BlueprintCallable, Category = "AI") void SetConcealed(bool bHidden, AActor* Anchor, FVector EntryLocation, FName PersistentKey = NAME_None);
    AActor* GetConcealmentAnchor() const { return ConcealmentAnchor.Get(); }
    FName GetConcealmentKey() const { return ConcealmentKey; }
    UPROPERTY(BlueprintAssignable, Category = "AI") FHorrorConcealmentChanged OnConcealmentChanged;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<class UAIPerceptionStimuliSourceComponent> Stimuli;
    bool bConcealed = false;
    TWeakObjectPtr<AActor> ConcealmentAnchor;
    FName ConcealmentKey;
};
