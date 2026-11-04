#include "ControlLookModifier.h"
#include "HorrorControlSettingsSubsystem.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
FVector UControlLookModifier::Transform(const FHorrorLookPreferences& Look, FVector Value)
{
    Value.X *= Look.SensitivityX * (Look.bInvertX ? -1.f : 1.f);
    Value.Y *= Look.SensitivityY * (Look.bInvertY ? -1.f : 1.f);
    return Value;
}
FInputActionValue UControlLookModifier::ModifyRaw_Implementation(const UEnhancedPlayerInput* Input, FInputActionValue Value, float DeltaTime)
{
    const APlayerController* PC = Input ? Input->GetOuterAPlayerController() : nullptr;
    const ULocalPlayer* Player = PC ? PC->GetLocalPlayer() : nullptr;
    const auto* Settings = Player ? Player->GetSubsystem<UHorrorControlSettingsSubsystem>() : nullptr;
    if (!Settings || !Settings->IsReady()) return Value;
    const auto& Preferences = Settings->GetPreferencesRef();
    return FInputActionValue(Value.GetValueType(), Transform(Device == EHorrorControlDevice::Gamepad ? Preferences.Gamepad : Preferences.Mouse, Value.Get<FVector>()));
}
