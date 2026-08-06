#include "HSRStageBuffAuthority.h"

#include "../Data/Definitions/HSRStageBuffDefinition.h"
#include "GameplayEffect.h"

bool UHSRStageBuffAuthority::RegisterEncounterBuffs(FName EncounterId,
	const TArray<UHSRStageBuffDefinition*>& Definitions)
{
	if (EncounterId.IsNone())
	{
		return false;
	}

	TArray<TObjectPtr<UHSRStageBuffDefinition>> Candidate;
	TSet<FName> Seen;
	for (UHSRStageBuffDefinition* Definition : Definitions)
	{
		if (!Definition || Definition->BuffId.IsNone() || Seen.Contains(Definition->BuffId)
			|| !Definition->GameplayEffectClass)
		{
			return false;
		}
		Seen.Add(Definition->BuffId);
		if (Definition->ResourceCost < 0
			|| (Definition->ResourceCost > 0 && Definition->ResourceItemId.IsNone()))
		{
			return false;
		}
		Candidate.Add(Definition);
	}

	FHSRStageBuffEncounterRegistry* Existing = EncounterRegistries.FindByPredicate(
		[EncounterId](const FHSRStageBuffEncounterRegistry& Entry)
		{
			return Entry.EncounterId == EncounterId;
		});
	if (Existing)
	{
		Existing->Definitions = MoveTemp(Candidate);
	}
	else
	{
		FHSRStageBuffEncounterRegistry& Entry = EncounterRegistries.AddDefaulted_GetRef();
		Entry.EncounterId = EncounterId;
		Entry.Definitions = MoveTemp(Candidate);
	}
	return true;
}

bool UHSRStageBuffAuthority::ValidateBuffIds(FName EncounterId, const TArray<FName>& BuffIds) const
{
	if (BuffIds.IsEmpty())
	{
		return true;
	}

	const FHSRStageBuffEncounterRegistry* Registry = EncounterRegistries.FindByPredicate(
		[EncounterId](const FHSRStageBuffEncounterRegistry& Entry)
		{
			return Entry.EncounterId == EncounterId;
		});
	if (!Registry)
	{
		return false;
	}

	TSet<FName> Seen;
	for (const FName BuffId : BuffIds)
	{
		if (BuffId.IsNone() || Seen.Contains(BuffId))
		{
			return false;
		}
		Seen.Add(BuffId);
		const TObjectPtr<UHSRStageBuffDefinition>* Found = Registry->Definitions.FindByPredicate(
			[BuffId](const TObjectPtr<UHSRStageBuffDefinition>& Definition)
			{
				return Definition && Definition->BuffId == BuffId && Definition->bEnabled
					&& Definition->GameplayEffectClass;
			});
		if (!Found)
		{
			return false;
		}
	}
	return true;
}

const UHSRStageBuffDefinition* UHSRStageBuffAuthority::FindBuff(FName EncounterId, FName BuffId) const
{
	const FHSRStageBuffEncounterRegistry* Registry = EncounterRegistries.FindByPredicate(
		[EncounterId](const FHSRStageBuffEncounterRegistry& Entry)
		{
			return Entry.EncounterId == EncounterId;
		});
	if (!Registry)
	{
		return nullptr;
	}
	const TObjectPtr<UHSRStageBuffDefinition>* Found = Registry->Definitions.FindByPredicate(
		[BuffId](const TObjectPtr<UHSRStageBuffDefinition>& Definition)
		{
			return Definition && Definition->BuffId == BuffId;
		});
	return Found ? Found->Get() : nullptr;
}

void UHSRStageBuffAuthority::Reset()
{
	EncounterRegistries.Reset();
}
