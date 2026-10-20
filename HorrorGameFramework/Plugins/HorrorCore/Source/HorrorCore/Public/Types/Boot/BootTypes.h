#pragma once

#include "CoreMinimal.h"
#include "BootTypes.generated.h"

UENUM(BlueprintType)
enum class EBootState : uint8
{
    None            UMETA(DisplayName = "None"),
    PreInit         UMETA(DisplayName = "Pre Init"),
    InitCore        UMETA(DisplayName = "Init Core Systems"),
    InitSystems     UMETA(DisplayName = "Init Systems"),
    LoadAssets      UMETA(DisplayName = "Load Primary Assets"),
    InitUI          UMETA(DisplayName = "Init UI"),
    Ready           UMETA(DisplayName = "Ready")
};
