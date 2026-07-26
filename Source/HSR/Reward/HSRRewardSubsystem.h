#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSRRewardTypes.h"
#include "HSRRewardSubsystem.generated.h"

class UHSRDropTableDefinition;
class UHSRInventorySubsystem;
class UHSRItemDefinition;
class UHSRRewardDefinition;

UCLASS()
class HSR_API UHSRRewardSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	EHSRRewardOperationResult RegisterDropTable(const UHSRDropTableDefinition& Definition);
	EHSRRewardOperationResult RegisterRewardDefinition(const UHSRRewardDefinition& Definition);
	EHSRRewardOperationResult RegisterBundle(const TArray<TObjectPtr<UHSRItemDefinition>>& ItemDefinitions, const UHSRDropTableDefinition& DropTable, const UHSRRewardDefinition& RewardDefinition);
	EHSRRewardOperationResult SubmitReward(const FHSRRewardRequest& Request, FHSRRewardReceipt& OutReceipt);
	bool GetReceipt(const FGuid& ClaimId, FHSRRewardReceipt& OutReceipt) const;
	void GetReceipts(TArray<FHSRRewardReceipt>& OutReceipts) const;
	void ExportSaveData(FHSRRewardSaveData& OutData) const;
	bool PrepareRestore(const FHSRRewardSaveData& Data, FHSRRewardRestoreState& OutCandidate) const;
	bool IsRestoreDifferent(const FHSRRewardRestoreState& Candidate) const;
	void CommitRestore(FHSRRewardRestoreState&& Candidate, bool bNotify);
	FHSRRewardCommitted& OnRewardCommitted() { return RewardCommitted; }
	FHSRRewardRestored& OnRewardRestored() { return RewardRestored; }

#if WITH_DEV_AUTOMATION_TESTS
	void InitializeForAutomation(UHSRInventorySubsystem* InInventory);
	void SetCommitFailureForAutomation(bool bValue) { bInjectCommitFailure = bValue; }
#endif
#if WITH_EDITOR
	void InitializeForDevelopmentTest(UHSRInventorySubsystem* InInventory);
#endif

private:
	EHSRRewardOperationResult CanRegisterDropTable(const UHSRDropTableDefinition& Definition) const;
	EHSRRewardOperationResult CanRegisterRewardDefinition(const UHSRRewardDefinition& Definition, FName AdditionalDropTableId = NAME_None) const;
	static FGuid MakeInstanceId(const FGuid& ClaimId, FName ItemId, int32 Ordinal);
	bool BuildGrants(const FHSRRewardRequest& Request, const FHSRRewardDefinitionRule& Reward, TArray<FHSRInventoryGrant>& OutGrants) const;

	TWeakObjectPtr<UHSRInventorySubsystem> Inventory;
	TMap<FName, FHSRRewardDefinitionRule> Rewards;
	TMap<FName, FHSRDropTableRule> DropTables;
	TMap<FGuid, FHSRRewardReceipt> Receipts;
	int64 Revision = 0;
	FHSRRewardCommitted RewardCommitted;
	FHSRRewardRestored RewardRestored;
#if WITH_DEV_AUTOMATION_TESTS
	bool bInjectCommitFailure = false;
#endif
};
