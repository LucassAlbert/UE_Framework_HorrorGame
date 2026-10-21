#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/Boot/BootTypes.h"
#include "Subsystems/GameFlowSubsystem.h"
#include "BootManagerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBootStateChanged, EBootState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBootCompleted);

UCLASS()
class HORRORCORE_API UBootManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    void StartBoot();
    void SetupMenuCamera();
    void SetupInputUI();

    UPROPERTY(BlueprintAssignable)
    FOnBootStateChanged OnBootStateChanged;
    UPROPERTY(BlueprintAssignable)
    FOnBootCompleted OnBootCompleted;
    UPROPERTY(BlueprintReadWrite)
    bool FinishBoot = false;

private:
    void SetState(EBootState NewState);
    void HandleInitUI();
    void HandleWorldLoaded(UWorld* World);
    UFUNCTION()
    void HandleUIReady(bool bSuccess);

    UFUNCTION() void HandleSaveLoadStarted();
    UFUNCTION() void HandleSaveLoaded(int32 SlotIndex, bool bSuccess, FText Message);
    UFUNCTION() void HandleSaveWorldReady();
    void HoldPreparationPause();
    void ReleasePreparationPause();
    TWeakObjectPtr<UWorld> PreparationWorld;
    TWeakObjectPtr<UWorld> BeforeLoadWorld;
    EGameFlowState BeforeLoadState = EGameFlowState::MainMenu;
    bool bPreparationPauseOwned = false;
    EBootState CurrentState = EBootState::None;
    FDelegateHandle MapLoadedHandle;
    bool bUIReady = false;
};
