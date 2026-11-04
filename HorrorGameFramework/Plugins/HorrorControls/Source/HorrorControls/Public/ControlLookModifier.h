#pragma once
#include "CoreMinimal.h"
#include "InputModifiers.h"
#include "ControlPreferences.h"
#include "ControlLookModifier.generated.h"
UCLASS()
class HORRORCONTROLS_API UControlLookModifier : public UInputModifier
{
    GENERATED_BODY()
public:
    UPROPERTY() EHorrorControlDevice Device = EHorrorControlDevice::KeyboardMouse;
    virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime) override;
    static FVector Transform(const FHorrorLookPreferences& Preferences, FVector Value);
};
