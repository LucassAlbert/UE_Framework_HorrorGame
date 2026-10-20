#if WITH_DEV_AUTOMATION_TESTS

#include "InspectionView.h"
#include "InspectionComponent.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/WorldInitializationValues.h"
#include "GameFramework/Actor.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorInspectionViewTest, "HorrorFramework.Inspection.View",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHorrorInspectionViewTest::RunTest(const FString& Parameters)
{
    FInspectionView View;
    const FRotator Initial(15.f, 35.f, -10.f);
    View.Reset(Initial);
    View.Adjust(FVector2D(180.f, 180.f), 1.f);
    TestFalse(TEXT("Can rotate beyond front-facing hemisphere"), View.Rotation.Equals(Initial.Quaternion()));
    for (int32 Index = 0; Index < 2000; ++Index) View.Adjust(FVector2D(13.f, -7.f), 0.f);
    TestTrue(TEXT("Repeated rotations stay normalized"), FMath::IsNearlyEqual(View.Rotation.SizeSquared(), 1.f));
    View.Adjust(FVector2D::ZeroVector, 10000.f);
    TestEqual(TEXT("Zoom cannot cross the item"), View.Zoom, 2.f);
    View.Adjust(FVector2D::ZeroVector, -10000.f);
    TestEqual(TEXT("Zoom cannot lose the item indefinitely"), View.Zoom, 0.5f);
    const FQuat Before = View.Rotation;
    TestFalse(TEXT("Nonfinite input is rejected"), View.Adjust(FVector2D(std::numeric_limits<double>::infinity(), 0.f), 0.f));
    TestTrue(TEXT("Rejected input does not change rotation"), View.Rotation.Equals(Before));
    View.Reset(Initial);
    TestTrue(TEXT("Reset restores authored orientation"), View.Rotation.Equals(Initial.Quaternion()));
    TestEqual(TEXT("Reset restores zoom"), View.Zoom, 1.f);
    FVector Offset;
    float Scale = 0.f;
    const FBoxSphereBounds Bounds(FVector(200.f, -70.f, 15.f), FVector(10.f), 20.f);
    TestTrue(TEXT("Off-center mesh can be framed"), FInspectionView::NormalizeBounds(Bounds, Offset, Scale));
    TestTrue(TEXT("Authored pivot is centered without changing asset"), (Bounds.Origin * Scale + Offset).IsNearlyZero());
    TestTrue(TEXT("Size is normalized independently of world scale"), FMath::IsNearlyEqual(Bounds.SphereRadius * Scale, 100.f));
    TestFalse(TEXT("Degenerate mesh is rejected"), FInspectionView::NormalizeBounds(FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.f), Offset, Scale));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorInspectionLifecycleTest, "HorrorFramework.Inspection.Lifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHorrorInspectionLifecycleTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    TStrongObjectPtr<UWorld> Host(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false,
        ERHIFeatureLevel::SM5, &Values));
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(Host.Get());
    AActor* Owner = Host->SpawnActor<AActor>();
    UInspectionComponent* Component = NewObject<UInspectionComponent>(Owner);
    Owner->AddInstanceComponent(Component);
    Component->RegisterComponent();
    TestFalse(TEXT("Null mesh request fails without a preview"), Component->StartInspection(nullptr, FRotator::ZeroRotator));
    TestEqual(TEXT("Failure is explicit"), Component->GetState(), EInspectionState::Failed);
    Component->CloseInspection();
    TestNull(TEXT("Close after failure releases render target"), Component->GetRenderTarget());
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (TestNotNull(TEXT("Test mesh available"), Mesh))
    {
        for (int32 Index = 0; Index < 3; ++Index)
        {
            TestTrue(TEXT("Preview opens repeatedly"), Component->StartInspection(Mesh, FRotator(0.f, 30.f, 0.f)));
            TestEqual(TEXT("Loaded mesh is ready"), Component->GetState(), EInspectionState::Ready);
            TestNotNull(TEXT("Ready preview has a render target"), Component->GetRenderTarget());
            Component->AdjustView(FVector2D(50.f, 70.f), 2.f);
            Component->CaptureIfDirty();
            Component->CloseInspection();
            Component->CloseInspection();
            TestEqual(TEXT("Repeated close is safe"), Component->GetState(), EInspectionState::Inactive);
            TestNull(TEXT("Closing releases rendering resource ownership"), Component->GetRenderTarget());
        }
    }
    Owner->Destroy();
    Host->DestroyWorld(false);
    GEngine->DestroyWorldContext(Host.Get());
    return true;
}
#endif
