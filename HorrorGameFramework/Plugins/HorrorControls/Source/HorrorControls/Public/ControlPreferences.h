#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "GameFramework/SaveGame.h"
#include "InputCoreTypes.h"
#include "ControlPreferences.generated.h"
class UInputAction;
class UInputMappingContext;

UENUM(BlueprintType)
enum class EHorrorControlDevice : uint8 { KeyboardMouse, Gamepad };

USTRUCT(BlueprintType)
struct HORRORCONTROLS_API FHorrorLookPreferences
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float SensitivityX = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float SensitivityY = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bInvertX = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bInvertY = false;
};

USTRUCT(BlueprintType)
struct HORRORCONTROLS_API FHorrorControlPreferences
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FHorrorLookPreferences Mouse;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FHorrorLookPreferences Gamepad;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float FieldOfView = 90.f;
    UPROPERTY(BlueprintReadOnly, SaveGame) TMap<FName, FKey> Bindings;
};

USTRUCT(BlueprintType)
struct HORRORCONTROLS_API FHorrorBindingDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UInputAction> Action;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FKey DefaultKey;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EHorrorControlDevice Device = EHorrorControlDevice::KeyboardMouse;
};

UCLASS(BlueprintType)
class HORRORCONTROLS_API UControlPreferencesDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ProfileId = TEXT("Default");
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FHorrorControlPreferences Defaults;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FHorrorBindingDefinition> Bindings;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UInputAction> LookAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float MinimumFOV = 70.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaximumFOV = 110.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float MinimumSensitivity = .1f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaximumSensitivity = 5.f;
    FHorrorControlPreferences MakeDefaults() const;
    bool Validate(FText& Error) const;
    bool ValidatePreferences(const FHorrorControlPreferences& Value, FText& Error) const;
    const FHorrorBindingDefinition* FindBinding(FName Id) const;
    static bool AcceptsKey(EHorrorControlDevice Device, FKey Key);
    bool Remap(FHorrorControlPreferences& Value, FName Id, FKey Key, FText& Message) const;
    UFUNCTION(BlueprintCallable, Category="Authoring") void ImportBindings(const TArray<UInputMappingContext*>& Contexts);
};

UCLASS(config=Game, defaultconfig, meta=(DisplayName="Horror Controls"))
class HORRORCONTROLS_API UHorrorControlsSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(config, EditAnywhere, Category="Defaults") TSoftObjectPtr<UControlPreferencesDefinition> Definition;
};

UCLASS()
class HORRORCONTROLS_API UControlPreferencesSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 Version = 1;
    UPROPERTY(SaveGame) FName ProfileId;
    UPROPERTY(SaveGame) FHorrorControlPreferences Preferences;
};
