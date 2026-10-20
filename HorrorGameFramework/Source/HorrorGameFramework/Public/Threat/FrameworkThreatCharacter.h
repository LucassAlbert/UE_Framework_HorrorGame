#pragma once
#include "Inventory/InventoryCharacter.h"
#include "FrameworkThreatCharacter.generated.h"

class UHidingParticipantComponent;
class UFrameworkThreatTarget;

UCLASS()
class HORRORGAMEFRAMEWORK_API AFrameworkThreatCharacter : public AInventoryCharacter
{
    GENERATED_BODY()
public:
    AFrameworkThreatCharacter();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Threat") TObjectPtr<UHidingParticipantComponent> Hiding;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Threat") TObjectPtr<UFrameworkThreatTarget> ThreatTarget;
};
