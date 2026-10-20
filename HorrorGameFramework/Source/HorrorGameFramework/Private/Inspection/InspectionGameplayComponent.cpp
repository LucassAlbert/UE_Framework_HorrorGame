#include "Inspection/InspectionGameplayComponent.h"
#include "Inspection/InspectionScreen.h"
#include "InspectionComponent.h"
#include "Inventory/InventoryGameplayComponent.h"
#include "InventoryItemDefinition.h"
#include "UIManagerSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UInspectionGameplayComponent::UInspectionGameplayComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    ScreenClass = UInspectionScreen::StaticClass();
}

void UInspectionGameplayComponent::BeginPlay()
{
    Super::BeginPlay();
    InventoryGameplay = GetOwner()->FindComponentByClass<UInventoryGameplayComponent>();
    if (bEnableInspection && InventoryGameplay.IsValid() && GetOwner()->FindComponentByClass<UInspectionComponent>())
        InventoryGameplay->OnInspectRequested.AddUniqueDynamic(this, &UInspectionGameplayComponent::InspectItem);
}

void UInspectionGameplayComponent::InspectItem(UInventoryItemDefinition* Item)
{
    APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    UUIManagerSubsystem* UI = PC ? PC->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
    UInspectionComponent* Inspection = GetOwner()->FindComponentByClass<UInspectionComponent>();
    if (!bEnableInspection || !UI || !Inspection || !Item || !Item->bCanInspect || !ScreenClass || !UI->GetTopScreen()) return;
    if (Screen.IsValid() && UI->GetTopScreen() == Screen.Get()) return;
    ParentScreen = UI->GetTopScreen();
    UInspectionScreen* NewScreen = Cast<UInspectionScreen>(UI->PushScreen(ScreenClass));
    if (!NewScreen) return;
    Screen = NewScreen;
    NewScreen->SetSource(Inspection, Item->DisplayName, Item->Description);
    Inspection->StartInspection(Item->Mesh, Item->InspectionRotation);
}

void UInspectionGameplayComponent::CloseScreens()
{
    if (Screen.IsValid()) Screen->CloseScreen();
    if (ParentScreen.IsValid()) ParentScreen->CloseScreen();
    Screen.Reset();
    ParentScreen.Reset();
    if (UInspectionComponent* Inspection = GetOwner()->FindComponentByClass<UInspectionComponent>())
        Inspection->CloseInspection();
}

void UInspectionGameplayComponent::OnSaveStateRestored() { CloseScreens(); }

void UInspectionGameplayComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (InventoryGameplay.IsValid())
        InventoryGameplay->OnInspectRequested.RemoveDynamic(this, &UInspectionGameplayComponent::InspectItem);
    CloseScreens();
    InventoryGameplay.Reset();
    Super::EndPlay(Reason);
}
