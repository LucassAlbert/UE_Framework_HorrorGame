#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ControlPreferences.h"
#include "ControlLookModifier.h"
#include "InputAction.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorControlModelTest,"HorrorFramework.Controls.PreferencesModel",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorControlModelTest::RunTest(const FString& Parameters)
{
    auto* Definition=NewObject<UControlPreferencesDefinition>();
    auto* Look=NewObject<UInputAction>();auto* Move=NewObject<UInputAction>();auto* Interact=NewObject<UInputAction>();auto* Crouch=NewObject<UInputAction>();
    Definition->LookAction=Look;
    auto Binding=[&](const TCHAR* Id,UInputAction* Action,FKey Key,EHorrorControlDevice Device)
    {
        auto& Row=Definition->Bindings.AddDefaulted_GetRef();Row.Id=Id;Row.DisplayName=FText::FromString(Id);Row.Action=Action;Row.DefaultKey=Key;Row.Device=Device;
    };
    Binding(TEXT("Forward"),Move,EKeys::W,EHorrorControlDevice::KeyboardMouse);
    Binding(TEXT("Interact"),Interact,EKeys::E,EHorrorControlDevice::KeyboardMouse);
    Binding(TEXT("PadInteract"),Interact,EKeys::Gamepad_FaceButton_Bottom,EHorrorControlDevice::Gamepad);
    Binding(TEXT("PadCrouch"),Crouch,EKeys::Gamepad_FaceButton_Right,EHorrorControlDevice::Gamepad);
    FText Error;TestTrue(TEXT("Configuration is valid"),Definition->Validate(Error));
    auto Value=Definition->MakeDefaults();
    TestTrue(TEXT("Keyboard conflict swaps both assignments"),Definition->Remap(Value,TEXT("Forward"),EKeys::E,Error));
    TestTrue(TEXT("Swap retains the other binding"),Value.Bindings[TEXT("Forward")]==EKeys::E && Value.Bindings[TEXT("Interact")]==EKeys::W);
    TestTrue(TEXT("Gamepad B remains assignable independently of menu Back"),Definition->Remap(Value,TEXT("PadInteract"),EKeys::Gamepad_FaceButton_Right,Error));
    TestTrue(TEXT("Both gamepad assignments are exchanged"),Value.Bindings[TEXT("PadInteract")]==EKeys::Gamepad_FaceButton_Right && Value.Bindings[TEXT("PadCrouch")]==EKeys::Gamepad_FaceButton_Bottom);
    const auto Before=Value.Bindings;
    TestFalse(TEXT("Wrong device is rejected"),Definition->Remap(Value,TEXT("PadInteract"),EKeys::LeftMouseButton,Error));
    TestFalse(TEXT("Analog stick cannot replace a digital action"),Definition->Remap(Value,TEXT("PadInteract"),EKeys::Gamepad_Left2D,Error));
    TestTrue(TEXT("Rejected edits preserve every assignment"),Before.OrderIndependentCompareEqual(Value.Bindings));
    TestTrue(TEXT("Author defaults were not mutated"),Definition->MakeDefaults().Bindings[TEXT("Forward")]==EKeys::W);
    Value.FieldOfView=111;TestFalse(TEXT("Out-of-range FOV is rejected"),Definition->ValidatePreferences(Value,Error));
    Value.FieldOfView=90;Value.Mouse.SensitivityX=std::numeric_limits<float>::quiet_NaN();TestFalse(TEXT("Non-finite sensitivity is rejected"),Definition->ValidatePreferences(Value,Error));
    Value=Definition->MakeDefaults();Value.Bindings[TEXT("Forward")]=EKeys::E;TestFalse(TEXT("A duplicated command cannot be persisted"),Definition->ValidatePreferences(Value,Error));
    FHorrorLookPreferences Mouse;Mouse.SensitivityX=2;Mouse.SensitivityY=.5f;Mouse.bInvertY=true;
    TestTrue(TEXT("Sensitivity and inversion act independently on each axis"),UControlLookModifier::Transform(Mouse,FVector(3,4,0)).Equals(FVector(6,-2,0)));
    FHorrorLookPreferences Pad;Pad.SensitivityX=.25f;Pad.bInvertX=true;
    TestTrue(TEXT("Controller configuration does not reuse mouse values"),UControlLookModifier::Transform(Pad,FVector(4,3,0)).Equals(FVector(-1,3,0)));
    return !HasAnyErrors();
}
#endif
