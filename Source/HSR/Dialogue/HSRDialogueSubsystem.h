#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSRDialogueTypes.h"
#include "../Quest/HSRQuestTypes.h"
#include "HSRDialogueSubsystem.generated.h"

class UHSRDialogueDefinition;
class UHSRQuestSubsystem;
class UHSRRewardSubsystem;
class UHSRBattleTransitionSubsystem;

UCLASS()
class HSR_API UHSRDialogueSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	EHSRQuestOperationResult RegisterDialogueDefinition(const UHSRDialogueDefinition& Definition);
	EHSRQuestOperationResult PreviewChoice(FName DialogueId, FName NodeId, FName ChoiceId, FHSRDialogueChoiceResult& OutResult) const;
	EHSRDialogueChoiceOperationResult SelectChoice(const FHSRDialogueChoiceRequest& Request, FHSRDialogueChoiceResult& OutResult);
	EHSRQuestOperationResult SelectChoice(FName DialogueId, FName NodeId, FName ChoiceId, FHSRDialogueChoiceResult& OutResult);
	EHSRQuestOperationResult GetNode(FName DialogueId, FName NodeId, FHSRDialogueNodeDefinition& OutNode) const;
	bool GetStartNode(FName DialogueId, FHSRDialogueNodeDefinition& OutNode) const;

#if WITH_DEV_AUTOMATION_TESTS
	void InitializeForAutomation(UHSRQuestSubsystem* InQuest, UHSRRewardSubsystem* InReward = nullptr,
		UHSRBattleTransitionSubsystem* InEncounter = nullptr);
#endif
#if WITH_EDITOR
	void InitializeForDevelopmentTest(UHSRQuestSubsystem* InQuest);
#endif

private:
	struct FDialogueRule
	{
		FName DialogueId;
		FName QuestId;
		FName StartNodeId;
		TArray<FHSRDialogueNodeDefinition> Nodes;
	};

	struct FDialogueBranchLedgerEntry
	{
		FName DialogueId;
		FName NodeId;
		FName ChoiceId;
		FHSRDialogueChoiceResult Result;
	};

	EHSRQuestOperationResult CanRegisterDialogueDefinition(const UHSRDialogueDefinition& Definition) const;
	const FHSRDialogueNodeDefinition* FindNode(const FDialogueRule& Rule, FName NodeId) const;
	const FHSRDialogueChoiceDefinition* FindChoice(const FHSRDialogueNodeDefinition& Node, FName ChoiceId) const;
	EHSRDialogueChoiceOperationResult DispatchChoiceBranch(FHSRDialogueChoiceResult& InOutResult);
	EHSRQuestOperationResult MapLegacyBranchResult(const FHSRDialogueChoiceResult& Result) const;

	TWeakObjectPtr<UHSRQuestSubsystem> Quest;
	TWeakObjectPtr<UHSRRewardSubsystem> Reward;
	TWeakObjectPtr<UHSRBattleTransitionSubsystem> Encounter;
	TMap<FName, FDialogueRule> Dialogues;
	TMap<FGuid, FDialogueBranchLedgerEntry> BranchLedger;
};
