#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/Guid.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../Data/Definitions/HSRDialogueDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRQuestDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Dialogue/HSRDialogueSubsystem.h"
#include "../Dialogue/HSRDialogueTypes.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Quest/HSRQuestSubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../UI/Dialogue/HSRDialoguePresentationViewModel.h"

namespace HSR::P17::DialogueAuthorityTests
{
	static FGuid Id(uint32 Seed)
	{
		return FGuid(Seed, Seed + 1, Seed + 2, Seed + 3);
	}

	static UHSRDialogueDefinition* MakeDefinition(const FHSRDialogueChoiceDefinition& Choice,
		FName DialogueId, FName StartNodeId, FName EndNodeId)
	{
		UHSRDialogueDefinition* Definition = NewObject<UHSRDialogueDefinition>();
		Definition->DialogueId = DialogueId;
		Definition->StartNodeId = StartNodeId;

		FHSRDialogueNodeDefinition Start;
		Start.NodeId = StartNodeId;
		Start.Choices.Add(Choice);
		FHSRDialogueNodeDefinition End;
		End.NodeId = EndNodeId;
		Definition->Nodes = {Start, End};
		return Definition;
	}

	struct FFixture
	{
		UGameInstance* GameInstance = nullptr;
		UHSRInventorySubsystem* Inventory = nullptr;
		UHSRRewardSubsystem* Reward = nullptr;
		UHSRQuestSubsystem* Quest = nullptr;
		UHSRDialogueSubsystem* Dialogue = nullptr;
		UHSRBattleTransitionSubsystem* Encounter = nullptr;

		void InitializeAuthorities()
		{
			GameInstance = NewObject<UGameInstance>(GetTransientPackage());
			Inventory = NewObject<UHSRInventorySubsystem>(GameInstance);
			Reward = NewObject<UHSRRewardSubsystem>(GameInstance);
			Quest = NewObject<UHSRQuestSubsystem>(GameInstance);
			Dialogue = NewObject<UHSRDialogueSubsystem>(GameInstance);

			Reward->InitializeForAutomation(Inventory);
			Quest->InitializeForAutomation(Reward);
			Dialogue->InitializeForAutomation(Quest, Reward, nullptr);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDialogueQuestBranchExactlyOnceTest,
	"HSR.Dialogue.Authority.QuestBranchExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDialogueQuestBranchExactlyOnceTest::RunTest(const FString&)
{
	using namespace HSR::P17::DialogueAuthorityTests;
	FFixture Fixture;
	Fixture.InitializeAuthorities();

	UHSRItemDefinition* Item = NewObject<UHSRItemDefinition>();
	Item->ItemId = TEXT("Item.Dialogue.QuestReward");
	Item->StorageKind = EHSRItemStorageKind::Stackable;
	Item->MaxStack = 99;
	TestEqual(TEXT("register quest reward item"), Fixture.Inventory->RegisterDefinition(*Item),
		EHSRInventoryOperationResult::Success);

	UHSRRewardDefinition* Reward = NewObject<UHSRRewardDefinition>();
	Reward->RewardDefinitionId = TEXT("Reward.Dialogue.Quest");
	Reward->FixedItems.Add({Item->ItemId, 3});
	TestEqual(TEXT("register quest reward"), Fixture.Reward->RegisterRewardDefinition(*Reward),
		EHSRRewardOperationResult::Success);

	UHSRQuestDefinition* Quest = NewObject<UHSRQuestDefinition>();
	Quest->QuestId = TEXT("Quest.Dialogue.ExactlyOnce");
	Quest->Objectives.Add({TEXT("Objective.Dialogue"), TEXT("QuestEvent.Dialogue"), 1});
	Quest->RewardDefinitionId = Reward->RewardDefinitionId;
	Quest->RewardSeed = 1703;
	TestEqual(TEXT("register quest"), Fixture.Quest->RegisterQuestDefinition(*Quest),
		EHSRQuestOperationResult::Success);

	FHSRDialogueChoiceDefinition Choice;
	Choice.ChoiceId = TEXT("Choice.Quest");
	Choice.TargetNodeId = TEXT("Node.End");
	Choice.Branch = EHSRDialogueChoiceBranch::Quest;
	Choice.BranchOperationId = Id(1703);
	Choice.QuestEventId = TEXT("QuestEvent.Dialogue");
	Choice.EventCount = 1;
	TestEqual(TEXT("register dialogue"), Fixture.Dialogue->RegisterDialogueDefinition(*MakeDefinition(
		Choice, TEXT("Dialogue.Authority.Quest"), TEXT("Node.Start"), TEXT("Node.End"))),
		EHSRQuestOperationResult::Success);
	FHSRQuestRuntimeState State;
	TestEqual(TEXT("start quest"), Fixture.Quest->StartQuest(Quest->QuestId, State),
		EHSRQuestOperationResult::Success);

	FHSRDialogueChoiceRequest Request;
	Request.DialogueId = TEXT("Dialogue.Authority.Quest");
	Request.NodeId = TEXT("Node.Start");
	Request.ChoiceId = Choice.ChoiceId;
	FHSRDialogueChoiceResult First;
	TestEqual(TEXT("first quest branch commits"), Fixture.Dialogue->SelectChoice(Request, First),
		EHSRDialogueChoiceOperationResult::Success);
	TestTrue(TEXT("quest branch submits its event"), First.bQuestEventSubmitted);

	FHSRInventorySnapshot AfterFirst;
	Fixture.Inventory->GetSnapshot(AfterFirst);
	FHSRDialogueChoiceResult Replay;
	TestEqual(TEXT("replayed quest branch is a no-op"), Fixture.Dialogue->SelectChoice(Request, Replay),
		EHSRDialogueChoiceOperationResult::NoOp);
	FHSRInventorySnapshot AfterReplay;
	Fixture.Inventory->GetSnapshot(AfterReplay);
	TestEqual(TEXT("replay preserves inventory revision"), AfterReplay.Revision, AfterFirst.Revision);
	TestEqual(TEXT("replay preserves reward quantity"), AfterReplay.Stacks[0].Quantity, AfterFirst.Stacks[0].Quantity);
	TestEqual(TEXT("replay preserves branch operation id"), Replay.OperationId, Choice.BranchOperationId);

	FHSRDialogueChoiceDefinition ConflictChoice = Choice;
	ConflictChoice.ChoiceId = TEXT("Choice.Conflict");
	TestEqual(TEXT("register operation conflict dialogue"), Fixture.Dialogue->RegisterDialogueDefinition(*MakeDefinition(
		ConflictChoice, TEXT("Dialogue.Authority.QuestConflict"), TEXT("Node.Start"), TEXT("Node.End"))),
		EHSRQuestOperationResult::Success);
	FHSRDialogueChoiceRequest ConflictRequest = Request;
	ConflictRequest.DialogueId = TEXT("Dialogue.Authority.QuestConflict");
	ConflictRequest.ChoiceId = ConflictChoice.ChoiceId;
	FHSRDialogueChoiceResult ConflictResult;
	TestEqual(TEXT("operation id conflict is rejected"), Fixture.Dialogue->SelectChoice(ConflictRequest, ConflictResult),
		EHSRDialogueChoiceOperationResult::OperationIdConflict);
	FHSRInventorySnapshot AfterConflict;
	Fixture.Inventory->GetSnapshot(AfterConflict);
	TestEqual(TEXT("operation conflict keeps inventory revision"), AfterConflict.Revision, AfterReplay.Revision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDialogueRewardBranchExactlyOnceTest,
	"HSR.Dialogue.Authority.RewardBranchExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDialogueRewardBranchExactlyOnceTest::RunTest(const FString&)
{
	using namespace HSR::P17::DialogueAuthorityTests;
	FFixture Fixture;
	Fixture.InitializeAuthorities();

	UHSRItemDefinition* Item = NewObject<UHSRItemDefinition>();
	Item->ItemId = TEXT("Item.Dialogue.Reward");
	Item->StorageKind = EHSRItemStorageKind::Stackable;
	Item->MaxStack = 99;
	TestEqual(TEXT("register reward item"), Fixture.Inventory->RegisterDefinition(*Item),
		EHSRInventoryOperationResult::Success);
	UHSRRewardDefinition* Reward = NewObject<UHSRRewardDefinition>();
	Reward->RewardDefinitionId = TEXT("Reward.Dialogue.Direct");
	Reward->FixedItems.Add({Item->ItemId, 4});
	TestEqual(TEXT("register direct reward"), Fixture.Reward->RegisterRewardDefinition(*Reward),
		EHSRRewardOperationResult::Success);

	FHSRDialogueChoiceDefinition Choice;
	Choice.ChoiceId = TEXT("Choice.Reward");
	Choice.TargetNodeId = TEXT("Node.End");
	Choice.Branch = EHSRDialogueChoiceBranch::Reward;
	Choice.BranchOperationId = Id(1704);
	Choice.RewardDefinitionId = Reward->RewardDefinitionId;
	Choice.RewardSeed = 1704;
	TestEqual(TEXT("register reward dialogue"), Fixture.Dialogue->RegisterDialogueDefinition(*MakeDefinition(
		Choice, TEXT("Dialogue.Authority.Reward"), TEXT("Node.Start"), TEXT("Node.End"))),
		EHSRQuestOperationResult::Success);

	FHSRDialogueChoiceRequest Request;
	Request.DialogueId = TEXT("Dialogue.Authority.Reward");
	Request.NodeId = TEXT("Node.Start");
	Request.ChoiceId = Choice.ChoiceId;
	FHSRDialogueChoiceResult First;
	TestEqual(TEXT("first reward branch commits"), Fixture.Dialogue->SelectChoice(Request, First),
		EHSRDialogueChoiceOperationResult::Success);
	TestEqual(TEXT("reward uses stable claim id"), First.RewardReceipt.Request.ClaimId, Choice.BranchOperationId);

	FHSRInventorySnapshot AfterFirst;
	Fixture.Inventory->GetSnapshot(AfterFirst);
	FHSRDialogueChoiceResult Replay;
	TestEqual(TEXT("replayed reward branch is a no-op"), Fixture.Dialogue->SelectChoice(Request, Replay),
		EHSRDialogueChoiceOperationResult::NoOp);
	FHSRInventorySnapshot AfterReplay;
	Fixture.Inventory->GetSnapshot(AfterReplay);
	TestEqual(TEXT("reward replay does not duplicate"), AfterReplay.Stacks[0].Quantity, AfterFirst.Stacks[0].Quantity);
	TestEqual(TEXT("replay returns same receipt revision"), Replay.RewardReceipt.Revision, First.RewardReceipt.Revision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDialogueEncounterBranchExactlyOnceTest,
	"HSR.Dialogue.Authority.EncounterBranchExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDialogueEncounterBranchExactlyOnceTest::RunTest(const FString&)
{
	using namespace HSR::P17::DialogueAuthorityTests;
	if (!GEngine)
	{
		AddError(TEXT("GEngine is required for the encounter authority fixture."));
		return false;
	}
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->AddToRoot();
	GameInstance->InitializeStandalone(FName(*FString::Printf(TEXT("HSRDialogueEncounter_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits))));
	UWorld* World = GameInstance->GetWorld();
	UHSRDialogueSubsystem* Dialogue = GameInstance->GetSubsystem<UHSRDialogueSubsystem>();
	UHSRBattleTransitionSubsystem* Encounter = GameInstance->GetSubsystem<UHSRBattleTransitionSubsystem>();
	if (!TestNotNull(TEXT("standalone world exists"), World)
		|| !TestNotNull(TEXT("dialogue authority exists"), Dialogue)
		|| !TestNotNull(TEXT("encounter authority exists"), Encounter))
	{
		GameInstance->Shutdown();
		GameInstance->RemoveFromRoot();
		return false;
	}
	Encounter->SetTravelSuppressedForAutomation(true);

	FHSRDialogueChoiceDefinition Choice;
	Choice.ChoiceId = TEXT("Choice.Encounter");
	Choice.TargetNodeId = TEXT("Node.End");
	Choice.Branch = EHSRDialogueChoiceBranch::Encounter;
	Choice.BranchOperationId = Id(1705);
	Choice.EncounterRequest.EncounterId = TEXT("Encounter.Dialogue");
	Choice.EncounterRequest.EnemyDefinitionId = TEXT("Enemy.Dialogue");
	Choice.EncounterRequest.BattleMapPath = TEXT("/Game/Maps/Map_Battle");
	Choice.EncounterRequest.ExplorationMapPath = TEXT("/Game/Maps/Map_Exploration");
	Choice.EncounterRequest.PlayerCharacterId = TEXT("Character.Dialogue");
	TestEqual(TEXT("register encounter dialogue"), Dialogue->RegisterDialogueDefinition(*MakeDefinition(
		Choice, TEXT("Dialogue.Authority.Encounter"), TEXT("Node.Start"), TEXT("Node.End"))),
		EHSRQuestOperationResult::Success);

	FHSRDialogueChoiceRequest Request;
	Request.DialogueId = TEXT("Dialogue.Authority.Encounter");
	Request.NodeId = TEXT("Node.Start");
	Request.ChoiceId = Choice.ChoiceId;
	FHSRDialogueChoiceResult First;
	TestEqual(TEXT("first encounter branch commits"), Dialogue->SelectChoice(Request, First),
		EHSRDialogueChoiceOperationResult::Success);
	TestEqual(TEXT("encounter uses stable request id"), First.EncounterResponse.RequestId, Choice.BranchOperationId);
	const FHSRTransitionAutomationSnapshot AfterFirst = Encounter->GetAutomationSnapshot(Choice.EncounterRequest.EncounterId);
	TestEqual(TEXT("encounter travel starts once"), AfterFirst.TravelInitiationCount, 1);
	TestEqual(TEXT("encounter request preserves authored context"), AfterFirst.PendingRequest.BattleMapPath,
		Choice.EncounterRequest.BattleMapPath);

	FHSRDialogueChoiceResult Replay;
	TestEqual(TEXT("replayed encounter branch is a no-op"), Dialogue->SelectChoice(Request, Replay),
		EHSRDialogueChoiceOperationResult::NoOp);
	const FHSRTransitionAutomationSnapshot AfterReplay = Encounter->GetAutomationSnapshot(Choice.EncounterRequest.EncounterId);
	TestEqual(TEXT("encounter replay does not start travel"), AfterReplay.TravelInitiationCount,
		AfterFirst.TravelInitiationCount);

	GameInstance->Shutdown();
	if (World)
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	GameInstance->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDialogueBranchFailurePreservesPresentationTest,
	"HSR.Dialogue.Authority.FailurePreservesPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDialogueBranchFailurePreservesPresentationTest::RunTest(const FString&)
{
	using namespace HSR::P17::DialogueAuthorityTests;
	FFixture Fixture;
	Fixture.InitializeAuthorities();

	FHSRDialogueChoiceDefinition Choice;
	Choice.ChoiceId = TEXT("Choice.MissingRewardAuthority");
	Choice.TargetNodeId = TEXT("Node.End");
	Choice.Branch = EHSRDialogueChoiceBranch::Reward;
	Choice.BranchOperationId = Id(1706);
	Choice.RewardDefinitionId = TEXT("Reward.MissingAuthority");
	TestEqual(TEXT("register missing-authority dialogue"), Fixture.Dialogue->RegisterDialogueDefinition(*MakeDefinition(
		Choice, TEXT("Dialogue.Authority.Failure"), TEXT("Node.Start"), TEXT("Node.End"))),
		EHSRQuestOperationResult::Success);
	Fixture.Dialogue->InitializeForAutomation(Fixture.Quest, nullptr, nullptr);

	UHSRDialoguePresentationViewModel* ViewModel = NewObject<UHSRDialoguePresentationViewModel>(Fixture.Dialogue);
	ViewModel->Initialize(Fixture.Dialogue);
	FHSRDialoguePresentationRequest Begin;
	Begin.QueryId = Id(1706);
	Begin.DialogueId = TEXT("Dialogue.Authority.Failure");
	Begin.NodeId = TEXT("Node.Start");
	TestEqual(TEXT("begin failure fixture dialogue"), ViewModel->BeginDialogue(Begin),
		EHSRDialoguePresentationResult::Success);
	FHSRDialoguePresentationSnapshot Before;
	ViewModel->GetSnapshot(Before);

	FHSRDialoguePresentationChoiceRequest Request;
	Request.QueryId = Begin.QueryId;
	Request.DialogueId = Begin.DialogueId;
	Request.NodeId = Begin.NodeId;
	Request.ChoiceId = Choice.ChoiceId;
	TestEqual(TEXT("missing reward authority is unavailable"), ViewModel->SubmitChoice(Request),
		EHSRDialoguePresentationResult::AuthorityUnavailable);
	FHSRDialoguePresentationSnapshot After;
	ViewModel->GetSnapshot(After);
	TestEqual(TEXT("authority failure preserves node"), After.NodeId, Before.NodeId);
	TestEqual(TEXT("authority failure preserves body"), After.BodyText.ToString(), Before.BodyText.ToString());
	TestEqual(TEXT("authority failure preserves choices"), After.Choices.Num(), Before.Choices.Num());
	TestEqual(TEXT("raw reward authority result remains visible"),
		ViewModel->GetLastChoiceResult().RewardResult, EHSRRewardOperationResult::InventoryRejected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDialogueBranchDefinitionValidationTest,
	"HSR.Dialogue.Authority.BranchDefinitionValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDialogueBranchDefinitionValidationTest::RunTest(const FString&)
{
	using namespace HSR::P17::DialogueAuthorityTests;
	FFixture Fixture;
	Fixture.InitializeAuthorities();

	FHSRDialogueChoiceDefinition IncompleteEncounter;
	IncompleteEncounter.ChoiceId = TEXT("Choice.IncompleteEncounter");
	IncompleteEncounter.TargetNodeId = TEXT("Node.End");
	IncompleteEncounter.Branch = EHSRDialogueChoiceBranch::Encounter;
	IncompleteEncounter.BranchOperationId = Id(1707);
	IncompleteEncounter.EncounterRequest.EncounterId = TEXT("Encounter.OnlyId");
	TestEqual(TEXT("encounter cannot infer enemy or map context"), Fixture.Dialogue->RegisterDialogueDefinition(*MakeDefinition(
		IncompleteEncounter, TEXT("Dialogue.Authority.InvalidEncounter"), TEXT("Node.Start"), TEXT("Node.End"))),
		EHSRQuestOperationResult::InvalidDefinition);

	FHSRDialogueChoiceDefinition MissingOperationId = IncompleteEncounter;
	MissingOperationId.ChoiceId = TEXT("Choice.MissingOperationId");
	MissingOperationId.EncounterRequest.EnemyDefinitionId = TEXT("Enemy.Dialogue");
	MissingOperationId.EncounterRequest.BattleMapPath = TEXT("/Game/Maps/Map_Battle");
	MissingOperationId.EncounterRequest.ExplorationMapPath = TEXT("/Game/Maps/Map_Exploration");
	MissingOperationId.BranchOperationId.Invalidate();
	TestEqual(TEXT("authority branch requires stable operation id"), Fixture.Dialogue->RegisterDialogueDefinition(*MakeDefinition(
		MissingOperationId, TEXT("Dialogue.Authority.InvalidOperation"), TEXT("Node.Start"), TEXT("Node.End"))),
		EHSRQuestOperationResult::InvalidDefinition);
	return true;
}

#endif
