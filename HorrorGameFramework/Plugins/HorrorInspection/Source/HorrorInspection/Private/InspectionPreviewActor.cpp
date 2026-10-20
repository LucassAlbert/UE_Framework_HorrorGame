#include "InspectionPreviewActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"

AInspectionPreviewActor::AInspectionPreviewActor()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
    Pivot->SetupAttachment(RootComponent);
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(Pivot);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetGenerateOverlapEvents(false);
    Mesh->SetCastShadow(false);
    Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
    Capture->SetupAttachment(RootComponent);
    Capture->FOVAngle = 35.f;
    Capture->CaptureSource = SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->ShowFlags.SetMotionBlur(false);
    Capture->ShowFlags.SetTemporalAA(false);
    Capture->ShowFlags.SetBloom(false);
    Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
    Capture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
    Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
    KeyLight->SetupAttachment(RootComponent);
    KeyLight->SetRelativeRotation(FRotator(-35.f, 25.f, 0.f));
    KeyLight->SetIntensity(4.f);
    KeyLight->SetCastShadows(false);
    FillLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("FillLight"));
    FillLight->SetupAttachment(RootComponent);
    FillLight->SetRelativeRotation(FRotator(25.f, -145.f, 0.f));
    FillLight->SetIntensity(1.5f);
    FillLight->SetCastShadows(false);
}
