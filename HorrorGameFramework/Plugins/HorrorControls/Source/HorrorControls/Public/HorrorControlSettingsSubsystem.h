#pragma once
#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ControlPreferences.h"
#include "HorrorControlSettingsSubsystem.generated.h"
class UInputMappingContext;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnControlPreferencesChanged);

USTRUCT()
struct FHorrorActiveInputContext
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UInputMappingContext> Source;
    UPROPERTY() TObjectPtr<UInputMappingContext> Runtime;
    int32 Priority = 0;
};

UCLASS()
class HORRORCONTROLS_API UHorrorControlSettingsSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    bool IsReady() const { return Definition != nullptr; }
    const UControlPreferencesDefinition* GetDefinition() const { return Definition; }
    UFUNCTION(BlueprintPure) FHorrorControlPreferences GetPreferences() const { return Current; }
    const FHorrorControlPreferences& GetPreferencesRef() const { return Current; }
    UFUNCTION(BlueprintPure) FText GetLoadError() const { return LoadError; }
    UPROPERTY(BlueprintAssignable) FOnControlPreferencesChanged OnPreferencesChanged;
    bool BeginEdit(UObject* Owner);
    bool Preview(UObject* Owner, const FHorrorControlPreferences& Preferences, FText& Error);
    bool Remap(UObject* Owner, FName Id, FKey Key, FText& Message);
    bool RestoreDefaults(UObject* Owner);
    bool Apply(UObject* Owner, FText& Error);
    void Cancel(UObject* Owner);
    void AddGameplayContext(UInputMappingContext* Context, int32 Priority);
    void RemoveGameplayContext(UInputMappingContext* Context);
    UInputMappingContext* GetRuntimeContext(UInputMappingContext* Source) const;
    FString GetStorageSlot() const;
private:
    void RebuildContexts();
    void Notify(bool bRebuild = true);
    bool OwnsEdit(UObject* Owner) const { return IsValid(Owner) && EditOwner.Get() == Owner; }
    UPROPERTY(Transient) TObjectPtr<UControlPreferencesDefinition> Definition;
    UPROPERTY(Transient) TArray<FHorrorActiveInputContext> Contexts;
    FHorrorControlPreferences Current, Applied;
    TWeakObjectPtr<UObject> EditOwner;
    bool bHasEdit = false;
    FText LoadError;
};
