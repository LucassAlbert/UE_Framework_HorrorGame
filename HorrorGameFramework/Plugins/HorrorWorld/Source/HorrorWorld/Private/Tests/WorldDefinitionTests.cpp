#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HorrorWorldDefinition.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHorrorWorldDefinitionTest, "HorrorFramework.Streaming.DefinitionAndResidency",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHorrorWorldDefinitionTest::RunTest(const FString& Parameters)
{
    UHorrorWorldDefinition* World = NewObject<UHorrorWorldDefinition>();
    World->WorldId = TEXT("TestWorld");
    World->PreloadDistance = 100.f;
    World->UnloadDistance = 200.f;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        FHorrorWorldArea Area;
        Area.Id = FName(*FString::Printf(TEXT("Area%d"), Index));
        Area.Level = TSoftObjectPtr<UWorld>(FSoftObjectPath(FString::Printf(TEXT("/Game/Test/Area%d.Area%d"), Index, Index)));
        Area.Bounds = FBox(FVector(Index * 1000.f, 0.f, 0.f), FVector(Index * 1000.f + 500.f, 500.f, 500.f));
        if (Index > 0) Area.Neighbors.Add(FName(*FString::Printf(TEXT("Area%d"), Index - 1)));
        if (Index < 2) Area.Neighbors.Add(FName(*FString::Printf(TEXT("Area%d"), Index + 1)));
        World->Areas.Add(Area);
    }
    FString Error;
    TestTrue(TEXT("A reciprocal three-area graph is valid"), World->Validate(Error));
    TestTrue(TEXT("Close area preloads"), World->SelectAreas(FVector(550.f, 250.f, 100.f), {}).Contains(TEXT("Area0")));
    TestFalse(TEXT("Unloaded area outside preload radius stays unloaded"), World->SelectAreas(FVector(650.f, 250.f, 100.f), {}).Contains(TEXT("Area0")));
    TestTrue(TEXT("Hysteresis retains loaded area at same position"), World->SelectAreas(FVector(650.f, 250.f, 100.f), {TEXT("Area0")}).Contains(TEXT("Area0")));
    TestFalse(TEXT("Distant resident area can unload"), World->SelectAreas(FVector(750.f, 250.f, 100.f), {TEXT("Area0")}).Contains(TEXT("Area0")));
    TSet<FName> Route;
    TestTrue(TEXT("Pursuit keeps a connected route"), World->AddRoute(TEXT("Area0"), TEXT("Area2"), Route));
    TestEqual(TEXT("Route includes the intermediate area"), Route.Num(), 3);
    TestFalse(TEXT("Invalid destination does not invent a connection"), World->AddRoute(TEXT("Area0"), TEXT("Missing"), Route));
    World->Areas[1].Neighbors.Remove(TEXT("Area0"));
    TestFalse(TEXT("One-way area connections are rejected"), World->Validate(Error));
    World->Areas[1].Neighbors.Add(TEXT("Area0"));
    World->Areas[2].Id = TEXT("Area0");
    TestFalse(TEXT("Duplicate persistent IDs are rejected"), World->Validate(Error));
    return !HasAnyErrors();
}
#endif
