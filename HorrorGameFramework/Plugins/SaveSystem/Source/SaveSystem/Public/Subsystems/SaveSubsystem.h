#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Registry/ActorRegistry.h"
#include "Interfaces/SaveWorldProvider.h"
#include "Save/ActorSaveData.h"
#include "SaveSubsystem.generated.h"

class USystemSaveGame;
class USaveGame;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSaveWorldEvent);
DECLARE_DELEGATE_TwoParams(FSaveAreaRestored, bool, FText);

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
    UFUNCTION(BlueprintCallable, Category = "Save System") bool LoadLatest();
    bool RegisterWorldProvider(UObject* Provider);
    void UnregisterWorldProvider(UObject* Provider);
    bool IsWorldReady() const;
    bool HasPendingLoad() const { return bBusy && bLoadingSlot; }
    void NotifyWorldReady();
    bool CacheStreamingArea(FName AreaId, FString& Error);
    bool RestoreStreamingArea(FName AreaId, FSaveAreaRestored Completion);
    void EvictStreamingRuntimeActors(FName AreaId);
    UPROPERTY(BlueprintAssignable, Category = "Save System") FSaveWorldEvent OnLoadStarted;
    UPROPERTY(BlueprintAssignable, Category = "Save System") FSaveWorldEvent OnWorldReady;
    UPROPERTY(BlueprintReadOnly, Transient, Category = "Save System") FText LastLoadError;
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
    friend class FHorrorSaveAreaCacheTest;
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
    bool CaptureWorld(USystemSaveGame* Snapshot, FString& Error, FName AreaId = NAME_None, bool bOnlyArea = false);
    bool RestoreWorld(const USystemSaveGame* Snapshot, FString& Error, FName AreaId = NAME_None, bool bOnlyArea = false);
    void OnSlotSaved(const FString& SlotName, int32 UserIndex, bool bSuccess);
    void OnSlotLoaded(const FString& SlotName, int32 UserIndex, USaveGame* LoadedGame, uint32 Request);
    void FinishSlotRestore();
    bool StartLatestScan(bool bCurrentMapOnly);
    void PreparePendingWorld(uint32 Request);
    void LoadResidentDependencies(uint32 Request);
    void HandleMapLoaded(UWorld* World);
    void CompleteLoad(bool bSuccess, const FText& Message);
    void BeginLoadDeadline();
    bool IsRecordResident(const FActorSaveData& Data) const;
    void ScanNextLatestSlot();
    void OnLatestSlotRead(const FString& SlotName, int32 UserIndex, USaveGame* LoadedGame, uint32 Request);
    void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

    UPROPERTY(Transient)
    TObjectPtr<USystemSaveGame> PendingSaveGame;
    UPROPERTY(Transient) TObjectPtr<USystemSaveGame> LatestCandidate;
    UPROPERTY(Transient) TMap<FGuid, FActorSaveData> CachedAreaActors;
    TSet<FName> VisitedAreaIds;
    TWeakObjectPtr<UObject> WorldProvider;
    FSaveAreaRestored AreaRestoreCompletion;
    FName RestoringAreaId;
    FDelegateHandle MapLoadedHandle;
    FTSTicker::FDelegateHandle LoadDeadlineHandle;
    uint32 LoadGeneration = 0;
    double LoadDeadline = 0.0;
    bool bCurrentMapOnly = true;
    bool bLoadingSlot = false;
    bool bAwaitingMap = false;
    bool bPreparingWorld = false;
    bool bRestoringArea = false;
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
