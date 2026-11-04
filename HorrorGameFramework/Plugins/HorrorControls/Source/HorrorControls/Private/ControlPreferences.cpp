#include "ControlPreferences.h"
#include "InputAction.h"
#include "InputMappingContext.h"

void UControlPreferencesDefinition::ImportBindings(const TArray<UInputMappingContext*>& Contexts)
{
    Bindings.Reset();
    for (const auto* Context : Contexts)
        if (Context) for (const auto& Mapping : Context->GetMappings())
        {
            if (!Mapping.Action || FSoftObjectPath(Mapping.Action) == LookAction.ToSoftObjectPath()) continue;
            const auto Device = Mapping.Key.IsGamepadKey() ? EHorrorControlDevice::Gamepad : EHorrorControlDevice::KeyboardMouse;
            if (!AcceptsKey(Device, Mapping.Key)) continue;
            const FName Id(*(Mapping.Action->GetPathName() + TEXT(".") + Mapping.Key.GetFName().ToString()));
            if (FindBinding(Id)) continue;
            auto& Binding = Bindings.AddDefaulted_GetRef();
            Binding.Id = Id; Binding.Action = const_cast<UInputAction*>(Mapping.Action.Get());
            Binding.DefaultKey = Mapping.Key; Binding.Device = Device;
            Binding.DisplayName = FText::FromString(Mapping.Action->GetName() + TEXT(" / ") + Mapping.Key.GetDisplayName().ToString());
        }
}

const FHorrorBindingDefinition* UControlPreferencesDefinition::FindBinding(FName Id) const
{
    return Bindings.FindByPredicate([Id](const auto& Binding) { return Binding.Id == Id; });
}
bool UControlPreferencesDefinition::AcceptsKey(EHorrorControlDevice Device, FKey Key)
{
    return Key.IsValid() && !Key.IsAxis1D() && !Key.IsAxis2D() && !Key.IsAxis3D()
        && (Key.IsGamepadKey() == (Device == EHorrorControlDevice::Gamepad));
}
FHorrorControlPreferences UControlPreferencesDefinition::MakeDefaults() const
{
    FHorrorControlPreferences Result = Defaults;
    Result.Bindings.Reset();
    for (const auto& Binding : Bindings) Result.Bindings.Add(Binding.Id, Binding.DefaultKey);
    return Result;
}
bool UControlPreferencesDefinition::Validate(FText& Error) const
{
    if (ProfileId.IsNone() || LookAction.IsNull() || Bindings.IsEmpty()
        || !FMath::IsFinite(MinimumFOV) || !FMath::IsFinite(MaximumFOV) || MinimumFOV < 1.f || MaximumFOV > 170.f || MinimumFOV > MaximumFOV
        || !FMath::IsFinite(MinimumSensitivity) || !FMath::IsFinite(MaximumSensitivity) || MinimumSensitivity <= 0 || MinimumSensitivity > MaximumSensitivity)
    { Error = NSLOCTEXT("Controls", "InvalidDefinition", "A configuração de controles contém limites ou identidade inválidos."); return false; }
    TSet<FName> Ids;
    for (const auto& Binding : Bindings)
    {
        if (Binding.Id.IsNone() || Binding.DisplayName.IsEmpty() || Binding.Action.IsNull() || Binding.Action == LookAction
            || Ids.Contains(Binding.Id) || !AcceptsKey(Binding.Device, Binding.DefaultKey))
        { Error = NSLOCTEXT("Controls", "InvalidBinding", "Há um comando inválido ou repetido na configuração de controles."); return false; }
        Ids.Add(Binding.Id);
    }
    return ValidatePreferences(MakeDefaults(), Error);
}
bool UControlPreferencesDefinition::ValidatePreferences(const FHorrorControlPreferences& Value, FText& Error) const
{
    auto ValidLook = [this](const FHorrorLookPreferences& Look)
    {
        return FMath::IsFinite(Look.SensitivityX) && FMath::IsFinite(Look.SensitivityY)
            && Look.SensitivityX >= MinimumSensitivity && Look.SensitivityX <= MaximumSensitivity
            && Look.SensitivityY >= MinimumSensitivity && Look.SensitivityY <= MaximumSensitivity;
    };
    if (!ValidLook(Value.Mouse) || !ValidLook(Value.Gamepad) || !FMath::IsFinite(Value.FieldOfView)
        || Value.FieldOfView < MinimumFOV || Value.FieldOfView > MaximumFOV || Value.Bindings.Num() != Bindings.Num())
    { Error = NSLOCTEXT("Controls", "InvalidPreferences", "Os valores de câmera ou os comandos salvos são inválidos."); return false; }
    TSet<FKey> Keyboard, Gamepad;
    for (const auto& Binding : Bindings)
    {
        const FKey* Key = Value.Bindings.Find(Binding.Id);
        TSet<FKey>& Used = Binding.Device == EHorrorControlDevice::Gamepad ? Gamepad : Keyboard;
        if (!Key || !AcceptsKey(Binding.Device, *Key) || Used.Contains(*Key))
        { Error = NSLOCTEXT("Controls", "InvalidKeys", "Os comandos contêm uma tecla inválida ou um conflito."); return false; }
        Used.Add(*Key);
    }
    return true;
}
bool UControlPreferencesDefinition::Remap(FHorrorControlPreferences& Value, FName Id, FKey Key, FText& Message) const
{
    const auto* Binding = FindBinding(Id);
    if (!Binding || !AcceptsKey(Binding->Device, Key) || !ValidatePreferences(Value, Message))
    { Message = NSLOCTEXT("Controls", "WrongDevice", "Escolha uma tecla ou botão do dispositivo indicado."); return false; }
    FHorrorControlPreferences Candidate = Value;
    const FKey OldKey = Candidate.Bindings.FindChecked(Id);
    Message = FText::GetEmpty();
    for (const auto& Other : Bindings)
        if (Other.Id != Id && Other.Device == Binding->Device && Candidate.Bindings.FindChecked(Other.Id) == Key)
        {
            Candidate.Bindings[Other.Id] = OldKey;
            Message = FText::Format(NSLOCTEXT("Controls", "Swapped", "Comandos trocados: {0} e {1}."), Binding->DisplayName, Other.DisplayName);
            break;
        }
    Candidate.Bindings[Id] = Key;
    FText Error;
    if (!ValidatePreferences(Candidate, Error)) { Message = Error; return false; }
    Value = MoveTemp(Candidate);
    return true;
}
