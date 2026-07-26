#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSRQuestTypes.h"
#include "HSRQuestSubsystem.generated.h"

class UHSRQuestDefinition;
class UHSRRewardSubsystem;

UCLASS()
class HSR_API UHSRQuestSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	EHSRQuestOperationResult RegisterQuestDefinition(const UHSRQuestDefinition& Definition);
	EHSRQuestOperationResult StartQuest(FName QuestId, FHSRQuestRuntimeState& OutState);
	EHSRQuestOperationResult SubmitEvent(const FHSRQuestDomainEvent& Event, TArray<FHSRQuestRuntimeState>& OutChangedStates);
	EHSRQuestOperationResult ClaimQuestReward(FName QuestId, FHSRQuestRewardClaimResult& OutResult);
	bool GetQuestState(FName QuestId, FHSRQuestRuntimeState& OutState) const;
	void GetQuestStates(TArray<FHSRQuestRuntimeState>& OutStates) const;
	void ExportSaveData(FHSRQuestSaveData& OutData) const;
	bool PrepareRestore(const FHSRQuestSaveData& Data, FHSRQuestRestoreState& OutCandidate) const;
	bool IsRestoreDifferent(const FHSRQuestRestoreState& Candidate) const;
	void CommitRestore(FHSRQuestRestoreState&& Candidate, bool bNotify);
	FHSRQuestChanged& OnQuestChanged() { return QuestChanged; }
	FHSRQuestRestored& OnQuestRestored() { return QuestRestored; }

#if WITH_DEV_AUTOMATION_TESTS
	void InitializeForAutomation(UHSRRewardSubsystem* InReward);
#endif
#if WITH_EDITOR
	void InitializeForDevelopmentTest(UHSRRewardSubsystem* InReward);
#endif

private:
	struct FQuestRule
	{
		FName QuestId;
		TArray<FHSRQuestObjectiveDefinition> Objectives;
		FName RewardDefinitionId;
		int32 RewardSeed = 0;
		bool bAutoClaimReward = true;
	};

	EHSRQuestOperationResult CanRegisterQuestDefinition(const UHSRQuestDefinition& Definition) const;
	bool BuildInitialState(const FQuestRule& Rule, FHSRQuestRuntimeState& OutState) const;
	bool IsComplete(const FHSRQuestRuntimeState& State) const;
	static FGuid MakeQuestRewardClaimId(FName QuestId);

	TWeakObjectPtr<UHSRRewardSubsystem> Reward;
	TMap<FName, FQuestRule> QuestDefinitions;
	TMap<FName, FHSRQuestRuntimeState> QuestStates;
	int64 Revision = 0;
	FHSRQuestChanged QuestChanged;
	FHSRQuestRestored QuestRestored;
};
