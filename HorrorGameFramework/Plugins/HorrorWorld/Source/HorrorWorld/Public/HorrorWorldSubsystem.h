#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/SaveWorldProvider.h"
#include "HorrorWorldSubsystem.generated.h"
class UHorrorWorldDefinition;
class ULevelStreaming;
class ULevel;
class USaveSubsystem;

UENUM(BlueprintType)
enum class EHorrorAreaState : uint8 { Unloaded, Loading, Restoring, Ready, Unloading, Failed };
USTRUCT()
struct FHorrorAreaRuntime
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<ULevelStreaming> Streaming;
    EHorrorAreaState State = EHorrorAreaState::Unloaded;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHorrorAreaChanged, FName, AreaId, EHorrorAreaState, State);

UCLASS()
class HORRORWORLD_API UHorrorWorldSubsystem : public UWorldSubsystem, public ISaveWorldProvider
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Deinitialize() override;
    bool Configure(UHorrorWorldDefinition* InDefinition, FString& Error);
    UFUNCTION(BlueprintPure, Category="World") bool IsAreaReady(FName AreaId) const;
    UFUNCTION(BlueprintPure, Category="World") FName GetActiveArea() const { return ActiveAreaId; }
    UPROPERTY(BlueprintAssignable, Category="World") FHorrorAreaChanged OnAreaChanged;
    virtual bool IsSaveWorldReady() const override;
    virtual bool CaptureWorldMetadata(USystemSaveGame& Snapshot, FString& Error) const override;
    virtual void PrepareSaveWorld(const USystemSaveGame& Snapshot, FSaveWorldPrepared Completion) override;
    virtual bool IsSaveAreaResident(FName AreaId) const override;
    void RefreshActorBindings();
    virtual void RefreshSaveAreaBindings() override { RefreshActorBindings(); }
private:
    void UpdateStreaming(float DeltaTime = 0.f);
    void PumpOperations();
    void BeginLoad(FName AreaId);
    void BeginUnload(FName AreaId);
    void RestoreShownArea();
    void FinishArea(FName AreaId, bool bSuccess, FText Message);
    void SetAreaState(FName AreaId, EHorrorAreaState State);
    void CheckReady();
    FName FindActorArea(const AActor* Actor) const;
    USaveSubsystem* GetSaves() const;
    UFUNCTION() void HandleShown();
    UFUNCTION() void HandleUnloaded();
    UFUNCTION() void HandleFileLoaded(int32 SlotIndex, bool bSuccess, FText Message);
    UPROPERTY(Transient) TObjectPtr<UHorrorWorldDefinition> Definition;
    UPROPERTY(Transient) TMap<FName, FHorrorAreaRuntime> Areas;
    TSet<FName> DesiredAreas;
    TSet<FName> SnapshotResidentAreas;
    FName ActiveAreaId;
    FName OperatingAreaId;
    FVector RestoreAnchor = FVector::ZeroVector;
    FSaveWorldPrepared RestoreCompletion;
    FTSTicker::FDelegateHandle UpdateTicker;
    double LastUpdate = 0.0;
    double OperationDeadline = 0.0;
    bool bInitialReady = false;
    bool bResettingAreas = false;
    bool bPreparingRestore = false;
    bool bApplyingFile = false;
    bool bEnding = false;
};
