#include "Core/HorrorGameInstance.h"
#include "Subsystems/Boot/BootManagerSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogHorrorGameInstance, Log, All);

void UHorrorGameInstance::Init()
{
    Super::Init();
    if (UBootManagerSubsystem* Boot = GetSubsystem<UBootManagerSubsystem>())
    {
        Boot->OnBootCompleted.AddUniqueDynamic(this, &UHorrorGameInstance::OnBootFinished);
        Boot->StartBoot();
    }
    else UE_LOG(LogHorrorGameInstance, Error, TEXT("BootManagerSubsystem is unavailable."));
}

void UHorrorGameInstance::OnBootFinished()
{
    if (UBootManagerSubsystem* Boot = GetSubsystem<UBootManagerSubsystem>())
        Boot->OnBootCompleted.RemoveDynamic(this, &UHorrorGameInstance::OnBootFinished);
    HandlePostBootFlow();
}

void UHorrorGameInstance::HandlePostBootFlow()
{
    UE_LOG(LogHorrorGameInstance, Log, TEXT("Framework initialization completed."));
}
