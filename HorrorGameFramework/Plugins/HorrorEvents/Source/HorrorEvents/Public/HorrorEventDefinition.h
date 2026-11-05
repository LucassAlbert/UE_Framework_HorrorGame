#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/Interface.h"
#include "HorrorEventDefinition.generated.h"

class AActor;
class UHorrorEventComponent;

UINTERFACE(BlueprintType)
class HORROREVENTS_API UHorrorEventTarget : public UInterface
{
    GENERATED_BODY()
};

class HORROREVENTS_API IHorrorEventTarget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Events")
    bool CanApplyEventState(FName State, FText& Error) const;
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Events")
    bool ApplyEventState(FName State, bool bRestoring);
};

UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class HORROREVENTS_API UHorrorEventCondition : public UObject
{
    GENERATED_BODY()
public:
    virtual bool ValidateData(FText& Error) const { return true; }
    virtual void GatherAssets(TArray<FSoftObjectPath>& Paths) const {}
    UFUNCTION(BlueprintNativeEvent, Category = "Events")
    bool Evaluate(UHorrorEventComponent* Event, AActor* Candidate, FText& Error) const;
};

UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class HORROREVENTS_API UHorrorEventAction : public UObject
{
    GENERATED_BODY()
public:
    virtual bool ValidateData(FText& Error) const { return true; }
    virtual void GatherAssets(TArray<FSoftObjectPath>& Paths) const {}
    UFUNCTION(BlueprintNativeEvent, Category = "Events")
    bool CanExecute(UHorrorEventComponent* Event, FText& Error) const;
    UFUNCTION(BlueprintNativeEvent, Category = "Events")
    bool Execute(UHorrorEventComponent* Event, FText& Error) const;
    // Transient actions, such as sounds, intentionally have no restoration effect.
    UFUNCTION(BlueprintNativeEvent, Category = "Events")
    bool RestoreFinalState(UHorrorEventComponent* Event, FText& Error) const;
};

UCLASS(DisplayName = "Apply target state")
class HORROREVENTS_API UHorrorEventStateAction : public UHorrorEventAction
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") FName TargetId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") FName State;
    virtual bool ValidateData(FText& Error) const override;
    virtual bool CanExecute_Implementation(UHorrorEventComponent* Event, FText& Error) const override;
    virtual bool Execute_Implementation(UHorrorEventComponent* Event, FText& Error) const override;
    virtual bool RestoreFinalState_Implementation(UHorrorEventComponent* Event, FText& Error) const override;
};

USTRUCT(BlueprintType)
struct HORROREVENTS_API FHorrorEventStep
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event", meta = (ClampMin = "0")) float DelayBefore = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Event") TObjectPtr<UHorrorEventAction> Action;
};

UCLASS(BlueprintType)
class HORROREVENTS_API UHorrorEventDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") FName EventId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event", meta = (ClampMin = "1")) int32 Revision = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") bool bOnceOnly = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event", meta = (ClampMin = "0")) float CooldownSeconds = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Event") TArray<TObjectPtr<UHorrorEventCondition>> Conditions;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event") TArray<FHorrorEventStep> Steps;
    bool Validate(FText& Error) const;
    void GatherAssets(TArray<FSoftObjectPath>& Paths) const;
};
