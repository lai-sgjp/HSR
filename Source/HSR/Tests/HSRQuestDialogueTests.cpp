#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/Definitions/HSRDialogueDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRQuestDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Dialogue/HSRDialogueSubsystem.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Quest/HSRQuestSubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../Save/HSRSaveSubsystem.h"

namespace HSR::P14::Tests
{
	struct FFixture
	{
		UGameInstance* GameInstance = nullptr;
		UHSRCharacterProfileSubsystem* Profiles = nullptr;
		UHSRPartySubsystem* Party = nullptr;
		UHSREquipmentSubsystem* Equipment = nullptr;
		UHSRInventorySubsystem* Inventory = nullptr;
		UHSRRewardSubsystem* Reward = nullptr;
		UHSRQuestSubsystem* Quest = nullptr;
		UHSRDialogueSubsystem* Dialogue = nullptr;
		UHSRSaveSubsystem* Save = nullptr;
	};

	static UHSRQuestDefinition* MakeQuestDefinition()
	{
		UHSRQuestDefinition* Quest = NewObject<UHSRQuestDefinition>();
		Quest->QuestId = TEXT("Quest.P14.Branching");
		Quest->Objectives.Add({TEXT("Objective.Greet"), TEXT("QuestEvent.P14.Greet"), 1});
		Quest->Objectives.Add({TEXT("Objective.Choice"), TEXT("QuestEvent.P14.ChoiceA"), 1});
		Quest->RewardDefinitionId = TEXT("Reward.P14.Quest");
		Quest->RewardSeed = 1401;
		Quest->bAutoClaimReward = true;
		return Quest;
	}

	static UHSRDialogueDefinition* MakeDialogueDefinition()
	{
		UHSRDialogueDefinition* Dialogue = NewObject<UHSRDialogueDefinition>();
		Dialogue->DialogueId = TEXT("Dialogue.P14.NPC");
		Dialogue->QuestId = TEXT("Quest.P14.Branching");
		Dialogue->StartNodeId = TEXT("Start");
		FHSRDialogueNodeDefinition Start;
		Start.NodeId = TEXT("Start");
		Start.Choices.Add({TEXT("Greet"), TEXT("Branch"), TEXT("QuestEvent.P14.Greet"), 1});
		FHSRDialogueNodeDefinition Branch;
		Branch.NodeId = TEXT("Branch");
		Branch.Choices.Add({TEXT("ChoiceA"), TEXT("EndA"), TEXT("QuestEvent.P14.ChoiceA"), 1});
		Branch.Choices.Add({TEXT("ChoiceB"), TEXT("EndB"), TEXT("QuestEvent.P14.ChoiceB"), 1});
		FHSRDialogueNodeDefinition EndA;
		EndA.NodeId = TEXT("EndA");
		FHSRDialogueNodeDefinition EndB;
		EndB.NodeId = TEXT("EndB");
		Dialogue->Nodes = {Start, Branch, EndA, EndB};
		return Dialogue;
	}

	static FFixture MakeFixture(FAutomationTestBase& Test)
	{
		FFixture F;
		F.GameInstance = NewObject<UGameInstance>();
		F.Profiles = NewObject<UHSRCharacterProfileSubsystem>(F.GameInstance);
		F.Party = NewObject<UHSRPartySubsystem>(F.GameInstance);
		F.Equipment = NewObject<UHSREquipmentSubsystem>(F.GameInstance);
		F.Inventory = NewObject<UHSRInventorySubsystem>(F.GameInstance);
		F.Reward = NewObject<UHSRRewardSubsystem>(F.GameInstance);
		F.Quest = NewObject<UHSRQuestSubsystem>(F.GameInstance);
		F.Dialogue = NewObject<UHSRDialogueSubsystem>(F.GameInstance);
		F.Save = NewObject<UHSRSaveSubsystem>(F.GameInstance);

		UHSRCharacterDefinition* Character = NewObject<UHSRCharacterDefinition>();
		Character->CharacterId = TEXT("Character.A");
		F.Profiles->RegisterDefinition(Character);
		F.Party->InitializeForDevelopmentTest(F.Profiles);
		F.Party->AddCharacter(Character->CharacterId);

		UHSRItemDefinition* Item = NewObject<UHSRItemDefinition>();
		Item->ItemId = TEXT("Item.P14.QuestToken");
		Item->StorageKind = EHSRItemStorageKind::Stackable;
		Item->MaxStack = 99;
		Test.TestEqual(TEXT("register quest item"), F.Inventory->RegisterDefinition(*Item), EHSRInventoryOperationResult::Success);

		UHSRRewardDefinition* RewardDefinition = NewObject<UHSRRewardDefinition>();
		RewardDefinition->RewardDefinitionId = TEXT("Reward.P14.Quest");
		RewardDefinition->FixedItems.Add({Item->ItemId, 3});
		F.Reward->InitializeForAutomation(F.Inventory);
		Test.TestEqual(TEXT("register quest reward"), F.Reward->RegisterRewardDefinition(*RewardDefinition), EHSRRewardOperationResult::Success);

		F.Quest->InitializeForAutomation(F.Reward);
		Test.TestEqual(TEXT("register quest"), F.Quest->RegisterQuestDefinition(*MakeQuestDefinition()), EHSRQuestOperationResult::Success);
		F.Dialogue->InitializeForAutomation(F.Quest);
		Test.TestEqual(TEXT("register dialogue"), F.Dialogue->RegisterDialogueDefinition(*MakeDialogueDefinition()), EHSRQuestOperationResult::Success);
		F.Save->InitializeForDevelopmentTest(F.Profiles, F.Party, F.Equipment, F.Inventory, F.Reward, F.Quest);
		return F;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRQuestDialogueBranchingRewardTest, "HSR.QuestDialogue.BranchingReward", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRQuestDialogueBranchingRewardTest::RunTest(const FString&)
{
	using namespace HSR::P14::Tests;
	FFixture F = MakeFixture(*this);
	FHSRQuestRuntimeState State;
	TestEqual(TEXT("start quest"), F.Quest->StartQuest(TEXT("Quest.P14.Branching"), State), EHSRQuestOperationResult::Success);
	FHSRDialogueChoiceResult Choice;
	TestEqual(TEXT("greet advances first objective"), F.Dialogue->SelectChoice(TEXT("Dialogue.P14.NPC"), TEXT("Start"), TEXT("Greet"), Choice), EHSRQuestOperationResult::Success);
	TestTrue(TEXT("greet submitted quest event"), Choice.bQuestEventSubmitted);
	TestEqual(TEXT("alternate branch is no-op before matching objective"), F.Dialogue->SelectChoice(TEXT("Dialogue.P14.NPC"), TEXT("Branch"), TEXT("ChoiceB"), Choice), EHSRQuestOperationResult::NoOp);
	TestEqual(TEXT("choice A completes quest"), F.Dialogue->SelectChoice(TEXT("Dialogue.P14.NPC"), TEXT("Branch"), TEXT("ChoiceA"), Choice), EHSRQuestOperationResult::Success);
	TestTrue(TEXT("quest state available"), F.Quest->GetQuestState(TEXT("Quest.P14.Branching"), State));
	TestEqual(TEXT("quest completed"), State.State, EHSRQuestState::Completed);
	TestTrue(TEXT("quest reward claimed"), State.bRewardClaimed);
	FHSRInventorySnapshot Snapshot;
	F.Inventory->GetSnapshot(Snapshot);
	TestEqual(TEXT("reward granted once"), Snapshot.Stacks.Num(), 1);
	TestEqual(TEXT("reward quantity"), Snapshot.Stacks[0].Quantity, 3);

	const FGuid ClaimId = State.RewardClaimId;
	FHSRQuestRewardClaimResult ClaimAgain;
	TestEqual(TEXT("explicit repeated claim no-op"), F.Quest->ClaimQuestReward(TEXT("Quest.P14.Branching"), ClaimAgain), EHSRQuestOperationResult::NoOp);
	F.Inventory->GetSnapshot(Snapshot);
	TestEqual(TEXT("repeat claim does not duplicate"), Snapshot.Stacks[0].Quantity, 3);
	FHSRRewardReceipt Receipt;
	TestTrue(TEXT("reward ledger has quest claim"), F.Reward->GetReceipt(ClaimId, Receipt));
	TestEqual(TEXT("ledger reward id"), Receipt.Request.RewardDefinitionId, FName(TEXT("Reward.P14.Quest")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRQuestSaveV4Test, "HSR.Save.QuestV4", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRQuestSaveV4Test::RunTest(const FString&)
{
	using namespace HSR::P14::Tests;
	FFixture Source = MakeFixture(*this);
	FHSRQuestRuntimeState State;
	Source.Quest->StartQuest(TEXT("Quest.P14.Branching"), State);
	FHSRDialogueChoiceResult Choice;
	Source.Dialogue->SelectChoice(TEXT("Dialogue.P14.NPC"), TEXT("Start"), TEXT("Greet"), Choice);
	Source.Dialogue->SelectChoice(TEXT("Dialogue.P14.NPC"), TEXT("Branch"), TEXT("ChoiceA"), Choice);
	FHSRSaveData Captured;
	TestEqual(TEXT("capture v4"), Source.Save->SaveSnapshot(Captured), EHSRSaveResult::Success);
	TestEqual(TEXT("schema v5"), Captured.SchemaVersion, 5);
	TestEqual(TEXT("one quest saved"), Captured.Quests.States.Num(), 1);
	TestEqual(TEXT("one reward receipt saved"), Captured.Rewards.Receipts.Num(), 1);

	FFixture Target = MakeFixture(*this);
	int32 QuestRestores = 0;
	int32 AggregateRestores = 0;
	Target.Quest->OnQuestRestored().AddLambda([&](int64) { ++QuestRestores; });
	Target.Save->OnRestoreCommitted().AddLambda([&](const FHSRRestoreCommitInfo& Info)
	{
		if (Info.bQuestsChanged)
		{
			++AggregateRestores;
		}
	});
	TestEqual(TEXT("load v4"), Target.Save->LoadSnapshot(Captured), EHSRSaveResult::Success);
	TestTrue(TEXT("restored quest state"), Target.Quest->GetQuestState(TEXT("Quest.P14.Branching"), State));
	TestTrue(TEXT("restored reward claimed"), State.bRewardClaimed);
	FHSRInventorySnapshot Snapshot;
	Target.Inventory->GetSnapshot(Snapshot);
	TestEqual(TEXT("restored reward quantity"), Snapshot.Stacks[0].Quantity, 3);
	TestEqual(TEXT("quest restore event once"), QuestRestores, 1);
	TestEqual(TEXT("aggregate restore event once"), AggregateRestores, 1);
	TestEqual(TEXT("repeat load succeeds"), Target.Save->LoadSnapshot(Captured), EHSRSaveResult::Success);
	TestEqual(TEXT("repeat quest restore silent"), QuestRestores, 1);
	TestEqual(TEXT("repeat aggregate restore silent"), AggregateRestores, 1);

	FHSRSaveData Bad = Captured;
	Bad.Quests.States[0].Objectives[0].CurrentCount = 99;
	TestEqual(TEXT("invalid quest objective rejected"), Target.Save->LoadSnapshot(Bad), EHSRSaveResult::InvalidData);
	Bad = Captured;
	Bad.SchemaVersion = 3;
	Bad.Quests = FHSRQuestSaveData();
	TestEqual(TEXT("v3 migrates empty quest state"), Target.Save->LoadSnapshot(Bad), EHSRSaveResult::Success);
	TestFalse(TEXT("quest state cleared by v3 migration"), Target.Quest->GetQuestState(TEXT("Quest.P14.Branching"), State));
	return true;
}

#endif
