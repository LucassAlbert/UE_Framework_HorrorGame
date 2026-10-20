#include "InspectionComponent.h"
#include "InspectionPreviewActor.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Engine/WorldInitializationValues.h"
#include "PixelFormat.h"

DEFINE_LOG_CATEGORY_STATIC(LogHorrorInspection, Log, All);

UInspectionComponent::UInspectionComponent() { PrimaryComponentTick.bCanEverTick = false; }

bool UInspectionComponent::StartInspection(TSoftObjectPtr<UStaticMesh> Mesh, FRotator InitialRotation)
{
    CloseInspection();
    if (Mesh.IsNull() || InitialRotation.ContainsNaN() || !GetWorld())
    {
        SetState(EInspectionState::Failed, NSLOCTEXT("Inspection", "InvalidItem", "Este item não possui uma apresentação válida para inspeção."));
        return false;
    }
    RequestedMesh = Mesh;
    View.Reset(InitialRotation);
    SetState(EInspectionState::Loading, NSLOCTEXT("Inspection", "Loading", "Carregando item…"));
    if (Mesh.Get()) FinishLoading();
    else
    {
        const uint64 Generation = RequestGeneration;
        LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Mesh.ToSoftObjectPath(),
            FStreamableDelegate::CreateWeakLambda(this, [this, Generation]()
            { if (Generation == RequestGeneration) FinishLoading(); }));
        if (!LoadHandle)
            SetState(EInspectionState::Failed, NSLOCTEXT("Inspection", "LoadStartFailed", "Não foi possível iniciar o carregamento do item."));
    }
    return State != EInspectionState::Failed;
}

void UInspectionComponent::SetState(EInspectionState NewState, const FText& Message)
{
    State = NewState;
    OnStateChanged.Broadcast(State, Message);
}

void UInspectionComponent::FinishLoading()
{
    if (State != EInspectionState::Loading) return;
    UStaticMesh* Mesh = RequestedMesh.Get();
    if (!Mesh || !CreatePreview(Mesh))
    {
        ReleasePreview();
        UE_LOG(LogHorrorInspection, Error, TEXT("Cannot create inspection preview for %s."), *RequestedMesh.ToString());
        SetState(EInspectionState::Failed, NSLOCTEXT("Inspection", "LoadFailed", "Não foi possível carregar a apresentação do item."));
        return;
    }
    SetState(EInspectionState::Ready, FText::GetEmpty());
    bCaptureDirty = true;
    CaptureIfDirty();
}

bool UInspectionComponent::CreatePreview(UStaticMesh* Mesh)
{
    FVector Offset;
    float Scale;
    if (!FInspectionView::NormalizeBounds(Mesh->GetBounds(), Offset, Scale)) return false;
    // A private runtime scene avoids gameplay lighting, collision, streaming and save registration.
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
        .CreateAISystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(false).SetTransactional(false);
    PreviewWorld = UWorld::CreateWorld(EWorldType::GamePreview, false, NAME_None, nullptr, false,
        GetWorld()->GetFeatureLevel(), &Values);
    if (!PreviewWorld) return false;
    FActorSpawnParameters Parameters;
    Parameters.ObjectFlags |= RF_Transient;
    PreviewActor = PreviewWorld->SpawnActor<AInspectionPreviewActor>(Parameters);
    if (!PreviewActor) return false;
    PreviewActor->Mesh->SetStaticMesh(Mesh);
    PreviewActor->Mesh->SetRelativeScale3D(FVector(Scale));
    PreviewActor->Mesh->SetRelativeLocation(Offset);
    RenderTarget = NewObject<UTextureRenderTarget2D>(this);
    RenderTarget->ClearColor = FLinearColor::Black;
    RenderTarget->InitCustomFormat(768, 768, PF_B8G8R8A8, false);
    PreviewActor->Capture->TextureTarget = RenderTarget;
    return true;
}

void UInspectionComponent::AdjustView(FVector2D Degrees, float ZoomSteps)
{
    if (State == EInspectionState::Ready && View.Adjust(Degrees, ZoomSteps)) bCaptureDirty = true;
}

void UInspectionComponent::ResetView()
{
    if (State != EInspectionState::Ready) return;
    View.Reset(View.InitialRotation.Rotator());
    bCaptureDirty = true;
}

void UInspectionComponent::CaptureIfDirty()
{
    if (!bCaptureDirty || State != EInspectionState::Ready || !IsValid(PreviewActor)) return;
    PreviewActor->Pivot->SetRelativeRotation(View.Rotation);
    const float Distance = 110.f / FMath::Sin(FMath::DegreesToRadians(PreviewActor->Capture->FOVAngle * 0.5f));
    PreviewActor->Capture->SetRelativeLocation(FVector(-Distance / View.Zoom, 0.f, 0.f));
    PreviewActor->Capture->CaptureScene();
    bCaptureDirty = false;
}

void UInspectionComponent::ReleasePreview()
{
    if (PreviewActor) PreviewActor->Capture->TextureTarget = nullptr;
    PreviewActor = nullptr;
    if (PreviewWorld) PreviewWorld->DestroyWorld(false);
    PreviewWorld = nullptr;
    RenderTarget = nullptr;
    bCaptureDirty = false;
}

void UInspectionComponent::CloseInspection()
{
    ++RequestGeneration;
    if (LoadHandle) LoadHandle->CancelHandle();
    LoadHandle.Reset();
    RequestedMesh.Reset();
    ReleasePreview();
    if (State != EInspectionState::Inactive) SetState(EInspectionState::Inactive, FText::GetEmpty());
}

void UInspectionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    CloseInspection();
    Super::EndPlay(Reason);
}
