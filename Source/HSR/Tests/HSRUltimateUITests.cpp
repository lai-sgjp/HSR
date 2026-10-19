#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../Battle/HSRBattleCoordinator.h"
#include "../UI/HSRBattleCommandViewModel.h"

namespace
{
	FHSRBattleCommandViewState MakeUltimateUIState()
	{
		FHSRBattleCommandViewState State;
		State.BattleId = FGuid::NewGuid();
		State.CurrentActorId = TEXT("Enemy");
		State.bActionPlaying = true;
		FHSRBattleCommandSkillView Normal;
		Normal.SkillId = TEXT("Skill.Basic");
		Normal.bAvailable = true;
		Normal.CandidateTargetIds = { TEXT("Enemy") };
		State.Skills.Add(Normal);
		for (const FName ActorId : { FName(TEXT("HeroA")), FName(TEXT("HeroB")) })
		{
			FHSRBattleCommandSkillView Ultimate;
			Ultimate.ActorParticipantId = ActorId;
			Ultimate.SkillId = TEXT("Skill.SharedUltimate");
			Ultimate.Category = EHSRSkillCategory::Ultimate;
			Ultimate.bAvailable = true;
			Ultimate.CandidateTargetIds = { TEXT("Enemy") };
			State.UltimateSkills.Add(Ultimate);
		}
		return State;
	}

	UHSRBattleCommandViewModel* MakeUltimateUIModel(const FHSRBattleCommandViewState& State)
	{
		auto* Coordinator = NewObject<UHSRBattleCoordinator>();
		auto* VM = NewObject<UHSRBattleCommandViewModel>(Coordinator);
		VM->BindCoordinator(Coordinator);
		VM->SetState(State);
		return VM;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateUIActorIdentityTest,
	"HSR.UI.Ultimate.ActorIdentityAndEnemyWindow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRUltimateUIActorIdentityTest::RunTest(const FString&)
{
	const auto State = MakeUltimateUIState();
	auto* VM = MakeUltimateUIModel(State);
	TestTrue(TEXT("normal skill can be inspected"), VM->SelectSkillById(TEXT("Skill.Basic")));
	TestFalse(TEXT("enemy action blocks ordinary execution"), VM->GetState().bCanSubmit);
	TestFalse(TEXT("ordinary command rejected during enemy presentation"), VM->BeginCommandSubmit(
		FGuid::NewGuid(), State.CurrentActorId, TEXT("Skill.Basic"), TEXT("Enemy")));
	TestTrue(TEXT("select second actor sharing same skill definition"), VM->SelectUltimate(TEXT("HeroB")));
	const auto* SelectedUltimate = VM->GetState().FindSelectedSkill();
	TestNotNull(TEXT("selected ultimate exists"), SelectedUltimate);
	if (SelectedUltimate)
		TestEqual(TEXT("snapshot resolves ultimate by actor and skill"), SelectedUltimate->ActorParticipantId, FName(TEXT("HeroB")));
	TestTrue(TEXT("ready ultimate remains submit-able during enemy action"), VM->GetState().bCanSubmit);
	TestFalse(TEXT("same skill id cannot submit for wrong owner"), VM->BeginCommandSubmit(
		FGuid::NewGuid(), TEXT("HeroA"), TEXT("Skill.SharedUltimate"), TEXT("Enemy")));
	TestTrue(TEXT("selected actor can submit during presentation"), VM->BeginCommandSubmit(
		FGuid::NewGuid(), TEXT("HeroB"), TEXT("Skill.SharedUltimate"), TEXT("Enemy")));
	const auto Intent = VM->GetState().BuildSelectedCommand(FGuid::NewGuid());
	TestEqual(TEXT("intent contains selected owner rather than normal actor"), Intent.ActorParticipantId, FName(TEXT("HeroB")));
	TestEqual(TEXT("intent preserves battle identity"), Intent.BattleId, State.BattleId);
	VM->UnbindCoordinator();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateUIPendingTest,
	"HSR.UI.Ultimate.PendingAndNormalLocks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRUltimateUIPendingTest::RunTest(const FString&)
{
	auto State = MakeUltimateUIState();
	State.UltimateSkills[0].bPending = true;
	State.UltimateSkills[0].AcceptedSequence = 17;
	auto* VM = MakeUltimateUIModel(State);
	VM->SelectUltimate(TEXT("HeroA"));
	TestFalse(TEXT("queued actor cannot submit again"), VM->GetState().bCanSubmit);
	TestFalse(TEXT("queued actor duplicate explicitly rejected"), VM->BeginCommandSubmit(
		FGuid::NewGuid(), TEXT("HeroA"), TEXT("Skill.SharedUltimate"), TEXT("Enemy")));
	VM->SelectUltimate(TEXT("HeroB"));
	TestTrue(TEXT("another ready actor unaffected by queued actor"), VM->GetState().bCanSubmit);
	State.UltimateSkills[0].bPending = false;
	State.UltimateSkills[0].bExecuting = true;
	VM->SetState(State);
	VM->SelectUltimate(TEXT("HeroA"));
	TestFalse(TEXT("executing actor cannot resubmit"), VM->GetState().bCanSubmit);
	VM->SelectUltimate(TEXT("HeroB"));
	TestTrue(TEXT("another ready actor can queue during ultimate"), VM->GetState().bCanSubmit);

	State.CurrentActorId = TEXT("HeroA");
	State.bCurrentActorPlayerControlled = true;
	State.bActionPlaying = false;
	VM->SetState(State);
	VM->SelectSkillById(TEXT("Skill.Basic"));
	const FGuid NormalActionId = FGuid::NewGuid();
	TestTrue(TEXT("normal command initially accepted"), VM->BeginCommandSubmit(
		NormalActionId, TEXT("HeroA"), TEXT("Skill.Basic"), TEXT("Enemy")));
	TestFalse(TEXT("normal duplicate remains rejected"), VM->BeginCommandSubmit(
		FGuid::NewGuid(), TEXT("HeroA"), TEXT("Skill.Basic"), TEXT("Enemy")));
	VM->SelectUltimate(TEXT("HeroB"));
	TestTrue(TEXT("normal pending lock does not swallow ready ultimate"), VM->GetState().bCanSubmit);
	TestTrue(TEXT("ready ultimate accepted while normal submit pending"), VM->BeginCommandSubmit(
		FGuid::NewGuid(), TEXT("HeroB"), TEXT("Skill.SharedUltimate"), TEXT("Enemy")));
	TestEqual(TEXT("ultimate intent leaves original normal pending correlation intact"), VM->GetState().PendingActionId, NormalActionId);
	VM->UnbindCoordinator();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateUIAllAlliesTest,
	"HSR.UI.Ultimate.AllAlliesIntent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRUltimateUIAllAlliesTest::RunTest(const FString&)
{
	auto State = MakeUltimateUIState();
	State.UltimateSkills[1].TargetType = EHSRTargetType::AllAllies;
	State.UltimateSkills[1].CandidateTargetIds = { TEXT("HeroA"), TEXT("HeroB") };
	auto* VM = MakeUltimateUIModel(State);
	VM->SelectUltimate(TEXT("HeroB"));
	TestTrue(TEXT("team ultimate needs no individual target selection"), VM->GetSelectedTargetId().IsNone());
	TestTrue(TEXT("nonempty exact team target set is submit-able"), VM->GetState().bCanSubmit);
	TestFalse(TEXT("team intent cannot be narrowed to a single clicked ally"), VM->SelectTarget(TEXT("HeroA")));
	TestTrue(TEXT("team confirmation accepts no individual target"), VM->BeginCommandSubmit(
		FGuid::NewGuid(), TEXT("HeroB"), TEXT("Skill.SharedUltimate"), NAME_None));
	const FGuid ActionId = FGuid::NewGuid();
	const FHSRBattleActionCommand Intent = VM->GetState().BuildSelectedCommand(ActionId);
	TestEqual(TEXT("team intent preserves action identity"), Intent.ActionId, ActionId);
	TestEqual(TEXT("team intent includes every displayed ally"), Intent.TargetParticipantIds.Num(), 2);
	TestTrue(TEXT("team intent preserves exact IDs and order"), Intent.TargetParticipantIds == State.UltimateSkills[1].CandidateTargetIds);
	State.UltimateSkills[1].CandidateTargetIds.Reset();
	VM->SetState(State);
	TestFalse(TEXT("empty team target set cannot submit"), VM->GetState().bCanSubmit);
	TestEqual(TEXT("already built intent is a value snapshot, never silently retargeted"), Intent.TargetParticipantIds.Num(), 2);
	VM->UnbindCoordinator();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateUIAllEnemiesTest,
	"HSR.UI.Ultimate.AllEnemiesIntent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRUltimateUIAllEnemiesTest::RunTest(const FString&)
{
	auto State = MakeUltimateUIState();
	State.UltimateSkills[0].TargetType = EHSRTargetType::AllEnemies;
	State.UltimateSkills[0].CandidateTargetIds = { TEXT("EnemyA"), TEXT("EnemyB"), TEXT("EnemyC") };
	auto* VM = MakeUltimateUIModel(State);
	TestTrue(TEXT("select enemy AOE ultimate"), VM->SelectUltimate(TEXT("HeroA")));
	TestTrue(TEXT("AOE does not choose a single enemy"), VM->GetSelectedTargetId().IsNone());
	TestTrue(TEXT("enemy animation allows ready AOE request"), VM->GetState().bCanSubmit);
	TestFalse(TEXT("single enemy click cannot narrow AOE intent"), VM->SelectTarget(TEXT("EnemyB")));
	TestTrue(TEXT("AOE confirmation accepts group target without individual id"), VM->BeginCommandSubmit(
		FGuid::NewGuid(), TEXT("HeroA"), TEXT("Skill.SharedUltimate"), NAME_None));
	const FHSRBattleActionCommand Intent = VM->GetState().BuildSelectedCommand(FGuid::NewGuid());
	TestEqual(TEXT("AOE intent retains selected source"), Intent.ActorParticipantId, FName(TEXT("HeroA")));
	TestTrue(TEXT("AOE intent includes exact full enemy set"), Intent.TargetParticipantIds == State.UltimateSkills[0].CandidateTargetIds);
	State.UltimateSkills[0].CandidateTargetIds.RemoveAt(1);
	VM->SetState(State);
	TestEqual(TEXT("confirmed AOE value is not silently narrowed after a target leaves"), Intent.TargetParticipantIds.Num(), 3);
	State.UltimateSkills[0].CandidateTargetIds.Reset();
	VM->SetState(State);
	TestFalse(TEXT("AOE with no enemies cannot submit"), VM->GetState().bCanSubmit);
	TestFalse(TEXT("empty AOE intent explicitly rejected"), VM->BeginCommandSubmit(
		FGuid::NewGuid(), TEXT("HeroA"), TEXT("Skill.SharedUltimate"), NAME_None));
	VM->UnbindCoordinator();
	return true;
}

#endif
