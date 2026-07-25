#include "HSRRelicSetResolver.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Data/Definitions/HSRRelicSetDefinition.h"

TMap<FName, FHSRRelicSetResolution> FHSRRelicSetResolver::Resolve(
	const FHSREquipmentLoadout& Loadout,
	const TArray<UHSRRelicDefinition*>& RelicDefinitions,
	const TArray<UHSRRelicSetDefinition*>& SetDefinitions)
{
	TMap<FName, FHSRRelicSetResolution> Result;
	TMap<FName, FName> DefinitionToSet;
	for (const UHSRRelicDefinition* Definition : RelicDefinitions)
	{
		if (!Definition || Definition->DefinitionId.IsNone() || Definition->SetId.IsNone() || DefinitionToSet.Contains(Definition->DefinitionId)) continue;
		DefinitionToSet.Add(Definition->DefinitionId, Definition->SetId);
	}
	for (const UHSRRelicSetDefinition* Set : SetDefinitions)
	{
		if (!Set || Set->SetId.IsNone() || Set->Threshold != 2 || !Set->SetGameplayEffectClass || Result.Contains(Set->SetId)) continue;
		Result.Add(Set->SetId, FHSRRelicSetResolution());
	}
	for (const TPair<EHSRRelicSlot, FHSREquipmentInstance>& Pair : Loadout.Relics)
	{
		const FName* SetId = DefinitionToSet.Find(Pair.Value.DefinitionId);
		if (SetId)
		{
			if (FHSRRelicSetResolution* Resolution = Result.Find(*SetId)) ++Resolution->Count;
		}
	}
	for (TPair<FName, FHSRRelicSetResolution>& Pair : Result)
	{
		Pair.Value.bActive = Pair.Value.Count >= 2;
		Pair.Value.SetSourceId = Pair.Value.bActive ? Pair.Key : NAME_None;
	}
	return Result;
}
