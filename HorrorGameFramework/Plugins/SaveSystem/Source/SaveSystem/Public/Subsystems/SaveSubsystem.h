#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Registry/ActorRegistry.h"
#include "Save/ActorSaveData.h"
#include "SaveSubsystem.generated.h"

class USystemSaveGame;
class USaveGame;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSaveOperationCompleted, int32, SlotIndex, bool, bSuccess, FText, Message);

UCLASS()
class SAVESYSTEM_API USaveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    static constexpr int32 SlotCount = 5;
    static bool IsValidSlot(int32 SlotIndex) { return SlotIndex >= 0 && SlotIndex < SlotCount; }
    static FString GetSlotName(int32 SlotIndex);

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Legacy Blueprint entry points retain slot zero.
    UFUNCTION(BlueprintCallable, Category = "Save System")
    void SaveGame();
    UFUNCTION(BlueprintCallable, Category = "Save System")
    void LoadGame();

    // Return value means the operation started, not that disk I/O succeeded.
    UFUNCTION(BlueprintCallable, Category = "Save System")
    bool SaveToSlot(int32 SlotIndex);
    UFUNCTION(BlueprintCallable, Category = "Save System")
    bool LoadFromSlot(int32 SlotIndex);
    UFUNCTION(BlueprintCallable, Category = "Save System")
    bool LoadLatestForCurrentLevel();
    UFUNCTION(BlueprintPure, Category = "Save System")
    bool IsBusy() const { return bBusy; }
    bool IsRestoring() const { return bRestoring; }
    UFUNCTION(BlueprintCallable, Category = "Save System")
    TArray<FSaveSlotInfo> GetSlots() const;

    UPROPERTY(BlueprintAssignable, Category = "Save System")
    FOnSaveOperationCompleted OnSaveCompleted;
    UPROPERTY(BlueprintAssignable, Category = "Save System")
    FOnSaveOperationCompleted OnLoadCompleted;

    UPROPERTY(Transient)
    TObjectPtr<USystemSaveGame> CurrentSaveGame;
    UPROPERTY(Transient)
    TObjectPtr<UActorRegistry> ActorRegistry;

    void RecordDestroyedActor(const FGuid& ActorID);

private:
    friend class FHorrorSaveRoundTripTest;
    friend class FHorrorRuntimeActorSaveTest;
    friend class FHorrorInventoryWorldSaveTest;
    friend class FHorrorInventoryColdRestartTest;
    friend class FHorrorProgressionWorldSaveTest;
    friend class FHorrorDocumentWorldSaveTest;
    friend class FHorrorDocumentColdRestartTest;
    friend class FHorrorDialogueWorldSaveTest;
    friend class FHorrorDialogueColdRestartTest;
    friend class FHorrorThreatWorldSaveTest;
    bool CaptureWorld(USystemSaveGame* Snapshot, FString& Error);
    bool RestoreWorld(const USystemSaveGame* Snapshot, FString& Error);
    void OnSlotSaved(const FString& SlotName, int32 UserIndex, bool bSuccess);
    void OnSlotLoaded(const FString& SlotName, int32 UserIndex, USaveGame* LoadedGame);
    void FinishSlotRestore();
    void ScanNextLatestSlot();
    void OnLatestSlotRead(const FString& SlotName, int32 UserIndex, USaveGame* LoadedGame);
    void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

    UPROPERTY(Transient)
    TObjectPtr<USystemSaveGame> PendingSaveGame;
    UPROPERTY(Transient) TObjectPtr<USystemSaveGame> LatestCandidate;
    int32 LatestScanIndex = 0;
    int32 LatestCandidateSlot = INDEX_NONE;
    bool bScanningLatest = false;
    TSharedPtr<FStreamableHandle> RestoreLoadHandle;
    TSet<FGuid> DestroyedActors;
    TWeakObjectPtr<UWorld> OperationWorld;
    FDelegateHandle WorldCleanupHandle;
    int32 ActiveSlot = INDEX_NONE;
    bool bBusy = false;
    bool bRestoring = false;
};
