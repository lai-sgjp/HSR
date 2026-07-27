#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSREquipmentTypes.h"
#include "HSREquipmentSubsystem.generated.h"

class UHSREquipmentDefinition;
class UHSRRelicDefinition;
struct FHSREquipmentSaveDto;

struct FHSREquipmentRestoreState
{
	FHSREquipmentLoadout Loadout;
	TMap<FName, int32> RelicSetCounts;
	int32 Revision = 0;
};
struct FHSRRelicSetSnapshot
{
	FName SetId;
	FName SetSourceId;
	int32 EquippedCount = 0;
	int32 Threshold = 2;
	bool bActive = false;
};
using FHSREquipmentRestoreMap = TMap<FGuid,FHSREquipmentRestoreState>;
DECLARE_DELEGATE_RetVal_OneParam(bool, FHSREquipmentRestoreProjection, const FHSREquipmentRestoreMap&);

DECLARE_MULTICAST_DELEGATE_TwoParams(FHSREquipmentLoadoutChanged, const FGuid& /* CharacterId */, int32 /* Revision */);

UCLASS()
class HSR_API UHSREquipmentSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	void ExportSaveData(TArray<struct FHSREquipmentSaveDto>& Out) const;
	bool PrepareRestore(const TArray<struct FHSREquipmentSaveDto>& In, FHSREquipmentRestoreMap& Out) const;
	void CommitRestore(const FHSREquipmentRestoreMap& Candidate);
	void NotifyRestored(const TSet<FGuid>& Changed);
	void SetRestoreProjection(FHSREquipmentRestoreProjection InProjection) { RestoreProjection=MoveTemp(InProjection); }
	bool ProjectRestore(const FHSREquipmentRestoreMap& Candidate) const { return !RestoreProjection.IsBound() || RestoreProjection.Execute(Candidate); }
	EHSREquipmentOperationResult RegisterDefinition(const UHSREquipmentDefinition& Definition);
	EHSREquipmentOperationResult RegisterDefinition(const UHSRRelicDefinition& Definition);
	bool HasDefinition(FName DefinitionId) const { return Definitions.Contains(DefinitionId); }

	EHSREquipmentOperationResult Equip(const FGuid& CharacterId, const FHSREquipmentInstance& Instance);
	EHSREquipmentOperationResult Replace(const FGuid& CharacterId, const FHSREquipmentInstance& Instance);
	EHSREquipmentOperationResult Unequip(const FGuid& CharacterId, EHSREquipmentKind Kind, int32 Slot, const FGuid& ExpectedInstanceId);
	EHSREquipmentOperationResult SetEnhancementLevel(const FGuid& CharacterId, const FGuid& InstanceId, int32 NewLevel);

	bool GetLoadout(const FGuid& CharacterId, FHSREquipmentLoadout& OutLoadout, int32& OutRevision) const;
	void GetRelicSetSnapshots(const FGuid& CharacterId, TArray<FHSRRelicSetSnapshot>& Out) const;
	FHSREquipmentLoadoutChanged& OnLoadoutChanged() { return LoadoutChanged; }
	const FHSREquipmentLoadoutChanged& OnLoadoutChanged() const { return LoadoutChanged; }

private:
	struct FDefinitionRule
	{
		EHSREquipmentKind Kind = EHSREquipmentKind::Equipment;
		int32 Slot = 0;
		int32 EnhancementCap = 0;
		FName SetId;
	};

	struct FLoadoutState
	{
		FHSREquipmentLoadout Loadout;
		int32 Revision = 0;
	};

	bool IsValidInstance(const FHSREquipmentInstance& Instance) const;
	bool IsValidModifiers(const TArray<FHSREquipmentModifier>& Modifiers) const;
	const FDefinitionRule* FindDefinition(const FHSREquipmentInstance& Instance) const;
	bool IsSlotValid(EHSREquipmentKind Kind, int32 Slot) const;
	bool IsSlotOccupied(const FHSREquipmentLoadout& Loadout, EHSREquipmentKind Kind, int32 Slot) const;
	FHSREquipmentInstance* FindInstance(FHSREquipmentLoadout& Loadout, EHSREquipmentKind Kind, int32 Slot);
	const FHSREquipmentInstance* FindInstance(const FHSREquipmentLoadout& Loadout, EHSREquipmentKind Kind, int32 Slot) const;
	void CommitLoadout(const FGuid& CharacterId, const FHSREquipmentLoadout& Candidate);

	TMap<FName, FDefinitionRule> Definitions;
	TMap<FGuid, FLoadoutState> Loadouts;
	TMap<FGuid, FGuid> InstanceOwners;
	FHSREquipmentLoadoutChanged LoadoutChanged;
	FHSREquipmentRestoreProjection RestoreProjection;
};
