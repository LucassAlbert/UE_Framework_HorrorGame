#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/ActorSaveData.h"
#include "SaveSlotCatalog.generated.h"

class USaveGame;
class USystemSaveGame;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSaveCatalogRefreshed);

UCLASS()
class SAVESYSTEM_API USaveSlotCatalog : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    UFUNCTION(BlueprintCallable, Category = "Save") bool Refresh();
    UFUNCTION(BlueprintPure, Category = "Save") bool IsReading() const { return bReading; }
    UFUNCTION(BlueprintPure, Category = "Save") TArray<FSaveSlotInfo> GetEntries() const { return Entries; }
    UFUNCTION(BlueprintPure, Category = "Save") int32 GetLatestSlot() const;
    UPROPERTY(BlueprintAssignable, Category = "Save") FSaveCatalogRefreshed OnRefreshed;
    static FSaveSlotInfo Describe(int32 SlotIndex, bool bExists, const USystemSaveGame* Snapshot);
private:
    void ReadNext(uint32 Request);
    void HandleRead(const FString& SlotName, int32 UserIndex, USaveGame* Snapshot, uint32 Request);
    UFUNCTION() void HandleSaved(int32 SlotIndex, bool bSuccess, FText Message);
    UPROPERTY(Transient) TArray<FSaveSlotInfo> Entries;
    TArray<FSaveSlotInfo> PendingEntries;
    uint32 Generation = 0;
    int32 ReadIndex = 0;
    bool bReading = false;
};
