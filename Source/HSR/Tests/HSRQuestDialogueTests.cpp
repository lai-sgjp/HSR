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
#include "../Save/HSRSaveVersion.h"

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
	TestEqual(TEXT("schema is current"), Captured.SchemaVersion, HSRSaveVersion::CurrentSchema);
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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRQuestCanonicalRestoreTest, "HSR.Save.QuestCanonicalRestore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRQuestCanonicalRestoreTest::RunTest(const FString&)
{
	using namespace HSR::P14::Tests;
	FFixture Source = MakeFixture(*this);
	FHSRQuestRuntimeState State;
	Source.Quest->StartQuest(TEXT("Quest.P14.Branching"), State);
	const FGuid OriginalClaimId = State.RewardClaimId;
	FHSRDialogueChoiceResult Choice;
	Source.Dialogue->SelectChoice(TEXT("Dialogue.P14.NPC"), TEXT("Start"), TEXT("Greet"), Choice);
	FHSRSaveData Captured;
	TestEqual(TEXT("capture partial quest"), Source.Save->SaveSnapshot(Captured), EHSRSaveResult::Success);
	const auto RoundTrip = [this](const FHSRSaveData& Input, FHSRSaveData& Output)
	{
		TArray<uint8> Bytes;
		if (!TestTrue(TEXT("encode real envelope"), HSRSaveVersion::EncodeEnvelope(Input, TEXT("QuestCanonicalRegression"), 0, FGuid(17, 18, 19, 20), 1, Bytes))) return false;
		return TestEqual(TEXT("decode real envelope"), HSRSaveVersion::DecodeEnvelope(Bytes, TEXT("QuestCanonicalRegression"), 0, Output), EHSRSaveDecodeResult::Success);
	};
	FHSRSaveData Decoded;
	if (!RoundTrip(Captured, Decoded)) return false;
	TestEqual(TEXT("wire order differs from authored order"), Decoded.Quests.States[0].Objectives[0].ObjectiveId, FName(TEXT("Objective.Choice")));
	TestEqual(TEXT("codec preserves original reward claim"), Decoded.Quests.States[0].RewardClaimId, OriginalClaimId);
	FFixture Target = MakeFixture(*this);
	TestEqual(TEXT("restore canonical partial quest"), Target.Save->LoadSnapshot(Decoded), EHSRSaveResult::Success);
	Target.Quest->GetQuestState(TEXT("Quest.P14.Branching"), State);
	TestEqual(TEXT("runtime restores authored objective order"), State.Objectives[0].ObjectiveId, FName(TEXT("Objective.Greet")));
	TestTrue(TEXT("progress remains on greeted objective"), State.Objectives[0].bCompleted);
	TestFalse(TEXT("choice remains unfinished"), State.Objectives[1].bCompleted);
	FHSRQuestSaveData Bad = Decoded.Quests;
	FHSRQuestRestoreState Sentinel;
	Sentinel.Revision = 777;
	Bad.States[0].Objectives[1] = Bad.States[0].Objectives[0];
	TestFalse(TEXT("duplicate objective rejected"), Target.Quest->PrepareRestore(Bad, Sentinel));
	TestEqual(TEXT("failed prepare leaves output untouched"), Sentinel.Revision, int64(777));
	Bad = Decoded.Quests;
	Bad.States[0].Objectives[1].ObjectiveId = TEXT("Objective.Unknown");
	TestFalse(TEXT("unknown/missing objective rejected"), Target.Quest->PrepareRestore(Bad, Sentinel));
	Bad = Decoded.Quests;
	Bad.States[0].RewardClaimId = FGuid(91, 92, 93, 94);
	TestFalse(TEXT("forged claim remains rejected"), Target.Quest->PrepareRestore(Bad, Sentinel));
	Bad = Decoded.Quests;
	Bad.States[0].Objectives[0].RequiredCount = 5;
	TestFalse(TEXT("changed objective requirements rejected"), Target.Quest->PrepareRestore(Bad, Sentinel));
	TestEqual(TEXT("restored active quest advances correct objective"),
		Target.Dialogue->SelectChoice(TEXT("Dialogue.P14.NPC"), TEXT("Branch"), TEXT("ChoiceA"), Choice), EHSRQuestOperationResult::Success);
	Target.Quest->GetQuestState(TEXT("Quest.P14.Branching"), State);
	TestEqual(TEXT("restored quest completes"), State.State, EHSRQuestState::Completed);
	TestTrue(TEXT("completion rewards claimed"), State.bRewardClaimed);
	TestEqual(TEXT("completion keeps original claim identity"), State.RewardClaimId, OriginalClaimId);
	TestEqual(TEXT("capture completed quest"), Target.Save->SaveSnapshot(Captured), EHSRSaveResult::Success);
	if (!RoundTrip(Captured, Decoded)) return false;
	FFixture Restarted = MakeFixture(*this);
	TestEqual(TEXT("completed disk quest restores"), Restarted.Save->LoadSnapshot(Decoded), EHSRSaveResult::Success);
	TestEqual(TEXT("repeated completed disk restore succeeds"), Restarted.Save->LoadSnapshot(Decoded), EHSRSaveResult::Success);
	FHSRQuestRewardClaimResult Claim;
	TestEqual(TEXT("restored reward cannot be claimed twice"), Restarted.Quest->ClaimQuestReward(TEXT("Quest.P14.Branching"), Claim), EHSRQuestOperationResult::NoOp);
	FHSRInventorySnapshot InventorySnapshot;
	Restarted.Inventory->GetSnapshot(InventorySnapshot);
	TestEqual(TEXT("reward quantity remains exactly once"), InventorySnapshot.GetStackQuantity(TEXT("Item.P14.QuestToken")), 3);
	FHSRRewardReceipt Receipt;
	TestTrue(TEXT("original reward ledger receipt preserved"), Restarted.Reward->GetReceipt(OriginalClaimId, Receipt));
	return true;
}

#endif
