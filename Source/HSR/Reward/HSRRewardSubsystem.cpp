#include "HSRRewardSubsystem.h"

#include "HSRRewardResolver.h"
#include "../Data/Definitions/HSRDropTableDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "Misc/Crc.h"
#include "HSRSettlementTypes.h"

namespace
{
	bool AreDropEntriesEqual(const TArray<FHSRDropTableEntry>& A, const TArray<FHSRDropTableEntry>& B)
	{
		if (A.Num() != B.Num()) return false;
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].ItemId != B[Index].ItemId || A[Index].MinQuantity != B[Index].MinQuantity || A[Index].MaxQuantity != B[Index].MaxQuantity || A[Index].Weight != B[Index].Weight) return false;
		}
		return true;
	}

	bool AreRewardEntriesEqual(const TArray<FHSRRewardItemEntry>& A, const TArray<FHSRRewardItemEntry>& B)
	{
		if (A.Num() != B.Num()) return false;
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].ItemId != B[Index].ItemId || A[Index].Quantity != B[Index].Quantity) return false;
		}
		return true;
	}
}

void UHSRRewardSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Inventory = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRInventorySubsystem>() : nullptr;
}

#if WITH_DEV_AUTOMATION_TESTS
void UHSRRewardSubsystem::InitializeForAutomation(UHSRInventorySubsystem* InInventory)
{
	Inventory = InInventory;
}
#endif

#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
void UHSRRewardSubsystem::InitializeForDevelopmentTest(UHSRInventorySubsystem* InInventory)
{
	Inventory = InInventory;
}
#endif

EHSRRewardOperationResult UHSRRewardSubsystem::RegisterDropTable(const UHSRDropTableDefinition& Definition)
{
	const EHSRRewardOperationResult Validation = CanRegisterDropTable(Definition);
	if (Validation != EHSRRewardOperationResult::Success)
	{
		return Validation;
	}
	DropTables.Add(Definition.DropTableId, {Definition.DropTableId, Definition.Entries});
	return EHSRRewardOperationResult::Success;
}

EHSRRewardOperationResult UHSRRewardSubsystem::CanRegisterDropTable(const UHSRDropTableDefinition& Definition) const
{
	if (Definition.DropTableId.IsNone())
	{
		return EHSRRewardOperationResult::InvalidDefinitionId;
	}
	if (Definition.Entries.IsEmpty() || Definition.Entries.Num() > FHSRRewardResolver::MaxDefinitionEntries)
	{
		return EHSRRewardOperationResult::InvalidDefinition;
	}
	TSet<FName> SeenItems;
	int32 TotalWeight = 0;
	for (const FHSRDropTableEntry& Entry : Definition.Entries)
	{
		if (Entry.ItemId.IsNone() || SeenItems.Contains(Entry.ItemId) || Entry.MinQuantity <= 0 || Entry.MaxQuantity < Entry.MinQuantity || Entry.Weight <= 0 || Entry.Weight > MAX_int32 - TotalWeight)
		{
			return EHSRRewardOperationResult::InvalidDefinition;
		}
		SeenItems.Add(Entry.ItemId);
		TotalWeight += Entry.Weight;
	}
	if (const FHSRDropTableRule* Existing = DropTables.Find(Definition.DropTableId))
	{
		return AreDropEntriesEqual(Existing->Entries, Definition.Entries)
			? EHSRRewardOperationResult::NoOp
			: EHSRRewardOperationResult::DuplicateDefinitionId;
	}
	return EHSRRewardOperationResult::Success;
}

EHSRRewardOperationResult UHSRRewardSubsystem::RegisterRewardDefinition(const UHSRRewardDefinition& Definition)
{
	const EHSRRewardOperationResult Validation = CanRegisterRewardDefinition(Definition);
	if (Validation != EHSRRewardOperationResult::Success)
	{
		return Validation;
	}
	Rewards.Add(Definition.RewardDefinitionId, {Definition.RewardDefinitionId, Definition.FixedItems, Definition.DropTableId, Definition.DropRolls});
	return EHSRRewardOperationResult::Success;
}

EHSRRewardOperationResult UHSRRewardSubsystem::CanRegisterRewardDefinition(const UHSRRewardDefinition& Definition, FName AdditionalDropTableId) const
{
	if (Definition.RewardDefinitionId.IsNone())
	{
		return EHSRRewardOperationResult::InvalidDefinitionId;
	}
	if (Definition.FixedItems.Num() > FHSRRewardResolver::MaxDefinitionEntries || Definition.DropRolls < 0 || Definition.DropRolls > FHSRRewardResolver::MaxDropRolls || (Definition.DropTableId.IsNone() != (Definition.DropRolls == 0)) || (Definition.FixedItems.IsEmpty() && Definition.DropRolls == 0))
	{
		return EHSRRewardOperationResult::InvalidDefinition;
	}
	TSet<FName> SeenFixedItems;
	for (const FHSRRewardItemEntry& Entry : Definition.FixedItems)
	{
		if (Entry.ItemId.IsNone() || SeenFixedItems.Contains(Entry.ItemId) || Entry.Quantity <= 0)
		{
			return EHSRRewardOperationResult::InvalidDefinition;
		}
		SeenFixedItems.Add(Entry.ItemId);
	}
	if (!Definition.DropTableId.IsNone() && !DropTables.Contains(Definition.DropTableId) && Definition.DropTableId != AdditionalDropTableId)
	{
		return EHSRRewardOperationResult::UnknownDropTable;
	}
	if (const FHSRRewardDefinitionRule* Existing = Rewards.Find(Definition.RewardDefinitionId))
	{
		return Existing->DropTableId == Definition.DropTableId && Existing->DropRolls == Definition.DropRolls && AreRewardEntriesEqual(Existing->FixedItems, Definition.FixedItems)
			? EHSRRewardOperationResult::NoOp
			: EHSRRewardOperationResult::DuplicateDefinitionId;
	}
	return EHSRRewardOperationResult::Success;
}

EHSRRewardOperationResult UHSRRewardSubsystem::CanRegisterBundle(
	const TArray<TObjectPtr<UHSRItemDefinition>>& ItemDefinitions,
	const UHSRDropTableDefinition& DropTable,
	const UHSRRewardDefinition& RewardDefinition) const
{
	UHSRInventorySubsystem* InventorySubsystem = Inventory.Get();
	if (!InventorySubsystem)
	{
		return EHSRRewardOperationResult::InventoryRejected;
	}
	bool bAnyChange = false;
	TMap<FName, const UHSRItemDefinition*> BundleItems;
	for (const UHSRItemDefinition* ItemDefinition : ItemDefinitions)
	{
		if (!ItemDefinition)
		{
			return EHSRRewardOperationResult::InvalidDefinition;
		}
		if (const UHSRItemDefinition* const* ExistingBundleItem = BundleItems.Find(ItemDefinition->ItemId))
		{
			if ((*ExistingBundleItem)->StorageKind != ItemDefinition->StorageKind || (*ExistingBundleItem)->MaxStack != ItemDefinition->MaxStack)
			{
				return EHSRRewardOperationResult::InvalidDefinition;
			}
		}
		else
		{
			BundleItems.Add(ItemDefinition->ItemId, ItemDefinition);
		}
		const EHSRInventoryOperationResult Result = InventorySubsystem->CanRegisterDefinition(*ItemDefinition);
		if (Result != EHSRInventoryOperationResult::Success && Result != EHSRInventoryOperationResult::NoOp)
		{
			return EHSRRewardOperationResult::InvalidDefinition;
		}
		bAnyChange |= Result == EHSRInventoryOperationResult::Success;
	}
	auto IsItemKnown = [InventorySubsystem, &BundleItems](FName ItemId)
	{
		EHSRItemStorageKind StorageKind;
		int32 MaxStack = 0;
		return BundleItems.Contains(ItemId) || InventorySubsystem->GetDefinitionInfo(ItemId, StorageKind, MaxStack);
	};
	for (const FHSRDropTableEntry& Entry : DropTable.Entries)
	{
		if (!IsItemKnown(Entry.ItemId))
		{
			UE_LOG(LogTemp, Error, TEXT("P13-003 RewardBundle Result=REJECTED Reason=UnknownDropItem ItemId=%s"), *Entry.ItemId.ToString());
			return EHSRRewardOperationResult::InvalidDefinition;
		}
	}
	for (const FHSRRewardItemEntry& Entry : RewardDefinition.FixedItems)
	{
		if (!IsItemKnown(Entry.ItemId))
		{
			UE_LOG(LogTemp, Error, TEXT("P13-003 RewardBundle Result=REJECTED Reason=UnknownFixedItem ItemId=%s"), *Entry.ItemId.ToString());
			return EHSRRewardOperationResult::InvalidDefinition;
		}
	}
	const EHSRRewardOperationResult DropValidation = CanRegisterDropTable(DropTable);
	if (DropValidation != EHSRRewardOperationResult::Success && DropValidation != EHSRRewardOperationResult::NoOp)
	{
		return DropValidation;
	}
	const EHSRRewardOperationResult RewardValidation = CanRegisterRewardDefinition(RewardDefinition, DropTable.DropTableId);
	if (RewardValidation != EHSRRewardOperationResult::Success && RewardValidation != EHSRRewardOperationResult::NoOp)
	{
		return RewardValidation;
	}

	bAnyChange |= DropValidation == EHSRRewardOperationResult::Success || RewardValidation == EHSRRewardOperationResult::Success;
	return bAnyChange ? EHSRRewardOperationResult::Success : EHSRRewardOperationResult::NoOp;
}

EHSRRewardOperationResult UHSRRewardSubsystem::RegisterBundle(
	const TArray<TObjectPtr<UHSRItemDefinition>>& ItemDefinitions,
	const UHSRDropTableDefinition& DropTable,
	const UHSRRewardDefinition& RewardDefinition)
{
	const EHSRRewardOperationResult Validation = CanRegisterBundle(ItemDefinitions, DropTable, RewardDefinition);
	if (Validation != EHSRRewardOperationResult::Success && Validation != EHSRRewardOperationResult::NoOp)
	{
		return Validation;
	}

	UHSRInventorySubsystem* InventorySubsystem = Inventory.Get();
	check(InventorySubsystem);
	for (const UHSRItemDefinition* ItemDefinition : ItemDefinitions)
	{
		InventorySubsystem->RegisterDefinition(*ItemDefinition);
	}
	RegisterDropTable(DropTable);
	RegisterRewardDefinition(RewardDefinition);
	return Validation;
}

EHSRRewardOperationResult UHSRRewardSubsystem::SubmitReward(const FHSRRewardRequest& Request, FHSRRewardReceipt& OutReceipt)
{
	if (!Request.ClaimId.IsValid())
	{
		return EHSRRewardOperationResult::InvalidClaimId;
	}
	if (Request.RewardDefinitionId.IsNone())
	{
		return EHSRRewardOperationResult::InvalidDefinitionId;
	}
	if (const FHSRRewardReceipt* Existing = Receipts.Find(Request.ClaimId))
	{
		if (Existing->Request.RewardDefinitionId != Request.RewardDefinitionId || Existing->Request.Seed != Request.Seed)
		{
			return EHSRRewardOperationResult::ClaimConflict;
		}
		OutReceipt = *Existing;
		return EHSRRewardOperationResult::NoOp;
	}
	const FHSRRewardDefinitionRule* Reward = Rewards.Find(Request.RewardDefinitionId);
	if (!Reward)
	{
		return EHSRRewardOperationResult::UnknownRewardDefinition;
	}
	if (!Inventory.IsValid())
	{
		return EHSRRewardOperationResult::InventoryRejected;
	}

	TArray<FHSRInventoryGrant> Grants;
	if (!BuildGrants(Request, *Reward, Grants))
	{
		return EHSRRewardOperationResult::ResolveFailed;
	}
#if WITH_DEV_AUTOMATION_TESTS
	if (bInjectCommitFailure)
	{
		return EHSRRewardOperationResult::InjectedFailure;
	}
#endif
	int64 InventoryRevision = 0;
	if (Inventory->ApplyGrantsInternal(Grants, false, InventoryRevision) != EHSRInventoryOperationResult::Success)
	{
		return EHSRRewardOperationResult::InventoryRejected;
	}

	FHSRRewardReceipt Receipt;
	Receipt.Request = Request;
	Receipt.Grants = MoveTemp(Grants);
	Receipt.Revision = ++Revision;
	Receipts.Add(Request.ClaimId, Receipt);
	OutReceipt = Receipt;
	Inventory->BroadcastRevision(InventoryRevision);
	RewardCommitted.Broadcast(Receipt);
	return EHSRRewardOperationResult::Success;
}

bool UHSRRewardSubsystem::GetReceipt(const FGuid& ClaimId, FHSRRewardReceipt& OutReceipt) const
{
	const FHSRRewardReceipt* Receipt = Receipts.Find(ClaimId);
	if (!Receipt)
	{
		return false;
	}
	OutReceipt = *Receipt;
	return true;
}

void UHSRRewardSubsystem::GetReceipts(TArray<FHSRRewardReceipt>& OutReceipts) const
{
	Receipts.GenerateValueArray(OutReceipts);
	OutReceipts.Sort([](const FHSRRewardReceipt& A, const FHSRRewardReceipt& B)
	{
		return A.Request.ClaimId < B.Request.ClaimId;
	});
}

void UHSRRewardSubsystem::ExportSaveData(FHSRRewardSaveData& OutData) const
{
	GetReceipts(OutData.Receipts);
	OutData.Revision = Revision;
}

bool UHSRRewardSubsystem::PrepareRestore(const FHSRRewardSaveData& Data, FHSRRewardRestoreState& OutCandidate) const
{
	if (Data.Revision < 0)
	{
		return false;
	}
	FHSRRewardRestoreState Candidate;
	Candidate.Revision = Data.Revision;
	int64 MaxReceiptRevision = 0;
	TSet<int64> SeenReceiptRevisions;
	TSet<FGuid> SeenInstances;
	for (const FHSRRewardReceipt& Receipt : Data.Receipts)
	{
		if (!Receipt.Request.ClaimId.IsValid() || Receipt.Request.RewardDefinitionId.IsNone() || !Rewards.Contains(Receipt.Request.RewardDefinitionId)
			|| Receipt.Revision <= 0 || Receipt.Revision > Data.Revision || SeenReceiptRevisions.Contains(Receipt.Revision)
			|| Candidate.Receipts.Contains(Receipt.Request.ClaimId) || Receipt.Grants.IsEmpty())
		{
			return false;
		}
		SeenReceiptRevisions.Add(Receipt.Revision);
		for (const FHSRInventoryGrant& Grant : Receipt.Grants)
		{
			EHSRItemStorageKind StorageKind;
			int32 MaxStack = 0;
			if (Grant.ItemId.IsNone() || Grant.Quantity <= 0 || !Inventory.IsValid() || !Inventory->GetDefinitionInfo(Grant.ItemId, StorageKind, MaxStack)
				|| (StorageKind == EHSRItemStorageKind::Stackable && !Grant.InstanceIds.IsEmpty())
				|| (StorageKind == EHSRItemStorageKind::Unique && Grant.InstanceIds.Num() != Grant.Quantity))
			{
				return false;
			}
			for (const FGuid& InstanceId : Grant.InstanceIds)
			{
				if (!InstanceId.IsValid() || SeenInstances.Contains(InstanceId)) return false;
				SeenInstances.Add(InstanceId);
			}
		}
		MaxReceiptRevision = FMath::Max(MaxReceiptRevision, Receipt.Revision);
		Candidate.Receipts.Add(Receipt.Request.ClaimId, Receipt);
	}
	if (Data.Revision != Data.Receipts.Num() || MaxReceiptRevision != Data.Revision)
	{
		return false;
	}
	OutCandidate = MoveTemp(Candidate);
	return true;
}

bool UHSRRewardSubsystem::IsRestoreDifferent(const FHSRRewardRestoreState& Candidate) const
{
	if (Revision != Candidate.Revision || Receipts.Num() != Candidate.Receipts.Num()) return true;
	for (const TPair<FGuid, FHSRRewardReceipt>& Entry : Receipts)
	{
		const FHSRRewardReceipt* Other = Candidate.Receipts.Find(Entry.Key);
		if (!Other || Other->Request.RewardDefinitionId != Entry.Value.Request.RewardDefinitionId || Other->Request.Seed != Entry.Value.Request.Seed
			|| Other->Revision != Entry.Value.Revision || Other->Grants.Num() != Entry.Value.Grants.Num()) return true;
		for (int32 Index = 0; Index < Entry.Value.Grants.Num(); ++Index)
		{
			const FHSRInventoryGrant& A = Entry.Value.Grants[Index];
			const FHSRInventoryGrant& B = Other->Grants[Index];
			if (A.ItemId != B.ItemId || A.Quantity != B.Quantity || A.InstanceIds != B.InstanceIds) return true;
		}
	}
	return false;
}

void UHSRRewardSubsystem::CommitRestore(FHSRRewardRestoreState&& Candidate, bool bNotify)
{
	Receipts = MoveTemp(Candidate.Receipts);
	Revision = Candidate.Revision;
	if (bNotify)
	{
		RewardRestored.Broadcast(Revision);
	}
}

EHSRRewardOperationResult UHSRRewardSubsystem::PrepareSettlementCandidate(const FHSRSettlementRequest& Request,
	FHSRRewardSettlementCandidate& OutCandidate, FHSRSettlementReceipt& OutPreparedReceipt,
	TArray<FHSRInventoryGrant>& OutGrants, FHSRSettlementReceipt& OutExistingReceipt) const
{
	if (const FHSRSettlementReceipt* Existing = SettlementLedger.Find(Request.TransactionId))
	{
		if (Existing->RewardDefinitionId != Request.RewardDefinitionId
			|| Existing->PlayerCharacterId != Request.PlayerCharacterId
			|| Existing->RewardSeed != Request.RewardSeed
			|| Existing->Experience != Request.Experience
			|| Existing->ExpectedInventoryRevision != Request.ExpectedInventoryRevision
			|| Existing->ExpectedProfileRevision != Request.ExpectedProfileRevision
			|| Existing->ExpectedRewardRevision != Request.ExpectedRewardRevision)
		{
			return EHSRRewardOperationResult::ClaimConflict;
		}
		OutExistingReceipt = *Existing;
		return EHSRRewardOperationResult::NoOp;
	}
	if (Request.ExpectedRewardRevision != Revision)
	{
		return EHSRRewardOperationResult::RevisionConflict;
	}
	const FHSRRewardDefinitionRule* Rule = Rewards.Find(Request.RewardDefinitionId);
	if (!Rule)
	{
		return EHSRRewardOperationResult::UnknownRewardDefinition;
	}
	FHSRRewardRequest RewardRequest;
	RewardRequest.ClaimId = Request.TransactionId;
	RewardRequest.RewardDefinitionId = Request.RewardDefinitionId;
	RewardRequest.Seed = Request.RewardSeed;
	if (!BuildGrants(RewardRequest, *Rule, OutGrants))
	{
		return EHSRRewardOperationResult::ResolveFailed;
	}

	FHSRRewardSettlementCandidate Candidate;
	Candidate.TransactionId = Request.TransactionId;
	Candidate.Receipts = Receipts;
	Candidate.SettlementLedger = SettlementLedger;
	Candidate.NextRevision = Revision + 1;
	Candidate.PublishedRewardReceipt.Request = RewardRequest;
	Candidate.PublishedRewardReceipt.Grants = OutGrants;
	Candidate.PublishedRewardReceipt.Revision = Candidate.NextRevision;
	Candidate.Receipts.Add(Request.TransactionId, Candidate.PublishedRewardReceipt);

	FHSRSettlementReceipt Receipt;
	Receipt.TransactionId = Request.TransactionId;
	Receipt.RewardDefinitionId = Request.RewardDefinitionId;
	Receipt.PlayerCharacterId = Request.PlayerCharacterId;
	Receipt.RewardSeed = Request.RewardSeed;
	Receipt.Experience = Request.Experience;
	Receipt.ExpectedInventoryRevision = Request.ExpectedInventoryRevision;
	Receipt.ExpectedProfileRevision = Request.ExpectedProfileRevision;
	Receipt.ExpectedRewardRevision = Request.ExpectedRewardRevision;
	Receipt.RewardReceipt = Candidate.PublishedRewardReceipt;
	Receipt.RewardRevision = Candidate.NextRevision;
	Candidate.SettlementLedger.Add(Request.TransactionId, Receipt);
	OutPreparedReceipt = Receipt;
	OutCandidate = MoveTemp(Candidate);
	return EHSRRewardOperationResult::Success;
}

void UHSRRewardSubsystem::InstallSettlementCandidateNoFail(FHSRRewardSettlementCandidate&& Candidate)
{
	Receipts = MoveTemp(Candidate.Receipts);
	SettlementLedger = MoveTemp(Candidate.SettlementLedger);
}

void UHSRRewardSubsystem::FinalizeSettlementRevisionNoFail(int64 PreparedRevision)
{
	Revision = PreparedRevision;
}

void UHSRRewardSubsystem::PublishSettlementCommit(const FHSRRewardReceipt& PreparedReceipt, int64 PreparedRevision)
{
	RewardCommitted.Broadcast(PreparedReceipt);
}

FGuid UHSRRewardSubsystem::MakeInstanceId(const FGuid& ClaimId, FName ItemId, int32 Ordinal)
{
	const uint32 ItemHash = FCrc::StrCrc32(*ItemId.ToString());
	FGuid Result(ClaimId.A ^ ItemHash, ClaimId.B ^ static_cast<uint32>(Ordinal + 1), ClaimId.C ^ (ItemHash << 1), ClaimId.D ^ static_cast<uint32>(Ordinal + 17));
	if (!Result.IsValid())
	{
		Result.D = 1;
	}
	return Result;
}

bool UHSRRewardSubsystem::BuildGrants(const FHSRRewardRequest& Request, const FHSRRewardDefinitionRule& Reward, TArray<FHSRInventoryGrant>& OutGrants) const
{
	const FHSRDropTableRule* DropTable = Reward.DropTableId.IsNone() ? nullptr : DropTables.Find(Reward.DropTableId);
	TArray<FHSRRewardItemEntry> ResolvedItems;
	if (!FHSRRewardResolver::Resolve(Reward, DropTable, Request.Seed, ResolvedItems))
	{
		return false;
	}

	OutGrants.Reset();
	FHSRInventorySnapshot InventorySnapshot;
	Inventory->GetSnapshot(InventorySnapshot);
	int32 UniqueOrdinal = 0;
	int32 TotalUniqueInstances = 0;
	for (const FHSRRewardItemEntry& Item : ResolvedItems)
	{
		EHSRItemStorageKind StorageKind;
		int32 MaxStack = 0;
		if (!Inventory->GetDefinitionInfo(Item.ItemId, StorageKind, MaxStack))
		{
			UE_LOG(LogTemp, Warning, TEXT("P13-003 RewardResolve Result=FAILED Reason=UnknownItem ItemId=%s ClaimId=%s"), *Item.ItemId.ToString(), *Request.ClaimId.ToString());
			return false;
		}
		FHSRInventoryGrant Grant;
		Grant.ItemId = Item.ItemId;
		Grant.Quantity = Item.Quantity;
		if (StorageKind == EHSRItemStorageKind::Unique)
		{
			if (Item.Quantity > InventorySnapshot.Capacity - TotalUniqueInstances)
			{
				return false;
			}
			TotalUniqueInstances += Item.Quantity;
			for (int32 Index = 0; Index < Item.Quantity; ++Index)
			{
				Grant.InstanceIds.Add(MakeInstanceId(Request.ClaimId, Item.ItemId, UniqueOrdinal++));
			}
		}
		OutGrants.Add(MoveTemp(Grant));
	}
	return true;
}
