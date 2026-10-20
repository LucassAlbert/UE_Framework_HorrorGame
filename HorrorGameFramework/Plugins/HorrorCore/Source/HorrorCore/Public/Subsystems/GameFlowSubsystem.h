#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFlowSubsystem.generated.h"

UENUM(BlueprintType)
enum class EGameFlowState : uint8
{
	MainMenu,
	Loading,
	Playing,
	Paused,
	GameOver
};

UCLASS()
class HORRORCORE_API UGameFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadOnly)
	EGameFlowState CurrentState = EGameFlowState::MainMenu;

	void SetGameState(EGameFlowState NewState);

	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void RestartLevel();

	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void LoadLevel(FName LevelName);

	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void PauseGame();

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    UFUNCTION()
    void HandleScreenStackChanged(bool bHasModalScreen);
};
