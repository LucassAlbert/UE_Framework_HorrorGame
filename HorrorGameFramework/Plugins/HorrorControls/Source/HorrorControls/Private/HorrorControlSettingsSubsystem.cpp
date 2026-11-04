#include "HorrorControlSettingsSubsystem.h"
#include "ControlLookModifier.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Kismet/GameplayStatics.h"

void UHorrorControlSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UEnhancedInputLocalPlayerSubsystem>();
    Definition = GetDefault<UHorrorControlsSettings>()->Definition.LoadSynchronous();
    if (!Definition) return;
    if (!Definition->Validate(LoadError)) { Definition = nullptr; return; }
    Current = Applied = Definition->MakeDefaults();
    if (!UGameplayStatics::DoesSaveGameExist(GetStorageSlot(), 0)) return;
    auto* Saved = Cast<UControlPreferencesSave>(UGameplayStatics::LoadGameFromSlot(GetStorageSlot(), 0));
    FText Error;
    if (Saved && Saved->Version == 1 && Saved->ProfileId == Definition->ProfileId && Definition->ValidatePreferences(Saved->Preferences, Error))
        Current = Applied = Saved->Preferences;
    else LoadError = NSLOCTEXT("Controls", "LoadFailed", "As preferências salvas não são compatíveis. Os padrões estão ativos; o arquivo só será substituído ao Aplicar.");
}
FString UHorrorControlSettingsSubsystem::GetStorageSlot() const
{
    return FString::Printf(TEXT("HorrorControlPreferences_%d"), FMath::Max(0, GetLocalPlayer()->GetLocalPlayerIndex()));
}
bool UHorrorControlSettingsSubsystem::BeginEdit(UObject* Owner)
{
    if (!IsReady() || !IsValid(Owner) || (bHasEdit && EditOwner.IsValid())) return false;
    if (bHasEdit) { Current = Applied; Notify(); }
    EditOwner = Owner; bHasEdit = true;
    return true;
}
bool UHorrorControlSettingsSubsystem::Preview(UObject* Owner, const FHorrorControlPreferences& Value, FText& Error)
{
    if (!OwnsEdit(Owner) || !Definition->ValidatePreferences(Value, Error)) return false;
    const bool bBindingsChanged = !Current.Bindings.OrderIndependentCompareEqual(Value.Bindings);
    Current = Value; Notify(bBindingsChanged); return true;
}
bool UHorrorControlSettingsSubsystem::Remap(UObject* Owner, FName Id, FKey Key, FText& Message)
{
    if (!OwnsEdit(Owner) || !Definition->Remap(Current, Id, Key, Message)) return false;
    Notify(); return true;
}
bool UHorrorControlSettingsSubsystem::RestoreDefaults(UObject* Owner)
{
    if (!OwnsEdit(Owner)) return false;
    Current = Definition->MakeDefaults(); Notify(); return true;
}
bool UHorrorControlSettingsSubsystem::Apply(UObject* Owner, FText& Error)
{
    if (!OwnsEdit(Owner) || !Definition->ValidatePreferences(Current, Error)) return false;
    auto* Saved = NewObject<UControlPreferencesSave>();
    Saved->ProfileId = Definition->ProfileId; Saved->Preferences = Current;
    if (!UGameplayStatics::SaveGameToSlot(Saved, GetStorageSlot(), 0))
    { Error = NSLOCTEXT("Controls", "SaveFailed", "Não foi possível gravar as preferências. A prévia não foi aplicada ao arquivo."); return false; }
    Applied = Current; LoadError = FText::GetEmpty(); return true;
}
void UHorrorControlSettingsSubsystem::Cancel(UObject* Owner)
{
    if (!OwnsEdit(Owner)) return;
    EditOwner.Reset(); bHasEdit = false; Current = Applied; Notify();
}
void UHorrorControlSettingsSubsystem::Notify(bool bRebuild)
{
    if (bRebuild) RebuildContexts(); OnPreferencesChanged.Broadcast();
}
void UHorrorControlSettingsSubsystem::AddGameplayContext(UInputMappingContext* Source, int32 Priority)
{
    if (!Source || Contexts.ContainsByPredicate([Source](const auto& Entry) { return Entry.Source == Source; })) return;
    auto* Input = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!IsReady()) { Input->AddMappingContext(Source, Priority); return; }
    FHorrorActiveInputContext Entry;
    Entry.Source = Source; Entry.Priority = Priority;
    Entry.Runtime = DuplicateObject<UInputMappingContext>(Source, this);
    // This is a private mapping instance. Original triggers, swizzles, dead zones and delta-time modifiers are kept.
    const auto& Mappings = Source->GetMappings();
    for (int32 Index = 0; Index < Mappings.Num(); ++Index)
        if (Mappings[Index].Action && FSoftObjectPath(Mappings[Index].Action) == Definition->LookAction.ToSoftObjectPath())
        {
            auto* Modifier = NewObject<UControlLookModifier>(Entry.Runtime);
            Modifier->Device = Mappings[Index].Key.IsGamepadKey() ? EHorrorControlDevice::Gamepad : EHorrorControlDevice::KeyboardMouse;
            Entry.Runtime->GetMapping(Index).Modifiers.Add(Modifier);
        }
    Contexts.Add(Entry);
    RebuildContexts();
    Input->AddMappingContext(Entry.Runtime, Priority);
}
void UHorrorControlSettingsSubsystem::RebuildContexts()
{
    if (!Definition) return;
    for (auto& Context : Contexts)
    {
        const auto& Mappings = Context.Source->GetMappings();
        for (int32 Index = 0; Index < Mappings.Num(); ++Index)
        {
            const auto& Source = Mappings[Index];
            if (!Source.Action) continue;
            for (const auto& Binding : Definition->Bindings)
                if (Binding.DefaultKey == Source.Key && Binding.Action.ToSoftObjectPath() == FSoftObjectPath(Source.Action))
                { Context.Runtime->GetMapping(Index).Key = Current.Bindings.FindChecked(Binding.Id); break; }
        }
    }
    FModifyContextOptions Options;
    Options.bIgnoreAllPressedKeysUntilRelease = true;
    Options.bForceImmediately = true;
    GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()->RequestRebuildControlMappings(Options, EInputMappingRebuildType::RebuildWithFlush);
}
UInputMappingContext* UHorrorControlSettingsSubsystem::GetRuntimeContext(UInputMappingContext* Source) const
{
    const auto* Entry = Contexts.FindByPredicate([Source](const auto& Item) { return Item.Source == Source; });
    return Entry ? Entry->Runtime.Get() : nullptr;
}
void UHorrorControlSettingsSubsystem::RemoveGameplayContext(UInputMappingContext* Source)
{
    if (!Source) return;
    auto* Input = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    const int32 Index = Contexts.IndexOfByPredicate([Source](const auto& Entry) { return Entry.Source == Source; });
    if (Index == INDEX_NONE) { Input->RemoveMappingContext(Source); return; }
    Input->RemoveMappingContext(Contexts[Index].Runtime);
    Contexts.RemoveAt(Index);
}
void UHorrorControlSettingsSubsystem::Deinitialize()
{
    if (auto* Input = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        for (const auto& Entry : Contexts) Input->RemoveMappingContext(Entry.Runtime);
    Contexts.Reset(); EditOwner.Reset(); Definition = nullptr;
    Super::Deinitialize();
}
