#pragma once

#include "CoreMinimal.h"
#include "HSRRewardTypes.h"
#include "../Inventory/HSRItemTypes.h"
#include "../Progression/HSRCharacterProfileTypes.h"

enum class EHSRSettlementResult : uint8
{
	Success,
	NoOp,
	InvalidTransactionId,
	InvalidRequest,
	MissingAuthority,
	StaleRevision,
	TransactionConflict,
	RewardRejected,
	InventoryRejected,
	ProfileRejected,
	InjectedPrepareFailure
};

struct FHSRSettlementRequest
{
	FGuid TransactionId;
	FName RewardDefinitionId;
	FName PlayerCharacterId;
	int32 RewardSeed = 0;
	int32 Experience = 0;
	int64 ExpectedInventoryRevision = 0;
	int64 ExpectedProfileRevision = 0;
	int64 ExpectedRewardRevision = 0;
};

struct FHSRSettlementReceipt
{
	FGuid TransactionId;
	FName RewardDefinitionId;
	FName PlayerCharacterId;
	int32 RewardSeed = 0;
	int32 Experience = 0;
	int64 ExpectedInventoryRevision = 0;
	int64 ExpectedProfileRevision = 0;
	int64 ExpectedRewardRevision = 0;
	FHSRRewardReceipt RewardReceipt;
	int64 InventoryRevision = 0;
	int64 ProfileRevision = 0;
	int64 RewardRevision = 0;
};

struct FHSRInventorySettlementCandidate
{
	FGuid TransactionId;
	TMap<FName, int32> Stacks;
	TMap<FGuid, FHSRItemInstance> UniqueItems;
	int64 NextRevision = 0;
};

struct FHSRProfileSettlementCandidate
{
	FGuid TransactionId;
	FName CharacterId;
	TMap<FName, FHSRCharacterProfileSnapshot> Profiles;
	int64 NextRevision = 0;
};

struct FHSRRewardSettlementCandidate
{
	FGuid TransactionId;
	TMap<FGuid, FHSRRewardReceipt> Receipts;
	TMap<FGuid, FHSRSettlementReceipt> SettlementLedger;
	FHSRRewardReceipt PublishedRewardReceipt;
	int64 NextRevision = 0;
};

struct FHSRSettlementCandidate
{
	FHSRSettlementRequest Request;
	FHSRInventorySettlementCandidate Inventory;
	FHSRProfileSettlementCandidate Profile;
	FHSRRewardSettlementCandidate Reward;
	FHSRSettlementReceipt Receipt;
};

#if WITH_DEV_AUTOMATION_TESTS
enum class EHSRSettlementPrepareFailurePoint : uint8
{
	None,
	AfterReward,
	AfterInventory,
	AfterProfile
};

struct FHSRSettlementAutomationSnapshot
{
	int32 AggregateInstallCount = 0;
	int32 PublicationCount = 0;
};
#endif
