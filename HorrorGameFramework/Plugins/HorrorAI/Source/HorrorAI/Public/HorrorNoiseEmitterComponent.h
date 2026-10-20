#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HorrorNoiseEmitterComponent.generated.h"

class UHorrorNoiseDefinition;
class UPrimitiveComponent;

UCLASS(ClassGroup = (Horror), meta = (BlueprintSpawnableComponent))
class HORRORAI_API UHorrorNoiseEmitterComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UHorrorNoiseEmitterComponent();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise") TObjectPtr<UHorrorNoiseDefinition> Definition;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise") bool bReportPhysicsImpacts = false;
    UFUNCTION(BlueprintCallable, Category = "Noise") bool ReportNoise(FVector Location, AActor* InstigatorActor = nullptr);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void HandleImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
    TWeakObjectPtr<UPrimitiveComponent> ImpactComponent;
    double LastNoiseTime = -DBL_MAX;
};
