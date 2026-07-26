#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSRDialogueTypes.h"
#include "../Quest/HSRQuestTypes.h"
#include "HSRDialogueSubsystem.generated.h"

class UHSRDialogueDefinition;
class UHSRQuestSubsystem;

UCLASS()
class HSR_API UHSRDialogueSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	EHSRQuestOperationResult RegisterDialogueDefinition(const UHSRDialogueDefinition& Definition);
	EHSRQuestOperationResult SelectChoice(FName DialogueId, FName NodeId, FName ChoiceId, FHSRDialogueChoiceResult& OutResult);
	bool GetStartNode(FName DialogueId, FHSRDialogueNodeDefinition& OutNode) const;

#if WITH_DEV_AUTOMATION_TESTS
	void InitializeForAutomation(UHSRQuestSubsystem* InQuest);
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

	EHSRQuestOperationResult CanRegisterDialogueDefinition(const UHSRDialogueDefinition& Definition) const;
	const FHSRDialogueNodeDefinition* FindNode(const FDialogueRule& Rule, FName NodeId) const;
	const FHSRDialogueChoiceDefinition* FindChoice(const FHSRDialogueNodeDefinition& Node, FName ChoiceId) const;

	TWeakObjectPtr<UHSRQuestSubsystem> Quest;
	TMap<FName, FDialogueRule> Dialogues;
};
