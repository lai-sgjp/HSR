#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSRItemTypes.h"
#include "HSRInventorySubsystem.generated.h"

class UHSRItemDefinition;
class UHSRRewardSubsystem;
class UHSRSettlementAuthority;
class UHSREquipmentSubsystem;
struct FHSRInventorySettlementCandidate;

UCLASS()
class HSR_API UHSRInventorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	EHSRInventoryOperationResult CanRegisterDefinition(const UHSRItemDefinition& Definition) const;
	EHSRInventoryOperationResult RegisterDefinition(const UHSRItemDefinition& Definition);
	EHSRInventoryOperationResult AddStack(FName ItemId, int32 Quantity);
	EHSRInventoryOperationResult RemoveStack(FName ItemId, int32 Quantity);
	EHSRInventoryOperationResult AddUnique(const FHSRItemInstance& Instance);
	EHSRInventoryOperationResult RemoveUnique(const FGuid& InstanceId);
	EHSRInventoryOperationResult ApplyGrants(const TArray<FHSRInventoryGrant>& Grants);
	bool GetDefinitionInfo(FName ItemId, EHSRItemStorageKind& OutStorageKind, int32& OutMaxStack) const;
	bool HasDefinition(FName ItemId) const { return Definitions.Contains(ItemId); }
	void GetSnapshot(FHSRInventorySnapshot& OutSnapshot) const;
	void ExportSaveData(FHSRInventorySaveData& OutData) const;
	bool PrepareRestore(const FHSRInventorySaveData& Data, FHSRInventoryRestoreState& OutCandidate) const;
	bool IsRestoreDifferent(const FHSRInventoryRestoreState& Candidate) const;
	void CommitRestore(FHSRInventoryRestoreState&& Candidate, bool bNotify);
	FHSRInventoryChanged& OnInventoryChanged() { return InventoryChanged; }

#if WITH_DEV_AUTOMATION_TESTS
	bool SetCapacityForAutomation(int32 NewCapacity);
#endif

private:
	friend class UHSRRewardSubsystem;
	friend class UHSRSettlementAuthority;
	friend class UHSREquipmentSubsystem;
	struct FDefinitionRule
	{
		EHSRItemStorageKind StorageKind = EHSRItemStorageKind::Stackable;
		int32 MaxStack = 1;
	};

	int32 GetUsedSlots(const TMap<FName, int32>& CandidateStacks, const TMap<FGuid, FHSRItemInstance>& CandidateUniqueItems) const;
	/**
	 * Applies grants onto candidate state without touching authoritative storage. Shared by the
	 * direct-grant and settlement-transaction paths so both reject identical input identically.
	 */
	EHSRInventoryOperationResult ApplyGrantsToCandidate(const TArray<FHSRInventoryGrant>& Grants,
		TMap<FName, int32>& CandidateStacks, TMap<FGuid, FHSRItemInstance>& CandidateUniqueItems) const;

	EHSRInventoryOperationResult ApplyGrantsInternal(const TArray<FHSRInventoryGrant>& Grants, bool bBroadcast, int64& OutRevision);
	void BroadcastRevision(int64 CommittedRevision) { InventoryChanged.Broadcast(CommittedRevision); }
	void Commit(TMap<FName, int32>&& CandidateStacks, TMap<FGuid, FHSRItemInstance>&& CandidateUniqueItems);
	EHSRInventoryOperationResult PrepareSettlementCandidate(const FGuid& TransactionId,
		const TArray<FHSRInventoryGrant>& Grants, int64 ExpectedRevision,
		FHSRInventorySettlementCandidate& OutCandidate) const;
	void InstallSettlementCandidateNoFail(FHSRInventorySettlementCandidate&& Candidate);
	void FinalizeSettlementRevisionNoFail(int64 PreparedRevision);
	void PublishSettlementCommit(int64 PreparedRevision);
	EHSRInventoryOperationResult PrepareEquipmentRemovalCandidate(const FGuid& InstanceId, FName ExpectedItemId,
		int64 ExpectedRevision, FHSRInventoryMovementCandidate& OutCandidate) const;
	EHSRInventoryOperationResult PrepareEquipmentAdditionCandidate(const FGuid& InstanceId, FName ItemId,
		int64 ExpectedRevision, FHSRInventoryMovementCandidate& OutCandidate) const;
	EHSRInventoryOperationResult PrepareEquipmentSwapCandidate(const FGuid& IncomingInstanceId, FName IncomingItemId,
		const FGuid& DisplacedInstanceId, FName DisplacedItemId, int64 ExpectedRevision,
		FHSRInventoryMovementCandidate& OutCandidate) const;
	EHSRInventoryOperationResult PrepareEquipmentEnhancementCandidate(FName MaterialItemId, int32 MaterialCost,
		int64 ExpectedRevision, FHSRInventoryEnhancementCandidate& OutCandidate) const;
	void InstallEquipmentMovementCandidateNoFail(FHSRInventoryMovementCandidate&& Candidate);
	void FinalizeEquipmentMovementRevisionNoFail(int64 PreparedRevision);
	void PublishEquipmentMovementCommit(int64 PreparedRevision);
	void InstallEquipmentEnhancementCandidateNoFail(FHSRInventoryEnhancementCandidate&& Candidate);
	void FinalizeEquipmentEnhancementRevisionNoFail(int64 PreparedRevision);
	void PublishEquipmentEnhancementCommit(int64 PreparedRevision);

	TMap<FName, FDefinitionRule> Definitions;
	TMap<FName, int32> Stacks;
	TMap<FGuid, FHSRItemInstance> UniqueItems;
	int32 Capacity = 100;
	int64 Revision = 0;
	FHSRInventoryChanged InventoryChanged;
};
