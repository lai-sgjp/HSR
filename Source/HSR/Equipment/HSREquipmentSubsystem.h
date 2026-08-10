#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSREquipmentTypes.h"
#include "HSREquipmentSubsystem.generated.h"

class UHSREquipmentDefinition;
class UHSRRelicDefinition;
class UHSRRelicSetDefinition;
class UHSREquipmentEnhancementCatalog;
class UHSRInventorySubsystem;
class UHSRItemEquipmentMappingCatalog;
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
struct FHSREquipmentRegistryRestoreState
{
	TMap<FGuid, FHSREquipmentInstance> Registry;
	FHSREquipmentRestoreMap Loadouts;
};
DECLARE_DELEGATE_RetVal_OneParam(bool, FHSREquipmentRestoreProjection, const FHSREquipmentRestoreMap&);

DECLARE_MULTICAST_DELEGATE_TwoParams(FHSREquipmentLoadoutChanged, const FGuid& /* CharacterId */, int32 /* Revision */);

UCLASS()
class HSR_API UHSREquipmentSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Registers authored relic/equipment definitions from Content assets so equipping works at runtime. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	DECLARE_DELEGATE_RetVal_TwoParams(bool, FMovementProjectionPreflight,
		const FHSREquipmentMovementRequest&, const FHSREquipmentLoadout&);
	DECLARE_DELEGATE_TwoParams(FMovementProjectionCommit,
		const FHSREquipmentMovementRequest&, const FHSREquipmentLoadout&);
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FMovementProjectionApply,
		const FHSREquipmentMovementRequest&, const FHSREquipmentLoadout&);
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FEnhancementProjectionPreflight,
		const FHSREquipmentEnhancementRequest&, const FHSREquipmentInstance&);
	DECLARE_DELEGATE_TwoParams(FEnhancementProjectionCommit,
		const FHSREquipmentEnhancementRequest&, const FHSREquipmentInstance&);

	void ExportSaveData(TArray<struct FHSREquipmentSaveDto>& Out) const;
	void ExportSaveData(TArray<struct FHSREquipmentRegistryDto>& OutRegistry, TArray<struct FHSREquipmentPlacementDto>& OutPlacements) const;
	bool PrepareRestore(const TArray<struct FHSREquipmentSaveDto>& In, FHSREquipmentRestoreMap& Out) const;
	bool PrepareRestore(const TArray<struct FHSREquipmentRegistryDto>& Registry, const TArray<struct FHSREquipmentPlacementDto>& Placements, FHSREquipmentRegistryRestoreState& Out) const;
	void CommitRestore(const FHSREquipmentRestoreMap& Candidate);
	void CommitRestore(const FHSREquipmentRegistryRestoreState& Candidate);
	void NotifyRestored(const TSet<FGuid>& Changed);
	void SetRestoreProjection(FHSREquipmentRestoreProjection InProjection) { RestoreProjection=MoveTemp(InProjection); }
	bool ProjectRestore(const FHSREquipmentRestoreMap& Candidate) const { return !RestoreProjection.IsBound() || RestoreProjection.Execute(Candidate); }
	EHSREquipmentOperationResult RegisterDefinition(const UHSREquipmentDefinition& Definition);
	EHSREquipmentOperationResult RegisterDefinition(const UHSRRelicDefinition& Definition);

	/**
	 * Registers a relic set's authored activation threshold.  Relic definitions only carry a SetId,
	 * so without this the subsystem has no way to read Threshold off UHSRRelicSetDefinition and every
	 * consumer falls back to the two-piece default -- raising a set's Threshold would then be honoured
	 * in some code paths and silently ignored in others.
	 */
	EHSREquipmentOperationResult RegisterSetDefinition(const UHSRRelicSetDefinition& Definition);

	/** Authored threshold for a set, or the two-piece default when the set was never registered. */
	int32 GetSetThreshold(FName SetId) const;
	bool HasDefinition(FName DefinitionId) const { return Definitions.Contains(DefinitionId); }
	bool IsDefinitionCompatible(FName DefinitionId,EHSREquipmentKind Kind,int32 Slot) const;
	EHSREquipmentOperationResult RegisterInstance(const FHSREquipmentInstance& Instance);
	/**
	 * Ensures an equipment instance exists for an inventory unique item (e.g. a relic dropped from
	 * a chest or reward).  The instance is minted from the mapping when absent, so callers can rely
	 * on it before equipping without requiring a separate registration step.
	 */
	EHSREquipmentOperationResult EnsureRegisteredFromItem(FName ItemId, const FGuid& InstanceId,
		const UHSRItemEquipmentMappingCatalog& MappingCatalog);
	bool FindRegisteredInstance(const FGuid& InstanceId, FHSREquipmentInstance& OutInstance) const;
	EHSREquipmentOperationResult EquipById(const FGuid& CharacterId, const FGuid& InstanceId);
	EHSREquipmentOperationResult ReplaceById(const FGuid& CharacterId, const FGuid& InstanceId);
	FHSREquipmentMovementResult ExecuteMovement(const FHSREquipmentMovementRequest& Request,
		UHSRInventorySubsystem& Inventory, const UHSRItemEquipmentMappingCatalog& MappingCatalog);
	FHSREquipmentEnhancementResult ExecuteEnhancement(const FHSREquipmentEnhancementRequest& Request,
		UHSRInventorySubsystem& Inventory, const UHSREquipmentEnhancementCatalog& Catalog);
	void SetMovementProjection(FMovementProjectionPreflight InPreflight, FMovementProjectionCommit InCommit)
	{
		MovementProjectionPreflight = MoveTemp(InPreflight);
		MovementProjectionApply.Unbind();
		MovementProjectionCommit = MoveTemp(InCommit);
	}
	void SetMovementProjection(FMovementProjectionPreflight InPreflight,FMovementProjectionApply InApply,FMovementProjectionCommit InCommit)
	{
		MovementProjectionPreflight=MoveTemp(InPreflight);MovementProjectionApply=MoveTemp(InApply);MovementProjectionCommit=MoveTemp(InCommit);
	}
	void SetEnhancementProjection(FEnhancementProjectionPreflight InPreflight,
		FEnhancementProjectionCommit InCommit)
	{
		EnhancementProjectionPreflight = MoveTemp(InPreflight);
		EnhancementProjectionCommit = MoveTemp(InCommit);
	}

	EHSREquipmentOperationResult Equip(const FGuid& CharacterId, const FHSREquipmentInstance& Instance);
	EHSREquipmentOperationResult Replace(const FGuid& CharacterId, const FHSREquipmentInstance& Instance);
	EHSREquipmentOperationResult Unequip(const FGuid& CharacterId, EHSREquipmentKind Kind, int32 Slot, const FGuid& ExpectedInstanceId);
	EHSREquipmentOperationResult SetEnhancementLevel(const FGuid& CharacterId, const FGuid& InstanceId, int32 NewLevel);

	bool GetLoadout(const FGuid& CharacterId, FHSREquipmentLoadout& OutLoadout, int32& OutRevision) const;
	bool FindInstanceOwner(const FGuid& InstanceId, FGuid& OutCharacterId) const;
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
		TMap<EHSREquipmentSlot, FGuid> Equipment;
		TMap<EHSRRelicSlot, FGuid> Relics;
		int32 Revision = 0;
	};
	struct FMovementLedgerEntry
	{
		FHSREquipmentMovementRequest Request;
		FHSREquipmentMovementResult Result;
	};
	struct FEnhancementLedgerEntry
	{
		FHSREquipmentEnhancementRequest Request;
		FHSREquipmentEnhancementResult Result;
	};

	/** Shared cap for the movement and enhancement idempotency ledgers. */
	static constexpr int32 MaxLedgerEntries = 128;

	/** Records one entry and evicts the oldest once the ledger exceeds MaxLedgerEntries. */
	template <typename TLedger, typename TEntry>
	static void RecordLedgerEntry(TLedger& Ledger, TArray<FGuid>& Order, const FGuid& OperationId, TEntry&& Entry)
	{
		Ledger.Add(OperationId, Forward<TEntry>(Entry));
		Order.Add(OperationId);
		if (Order.Num() > MaxLedgerEntries)
		{
			Ledger.Remove(Order[0]);
			Order.RemoveAt(0);
		}
	}

	/**
	 * Validates one restored instance against its authored definition. Shared by both PrepareRestore
	 * overloads so a blob is accepted or rejected identically regardless of which schema wrote it.
	 */
	bool ValidateRestoreInstance(FName DefinitionId, EHSREquipmentKind Kind, int32 Slot,
		int32 EnhancementLevel, FName SetId, const TArray<FHSREquipmentModifier>& Modifiers) const;

	/** Places a validated instance into a per-character restore state, rejecting duplicate slots. */
	static bool InsertIntoRestoreState(FHSREquipmentRestoreState& State,
		const FHSREquipmentInstance& Instance, int32 Slot, FName SetId);

	bool IsValidInstance(const FHSREquipmentInstance& Instance) const;
	bool IsValidModifiers(const TArray<FHSREquipmentModifier>& Modifiers) const;
	const FDefinitionRule* FindDefinition(const FHSREquipmentInstance& Instance) const;
	bool IsSlotValid(EHSREquipmentKind Kind, int32 Slot) const;
	bool IsSlotOccupied(const FLoadoutState& Loadout, EHSREquipmentKind Kind, int32 Slot) const;
	const FGuid* FindPlacedInstance(const FLoadoutState& Loadout, EHSREquipmentKind Kind, int32 Slot) const;
	void CommitLoadout(const FGuid& CharacterId, FLoadoutState Candidate);
	bool ResolveLoadout(const FLoadoutState& State, FHSREquipmentLoadout& OutLoadout) const;
	static bool IsSamePayload(const FHSREquipmentInstance& A, const FHSREquipmentInstance& B);
	static bool IsSameMovementRequest(const FHSREquipmentMovementRequest& A, const FHSREquipmentMovementRequest& B);
	static bool IsSameEnhancementRequest(const FHSREquipmentEnhancementRequest& A,
		const FHSREquipmentEnhancementRequest& B);

	TMap<FName, FDefinitionRule> Definitions;
	TMap<FName, int32> SetThresholds;
	TMap<FGuid, FHSREquipmentInstance> InstanceRegistry;
	TMap<FGuid, FLoadoutState> Loadouts;
	TMap<FGuid, FGuid> InstanceOwners;
	FHSREquipmentLoadoutChanged LoadoutChanged;
	FHSREquipmentRestoreProjection RestoreProjection;
	TMap<FGuid, FMovementLedgerEntry> MovementLedger;
	TArray<FGuid> MovementLedgerOrder;
	TMap<FGuid, FEnhancementLedgerEntry> EnhancementLedger;
	TArray<FGuid> EnhancementLedgerOrder;
	FMovementProjectionPreflight MovementProjectionPreflight;
	FMovementProjectionApply MovementProjectionApply;
	FMovementProjectionCommit MovementProjectionCommit;
	FEnhancementProjectionPreflight EnhancementProjectionPreflight;
	FEnhancementProjectionCommit EnhancementProjectionCommit;
};
