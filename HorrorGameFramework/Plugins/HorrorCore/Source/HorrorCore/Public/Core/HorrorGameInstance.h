#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "HorrorGameInstance.generated.h"


UCLASS()
class HORRORCORE_API UHorrorGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	UFUNCTION()
	void OnBootFinished();

	void HandlePostBootFlow();
};
