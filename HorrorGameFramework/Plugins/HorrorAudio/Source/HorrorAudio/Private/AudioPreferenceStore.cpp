#include "AudioPreferenceStore.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool HorrorAudioPreferences::Load(const FString& Path, FHorrorAudioVolumes& OutVolumes, FString& Error)
{
    FString Content;
    TSharedPtr<FJsonObject> Object;
    if (!FFileHelper::LoadFileToString(Content, *Path)
        || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Content), Object) || !Object)
    { Error = TEXT("Cannot read audio preferences JSON."); return false; }
    double Version;
    if (!Object->TryGetNumberField(TEXT("version"), Version) || Version != 1)
    { Error = TEXT("Unsupported audio preferences version."); return false; }
    FHorrorAudioVolumes Loaded;
    const TCHAR* Keys[] = { TEXT("master"), TEXT("ambience"), TEXT("effects"), TEXT("interface"), TEXT("music") };
    for (uint8 Index = 0; Index < UE_ARRAY_COUNT(Keys); ++Index)
    {
        double Value;
        if (!Object->TryGetNumberField(Keys[Index], Value) || !FMath::IsFinite(Value) || Value < 0.0 || Value > 1.0
            || !Loaded.Set(static_cast<EHorrorAudioChannel>(Index), static_cast<float>(Value)))
        { Error = FString::Printf(TEXT("Invalid audio preference '%s'."), Keys[Index]); return false; }
    }
    OutVolumes = Loaded;
    return true;
}

bool HorrorAudioPreferences::Save(const FString& Path, const FHorrorAudioVolumes& Volumes, FString& Error)
{
    if (!Volumes.Validate()) { Error = TEXT("Invalid audio volumes."); return false; }
    TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("version"), 1);
    const TCHAR* Keys[] = { TEXT("master"), TEXT("ambience"), TEXT("effects"), TEXT("interface"), TEXT("music") };
    for (uint8 Index = 0; Index < UE_ARRAY_COUNT(Keys); ++Index)
        Object->SetNumberField(Keys[Index], Volumes.Get(static_cast<EHorrorAudioChannel>(Index)));
    FString Content;
    if (!FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Content)))
    { Error = TEXT("Cannot serialize audio preferences."); return false; }
    IFileManager& Files = IFileManager::Get();
    if (!Files.MakeDirectory(*FPaths::GetPath(Path), true))
    { Error = TEXT("Cannot create the audio preferences directory."); return false; }
    const FString TemporaryPath = Path + TEXT(".") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
    if (!FFileHelper::SaveStringToFile(Content, *TemporaryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        || !Files.Move(*Path, *TemporaryPath, true, false))
    {
        Files.Delete(*TemporaryPath, false, false, true);
        Error = TEXT("Cannot replace audio preferences; previous applied values were retained.");
        return false;
    }
    return true;
}
