#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorFrameworkGamepadTest, "HorrorFramework.Consolidation.GamepadMappings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FHorrorFrameworkGamepadTest::RunTest(const FString& Parameters)
{
    const TCHAR* ContextPaths[] = {
        TEXT("/HorrorCore/Input/MainMappingContext.MainMappingContext"),
        TEXT("/Game/_Project/Inventory/Input/IMC_InventoryDemo.IMC_InventoryDemo")
    };
    for (const TCHAR* Path : ContextPaths)
    {
        const UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, Path);
        if (!TestNotNull(Path, Context)) continue;
        const auto Find = [Context](const FKey& Key, const TCHAR* ActionName) -> const FEnhancedActionKeyMapping*
        {
            return Context->GetMappings().FindByPredicate([&](const FEnhancedActionKeyMapping& Mapping)
            {
                return Mapping.Key == Key && Mapping.Action && Mapping.Action->GetFName() == FName(ActionName);
            });
        };
        TestNotNull(TEXT("Left stick moves"), Find(EKeys::Gamepad_Left2D, TEXT("IA_Move")));
        TestNotNull(TEXT("A interacts"), Find(EKeys::Gamepad_FaceButton_Bottom, TEXT("IA_Interect")));
        TestNotNull(TEXT("B crouches"), Find(EKeys::Gamepad_FaceButton_Right, TEXT("IA_Crouch")));
        TestNotNull(TEXT("Start pauses"), Find(EKeys::Gamepad_Special_Right, TEXT("IA_PauseGame")));
        const FEnhancedActionKeyMapping* Look = Find(EKeys::Gamepad_Right2D, TEXT("IA_Look"));
        if (!TestNotNull(TEXT("Right stick looks"), Look)) continue;
        const auto Apply = [Look](FVector2D Input, float DeltaTime)
        {
            FInputActionValue Value(Input);
            for (UInputModifier* Modifier : Look->Modifiers)
            {
                if (Modifier) Value = Modifier->ModifyRaw(nullptr, Value, DeltaTime);
            }
            return Value.Get<FVector2D>();
        };
        TestTrue(TEXT("Stick drift is filtered"), Apply(FVector2D(.05f, .05f), 1.f / 60.f).IsNearlyZero());
        const FVector2D Reference = Apply(FVector2D(.7f, .3f), 1.f);
        TestTrue(TEXT("Right and up stick have the intended camera signs"), Reference.X > 0.f && Reference.Y < 0.f);
        for (int32 FPS : {15, 30, 120})
        {
            FVector2D Total = FVector2D::ZeroVector;
            for (int32 Frame = 0; Frame < FPS; ++Frame) Total += Apply(FVector2D(.7f, .3f), 1.f / FPS);
            TestTrue(FString::Printf(TEXT("Look speed is stable at %d FPS"), FPS), Total.Equals(Reference, .001f));
        }
        if (FCString::Strstr(Path, TEXT("IMC_InventoryDemo")))
            TestNotNull(TEXT("View opens inventory"), Find(EKeys::Gamepad_Special_Left, TEXT("IA_Inventory")));
    }
    return !HasAnyErrors();
}
#endif
