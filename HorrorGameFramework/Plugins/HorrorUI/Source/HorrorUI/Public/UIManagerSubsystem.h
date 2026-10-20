#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UIScreenBase.h"
#include "UIDataAsset.h"
#include "UIManagerSubsystem.generated.h"

struct FStreamableHandle;
class APlayerController;

UENUM(BlueprintType)
enum class EHorrorScreen : uint8
{
    MainMenu, Settings, Pause, Inventory, Inspection, Documents, Objectives, SaveSlots, GameOver, AudioSettings
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUIReady, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScreenStackChanged, bool, bHasModalScreen);

UCLASS()
class HORRORUI_API UUIManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    void InitializeUI();
    void OnUIConfigLoaded();

    UFUNCTION(BlueprintCallable, Category = "Open UI")
    void OpenMainMenuUI();
    UFUNCTION(BlueprintCallable, Category = "Open UI")
    void OpenSettingsUI();
    UFUNCTION(BlueprintCallable, Category = "Open UI")
    void OpenAudioSettingsUI();
    UFUNCTION(BlueprintCallable, Category = "Open UI")
    void OpenGameplayUI();
    UFUNCTION(BlueprintCallable, Category = "Open UI")
    void OpenPauseUI();

    UFUNCTION(BlueprintCallable, Category = "Open UI")
    UUIScreenBase* OpenScreen(EHorrorScreen ScreenType);

    UFUNCTION(BlueprintCallable, Category = "UI")
    UUIScreenBase* PushScreen(TSubclassOf<UUIScreenBase> ScreenClass);
    UFUNCTION(BlueprintCallable, Category = "UI")
    void PopScreen();
    void RemoveScreen(UUIScreenBase* Screen);
    void ReleasePlayerPresentation(APlayerController* Controller);
    UFUNCTION(BlueprintCallable, Category = "UI")
    void ClearScreens();
    UFUNCTION(BlueprintPure, Category = "UI")
    UUIScreenBase* GetTopScreen() const;
    UFUNCTION(BlueprintPure, Category = "UI")
    bool HasModalScreen() const { return ScreenStack.Num() > 0; }
    bool DoesScreenStackPauseWorld() const;
    bool IsReady() const { return bReady; }

    UPROPERTY(BlueprintAssignable, Category = "UI")
    FOnUIReady OnUIReady;
    UPROPERTY(BlueprintAssignable, Category = "UI")
    FOnScreenStackChanged OnScreenStackChanged;

    UPROPERTY(EditDefaultsOnly, Category = "UI Settings")
    TSoftObjectPtr<UUIDataAsset> UIConfigAssetRef;

    UPROPERTY(Transient)
    TObjectPtr<UUIDataAsset> UIConfigAsset;

private:
    friend class FHorrorUIControlOwnershipTest;
    friend class FHorrorFrameworkLifecycleTest;
    void ApplyScreenState();
    void ReleaseControlState();
    void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
    TSubclassOf<UUIScreenBase> GetScreenClass(EHorrorScreen ScreenType) const;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UUIScreenBase>> ScreenStack;
    UPROPERTY(Transient)
    TObjectPtr<UUIScreenBase> GameplayScreen;

    TSharedPtr<FStreamableHandle> ConfigLoadHandle;
    FDelegateHandle WorldCleanupHandle;
    bool bReady = false;
    bool bInitializing = false;
    bool bPauseOwned = false;
    bool bInputOwned = false;
    TWeakObjectPtr<APlayerController> ControlOwner;
};
