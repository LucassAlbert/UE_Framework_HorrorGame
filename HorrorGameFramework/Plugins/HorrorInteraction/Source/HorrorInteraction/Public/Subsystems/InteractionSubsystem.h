#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InteractionSubsystem.generated.h"

class AActor;
class UInteractableComponent;


UCLASS(BlueprintType)
class HORRORINTERACTION_API UInteractionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	void SetFocusedActor(AActor* NewFocusedActor);

	void Interact(AActor* InstigatorActor);
    bool TryInteract(AActor* InstigatorActor);
    void RefreshFocusedActor();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	AActor* GetFocusedActor() const;

	//Event
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUIChanged, FString, InteractionText);

	UPROPERTY(BlueprintAssignable)
	FOnUIChanged OnFocusOn;

	UPROPERTY(BlueprintAssignable)
	FOnUIChanged OnFocusEnd;

private:

	TWeakObjectPtr<AActor> FocusedActor;
	FString FocusedText;
};
