#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InspectionView.h"
#include "InspectionComponent.generated.h"

class UStaticMesh;
class UWorld;
class UTextureRenderTarget2D;
class AInspectionPreviewActor;
struct FStreamableHandle;

UENUM(BlueprintType)
enum class EInspectionState : uint8 { Inactive, Loading, Ready, Failed };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInspectionStateChanged, EInspectionState, State, FText, Message);

UCLASS(ClassGroup = (Inspection), meta = (BlueprintSpawnableComponent))
class HORRORINSPECTION_API UInspectionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UInspectionComponent();
    UPROPERTY(BlueprintAssignable, Category = "Inspection") FOnInspectionStateChanged OnStateChanged;
    UFUNCTION(BlueprintCallable, Category = "Inspection")
    bool StartInspection(TSoftObjectPtr<UStaticMesh> Mesh, FRotator InitialRotation);
    UFUNCTION(BlueprintCallable, Category = "Inspection") void CloseInspection();
    UFUNCTION(BlueprintCallable, Category = "Inspection") void AdjustView(FVector2D Degrees, float ZoomSteps);
    UFUNCTION(BlueprintCallable, Category = "Inspection") void ResetView();
    // Call from presentation updates: captures only when the item/view has changed.
    UFUNCTION(BlueprintCallable, Category = "Inspection") void CaptureIfDirty();
    UFUNCTION(BlueprintPure, Category = "Inspection") EInspectionState GetState() const { return State; }
    UFUNCTION(BlueprintPure, Category = "Inspection") UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }
    const FInspectionView& GetView() const { return View; }
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void FinishLoading();
    bool CreatePreview(UStaticMesh* Mesh);
    void SetState(EInspectionState NewState, const FText& Message);
    void ReleasePreview();
    EInspectionState State = EInspectionState::Inactive;
    FInspectionView View;
    bool bCaptureDirty = false;
    uint64 RequestGeneration = 0;
    TSoftObjectPtr<UStaticMesh> RequestedMesh;
    TSharedPtr<FStreamableHandle> LoadHandle;
    UPROPERTY(Transient) TObjectPtr<UWorld> PreviewWorld;
    UPROPERTY(Transient) TObjectPtr<AInspectionPreviewActor> PreviewActor;
    UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> RenderTarget;
};
