#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HorrorEnemyDefinition.generated.h"

UENUM(BlueprintType)
enum class EHorrorEnemyState : uint8 { Patrol, Investigate, Chase, Search, Captured, Disabled };

USTRUCT(BlueprintType)
struct HORRORAI_API FHorrorEnemyMemory
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly) EHorrorEnemyState State = EHorrorEnemyState::Patrol;
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly) int32 PatrolIndex = 0;
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly) FVector LastKnownLocation = FVector::ZeroVector;
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly) bool bHasLastKnownLocation = false;
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly) float RemainingTime = 0.f;
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly) bool bPatrolWaiting = false;
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly) FName WitnessedConcealmentKey;
    UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly) FVector WitnessedEntry = FVector::ZeroVector;
    bool Validate() const;
};

UCLASS(BlueprintType)
class HORRORAI_API UHorrorEnemyDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "1")) float PatrolSpeed = 160.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "1")) float InvestigateSpeed = 220.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "1")) float ChaseSpeed = 420.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception", meta = (ClampMin = "1")) float SightRadius = 1400.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception", meta = (ClampMin = "1")) float LoseSightRadius = 1700.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception", meta = (ClampMin = "1", ClampMax = "179")) float SightHalfAngle = 65.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception", meta = (ClampMin = "1")) float HearingRange = 1800.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.05", ClampMax = "1")) float UpdateInterval = .15f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0")) float PatrolWaitSeconds = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.1")) float SearchSeconds = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "1")) float MoveTimeoutSeconds = 20.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Search", meta = (ClampMin = "0")) float SearchRadius = 250.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "1")) float ArrivalRadius = 70.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Capture", meta = (ClampMin = "1")) float CaptureDistance = 120.f;
    bool Validate(FString& Error) const;
};
