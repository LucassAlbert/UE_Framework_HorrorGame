#include "Threat/FrameworkThreatCharacter.h"
#include "Threat/FrameworkThreatTarget.h"
#include "HidingParticipantComponent.h"

AFrameworkThreatCharacter::AFrameworkThreatCharacter()
{
    Hiding = CreateDefaultSubobject<UHidingParticipantComponent>(TEXT("Hiding"));
    ThreatTarget = CreateDefaultSubobject<UFrameworkThreatTarget>(TEXT("ThreatTarget"));
}
