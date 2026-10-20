#pragma once

#include "CoreMinimal.h"
#include "ActorSaveData.generated.h"

USTRUCT()
struct SAVESYSTEM_API FComponentSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    FName ComponentName;
    UPROPERTY()
    TArray<uint8> ByteData;
};

USTRUCT(BlueprintType)
struct SAVESYSTEM_API FActorSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid ActorID;
    // Only explicitly opted-in runtime actors may be reconstructed.
    UPROPERTY()
    TSoftClassPtr<AActor> RuntimeClass;
    UPROPERTY()
    TArray<FSoftObjectPath> Dependencies;
    UPROPERTY() bool bHasPhysicsState = false;
    UPROPERTY() bool bPhysicsSleeping = false;
    UPROPERTY() FVector LinearVelocity = FVector::ZeroVector;
    UPROPERTY() FVector AngularVelocity = FVector::ZeroVector;
    UPROPERTY()
    FTransform Transform;
    UPROPERTY()
    TArray<uint8> ByteData;
    UPROPERTY()
    TArray<FComponentSaveData> Components;
};

USTRUCT(BlueprintType)
struct SAVESYSTEM_API FSaveSlotInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Save")
    int32 SlotIndex = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Save")
    bool bExists = false;
};
