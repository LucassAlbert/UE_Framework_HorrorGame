#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UIDataAsset.h"
#include "UIManagerSettings.generated.h"

UCLASS(Config = Game, defaultconfig, meta = (DisplayName = "Horror UI"))
class HORRORUI_API UUIManagerSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:

    UPROPERTY(Config, EditAnywhere, Category = "UI")
    TSoftObjectPtr<UUIDataAsset> UIConfigAsset;
};
