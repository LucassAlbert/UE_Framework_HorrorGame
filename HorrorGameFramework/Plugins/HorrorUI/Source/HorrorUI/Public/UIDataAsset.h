#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Blueprint/UserWidget.h"
#include "UIDataAsset.generated.h"


UCLASS(BlueprintType)
class HORRORUI_API UUIDataAsset : public UDataAsset
{
	GENERATED_BODY()

    public:

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<class UUIScreenBase> MainMenu;
    //TSubclassOf<UUserWidget> MainMenu;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<class UUIScreenBase> Settings;
    //TSubclassOf<UUserWidget> Settings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<class UUIScreenBase> GameplayHUD;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<class UUIScreenBase> PauseGame;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<class UUIScreenBase> Inventory;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<class UUIScreenBase> Inspection;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<class UUIScreenBase> Documents;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<class UUIScreenBase> Objectives;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<class UUIScreenBase> SaveSlots;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<class UUIScreenBase> GameOver;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<class UUIScreenBase> AudioSettings;

    // Explicit prototype mode: missing screens use a labelled native placeholder.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    bool bEnablePlaceholderScreens = true;
   // TSubclassOf<UUserWidget> GameplayHUD;

	
};
