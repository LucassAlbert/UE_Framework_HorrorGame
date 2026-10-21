#include "HorrorWorldDefinition.h"
#include "Engine/World.h"
bool UHorrorWorldDefinition::Validate(FString& Error) const
{
    if (WorldId.IsNone() || Areas.IsEmpty() || !FMath::IsFinite(PreloadDistance) || PreloadDistance <= 0.f
        || !FMath::IsFinite(UnloadDistance) || UnloadDistance <= PreloadDistance
        || !FMath::IsFinite(UpdateInterval) || UpdateInterval < .05f
        || !FMath::IsFinite(OperationTimeout) || OperationTimeout < 1.f)
    { Error = TEXT("Configure a world ID, areas, positive timings and unload distance greater than preload distance."); return false; }
    TSet<FName> IDs;
    TSet<FSoftObjectPath> Maps;
    for (const FHorrorWorldArea& Area : Areas)
    {
        if (Area.Id.IsNone() || IDs.Contains(Area.Id) || Area.Level.IsNull() || Maps.Contains(Area.Level.ToSoftObjectPath())
            || !Area.Bounds.IsValid || Area.Bounds.Min.ContainsNaN() || Area.Bounds.Max.ContainsNaN()
            || Area.Bounds.GetVolume() <= 0.0)
        { Error = TEXT("Area IDs and maps must be unique; each area needs finite, nonempty world bounds."); return false; }
        IDs.Add(Area.Id); Maps.Add(Area.Level.ToSoftObjectPath());
    }
    for (const FHorrorWorldArea& Area : Areas)
        for (FName Neighbor : Area.Neighbors)
            if (Neighbor == Area.Id || !IDs.Contains(Neighbor) || !FindArea(Neighbor)->Neighbors.Contains(Area.Id))
            { Error = TEXT("Area connections must reference existing, distinct, mutually connected areas."); return false; }
    return true;
}
const FHorrorWorldArea* UHorrorWorldDefinition::FindArea(FName Id) const
{ return Areas.FindByPredicate([Id](const FHorrorWorldArea& Area) { return Area.Id == Id; }); }
FName UHorrorWorldDefinition::FindContainingArea(FVector Location) const
{
    for (const FHorrorWorldArea& Area : Areas) if (Area.Bounds.IsInsideOrOn(Location)) return Area.Id;
    return NAME_None;
}
TSet<FName> UHorrorWorldDefinition::SelectAreas(FVector Anchor, const TSet<FName>& Resident) const
{
    TSet<FName> Selected;
    for (const FHorrorWorldArea& Area : Areas)
    {
        const float Distance = Resident.Contains(Area.Id) ? UnloadDistance : PreloadDistance;
        if (Area.Bounds.ComputeSquaredDistanceToPoint(Anchor) <= FMath::Square(Distance)) Selected.Add(Area.Id);
    }
    return Selected;
}
bool UHorrorWorldDefinition::AddRoute(FName From, FName To, TSet<FName>& AreasToKeep) const
{
    if (!FindArea(From) || !FindArea(To)) return false;
    TMap<FName, FName> Previous;
    TArray<FName> Queue { From };
    Previous.Add(From, NAME_None);
    for (int32 Index = 0; Index < Queue.Num(); ++Index)
    {
        const FName Id = Queue[Index];
        if (Id == To)
        {
            for (FName Step = To; !Step.IsNone(); Step = Previous[Step]) AreasToKeep.Add(Step);
            return true;
        }
        for (FName Neighbor : FindArea(Id)->Neighbors)
            if (!Previous.Contains(Neighbor)) { Previous.Add(Neighbor, Id); Queue.Add(Neighbor); }
    }
    return false;
}
