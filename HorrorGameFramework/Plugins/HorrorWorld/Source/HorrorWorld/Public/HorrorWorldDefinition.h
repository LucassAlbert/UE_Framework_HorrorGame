#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HorrorWorldDefinition.generated.h"
class UWorld;

USTRUCT(BlueprintType)
struct HORRORWORLD_API FHorrorWorldArea
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UWorld> Level;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FBox Bounds = FBox(EForceInit::ForceInit);
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> Neighbors;
};
UCLASS(BlueprintType)
class HORRORWORLD_API UHorrorWorldDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName WorldId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FHorrorWorldArea> Areas;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) float PreloadDistance = 1800.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) float UnloadDistance = 2400.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.05")) float UpdateInterval = .25f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) float OperationTimeout = 45.f;
    bool Validate(FString& Error) const;
    const FHorrorWorldArea* FindArea(FName Id) const;
    FName FindContainingArea(FVector Location) const;
    TSet<FName> SelectAreas(FVector Anchor, const TSet<FName>& Resident) const;
    bool AddRoute(FName From, FName To, TSet<FName>& AreasToKeep) const;
};
