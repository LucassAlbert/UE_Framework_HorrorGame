#pragma once
#include "CoreMinimal.h"
#include "NavLinkCustomComponent.h"
#include "HorrorDoorNavigationComponent.generated.h"

class UHorrorDoorTraversalDefinition;
UCLASS(ClassGroup=(Horror), meta=(BlueprintSpawnableComponent))
class HORRORAI_API UHorrorDoorNavigationComponent : public UNavLinkCustomComponent
{
    GENERATED_BODY()
public:
    UHorrorDoorNavigationComponent(const FObjectInitializer& Initializer);
    // Null explicitly opts out, preserving existing door assets.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Door") TObjectPtr<UHorrorDoorTraversalDefinition> Definition;
    // Offsets use actor orientation and centimetres, independent of actor scale.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Door|Navigation") FVector StartOffset = FVector(-180, 100, 0);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Door|Navigation") FVector EndOffset = FVector(180, 100, 0);
    UFUNCTION(BlueprintCallable, Category="Door|Navigation") void RefreshDoorNavigation();
    bool HasValidConfiguration() const;
    virtual bool IsLinkPathfindingAllowed(const UObject* Querier) const override;
    virtual bool OnLinkMoveStarted(UObject* PathComp, const FVector& DestPoint) override;
    virtual void OnLinkMoveFinished(UObject* PathComp) override;
    virtual void OnRegister() override;
    virtual void OnUnregister() override;
protected:
    virtual void BeginPlay() override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
};
