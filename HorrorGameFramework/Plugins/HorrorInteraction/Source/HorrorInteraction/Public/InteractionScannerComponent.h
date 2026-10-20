#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionScannerComponent.generated.h"

class UInteractionSubsystem;

UCLASS( ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent) )
class HORRORINTERACTION_API UInteractionScannerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UInteractionScannerComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:

	void Scan();

	void ScanSingleByChannel();

	bool CheckDistance(AActor* NewActor, float Distance);

	void HandleFocusStability(AActor* NewActor);

	UPROPERTY(EditAnywhere, Category = "Interaction")
	float ScanDistance = 300.f;

    UPROPERTY(EditAnywhere, Category = "Interaction", meta = (ClampMin = "0.02"))
    float ScanInterval = 0.08f;

    UPROPERTY(EditAnywhere, Category = "Interaction")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_GameTraceChannel1;

	UPROPERTY(EditAnywhere, Category = "Interaction")
	float DistanceWeight = 0.01f; //penaliza objetos longe

	UPROPERTY(EditAnywhere, Category = "Interaction")
	float LookAlignmentWeight = 5.f; //recompensa objetos no centro da tela

	UPROPERTY(EditAnywhere, Category = "Interaction")
	float FocusStabilityTime = 0.15f; //evita troca rápida de foco

	TWeakObjectPtr<AActor> LastBestActor;

	float LastFocusChangeTime = 0.f;

private:

	UPROPERTY(Transient)
	TObjectPtr<UInteractionSubsystem> InteractionSubsystem;

	FTimerHandle ScanTimer;
};
