#include "Subsystems/SaveSlotCatalog.h"
#include "Subsystems/SaveSubsystem.h"
#include "Save/SystemSaveGame.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

void USaveSlotCatalog::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<USaveSubsystem>();
    for (int32 Index = 0; Index < USaveSubsystem::SlotCount; ++Index) Entries.Add(Describe(Index, false, nullptr));
    GetGameInstance()->GetSubsystem<USaveSubsystem>()->OnSaveCompleted.AddUniqueDynamic(this, &USaveSlotCatalog::HandleSaved);
}
void USaveSlotCatalog::Deinitialize()
{
    ++Generation;
    bReading = false;
    if (USaveSubsystem* Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>())
        Saves->OnSaveCompleted.RemoveDynamic(this, &USaveSlotCatalog::HandleSaved);
    Super::Deinitialize();
}
FSaveSlotInfo USaveSlotCatalog::Describe(int32 SlotIndex, bool bExists, const USystemSaveGame* Snapshot)
{
    FSaveSlotInfo Info;
    Info.SlotIndex = SlotIndex;
    Info.bExists = bExists;
    if (!bExists) return Info;
    Info.bReadable = Snapshot && Snapshot->FormatVersion >= 1 && Snapshot->FormatVersion <= 3 && !Snapshot->CurrentLevel.IsEmpty();
    if (Info.bReadable)
    {
        Info.SavedAt = Snapshot->SavedAt;
        Info.MapName = Snapshot->CurrentLevel;
    }
    else Info.Error = NSLOCTEXT("HorrorSave", "InvalidSlot", "Save inválido ou de uma versão não compatível.");
    return Info;
}
bool USaveSlotCatalog::Refresh()
{
    if (bReading) return false;
    bReading = true;
    ReadIndex = 0;
    PendingEntries.Reset();
    ReadNext(++Generation);
    return true;
}
void USaveSlotCatalog::ReadNext(uint32 Request)
{
    if (!bReading || Request != Generation) return;
    while (ReadIndex < USaveSubsystem::SlotCount)
    {
        const FString Name = USaveSubsystem::GetSlotName(ReadIndex);
        if (UGameplayStatics::DoesSaveGameExist(Name, 0))
        {
            UGameplayStatics::AsyncLoadGameFromSlot(Name, 0,
                FAsyncLoadGameFromSlotDelegate::CreateUObject(this, &USaveSlotCatalog::HandleRead, Request));
            return;
        }
        PendingEntries.Add(Describe(ReadIndex++, false, nullptr));
    }
    Entries = MoveTemp(PendingEntries);
    bReading = false;
    OnRefreshed.Broadcast();
}
void USaveSlotCatalog::HandleRead(const FString& SlotName, int32 UserIndex, USaveGame* Snapshot, uint32 Request)
{
    if (!bReading || Request != Generation) return;
    PendingEntries.Add(Describe(ReadIndex++, true, Cast<USystemSaveGame>(Snapshot)));
    ReadNext(Request);
}
int32 USaveSlotCatalog::GetLatestSlot() const
{
    const FSaveSlotInfo* Latest = nullptr;
    for (const FSaveSlotInfo& Info : Entries)
        if (Info.bReadable && (!Latest || Info.SavedAt > Latest->SavedAt)) Latest = &Info;
    return Latest ? Latest->SlotIndex : INDEX_NONE;
}
void USaveSlotCatalog::HandleSaved(int32 SlotIndex, bool bSuccess, FText Message)
{
    // A pending refresh may have read this slot before the write. Invalidate its callback.
    if (!bSuccess) return;
    ++Generation;
    bReading = false;
    Refresh();
}
