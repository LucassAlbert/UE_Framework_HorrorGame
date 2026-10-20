#include "Core/HorrorGameMode.h"
#include "Core/HorrorPlayerController.h"


AHorrorGameMode::AHorrorGameMode()
{
	PlayerControllerClass = AHorrorPlayerController::StaticClass();
}
