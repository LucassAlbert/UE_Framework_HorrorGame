#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/SaveStateListener.h"
#include "DialogueGameplayComponent.generated.h"

class ADialogueNPC;
class APlayerController;
class AController;
class APawn;
class UDialogueDefinition;
class UDialogueSessionComponent;
class UDialogueScreen;
class UInteractionScannerComponent;
class UUIManagerSubsystem;

UCLASS(ClassGroup = (Dialogue), meta = (BlueprintSpawnableComponent))
class HORRORGAMEFRAMEWORK_API UDialogueGameplayComponent : public UActorComponent, public ISaveStateListener
{
    GENERATED_BODY()
public:
    UDialogueGameplayComponent();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue") bool bEnableDialogue = true;
    UPROPERTY(EditAnywhere, Category = "Dialogue") TSubclassOf<UDialogueScreen> ScreenClass;
    UFUNCTION(BlueprintCallable, Category = "Dialogue") bool StartDialogue(ADialogueNPC* NPC);
    UFUNCTION(BlueprintCallable, Category = "Dialogue") void Advance();
    UFUNCTION(BlueprintCallable, Category = "Dialogue") void Cancel();
    bool CanStart(const ADialogueNPC* NPC) const;
    UDialogueSessionComponent* GetSession() const;
    virtual bool ValidateSaveState(FString& Error) const override;
    virtual void GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const override {}
    virtual void OnSaveStateRestored() override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UUIManagerSubsystem* GetUI() const;
    bool ValidateTarget(const ADialogueNPC* NPC) const;
    void UpdateConversation();
    void Cleanup();
    UFUNCTION() void HandleEnded(UDialogueDefinition* Definition, bool bCompleted, bool bFirstCompletion);
    UFUNCTION() void HandleNPCExit(AActor* Actor, EEndPlayReason::Type Reason);
    UFUNCTION() void HandleStackChanged(bool bHasScreen);
    UFUNCTION() void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);
    TWeakObjectPtr<ADialogueNPC> Speaker;
    TWeakObjectPtr<APlayerController> ConversationController;
    TWeakObjectPtr<UUIManagerSubsystem> ConversationUI;
    TWeakObjectPtr<UDialogueScreen> Screen;
    TArray<TWeakObjectPtr<UInteractionScannerComponent>> SuspendedScanners;
    FTimerHandle ConversationTimer;
    bool bStarting = false;
    bool bCleaning = false;
};
