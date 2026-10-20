#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DialogueSessionComponent.generated.h"

class UDialogueDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDialogueLineChanged, UDialogueDefinition*, Definition, int32, LineIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDialogueEnded, UDialogueDefinition*, Definition, bool, bCompleted, bool, bFirstCompletion);

UCLASS(ClassGroup = (Dialogue), meta = (BlueprintSpawnableComponent))
class HORRORDIALOGUE_API UDialogueSessionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UDialogueSessionComponent();
    UFUNCTION(BlueprintCallable, Category = "Dialogue") bool Start(UDialogueDefinition* Definition);
    UFUNCTION(BlueprintCallable, Category = "Dialogue") bool Advance();
    UFUNCTION(BlueprintCallable, Category = "Dialogue") void Cancel();
    UFUNCTION(BlueprintPure, Category = "Dialogue") bool IsConversationActive() const { return ActiveDefinition != nullptr; }
    UFUNCTION(BlueprintPure, Category = "Dialogue") bool HasCompleted(FName Id) const { return CompletedDialogues.Contains(Id); }
    UFUNCTION(BlueprintPure, Category = "Dialogue") UDialogueDefinition* GetDefinition() const { return ActiveDefinition; }
    UFUNCTION(BlueprintPure, Category = "Dialogue") int32 GetLineIndex() const { return LineIndex; }
    UPROPERTY(BlueprintAssignable, Category = "Dialogue") FOnDialogueLineChanged OnLineChanged;
    UPROPERTY(BlueprintAssignable, Category = "Dialogue") FOnDialogueEnded OnEnded;
    bool ValidateState(FString& Error) const;
    void NotifyStateRestored();
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void Finish(bool bCompleted);
    UPROPERTY(SaveGame) TArray<FName> CompletedDialogues;
    UPROPERTY(Transient) TObjectPtr<UDialogueDefinition> ActiveDefinition;
    int32 LineIndex = INDEX_NONE;
    bool bNotifying = false;
};
