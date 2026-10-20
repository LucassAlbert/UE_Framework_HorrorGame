#include "Inventory/InventoryGameplayComponent.h"
#include "Inventory/InventoryScreen.h"
#include "Inventory/InventoryDroppedItem.h"
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "InventoryItemTarget.h"
#include "InventoryRecipe.h"
#include "Components/SaveComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UIManagerSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogInventoryGameplay, Log, All);

UInventoryGameplayComponent::UInventoryGameplayComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    DroppedItemClass = AInventoryDroppedItem::StaticClass();
    InventoryScreenClass = UInventoryScreen::StaticClass();
}

UInventoryComponent* UInventoryGameplayComponent::GetInventory() const
{
    return GetOwner() ? GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
}

void UInventoryGameplayComponent::BeginPlay()
{
    Super::BeginPlay();
    if (UInventoryComponent* Inventory = GetInventory())
        Inventory->OnInventoryChanged.AddUniqueDynamic(this, &UInventoryGameplayComponent::HandleInventoryChanged);
    else UE_LOG(LogInventoryGameplay, Error, TEXT("%s requires InventoryComponent."), *GetNameSafe(GetOwner()));
}

void UInventoryGameplayComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    CloseInventoryScreen();
    if (UInventoryComponent* Inventory = GetInventory())
        Inventory->OnInventoryChanged.RemoveDynamic(this, &UInventoryGameplayComponent::HandleInventoryChanged);
    if (EquipmentLoadHandle) EquipmentLoadHandle->CancelHandle();
    if (RecipeLoadHandle) RecipeLoadHandle->CancelHandle();
    if (DropLoadHandle) DropLoadHandle->CancelHandle();
    EquipmentLoadHandle.Reset();
    RecipeLoadHandle.Reset();
    DropLoadHandle.Reset();
    LoadedRecipeAssets.Reset();
    if (IsValid(EquipmentActor)) EquipmentActor->Destroy();
    EquipmentActor = nullptr;
    InventoryScreen.Reset();
    Super::EndPlay(Reason);
}

void UInventoryGameplayComponent::OpenInventory()
{
    APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    UUIManagerSubsystem* UI = Controller ? Controller->GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
    if (!UI || UI->HasModalScreen() || !GetInventory() || !GetInventory()->ValidateState().bSuccess) return;
    UUIScreenBase* Screen = UI->PushScreen(InventoryScreenClass);
    InventoryScreen = Screen;
    if (UInventoryScreen* NativeScreen = Cast<UInventoryScreen>(Screen)) NativeScreen->SetSource(this);
}

void UInventoryGameplayComponent::ReportResult(const FInventoryResult& Result)
{
    if (!Result.Message.IsEmpty()) OnFeedback.Broadcast(Result.Message);
}

void UInventoryGameplayComponent::RequestObjectives()
{
    OnObjectivesRequested.Broadcast();
}

void UInventoryGameplayComponent::RequestDocuments() { OnDocumentsRequested.Broadcast(); }

void UInventoryGameplayComponent::CloseInventoryScreen()
{
    if (UUIScreenBase* Screen = InventoryScreen.Get()) Screen->CloseScreen();
}

FInventoryResult UInventoryGameplayComponent::UseSlot(int32 SlotIndex)
{
    UInventoryComponent* Inventory = GetInventory();
    UInventoryItemDefinition* Item = Inventory ? Inventory->GetItemAt(SlotIndex) : nullptr;
    if (!Item || !Item->bCanUse || !Inventory->ValidateState().bSuccess)
        return FInventoryResult::Failure(NSLOCTEXT("Inventory", "CannotUse", "Este item não pode ser usado."));
    APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!Controller) return FInventoryResult::Failure(NSLOCTEXT("Inventory", "NoPlayer", "Jogador indisponível."));
    FVector Origin;
    FRotator Rotation;
    Controller->GetPlayerViewPoint(Origin, Rotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(InventoryUse), false, GetOwner());
    if (EquipmentActor) Query.AddIgnoredActor(EquipmentActor);
    FHitResult Hit;
    GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Rotation.Vector() * UseDistance, TargetChannel, Query);
    AActor* Target = Hit.GetActor();
    FHitResult Obstruction;
    const bool bVisible = Target && (!GetWorld()->LineTraceSingleByChannel(Obstruction, Origin, Hit.ImpactPoint,
        ECC_Visibility, Query) || Obstruction.GetActor() == Target);
    if (!IsValid(Target) || !bVisible || !Target->Implements<UInventoryItemTarget>()
        || !IInventoryItemTarget::Execute_CanApplyInventoryItem(Target, Item, GetOwner())
        || !IInventoryItemTarget::Execute_ApplyInventoryItem(Target, Item, GetOwner()))
        return FInventoryResult::Failure(NSLOCTEXT("Inventory", "NoTarget", "Não há um alvo válido ao alcance para este item."));
    if (Item->bConsumeOnUse)
    {
        const FInventoryResult Result = Inventory->RemoveFromSlot(SlotIndex, 1);
        if (!Result.bSuccess) return Result;
    }
    CloseInventoryScreen();
    return FInventoryResult::Success();
}

FInventoryResult UInventoryGameplayComponent::EquipSlot(int32 SlotIndex)
{
    UInventoryComponent* Inventory = GetInventory();
    UInventoryItemDefinition* Item = Inventory ? Inventory->GetItemAt(SlotIndex) : nullptr;
    if (!Item || !Item->bCanEquip || !Inventory->ValidateState().bSuccess || Item->EquipmentActorClass.IsNull()
        || !GetOwner()->FindComponentByClass<UCameraComponent>())
        return FInventoryResult::Failure(NSLOCTEXT("Inventory", "NoEquipment", "O equipamento não está configurado."));
    // Load before committing so a missing class never equips an invisible item.
    if (!Item->EquipmentActorClass.Get())
    {
        if (EquipmentLoadHandle) EquipmentLoadHandle->CancelHandle();
        const FGuid StackId = Inventory->FindSlot(SlotIndex)->StackId;
        EquipmentLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Item->EquipmentActorClass.ToSoftObjectPath(),
            FStreamableDelegate::CreateWeakLambda(this, [this, SlotIndex, StackId]()
            {
                UInventoryComponent* Current = GetInventory();
                const FInventorySlot* Slot = Current ? Current->FindSlot(SlotIndex) : nullptr;
                UInventoryItemDefinition* Definition = Current ? Current->GetItemAt(SlotIndex) : nullptr;
                if (Slot && Slot->StackId == StackId && Definition && Definition->EquipmentActorClass.Get())
                    ReportResult(EquipSlot(SlotIndex));
                else ReportResult(FInventoryResult::Failure(NSLOCTEXT("Inventory", "LoadEquipmentFailed", "Não foi possível carregar o equipamento.")));
            }));
        return FInventoryResult::Failure(NSLOCTEXT("Inventory", "LoadingEquipment", "Carregando equipamento…"));
    }
    const FGuid StackId = Inventory->FindSlot(SlotIndex)->StackId;
    AActor* Candidate = SpawnEquipment(Item);
    if (!Candidate) return FInventoryResult::Failure(NSLOCTEXT("Inventory", "EquipmentSpawnFailed", "Não foi possível criar o equipamento."));
    AActor* Previous = EquipmentActor;
    const FGuid PreviousId = EquippedPresentationId;
    EquipmentActor = Candidate;
    EquippedPresentationId = StackId;
    const FInventoryResult Result = Inventory->EquipSlot(SlotIndex);
    if (Result.bSuccess)
    {
        if (IsValid(Previous)) Previous->Destroy();
        CloseInventoryScreen();
    }
    else
    {
        Candidate->Destroy();
        EquipmentActor = Previous;
        EquippedPresentationId = PreviousId;
    }
    return Result;
}

FInventoryResult UInventoryGameplayComponent::DropSlot(int32 SlotIndex)
{
    UInventoryComponent* Inventory = GetInventory();
    UInventoryItemDefinition* Item = Inventory ? Inventory->GetItemAt(SlotIndex) : nullptr;
    APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!Item || !Item->bCanDrop || !Controller || !DroppedItemClass || !Inventory->ValidateState().bSuccess)
        return FInventoryResult::Failure(NSLOCTEXT("Inventory", "CannotDrop", "Não foi possível dropar este item."));
    if (!Item->Mesh.Get())
    {
        if (Item->Mesh.IsNull()) return FInventoryResult::Failure(NSLOCTEXT("Inventory", "DropMeshMissing", "O item não possui mesh para o drop."));
        if (DropLoadHandle) DropLoadHandle->CancelHandle();
        DropLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Item->Mesh.ToSoftObjectPath(),
            FStreamableDelegate::CreateWeakLambda(this, [this]()
            { OnFeedback.Broadcast(NSLOCTEXT("Inventory", "DropMeshLoaded", "Carregamento concluído. Confirme o drop novamente.")); }));
        return FInventoryResult::Failure(NSLOCTEXT("Inventory", "DropMeshLoading", "Carregando o item…"));
    }
    const UBodySetup* Body = Item->Mesh.Get()->GetBodySetup();
    if (!Body || Body->AggGeom.GetElementCount() == 0 || Body->CollisionTraceFlag == CTF_UseComplexAsSimple)
        return FInventoryResult::Failure(NSLOCTEXT("Inventory", "DropCollisionMissing", "O mesh do item precisa de colisão simples para cair com física."));
    FVector Origin;
    FRotator Rotation;
    Controller->GetPlayerViewPoint(Origin, Rotation);
    const float Radius = FMath::Max(5.f, Item->Mesh.Get()->GetBounds().SphereRadius * Item->WorldScale);
    const FVector Desired = Origin + Rotation.Vector() * (80.f + Radius * 2.f);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(InventoryDrop), false, GetOwner());
    if (EquipmentActor) Query.AddIgnoredActor(EquipmentActor);
    FHitResult Hit;
    if (GetWorld()->SweepSingleByChannel(Hit, Origin, Desired, FQuat::Identity, ECC_Visibility,
        FCollisionShape::MakeSphere(Radius), Query))
        return FInventoryResult::Failure(NSLOCTEXT("Inventory", "DropBlocked", "Afaste-se do obstáculo para dropar o item."));
    AInventoryWorldItem* Drop = GetWorld()->SpawnActorDeferred<AInventoryWorldItem>(DroppedItemClass,
        FTransform(FRotator::ZeroRotator, Desired), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Drop) return FInventoryResult::Failure(NSLOCTEXT("Inventory", "DropSpawnFailed", "Não foi possível criar o item no cenário."));
    if (!Drop->SaveComponent->bRecreateOnLoad || !Drop->SaveComponent->PersistentKey.IsNone())
    {
        Drop->Destroy();
        return FInventoryResult::Failure(NSLOCTEXT("Inventory", "DropSaveInvalid", "A classe de drop não permite restauração pelo save."));
    }
    Drop->Item = Item;
    Drop->Quantity = 1;
    Drop->SaveComponent->PersistentID = FGuid::NewGuid();
    Drop->FinishSpawning(FTransform(FRotator::ZeroRotator, Desired));
    const FInventoryResult Result = Inventory->RemoveFromSlot(SlotIndex, 1);
    if (!Result.bSuccess) Drop->Destroy();
    return Result;
}

FInventoryResult UInventoryGameplayComponent::CombineSlots(int32 FirstSlot, int32 SecondSlot)
{
    UInventoryComponent* Inventory = GetInventory();
    UInventoryItemDefinition* First = Inventory ? Inventory->GetItemAt(FirstSlot) : nullptr;
    if (!First) return FInventoryResult::Failure(NSLOCTEXT("Inventory", "MissingIngredient", "Selecione dois ingredientes."));
    TArray<FSoftObjectPath> Pending;
    for (const TSoftObjectPtr<UInventoryRecipe>& Reference : First->Recipes)
    {
        UInventoryRecipe* Recipe = Reference.Get();
        if (!Recipe) { if (!Reference.IsNull()) Pending.AddUnique(Reference.ToSoftObjectPath()); continue; }
        LoadedRecipeAssets.AddUnique(Recipe);
        if (Recipe->ResultItem.Get()) LoadedRecipeAssets.AddUnique(Recipe->ResultItem.Get());
        if (!Recipe->ResultItem.Get() && !Recipe->ResultItem.IsNull()) Pending.AddUnique(Recipe->ResultItem.ToSoftObjectPath());
        const FInventorySlot* Second = Inventory->FindSlot(SecondSlot);
        const FInventorySlot* Slot = Inventory->FindSlot(FirstSlot);
        if (Second && Slot && Recipe->ResultItem.Get()
            && ((Slot->Item == Recipe->FirstIngredient && Second->Item == Recipe->SecondIngredient)
                || (Slot->Item == Recipe->SecondIngredient && Second->Item == Recipe->FirstIngredient)))
            return Inventory->CombineSlots(FirstSlot, SecondSlot, Recipe);
    }
    if (!Pending.IsEmpty())
    {
        if (RecipeLoadHandle) RecipeLoadHandle->CancelHandle();
        // Loading never consumes ingredients. The player confirms the action again after completion.
        RecipeLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Pending,
            FStreamableDelegate::CreateWeakLambda(this, [this]()
            { OnFeedback.Broadcast(NSLOCTEXT("Inventory", "RecipeReady", "Receitas carregadas. Confirme a combinação novamente.")); }));
        return FInventoryResult::Failure(NSLOCTEXT("Inventory", "RecipeLoading", "Carregando receitas…"));
    }
    return FInventoryResult::Failure(NSLOCTEXT("Inventory", "NoRecipe", "Esses itens não podem ser combinados."));
}

FInventoryResult UInventoryGameplayComponent::InspectSlot(int32 SlotIndex)
{
    UInventoryItemDefinition* Item = GetInventory() ? GetInventory()->GetItemAt(SlotIndex) : nullptr;
    if (!Item || !Item->bCanInspect || !OnInspectRequested.IsBound())
        return FInventoryResult::Failure(NSLOCTEXT("Inventory", "InspectionMissing", "O sistema de inspeção ainda não está conectado."));
    OnInspectRequested.Broadcast(Item);
    return FInventoryResult::Success();
}

void UInventoryGameplayComponent::HandleInventoryChanged() { UpdateEquipment(); }

void UInventoryGameplayComponent::UpdateEquipment()
{
    UInventoryComponent* Inventory = GetInventory();
    const FInventorySlot* Slot = Inventory ? Inventory->FindSlot(Inventory->GetEquippedSlot()) : nullptr;
    if (Slot && Slot->StackId == EquippedPresentationId && IsValid(EquipmentActor)) return;
    if (IsValid(EquipmentActor)) EquipmentActor->Destroy();
    EquipmentActor = nullptr;
    EquippedPresentationId.Invalidate();
    UInventoryItemDefinition* Item = Inventory ? Inventory->GetItemAt(Inventory->GetEquippedSlot()) : nullptr;
    if (!Item) return;
    EquipmentActor = SpawnEquipment(Item);
    if (EquipmentActor && Slot) EquippedPresentationId = Slot->StackId;
    else UE_LOG(LogInventoryGameplay, Error, TEXT("Could not restore equipment for %s."), *Item->ItemId.ToString());
}

AActor* UInventoryGameplayComponent::SpawnEquipment(UInventoryItemDefinition* Item)
{
    UCameraComponent* Camera = GetOwner()->FindComponentByClass<UCameraComponent>();
    if (!Item || !Camera || !Item->EquipmentActorClass.Get()) return nullptr;
    FActorSpawnParameters Parameters;
    Parameters.Owner = GetOwner();
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Actor = GetWorld()->SpawnActor<AActor>(Item->EquipmentActorClass.Get(), Camera->GetComponentTransform(), Parameters);
    if (Actor)
    {
        Actor->SetActorEnableCollision(false);
        Actor->AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        Actor->SetActorRelativeTransform(Item->EquipmentTransform);
    }
    return Actor;
}

void UInventoryGameplayComponent::GatherSaveDependencies(TArray<FSoftObjectPath>& Paths) const
{
    UInventoryComponent* Inventory = GetInventory();
    if (!Inventory) return;
    for (const FInventorySlot& Slot : Inventory->GetSlots())
    {
        if (!Slot.Item.IsNull()) Paths.AddUnique(Slot.Item.ToSoftObjectPath());
        UInventoryItemDefinition* Item = Slot.Item.Get();
        if (Item && !Item->EquipmentActorClass.IsNull()) Paths.AddUnique(Item->EquipmentActorClass.ToSoftObjectPath());
    }
}

void UInventoryGameplayComponent::OnSaveStateRestored()
{
    EquippedPresentationId.Invalidate();
    if (EquipmentLoadHandle) EquipmentLoadHandle->CancelHandle();
    if (RecipeLoadHandle) RecipeLoadHandle->CancelHandle();
    if (DropLoadHandle) DropLoadHandle->CancelHandle();
    CloseInventoryScreen();
    if (UInventoryComponent* Inventory = GetInventory())
    {
        const FInventoryResult Result = Inventory->NotifyStateRestored();
        ReportResult(Result);
        if (!Result.bSuccess) UE_LOG(LogInventoryGameplay, Error, TEXT("Invalid restored inventory: %s"), *Result.Message.ToString());
    }
}

bool UInventoryGameplayComponent::ValidateSaveState(FString& Error) const
{
    UInventoryComponent* Inventory = GetInventory();
    if (!Inventory) { Error = TEXT("Inventory component is missing."); return false; }
    const FInventoryResult Result = Inventory->ValidateState();
    if (!Result.bSuccess) { Error = Result.Message.ToString(); return false; }
    UInventoryItemDefinition* Equipped = Inventory->GetItemAt(Inventory->GetEquippedSlot());
    if (Equipped && (!Equipped->EquipmentActorClass.Get() || !GetOwner()->FindComponentByClass<UCameraComponent>()))
    { Error = TEXT("Equipped item presentation is missing."); return false; }
    return true;
}
