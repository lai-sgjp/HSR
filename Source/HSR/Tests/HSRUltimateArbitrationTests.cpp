#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "../Battle/HSRBattleCoordinator.h"
#include "../Battle/HSRBattleGameMode.h"
#include "../Battle/HSRTurnManager.h"
#include "../Data/HSRSkillDefinition.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/Definitions/HSRStatusDefinition.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "../GAS/HSRCombatEnergyEffect.h"

namespace HSR::UltimateTests
{
	// This fixture uses the production Coordinator, TurnManager, Actor ASCs and authored
	// Blueprint effects. Only initial attributes and roster duplication are test setup.
	struct FFixture
	{
		UGameInstance* GI = nullptr;
		UWorld* World = nullptr;
		UHSRBattleCoordinator* Coordinator = nullptr;
		UHSRSkillDefinition* Ultimate = nullptr;
		UHSRSkillDefinition* Basic = nullptr;
		UHSRSkillDefinition* Skill = nullptr;

		~FFixture()
		{
			if (Coordinator)
			{
				for (const auto& P : Coordinator->GetParticipants())
					if (P.AbilitySystemComponent.IsValid()) P.AbilitySystemComponent->GameplayEffectApplicationQueries.Reset();
				Coordinator->Reset();
			}
			if (!GI) return;
			GI->Shutdown();
			if (World) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
			GI->RemoveFromRoot();
		}

		bool Initialize(FAutomationTestBase& Test, bool bHealing = false)
		{
			if (!GEngine) return false;
			GI = NewObject<UGameInstance>(GEngine);
			GI->AddToRoot();
			GI->InitializeStandalone(FName(*FString::Printf(TEXT("HSRUltimate_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))));
			World = GI->GetWorld();
			auto GameMode = LoadClass<AHSRBattleGameMode>(nullptr, TEXT("/Game/Blueprints/Framework/BP_HSRBattleGameMode.BP_HSRBattleGameMode_C"));
			FText Failure;
			Coordinator = AHSRBattleGameMode::CreateRepeatableBreakAutomationFixture(GI, World, GameMode, Failure);
			if (!Test.TestNotNull(*FString::Printf(TEXT("Production world fixture: %s"), *Failure.ToString()), Coordinator)) return false;
			const auto* AuthoredUltimate = LoadObject<UHSRSkillDefinition>(nullptr, TEXT("/Game/Data/Skills/DA_Ultimate_Single_Test.DA_Ultimate_Single_Test"));
			if (!Test.TestNotNull(TEXT("Authored Ultimate definition loads"), AuthoredUltimate)) return false;
			Ultimate = DuplicateObject<UHSRSkillDefinition>(AuthoredUltimate, GI);
			// Cost remains the real authored GE. Disable post-cast recovery in this fixture
			// so a single commit has an exact observable 100 -> 0 resource transition.
			Ultimate->EnergyGain = 0.f;
			Basic = const_cast<UHSRSkillDefinition*>(Coordinator->FindDefaultSkillByCategory(EHSRSkillCategory::BasicAttack));
			Skill = const_cast<UHSRSkillDefinition*>(Coordinator->FindDefaultSkillByCategory(EHSRSkillCategory::Skill));
			if (!Test.TestNotNull(TEXT("Production Basic exists"), Basic) || !Test.TestNotNull(TEXT("Production Skill exists"), Skill)) return false;
			if (bHealing)
			{
				Ultimate->bUltimateRestoresHealth = true;
				Ultimate->TargetType = EHSRTargetType::SingleAlly;
				Ultimate->EffectGameplayEffectClass = LoadClass<UGameplayEffect>(nullptr, TEXT("/Game/GameplayEffects/BP_GE_P6_Heal.BP_GE_P6_Heal_C"));
			}
			Coordinator->SetDefaultSkillLoadout({Basic, Skill, Ultimate});
			TArray<FHSRBattleRosterEntry> Roster = Coordinator->GetPlayerRoster();
			auto* Profiles = GI->GetSubsystem<UHSRCharacterProfileSubsystem>();
			const UHSRCharacterDefinition* Original = nullptr;
			if (Roster.IsEmpty() || !Profiles || !Profiles->GetDefinition(Roster[0].CharacterId, Original)) return false;
			for (int32 Index = Roster.Num(); Index < 3; ++Index)
			{
				auto* Definition = DuplicateObject<UHSRCharacterDefinition>(Original, GI);
				Definition->CharacterId = FName(*FString::Printf(TEXT("Automation.Ultimate.Character%d"), Index));
				if (!Test.TestEqual(TEXT("Unique ally profile registered"), Profiles->RegisterDefinition(Definition), EHSRCharacterProfileResult::Success)) return false;
				FHSRCharacterProgressionContext Context;
				if (!Profiles->GetProgressionContext(Definition->CharacterId, Context)) return false;
				Coordinator->SetCharacterProgressionContext(FName(*FString::Printf(TEXT("Player%d"), Index + 1)), Context);
				Roster.Add({Definition->CharacterId, Roster[0].PawnClass});
			}
			const auto* Enemy = Coordinator->FindFirstOfTeam(EHSRBattleParticipantTeam::Enemy);
			if (!Enemy) return false;
			Coordinator->SetPlayerRoster(Roster);
			Coordinator->SetEnemyRoster({{Enemy->DefinitionId, APawn::StaticClass()}, {Enemy->DefinitionId, APawn::StaticClass()}});
			return Rebuild(Test);
		}

		bool Rebuild(FAutomationTestBase& Test)
		{
			if (!Test.TestTrue(TEXT("Production multi-unit battle rebuilds"), Coordinator->ResetAndRebuildForDevelopmentTest(World).IsSuccess())) return false;
			for (const auto& P : Coordinator->GetParticipants())
			{
				Coordinator->FindParticipant(P.ParticipantId)->WeaknessTags.Reset();
				auto* ASC = P.AbilitySystemComponent.Get();
				ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 10000.f);
				ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 10000.f);
				ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetAttackAttribute(), 100.f);
				ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetDefenseAttribute(), 0.f);
				ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetCritRateAttribute(), 0.f);
				ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxEnergyAttribute(), 200.f);
				ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetEnergyAttribute(), 100.f);
				ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), P.Team == EHSRBattleParticipantTeam::Player ? 200.f : 100.f);
			}
			return Test.TestTrue(TEXT("Deterministic ordinary order initializes"), Coordinator->GetTurnManager()->Initialize(Coordinator->GetParticipants()));
		}

		UAbilitySystemComponent* ASC(FName Id) const { const auto* P = Coordinator->FindParticipant(Id); return P ? P->AbilitySystemComponent.Get() : nullptr; }
		float Health(FName Id) const { return ASC(Id)->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()); }
		float Energy(FName Id) const { return ASC(Id)->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute()); }
		float TeamHealth() const { return Health(TEXT("Player")) + Health(TEXT("Player2")) + Health(TEXT("Player3")); }
		void SetHealth(FName Id, float Value) { ASC(Id)->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), Value); }
		void SetEnergy(FName Id, float Value) { ASC(Id)->SetNumericAttributeBase(UHSRCoreAttributeSet::GetEnergyAttribute(), Value); }
		FHSRBattleActionCommand Command(FName Actor, FName Target, const UHSRSkillDefinition* Definition = nullptr) const
		{
			FHSRBattleActionCommand C;
			C.ActionId = FGuid::NewGuid(); C.BattleId = Coordinator->GetCurrentRequestId();
			C.ActorParticipantId = Actor; C.SkillId = (Definition ? Definition : Ultimate)->SkillId;
			C.TargetParticipantIds.Add(Target);
			return C;
		}
		const FHSRUltimateRequestRecord* Record(const FGuid& Id) const
		{
			return Coordinator->GetUltimateRequestHistory().FindByPredicate([&](const auto& R) { return R.Command.ActionId == Id; });
		}
		bool HasState(const FGuid& Id, EHSRUltimateRequestState State) const { const auto* R = Record(Id); return R && R->State == State; }
		TArray<float> AV() const
		{
			TArray<float> Values;
			for (const auto& P : Coordinator->GetTurnManager()->GetOrderedParticipants())
			{ Values.Add(P.BaseActionDistance); Values.Add(P.RemainingActionDistance); }
			return Values;
		}
		void Advance(float Seconds)
		{
			// TimerManager deduplicates by engine frame. A standalone simulation must
			// advance the real frame counter along with each real UWorld tick.
			for (float Elapsed = 0.f; Elapsed + KINDA_SMALL_NUMBER < Seconds; Elapsed += .05f)
			{ ++GFrameCounter; World->Tick(LEVELTICK_All, FMath::Min(.05f, Seconds - Elapsed)); }
		}
		bool EnemyFirst(FAutomationTestBase& Test)
		{
			ASC(TEXT("Enemy"))->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 1000.f);
			if (!Test.TestTrue(TEXT("Enemy-first order initialized"), Coordinator->GetTurnManager()->Initialize(Coordinator->GetParticipants()))) return false;
			// Initialize queues the ordinary turn event; it deliberately does not pump
			// actions recursively. Start the real enemy action through the public entry.
			return Test.TestEqual(TEXT("Real enemy attack presentation starts"), Coordinator->SubmitBattleCommand(Command(TEXT("Enemy"), TEXT("Player"), Basic)).Status, EHSRAbilityResolutionStatus::Accepted);
		}
	};
	static bool Accepted(const FHSRAbilityResolution& R) { return R.Status == EHSRAbilityResolutionStatus::Accepted || R.Succeeded(); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateRequestContractTest, "HSR.Battle.Ultimate.RequestContract.U01_U02_U03_U07_U13", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateRequestContractTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this)) return false;
	F.Coordinator->EnableActionPresentation(F.World);
	F.SetEnergy(TEXT("Player2"), 99.f);
	const float HealthBefore = F.Health(TEXT("Enemy"));
	const auto Insufficient = F.Coordinator->SubmitBattleCommand(F.Command(TEXT("Player2"), TEXT("Enemy")));
	TestEqual(TEXT("U01 insufficient personal energy rejects"), Insufficient.FailureReason, EHSRAbilityFailureReason::InsufficientEnergy);
	TestEqual(TEXT("U01 no debit"), F.Energy(TEXT("Player2")), 99.f);
	TestEqual(TEXT("U01 no damage"), F.Health(TEXT("Enemy")), HealthBefore);
	TestTrue(TEXT("U01 no request reservation"), F.Coordinator->GetUltimateRequestHistory().IsEmpty());
	F.SetEnergy(TEXT("Player2"), 100.f);
	const auto BeforeAV = F.AV();
	const auto NormalActor = F.Coordinator->GetTurnManager()->GetCurrentParticipantId();
	const uint64 Sequence = F.Coordinator->GetTurnManager()->GetTurnSequence();
	const auto Own = F.Command(NormalActor, TEXT("Enemy"));
	TestTrue(TEXT("U02 own-turn ultimate accepted"), Accepted(F.Coordinator->SubmitBattleCommand(Own)));
	TestEqual(TEXT("U02 accepted request has not spent energy before impact"), F.Energy(NormalActor), 100.f);
	TestTrue(TEXT("U07 exact same ID is acknowledged"), Accepted(F.Coordinator->SubmitBattleCommand(Own)));
	const auto Duplicate = F.Coordinator->SubmitBattleCommand(F.Command(NormalActor, TEXT("Enemy")));
	TestEqual(TEXT("U07 new keypress from pending owner rejects duplicate"), Duplicate.FailureReason, EHSRAbilityFailureReason::DuplicateAction);
	TestEqual(TEXT("U07 only one accepted journal entry"), F.Coordinator->GetUltimateRequestHistory().Num(), 1);
	F.Advance(1.7f);
	TestTrue(TEXT("U02 one own ultimate completed"), F.HasState(Own.ActionId, EHSRUltimateRequestState::Completed));
	TestEqual(TEXT("U02 ordinary owner preserved"), F.Coordinator->GetTurnManager()->GetCurrentParticipantId(), NormalActor);
	TestEqual(TEXT("U13 no normal turn event"), F.Coordinator->GetTurnManager()->GetTurnSequence(), Sequence);
	TestTrue(TEXT("U13 full ordinary AV state unchanged"), F.AV() == BeforeAV);
	TestEqual(TEXT("U07 cost exactly once"), F.Energy(NormalActor), 0.f);
	const float AfterOwnHealth = F.Health(TEXT("Enemy"));
	TestTrue(TEXT("U07 completed replay returns cached success"), F.Coordinator->SubmitBattleCommand(Own).Succeeded());
	F.Advance(2.f);
	TestEqual(TEXT("U07 completed replay cannot apply damage again"), F.Health(TEXT("Enemy")), AfterOwnHealth);
	const auto OffTurn = F.Command(TEXT("Player2"), TEXT("Enemy"));
	TestTrue(TEXT("U03 other ally can request while normal owner selects"), Accepted(F.Coordinator->SubmitBattleCommand(OffTurn)));
	F.Advance(1.7f);
	TestTrue(TEXT("U03 off-turn ultimate completes"), F.HasState(OffTurn.ActionId, EHSRUltimateRequestState::Completed));
	TestEqual(TEXT("U03 original normal actor resumes"), F.Coordinator->GetTurnManager()->GetCurrentParticipantId(), NormalActor);
	// A fresh request after refill must be independent of the previous execution's context.
	F.SetEnergy(TEXT("Player2"), 100.f);
	const auto Refilled = F.Command(TEXT("Player2"), TEXT("Enemy"));
	TestTrue(TEXT("Fresh request after completed activation and refill accepts"), Accepted(F.Coordinator->SubmitBattleCommand(Refilled)));
	F.Advance(1.7f);
	TestTrue(TEXT("Fresh action ID executes after refill"), F.HasState(Refilled.ActionId, EHSRUltimateRequestState::Completed));
	TestTrue(TEXT("U18 normal action remains usable after ultimates"), Accepted(F.Coordinator->SubmitBattleCommand(F.Command(NormalActor, TEXT("Enemy"), F.Basic))));
	F.Advance(1.7f);
	TestTrue(TEXT("U18 normal action advances ordinary turn"), F.Coordinator->GetTurnManager()->GetTurnSequence() > Sequence);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateAtomicFIFOTest, "HSR.Battle.Ultimate.AtomicFIFO.U04_U05_U06_U08_U14_U18", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateAtomicFIFOTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this)) return false;
	F.Coordinator->EnableActionPresentation(F.World);
	const FName A = F.Coordinator->GetTurnManager()->GetCurrentParticipantId();
	const auto* AuthoredBuff = F.Coordinator->GetStatusDefinition();
	if (!TestNotNull(TEXT("Production turn-counted buff exists"), AuthoredBuff)) return false;
	auto* Buff = DuplicateObject<UHSRStatusDefinition>(AuthoredBuff, F.GI);
	Buff->DurationTurns = 1;
	TestEqual(TEXT("U14 real infinite GE buff applied"), F.Coordinator->AddSpecificStatusForDevelopmentTest(Buff, A, A), EHSRStatusOperationResult::Success);
	const uint64 OrdinarySequence = F.Coordinator->GetTurnManager()->GetTurnSequence();
	const auto BeforeAV = F.AV();
	const float Before = F.Health(TEXT("Enemy"));
	TestTrue(TEXT("U18 ordinary attack starts presentation"), Accepted(F.Coordinator->SubmitBattleCommand(F.Command(A, TEXT("Enemy"), F.Basic))));
	const auto B = F.Command(TEXT("Player2"), TEXT("Enemy"));
	const auto C = F.Command(TEXT("Player3"), TEXT("Enemy"));
	TestTrue(TEXT("U04 B accepted during ordinary attack windup"), Accepted(F.Coordinator->SubmitBattleCommand(B)));
	TestTrue(TEXT("U05 C accepted in same logical frame"), Accepted(F.Coordinator->SubmitBattleCommand(C)));
	if (!TestTrue(TEXT("U06 accepted records exist"), F.Record(B.ActionId) && F.Record(C.ActionId))) return false;
	TestTrue(TEXT("U06 stable monotonically ordered same-frame events"), F.Record(B.ActionId)->AcceptedSequence < F.Record(C.ActionId)->AcceptedSequence);
	TestEqual(TEXT("U04 requests do not preempt normal impact"), F.Health(TEXT("Enemy")), Before);
	const auto Reentry = F.Coordinator->RequestAction(F.Command(A, TEXT("Enemy"), F.Basic));
	TestEqual(TEXT("U18 normal reentry remains Reject while busy"), Reentry.Status, EHSRAbilityResolutionStatus::Rejected);
	F.Advance(.65f);
	TestTrue(TEXT("U04 original attack fully settles through GE"), F.Health(TEXT("Enemy")) < Before);
	TestTrue(TEXT("U04 ultimates still waiting before presentation safe point"), F.HasState(B.ActionId, EHSRUltimateRequestState::Accepted));
	TestEqual(TEXT("U14 no early normal end before safe point"), F.Coordinator->GetTurnManager()->GetTurnSequence(), OrdinarySequence);
	TestEqual(TEXT("U14 buff still has one ordinary turn"), F.Coordinator->GetStatusSnapshotForDevelopmentTest(A, Buff->StatusId).RemainingTurns, 1);
	TestTrue(TEXT("U13 presentation elapsed time does not advance AV"), F.AV() == BeforeAV);
	F.Advance(1.f);
	TestTrue(TEXT("U05 B enters execution first"), F.HasState(B.ActionId, EHSRUltimateRequestState::Executing));
	TestTrue(TEXT("U05 C remains queued"), F.HasState(C.ActionId, EHSRUltimateRequestState::Accepted));
	TestEqual(TEXT("U14 original turn still suspended throughout B"), F.Coordinator->GetTurnManager()->GetTurnSequence(), OrdinarySequence);
	F.Advance(1.5f);
	TestTrue(TEXT("U05 B completes before C"), F.HasState(B.ActionId, EHSRUltimateRequestState::Completed));
	TestTrue(TEXT("U05 C executes after B"), F.HasState(C.ActionId, EHSRUltimateRequestState::Executing));
	TestEqual(TEXT("U14 one-turn buff survives the entire extra-action chain"), F.Coordinator->GetStatusSnapshotForDevelopmentTest(A, Buff->StatusId).RemainingTurns, 1);
	F.Advance(1.7f);
	TestTrue(TEXT("U05 C completes"), F.HasState(C.ActionId, EHSRUltimateRequestState::Completed));
	TestEqual(TEXT("U14 ordinary action finalizes exactly once"), F.Coordinator->GetTurnManager()->GetTurnSequence(), OrdinarySequence + 1);
	TestEqual(TEXT("U14 buff removed exactly at original ordinary end"), F.Coordinator->GetStatusSnapshotForDevelopmentTest(A, Buff->StatusId).InstanceCount, 0);
	TestEqual(TEXT("U05 B cost once"), F.Energy(TEXT("Player2")), 0.f);
	TestEqual(TEXT("U05 C cost once"), F.Energy(TEXT("Player3")), 0.f);
	// A second chain specifically submits C while B is already presenting.
	F.SetEnergy(TEXT("Player2"), 100.f); F.SetEnergy(TEXT("Player3"), 100.f);
	const auto B2 = F.Command(TEXT("Player2"), TEXT("Enemy"));
	TestTrue(TEXT("U08 B starts without a normal action"), Accepted(F.Coordinator->SubmitBattleCommand(B2)));
	F.Advance(.2f);
	const auto C2 = F.Command(TEXT("Player3"), TEXT("Enemy"));
	TestTrue(TEXT("U08 C accepted during B ultimate"), Accepted(F.Coordinator->SubmitBattleCommand(C2)));
	TestTrue(TEXT("U08 C is queued without nesting"), F.HasState(C2.ActionId, EHSRUltimateRequestState::Accepted));
	F.Advance(1.5f);
	TestTrue(TEXT("U08 B finished before C starts"), F.HasState(B2.ActionId, EHSRUltimateRequestState::Completed));
	TestTrue(TEXT("U08 C now executing"), F.HasState(C2.ActionId, EHSRUltimateRequestState::Executing));
	F.Advance(1.7f);
	TestTrue(TEXT("U08 C finishes"), F.HasState(C2.ActionId, EHSRUltimateRequestState::Completed));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateEnemyBoundaryTest, "HSR.Battle.Ultimate.EnemyBoundary.U04_U11", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateEnemyBoundaryTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this)) return false;
	F.Coordinator->EnableActionPresentation(F.World);
	if (!F.EnemyFirst(*this)) return false;
	const float PlayerBefore = F.TeamHealth();
	const float EnemyBefore = F.Health(TEXT("Enemy2"));
	const uint64 Sequence = F.Coordinator->GetTurnManager()->GetTurnSequence();
	const auto B = F.Command(TEXT("Player2"), TEXT("Enemy2"));
	TestTrue(TEXT("U04 request accepted during real enemy windup"), Accepted(F.Coordinator->SubmitBattleCommand(B)));
	F.Advance(.2f);
	TestEqual(TEXT("U04 original enemy has not hit before its impact"), F.TeamHealth(), PlayerBefore);
	TestEqual(TEXT("U04 ultimate has not hit during enemy windup"), F.Health(TEXT("Enemy2")), EnemyBefore);
	F.Advance(.45f);
	TestTrue(TEXT("U04 original enemy damage reaches player before ultimate"), F.TeamHealth() < PlayerBefore);
	TestTrue(TEXT("U04 ultimate still waits for enemy presentation completion"), F.HasState(B.ActionId, EHSRUltimateRequestState::Accepted));
	TestEqual(TEXT("U04 enemy ordinary end remains suspended"), F.Coordinator->GetTurnManager()->GetTurnSequence(), Sequence);
	F.SetHealth(TEXT("Enemy2"), 1.f);
	F.Advance(3.f);
	TestTrue(TEXT("U11 ultimate kills next ordinary enemy through real GE"), !F.Coordinator->FindParticipant(TEXT("Enemy2"))->IsAlive());
	TestTrue(TEXT("U11 forecast skips killed enemy"), !F.Coordinator->GetTurnManager()->BuildTurnForecast(8).ContainsByPredicate([](const auto& E) { return E.ParticipantId == FName(TEXT("Enemy2")); }));
	TestTrue(TEXT("U04 ultimate completes and ordinary flow advances"), F.HasState(B.ActionId, EHSRUltimateRequestState::Completed) && F.Coordinator->GetTurnManager()->GetTurnSequence() > Sequence);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateInvalidationTest, "HSR.Battle.Ultimate.Invalidation.U09_U10_U15", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateInvalidationTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this)) return false;
	F.Coordinator->EnableActionPresentation(F.World);
	const auto Normal = F.Command(TEXT("Player"), TEXT("Enemy2"), F.Basic);
	TestTrue(TEXT("Ordinary action opens bounded windup"), Accepted(F.Coordinator->SubmitBattleCommand(Normal)));
	const auto Dying = F.Command(TEXT("Player2"), TEXT("Enemy"));
	const auto LostTarget = F.Command(TEXT("Player3"), TEXT("Enemy"));
	TestTrue(TEXT("U09 queued actor initially valid"), Accepted(F.Coordinator->SubmitBattleCommand(Dying)));
	TestTrue(TEXT("U10 queued target initially valid"), Accepted(F.Coordinator->SubmitBattleCommand(LostTarget)));
	F.SetHealth(TEXT("Player2"), 0.f);
	F.SetHealth(TEXT("Enemy"), 0.f);
	const float OtherEnemyHealth = F.Health(TEXT("Enemy2"));
	F.Advance(4.f);
	TestTrue(TEXT("U09 dead queued actor cancelled"), F.HasState(Dying.ActionId, EHSRUltimateRequestState::Cancelled));
	TestTrue(TEXT("U10 dead confirmed target cancelled"), F.HasState(LostTarget.ActionId, EHSRUltimateRequestState::Cancelled));
	TestEqual(TEXT("U09 no energy debit before commit"), F.Energy(TEXT("Player2")), 100.f);
	TestEqual(TEXT("U10 no energy debit on target cancellation"), F.Energy(TEXT("Player3")), 100.f);
	TestTrue(TEXT("U10 only original ordinary action damages other enemy"), F.Health(TEXT("Enemy2")) < OtherEnemyHealth);
	const float AfterNormal = F.Health(TEXT("Enemy2"));
	F.Advance(2.f);
	TestEqual(TEXT("U10 cancelled request never silently retargets later"), F.Health(TEXT("Enemy2")), AfterNormal);
	TestTrue(TEXT("U15 valid later request still executes after cancellations"), Accepted(F.Coordinator->SubmitBattleCommand(F.Command(TEXT("Player3"), TEXT("Enemy2")))));
	F.Advance(1.7f);
	TestTrue(TEXT("U15 queue recovered and valid effect applied"), F.Health(TEXT("Enemy2")) < AfterNormal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateFailureCompensationTest, "HSR.Battle.Ultimate.Failures.U15", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateFailureCompensationTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this)) return false;
	const auto BeforeAV = F.AV();
	const float BeforeHealth = F.Health(TEXT("Enemy"));
	const auto Failed = F.Command(TEXT("Player2"), TEXT("Enemy"));
	F.Coordinator->SetDamageTestInjectionForAction(Failed.ActionId, EHSRDamageTestInjection::ForcePostCostApplyFailure);
	F.Coordinator->SubmitBattleCommand(Failed);
	TestTrue(TEXT("U15 post-commit effect failure cancels request"), F.HasState(Failed.ActionId, EHSRUltimateRequestState::Cancelled));
	TestEqual(TEXT("U15 post-commit failure refunds exactly once"), F.Energy(TEXT("Player2")), 100.f);
	TestEqual(TEXT("U15 effect failure deals no damage"), F.Health(TEXT("Enemy")), BeforeHealth);
	F.Coordinator->SubmitBattleCommand(Failed);
	TestEqual(TEXT("U15 replay cannot refund twice"), F.Energy(TEXT("Player2")), 100.f);
	F.Coordinator->ClearDamageTestInjection();
	F.ASC(TEXT("Player3"))->GameplayEffectApplicationQueries.Add(FGameplayEffectApplicationQuery::CreateLambda(
		[](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec&) { return false; }));
	const auto CostFailed = F.Command(TEXT("Player3"), TEXT("Enemy"));
	F.Coordinator->SubmitBattleCommand(CostFailed);
	TestTrue(TEXT("U15 rejected cost cancels request"), F.HasState(CostFailed.ActionId, EHSRUltimateRequestState::Cancelled));
	TestEqual(TEXT("U15 rejected cost cannot grant free damage"), F.Health(TEXT("Enemy")), BeforeHealth);
	TestEqual(TEXT("U15 rejected cost preserves energy"), F.Energy(TEXT("Player3")), 100.f);
	F.ASC(TEXT("Player3"))->GameplayEffectApplicationQueries.Reset();
	const auto Recovery = F.Command(TEXT("Player3"), TEXT("Enemy"));
	TestTrue(TEXT("U15 later valid request executes without permanent Busy"), F.Coordinator->SubmitBattleCommand(Recovery).Succeeded());
	TestTrue(TEXT("U15 successful recovery applies real damage"), F.Health(TEXT("Enemy")) < BeforeHealth);
	TestTrue(TEXT("U15 all failure/recovery paths preserve normal AV"), F.AV() == BeforeAV);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateEffectAVTest, "HSR.Battle.Ultimate.AuthoritativeAV.U12_U13", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateEffectAVTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this)) return false;
	F.Coordinator->EnableActionPresentation(F.World);
	const auto Request = F.Command(TEXT("Player2"), TEXT("Enemy"));
	TestTrue(TEXT("Ultimate presenting while legal effects can change AV"), Accepted(F.Coordinator->SubmitBattleCommand(Request)));
	auto* SpeedGE = NewObject<UGameplayEffect>(F.GI);
	SpeedGE->DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& Modifier = SpeedGE->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UHSRCoreAttributeSet::GetSpeedAttribute(); Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FScalableFloat(50.f);
	FGameplayEffectSpec SpeedSpec(SpeedGE, F.ASC(TEXT("Player3"))->MakeEffectContext(), 1.f);
	TestTrue(TEXT("U12 real Speed GE is applied during ultimate"), F.ASC(TEXT("Player3"))->ApplyGameplayEffectSpecToSelf(SpeedSpec).WasSuccessfullyApplied());
	float Speed = 0.f, Base = 0.f, Remaining = 0.f; int32 Pending = 0;
	F.Coordinator->GetTurnManager()->GetActionDistanceForAutomation(TEXT("Player3"), Speed, Base, Remaining, Pending);
	TestEqual(TEXT("U12 existing speed delegate updates AV authority"), Speed, 250.f);
	TestTrue(TEXT("U12 correct 10000/newSpeed base"), FMath::IsNearlyEqual(Base, 40.f));
	FHSRActionDistanceRequest Advance;
	Advance.BattleEpoch = F.Coordinator->GetTurnManager()->GetBattleEpoch(); Advance.OperationId = FGuid::NewGuid();
	Advance.TargetParticipantId = TEXT("Player3"); Advance.Kind = EHSRActionDistanceAdjustmentKind::Advance; Advance.Ratio = .25f;
	TestEqual(TEXT("U12 official action-distance entry accepts explicit effect"), F.Coordinator->GetTurnManager()->RequestActionDistanceAdjustment(Advance).Result, EHSRActionDistanceAdjustmentResult::Accepted);
	const auto UpdatedAV = F.AV();
	F.Advance(1.7f);
	TestTrue(TEXT("U12 legitimate changes survive ultimate resume"), F.AV() == UpdatedAV);
	TestTrue(TEXT("U13 no cosmetic elapsed-time AV consumption"), F.HasState(Request.ActionId, EHSRUltimateRequestState::Completed));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateResetTest, "HSR.Battle.Ultimate.Generation.U16_U17", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateResetTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this)) return false;
	F.Coordinator->EnableActionPresentation(F.World);
	const auto OldRequest = F.Command(TEXT("Player2"), TEXT("Enemy"));
	TestTrue(TEXT("U16 old battle action accepted"), Accepted(F.Coordinator->SubmitBattleCommand(OldRequest)));
	const auto Queued = F.Command(TEXT("Player3"), TEXT("Enemy"));
	TestTrue(TEXT("U16 old battle has pending request"), Accepted(F.Coordinator->SubmitBattleCommand(Queued)));
	const uint64 OldGeneration = F.Coordinator->GetBattleGeneration();
	if (!F.Rebuild(*this)) return false;
	TestTrue(TEXT("U17 battle generation advances on rebuild"), F.Coordinator->GetBattleGeneration() != OldGeneration);
	const auto NewAV = F.AV();
	const float HealthBefore = F.Health(TEXT("Enemy"));
	F.Advance(5.f);
	TestEqual(TEXT("U17 stale world timer cannot damage new battle"), F.Health(TEXT("Enemy")), HealthBefore);
	TestTrue(TEXT("U17 stale completion cannot advance new ordinary turn"), F.AV() == NewAV);
	TestTrue(TEXT("U17 accepted history does not leak across reset"), F.Coordinator->GetUltimateRequestHistory().IsEmpty());
	TestEqual(TEXT("U17 stale command rejects battle identity"), F.Coordinator->SubmitBattleCommand(OldRequest).FailureReason, EHSRAbilityFailureReason::InvalidBattle);
	F.Coordinator->EnableActionPresentation(F.World);
	const auto Fresh = F.Command(TEXT("Player2"), TEXT("Enemy"));
	TestTrue(TEXT("U17 new battle receives fresh request"), Accepted(F.Coordinator->SubmitBattleCommand(Fresh)));
	F.Advance(1.7f);
	TestTrue(TEXT("U17 new battle action completes"), F.HasState(Fresh.ActionId, EHSRUltimateRequestState::Completed));
	// End an active battle while another request is waiting, then allow old timers to run.
	F.SetEnergy(TEXT("Player2"), 100.f);
	const auto Active = F.Command(TEXT("Player2"), TEXT("Enemy"));
	const auto Waiting = F.Command(TEXT("Player3"), TEXT("Enemy2"));
	F.Coordinator->SubmitBattleCommand(Active); F.Coordinator->SubmitBattleCommand(Waiting);
	F.SetHealth(TEXT("Enemy"), 0.f); F.SetHealth(TEXT("Enemy2"), 0.f);
	TestEqual(TEXT("U16 team wipe finishes battle"), F.Coordinator->GetCurrentState(), EHSRBattleCoordinatorState::Finished);
	F.Advance(3.f);
	TestTrue(TEXT("U16 waiting request cancelled on battle end"), F.HasState(Waiting.ActionId, EHSRUltimateRequestState::Cancelled));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateHealingThresholdTest, "HSR.Battle.Ultimate.SupportAndThreshold", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateHealingThresholdTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this, true)) return false;
	F.SetHealth(TEXT("Player"), 100.f);
	F.Ultimate->UltimateReleaseThreshold = 120.f;
	if (!F.Rebuild(*this)) return false;
	F.SetHealth(TEXT("Player"), 100.f);
	const auto BelowThreshold = F.Command(TEXT("Player2"), TEXT("Player"));
	TestEqual(TEXT("Custom threshold rejects despite sufficient 100 cost"), F.Coordinator->SubmitBattleCommand(BelowThreshold).FailureReason, EHSRAbilityFailureReason::InsufficientEnergy);
	F.SetEnergy(TEXT("Player2"), 120.f);
	const auto BeforeAV = F.AV();
	const auto Heal = F.Command(TEXT("Player2"), TEXT("Player"));
	TestTrue(TEXT("Support Ultimate goes through production ASC Ability and Heal GE"), F.Coordinator->SubmitBattleCommand(Heal).Succeeded());
	TestTrue(TEXT("Support Ultimate really increases ally Health"), F.Health(TEXT("Player")) > 100.f);
	TestEqual(TEXT("Threshold is independent from actual GAS cost"), F.Energy(TEXT("Player2")), 20.f);
	TestTrue(TEXT("Support Ultimate preserves ordinary AV"), F.AV() == BeforeAV);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateTeamHealTest, "HSR.Battle.Ultimate.TeamHeal.AllAlliesCostOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateTeamHealTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this, true)) return false;
	F.Ultimate->TargetType = EHSRTargetType::AllAllies;
	if (!F.Rebuild(*this)) return false;
	const TArray<FName> Allies = {TEXT("Player"), TEXT("Player2"), TEXT("Player3")};
	for (const auto Id : Allies) F.SetHealth(Id, 100.f);
	F.Coordinator->EnableActionPresentation(F.World);
	TArray<int32> HealEvents = {0, 0, 0};
	TArray<FDelegateHandle> Handles;
	for (int32 Index = 0; Index < Allies.Num(); ++Index)
		Handles.Add(F.ASC(Allies[Index])->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetHealthAttribute()).AddLambda(
			[&, Index](const FOnAttributeChangeData& Data) { if (Data.NewValue > Data.OldValue) ++HealEvents[Index]; }));
	int32 CostEvents = 0;
	const auto CostHandle = F.ASC(TEXT("Player2"))->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Data) { if (FMath::IsNearlyEqual(Data.OldValue - Data.NewValue, 100.f)) ++CostEvents; });
	auto Heal = F.Command(TEXT("Player2"), TEXT("Player")); Heal.TargetParticipantIds = Allies;
	const auto AV = F.AV();
	TestTrue(TEXT("AllAllies request accepted through production targeting policy"), Accepted(F.Coordinator->SubmitBattleCommand(Heal)));
	F.Coordinator->SubmitBattleCommand(Heal);
	F.Advance(1.7f);
	TestTrue(TEXT("AllAllies ultimate completes"), F.HasState(Heal.ActionId, EHSRUltimateRequestState::Completed));
	TestEqual(TEXT("Team heal commits one cost for all three recipients"), CostEvents, 1);
	TestEqual(TEXT("Team heal spends actual authored 100 energy once"), F.Energy(TEXT("Player2")), 0.f);
	for (int32 Index = 0; Index < Allies.Num(); ++Index)
	{
		TestTrue(*FString::Printf(TEXT("Real ally %s healed"), *Allies[Index].ToString()), F.Health(Allies[Index]) > 100.f);
		TestEqual(*FString::Printf(TEXT("Ally %s healed exactly once despite repeated input"), *Allies[Index].ToString()), HealEvents[Index], 1);
		F.ASC(Allies[Index])->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetHealthAttribute()).Remove(Handles[Index]);
	}
	F.ASC(TEXT("Player2"))->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetEnergyAttribute()).Remove(CostHandle);
	TestTrue(TEXT("Team heal preserves all normal AV state"), F.AV() == AV);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateAOETest, "HSR.Battle.Ultimate.AOE.AllEnemiesCostAndCasualties", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateAOETest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this)) return false;
	F.Ultimate->TargetType = EHSRTargetType::AllEnemies;
	if (!F.Rebuild(*this)) return false;
	F.Coordinator->EnableActionPresentation(F.World);
	int32 FirstDamage = 0, SecondDamage = 0, Cost = 0;
	const float FirstBefore = F.Health(TEXT("Enemy")), SecondBefore = F.Health(TEXT("Enemy2"));
	const auto H1 = F.ASC(TEXT("Enemy"))->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetHealthAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& D) { if (D.NewValue < D.OldValue) ++FirstDamage; });
	const auto H2 = F.ASC(TEXT("Enemy2"))->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetHealthAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& D) { if (D.NewValue < D.OldValue) ++SecondDamage; });
	const auto CostHandle = F.ASC(TEXT("Player2"))->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& D) { if (FMath::IsNearlyEqual(D.OldValue - D.NewValue, 100.f)) ++Cost; });
	auto AOE = F.Command(TEXT("Player2"), TEXT("Enemy")); AOE.TargetParticipantIds.Add(TEXT("Enemy2"));
	const auto BeforeAV = F.AV();
	TestTrue(TEXT("AOE confirmed two-enemy request accepted"), Accepted(F.Coordinator->SubmitBattleCommand(AOE)));
	F.Coordinator->SubmitBattleCommand(AOE);
	F.Advance(1.7f);
	TestTrue(TEXT("AOE completes through real two-target damage GE path"), F.HasState(AOE.ActionId, EHSRUltimateRequestState::Completed));
	TestTrue(TEXT("AOE damage reaches both enemy ASCs"), F.Health(TEXT("Enemy")) < FirstBefore && F.Health(TEXT("Enemy2")) < SecondBefore);
	TestEqual(TEXT("AOE first target receives exactly one damage transaction"), FirstDamage, 1);
	TestEqual(TEXT("AOE second target receives exactly one damage transaction"), SecondDamage, 1);
	TestEqual(TEXT("AOE pays one cost for all targets"), Cost, 1);
	TestEqual(TEXT("AOE costs exactly100 energy"), F.Energy(TEXT("Player2")), 0.f);
	TestTrue(TEXT("AOE does not consume ordinary AV"), F.AV() == BeforeAV);
	const float FirstAfter = F.Health(TEXT("Enemy")), SecondAfter = F.Health(TEXT("Enemy2"));
	F.Coordinator->SubmitBattleCommand(AOE); F.Advance(2.f);
	TestEqual(TEXT("Completed AOE replay cannot damage first target twice"), F.Health(TEXT("Enemy")), FirstAfter);
	TestEqual(TEXT("Completed AOE replay cannot damage second target twice"), F.Health(TEXT("Enemy2")), SecondAfter);
	TestEqual(TEXT("Completed AOE replay cannot spend twice"), Cost, 1);
	F.ASC(TEXT("Enemy"))->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetHealthAttribute()).Remove(H1);
	F.ASC(TEXT("Enemy2"))->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetHealthAttribute()).Remove(H2);
	F.ASC(TEXT("Player2"))->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetEnergyAttribute()).Remove(CostHandle);
	// A terminal multi-kill must register both defeated actors even though the final
	// defeat completes the battle while the same atomic AOE is still settling.
	if (!F.Rebuild(*this)) return false;
	F.Coordinator->EnableActionPresentation(F.World);
	F.SetHealth(TEXT("Enemy"), 1.f); F.SetHealth(TEXT("Enemy2"), 1.f);
	auto Lethal = F.Command(TEXT("Player2"), TEXT("Enemy")); Lethal.TargetParticipantIds.Add(TEXT("Enemy2"));
	TestTrue(TEXT("Terminal AOE accepted"), Accepted(F.Coordinator->SubmitBattleCommand(Lethal)));
	F.Advance(1.7f);
	TestFalse(TEXT("AOE first casualty registered"), F.Coordinator->FindParticipant(TEXT("Enemy"))->IsAlive());
	TestFalse(TEXT("AOE second casualty registered"), F.Coordinator->FindParticipant(TEXT("Enemy2"))->IsAlive());
	TestEqual(TEXT("AOE retains both death registrations"), F.Coordinator->GetDefeatCountForDevelopmentTest(), 2);
	TestEqual(TEXT("AOE terminal team wipe finishes battle"), F.Coordinator->GetCurrentState(), EHSRBattleCoordinatorState::Finished);
	TestEqual(TEXT("AOE battle result produced once"), F.Coordinator->GetBattleResultBroadcastCountForDevelopmentTest(), 1);
	TestTrue(TEXT("Settled lethal AOE remains completed rather than cancelled"), F.HasState(Lethal.ActionId, EHSRUltimateRequestState::Completed));
	// The confirmed target set is part of the request. Losing a member while
	// waiting cancels; it must not silently become a different one-target skill.
	if (!F.Rebuild(*this)) return false;
	F.Coordinator->EnableActionPresentation(F.World);
	F.Coordinator->SubmitBattleCommand(F.Command(TEXT("Player"), TEXT("Enemy2"), F.Basic));
	auto Invalidated = F.Command(TEXT("Player2"), TEXT("Enemy")); Invalidated.TargetParticipantIds.Add(TEXT("Enemy2"));
	TestTrue(TEXT("Queued AOE admitted with both original targets"), Accepted(F.Coordinator->SubmitBattleCommand(Invalidated)));
	F.SetHealth(TEXT("Enemy"), 0.f);
	F.Advance(.65f);
	const float AfterNormal = F.Health(TEXT("Enemy2"));
	F.Advance(3.f);
	TestTrue(TEXT("Changed queued AOE target set cancels"), F.HasState(Invalidated.ActionId, EHSRUltimateRequestState::Cancelled));
	TestEqual(TEXT("Invalid AOE does not retarget surviving enemy"), F.Health(TEXT("Enemy2")), AfterNormal);
	TestEqual(TEXT("Invalid AOE target set never spends energy"), F.Energy(TEXT("Player2")), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateRevalidationTest, "HSR.Battle.Ultimate.RevalidateSpecAndResource.U15", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateRevalidationTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this)) return false;
	F.Coordinator->EnableActionPresentation(F.World);
	TestTrue(TEXT("Normal atomic action starts"), Accepted(F.Coordinator->SubmitBattleCommand(F.Command(TEXT("Player"), TEXT("Enemy"), F.Basic))));
	const auto SpecLost = F.Command(TEXT("Player2"), TEXT("Enemy"));
	const auto EnergyLost = F.Command(TEXT("Player3"), TEXT("Enemy"));
	TestTrue(TEXT("Owned ability initially admits queued request"), Accepted(F.Coordinator->SubmitBattleCommand(SpecLost)));
	TestTrue(TEXT("Sufficient resource initially admits queued request"), Accepted(F.Coordinator->SubmitBattleCommand(EnergyLost)));
	if (const auto* Spec = F.ASC(TEXT("Player2"))->FindAbilitySpecFromClass(F.Ultimate->AbilityClass)) F.ASC(TEXT("Player2"))->ClearAbility(Spec->Handle);
	F.SetEnergy(TEXT("Player3"), 99.f);
	F.Advance(4.f);
	TestTrue(TEXT("Removed AbilitySpec cancels at safe point without lock"), F.HasState(SpecLost.ActionId, EHSRUltimateRequestState::Cancelled));
	TestTrue(TEXT("Resource is revalidated before execution"), F.HasState(EnergyLost.ActionId, EHSRUltimateRequestState::Cancelled));
	TestEqual(TEXT("Removed ability did not spend resource"), F.Energy(TEXT("Player2")), 100.f);
	TestEqual(TEXT("Resource invalidation did not debit again"), F.Energy(TEXT("Player3")), 99.f);
	TestTrue(TEXT("Queued spec invalidation preserves normal continuation"), F.Coordinator->GetTurnManager()->GetCurrentParticipantId() != FName(TEXT("Player")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateStressTest, "HSR.Battle.Ultimate.Stress.U19_20Battles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateStressTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this)) return false;
	constexpr int32 Rounds = 20;
	int32 SuccessfulRounds = 0;
	for (int32 Round = 0; Round < Rounds; ++Round)
	{
		if (Round && !F.Rebuild(*this)) break;
		F.Coordinator->EnableActionPresentation(F.World);
		if (Round % 2 && !F.EnemyFirst(*this)) break;
		if (Round % 2 == 0)
			if (!Accepted(F.Coordinator->SubmitBattleCommand(F.Command(TEXT("Player"), TEXT("Enemy"), F.Basic)))) break;
		F.SetHealth(TEXT("Enemy2"), 1.f);
		const auto B = F.Command(TEXT("Player2"), TEXT("Enemy"));
		const auto C = F.Command(TEXT("Player3"), TEXT("Enemy2"));
		int32 BCostCount = 0, CCostCount = 0;
		float BRecovery = 0.f, CRecovery = 0.f;
		const auto BHandle = F.ASC(TEXT("Player2"))->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetEnergyAttribute()).AddLambda(
			[&](const FOnAttributeChangeData& Data) { if (FMath::IsNearlyEqual(Data.OldValue - Data.NewValue, 100.f)) ++BCostCount; if (Data.NewValue > Data.OldValue) BRecovery += Data.NewValue - Data.OldValue; });
		const auto CHandle = F.ASC(TEXT("Player3"))->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetEnergyAttribute()).AddLambda(
			[&](const FOnAttributeChangeData& Data) { if (FMath::IsNearlyEqual(Data.OldValue - Data.NewValue, 100.f)) ++CCostCount; if (Data.NewValue > Data.OldValue) CRecovery += Data.NewValue - Data.OldValue; });
		const bool bAccepted = Accepted(F.Coordinator->SubmitBattleCommand(B)) && Accepted(F.Coordinator->SubmitBattleCommand(C));
		for (int32 Press = 0; Press < 8; ++Press) F.Coordinator->SubmitBattleCommand(F.Command(TEXT("Player2"), TEXT("Enemy")));
		F.Advance(6.f);
		const bool bPassed = bAccepted && F.HasState(B.ActionId, EHSRUltimateRequestState::Completed)
			&& F.HasState(C.ActionId, EHSRUltimateRequestState::Completed)
			&& F.Record(B.ActionId)->AcceptedSequence < F.Record(C.ActionId)->AcceptedSequence
			&& F.Coordinator->GetUltimateRequestHistory().Num() == 2
			&& BCostCount == 1 && CCostCount == 1
			&& FMath::IsNearlyEqual(F.Energy(TEXT("Player2")), BRecovery)
			&& FMath::IsNearlyEqual(F.Energy(TEXT("Player3")), CRecovery) && CRecovery >= 10.f
			&& !F.Coordinator->FindParticipant(TEXT("Enemy2"))->IsAlive()
			&& F.Coordinator->GetMaxCoreExecutionDepthForDevelopmentTest() == 1;
		F.ASC(TEXT("Player2"))->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetEnergyAttribute()).Remove(BHandle);
		F.ASC(TEXT("Player3"))->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetEnergyAttribute()).Remove(CHandle);
		UE_LOG(LogTemp, Log, TEXT("UltimateStress Round=%d/%d EnemyFirst=%d Generation=%llu Accepted=%d BCost=%d CCost=%d BRecovery=%.2f CRecovery=%.2f Result=%s"),
			Round + 1, Rounds, Round % 2, F.Coordinator->GetBattleGeneration(), F.Coordinator->GetUltimateRequestHistory().Num(), BCostCount, CCostCount, BRecovery, CRecovery, bPassed ? TEXT("PASS") : TEXT("FAIL"));
		if (!TestTrue(*FString::Printf(TEXT("U19 real battle stress round %d"), Round + 1), bPassed)) break;
		++SuccessfulRounds;
	}
	UE_LOG(LogTemp, Log, TEXT("UltimateStress Summary ActualRounds=%d RequiredRounds=%d Successes=%d Failures=%d"), SuccessfulRounds == Rounds ? Rounds : SuccessfulRounds + 1, Rounds, SuccessfulRounds, SuccessfulRounds == Rounds ? 0 : 1);
	TestEqual(TEXT("U19 all twenty independent battle generations passed"), SuccessfulRounds, Rounds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimatePauseResumeTest, "HSR.Battle.Ultimate.PauseResume", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimatePauseResumeTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this)) return false;
	F.Coordinator->EnableActionPresentation(F.World);
	const auto Request = F.Command(TEXT("Player2"), TEXT("Enemy"));
	TestTrue(TEXT("Ultimate begins before pause"), Accepted(F.Coordinator->SubmitBattleCommand(Request)));
	F.Advance(.15f);
	const float Before = F.Health(TEXT("Enemy"));
	auto* Pauser = F.World->SpawnActor<APlayerState>();
	if (!TestNotNull(TEXT("Real world pause owner"), Pauser)) return false;
	F.World->GetWorldSettings()->SetPauserPlayerState(Pauser);
	TestTrue(TEXT("World reports actual gameplay pause"), F.World->IsPaused());
	F.Advance(2.f);
	TestEqual(TEXT("Paused world does not resolve damage timer"), F.Health(TEXT("Enemy")), Before);
	TestEqual(TEXT("Paused world does not debit cost before impact"), F.Energy(TEXT("Player2")), 100.f);
	TestTrue(TEXT("Paused active request remains bounded in executing state"), F.HasState(Request.ActionId, EHSRUltimateRequestState::Executing));
	F.World->GetWorldSettings()->SetPauserPlayerState(nullptr);
	F.Advance(1.7f);
	TestTrue(TEXT("Resume runs pending action and releases scheduler"), F.HasState(Request.ActionId, EHSRUltimateRequestState::Completed));
	TestTrue(TEXT("Resume reaches real damage once"), F.Health(TEXT("Enemy")) < Before);
	TestEqual(TEXT("Resume commits cost once"), F.Energy(TEXT("Player2")), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateCostTriggeredEnergyTest, "HSR.Battle.Ultimate.CostTriggeredEnergy.U15", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateCostTriggeredEnergyTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this)) return false;
	bool bRecoveredOnSpend = false;
	int32 CostCount = 0;
	auto* Source = F.ASC(TEXT("Player2"));
	const auto Handle = Source->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Data)
		{
			if (!FMath::IsNearlyEqual(Data.OldValue - Data.NewValue, 100.f)) return;
			++CostCount;
			if (bRecoveredOnSpend) return;
			bRecoveredOnSpend = true;
			auto Spec = Source->MakeOutgoingSpec(UHSRCombatEnergyEffect::StaticClass(), 1.f, Source->MakeEffectContext());
			if (!Spec.IsValid()) return;
			Spec.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(TEXT("Resource.EnergyDelta")), 10.f);
			Source->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		});
	const float Before = F.Health(TEXT("Enemy"));
	const auto Request = F.Command(TEXT("Player2"), TEXT("Enemy"));
	const auto Result = F.Coordinator->SubmitBattleCommand(Request);
	Source->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetEnergyAttribute()).Remove(Handle);
	TestTrue(TEXT("Cost-triggered legal Energy GE actually ran"), bRecoveredOnSpend);
	TestEqual(TEXT("Cost-triggered delegate does not repeat cost"), CostCount, 1);
	TestTrue(TEXT("Net resource delta cannot masquerade as failed cost"), Result.Succeeded());
	TestEqual(TEXT("Real cost100 plus explicit GE recovery10 yields10"), F.Energy(TEXT("Player2")), 10.f);
	TestTrue(TEXT("Successful cost still reaches damage effect"), F.Health(TEXT("Enemy")) < Before);
	F.SetEnergy(TEXT("Player2"), 100.f);
	bRecoveredOnSpend = false;
	const auto RefundHandle = Source->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Data)
		{
			if (bRecoveredOnSpend || !FMath::IsNearlyEqual(Data.OldValue - Data.NewValue, 100.f)) return;
			bRecoveredOnSpend = true;
			auto Spec = Source->MakeOutgoingSpec(UHSRCombatEnergyEffect::StaticClass(), 1.f, Source->MakeEffectContext());
			if (!Spec.IsValid()) return;
			Spec.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(TEXT("Resource.EnergyDelta")), 10.f);
			Source->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		});
	const auto Failed = F.Command(TEXT("Player2"), TEXT("Enemy"));
	const float HealthBeforeFailure = F.Health(TEXT("Enemy"));
	F.Coordinator->SetDamageTestInjectionForAction(Failed.ActionId, EHSRDamageTestInjection::ForcePostCostApplyFailure);
	F.Coordinator->SubmitBattleCommand(Failed);
	Source->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetEnergyAttribute()).Remove(RefundHandle);
	F.Coordinator->ClearDamageTestInjection();
	TestTrue(TEXT("Failed effect cancels even when cost triggers resource gain"), F.HasState(Failed.ActionId, EHSRUltimateRequestState::Cancelled));
	TestEqual(TEXT("Compensation refunds100 and preserves legitimate recovered10"), F.Energy(TEXT("Player2")), 110.f);
	TestEqual(TEXT("Compensated failure deals no damage"), F.Health(TEXT("Enemy")), HealthBeforeFailure);
	F.Coordinator->SubmitBattleCommand(Failed);
	TestEqual(TEXT("Failed replay cannot compensate twice"), F.Energy(TEXT("Player2")), 110.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateAtomicResetTest, "HSR.Battle.Ultimate.AtomicDelegateReset.U15_U17", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateAtomicResetTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this)) return false;
	F.Coordinator->EnableActionPresentation(F.World);
	const uint64 Generation = F.Coordinator->GetBattleGeneration();
	bool bRequestedReset = false;
	FHSRAbilityResolution AfterResetIntent;
	const auto C = F.Command(TEXT("Player3"), TEXT("Enemy"));
	auto* ObservedASC = F.ASC(TEXT("Enemy"));
	const auto Handle = ObservedASC->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetHealthAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Data)
		{
			if (bRequestedReset || Data.NewValue >= Data.OldValue) return;
			bRequestedReset = true;
			F.Coordinator->Reset();
			AfterResetIntent = F.Coordinator->SubmitBattleCommand(C);
		});
	const auto Normal = F.Command(TEXT("Player"), TEXT("Enemy"), F.Basic);
	const auto B = F.Command(TEXT("Player2"), TEXT("Enemy"));
	TestTrue(TEXT("Real ordinary damage scheduled before callback Reset"), Accepted(F.Coordinator->SubmitBattleCommand(Normal)));
	TestTrue(TEXT("One request queued before callback Reset"), Accepted(F.Coordinator->SubmitBattleCommand(B)));
	F.Advance(.65f);
	ObservedASC->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetHealthAttribute()).Remove(Handle);
	TestTrue(TEXT("Real GE Health delegate requested Reset synchronously"), bRequestedReset);
	TestEqual(TEXT("New Ultimate after deferred Reset intent rejects immediately"), AfterResetIntent.Status, EHSRAbilityResolutionStatus::Rejected);
	TestTrue(TEXT("Outer atomic dispatcher executes deferred reset at safe return"), F.Coordinator->GetBattleGeneration() != Generation);
	TestTrue(TEXT("Deferred reset clears all pending request records"), F.Coordinator->GetUltimateRequestHistory().IsEmpty());
	TestTrue(TEXT("Deferred reset clears runtime participant registry after callback returns"), F.Coordinator->GetParticipants().IsEmpty());
	F.Advance(3.f);
	TestTrue(TEXT("Old presentation completion cannot resurrect request history"), F.Coordinator->GetUltimateRequestHistory().IsEmpty());
	if (!F.Rebuild(*this)) return false;
	F.Coordinator->EnableActionPresentation(F.World);
	const auto Fresh = F.Command(TEXT("Player2"), TEXT("Enemy"));
	TestTrue(TEXT("New generation can accept fresh action after atomic Reset"), Accepted(F.Coordinator->SubmitBattleCommand(Fresh)));
	F.Advance(1.7f);
	TestTrue(TEXT("New generation has no inherited Busy lock"), F.HasState(Fresh.ActionId, EHSRUltimateRequestState::Completed));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateReentrantSettlementTest, "HSR.Battle.Ultimate.ReentrantSettlement.U07_U15_U18", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateReentrantSettlementTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this)) return false;
	F.Coordinator->EnableActionPresentation(F.World);
	bool bSubmittedFromDamage = false;
	FHSRAbilityResolution OrdinaryReentry, UltimateReentry;
	const auto C = F.Command(TEXT("Player3"), TEXT("Enemy"));
	const auto DamageHandle = F.ASC(TEXT("Enemy"))->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetHealthAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Data)
		{
			if (bSubmittedFromDamage || Data.NewValue >= Data.OldValue) return;
			bSubmittedFromDamage = true;
			OrdinaryReentry = F.Coordinator->RequestAction(F.Command(TEXT("Player"), TEXT("Enemy"), F.Basic));
			UltimateReentry = F.Coordinator->SubmitBattleCommand(C);
		});
	const auto Normal = F.Command(TEXT("Player"), TEXT("Enemy"), F.Basic);
	const auto B = F.Command(TEXT("Player2"), TEXT("Enemy"));
	TestTrue(TEXT("Normal begins before delegate reentry"), Accepted(F.Coordinator->SubmitBattleCommand(Normal)));
	TestTrue(TEXT("B queues before damage event"), Accepted(F.Coordinator->SubmitBattleCommand(B)));
	F.Advance(.65f);
	TestTrue(TEXT("Real Health GE notification invoked reentrant test"), bSubmittedFromDamage);
	TestEqual(TEXT("U18 normal reentry inside atomic damage settlement rejects"), OrdinaryReentry.Status, EHSRAbilityResolutionStatus::Rejected);
	TestTrue(TEXT("U15 ultimate intent accepted from atomic effect delegate"), Accepted(UltimateReentry));
	TestTrue(TEXT("U15 no nested ability dispatch inside damage delegate"), F.HasState(C.ActionId, EHSRUltimateRequestState::Accepted));
	F.Advance(5.f);
	F.ASC(TEXT("Enemy"))->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetHealthAttribute()).Remove(DamageHandle);
	TestTrue(TEXT("B completes before delegate-accepted C"), F.HasState(B.ActionId, EHSRUltimateRequestState::Completed) && F.HasState(C.ActionId, EHSRUltimateRequestState::Completed));
	if (F.Record(B.ActionId) && F.Record(C.ActionId))
		TestTrue(TEXT("Delegate reentry preserves deterministic FIFO"), F.Record(B.ActionId)->AcceptedSequence < F.Record(C.ActionId)->AcceptedSequence);
	TestEqual(TEXT("Exactly normal plus B plus C rules transactions; rejected input does not execute"), F.Coordinator->GetMaxCoreExecutionDepthForDevelopmentTest(), 1);
	TestEqual(TEXT("No duplicate ultimate debit from damage delegate"), F.Energy(TEXT("Player3")), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateMixedInputIdentityTest, "HSR.Battle.Ultimate.MixedInputAndIdentity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateMixedInputIdentityTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this)) return false;
	F.Coordinator->EnableActionPresentation(F.World);
	bool bDelegateInvoked = false;
	FHSRAbilityResolution Reentrant;
	const auto Handle = F.ASC(TEXT("Enemy"))->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetHealthAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& D)
		{
			if (bDelegateInvoked || D.NewValue >= D.OldValue) return;
			bDelegateInvoked = true;
			Reentrant = F.Coordinator->SubmitBattleCommand(F.Command(TEXT("Player"), TEXT("Enemy"), F.Basic));
		});
	TestTrue(TEXT("Synchronous public action runs with presentation configured"), F.Coordinator->RequestAction(F.Command(TEXT("Player"), TEXT("Enemy"), F.Basic)).Succeeded());
	F.ASC(TEXT("Enemy"))->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetHealthAttribute()).Remove(Handle);
	TestTrue(TEXT("Mixed-input test actually entered a real damage callback"), bDelegateInvoked);
	TestEqual(TEXT("Ordinary presented input rejects during synchronous public action"), Reentrant.Status, EHSRAbilityResolutionStatus::Rejected);
	TestFalse(TEXT("Rejected reentry did not start a presentation"), F.Coordinator->GetCommandViewState().bActionPlaying);
	const auto Request = F.Command(TEXT("Player2"), TEXT("Enemy"));
	TestTrue(TEXT("New ultimate accepts after synchronous action returned"), Accepted(F.Coordinator->SubmitBattleCommand(Request)));
	auto ChangedTarget = Request; ChangedTarget.TargetParticipantIds = {TEXT("Enemy2")};
	TestEqual(TEXT("Same pending ID with changed target rejects identity conflict"), F.Coordinator->SubmitBattleCommand(ChangedTarget).FailureReason, EHSRAbilityFailureReason::DuplicateAction);
	auto ChangedOwner = Request; ChangedOwner.ActorParticipantId = TEXT("Player3");
	TestEqual(TEXT("Same pending ID with changed owner rejects identity conflict"), F.Coordinator->SubmitBattleCommand(ChangedOwner).FailureReason, EHSRAbilityFailureReason::DuplicateAction);
	const float UnselectedEnemyHealth = F.Health(TEXT("Enemy2"));
	F.Advance(1.7f);
	TestTrue(TEXT("Original immutable request still completes"), F.HasState(Request.ActionId, EHSRUltimateRequestState::Completed));
	TestEqual(TEXT("Rejected conflicting replay cannot retarget original action"), F.Health(TEXT("Enemy2")), UnselectedEnemyHealth);
	TestEqual(TEXT("Completed same ID with changed target also rejects"), F.Coordinator->SubmitBattleCommand(ChangedTarget).FailureReason, EHSRAbilityFailureReason::DuplicateAction);
	TestEqual(TEXT("Conflicting owner was never charged"), F.Energy(TEXT("Player3")), 100.f);
	TestEqual(TEXT("Identity conflicts do not create additional records"), F.Coordinator->GetUltimateRequestHistory().Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateActorDestroyedSelectionTest, "HSR.Battle.Ultimate.ActorDestroyedSelection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateActorDestroyedSelectionTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this)) return false;
	const FName OldCurrent = F.Coordinator->GetTurnManager()->GetCurrentParticipantId();
	AActor* CurrentActor = F.Coordinator->FindParticipant(OldCurrent)->Actor.Get();
	if (!TestNotNull(TEXT("Current ordinary actor exists before destruction"), CurrentActor)) return false;
	TestTrue(TEXT("Destroy real current actor while selecting"), CurrentActor->Destroy());
	const FName NewCurrent = F.Coordinator->GetTurnManager()->GetCurrentParticipantId();
	const auto* Survivor = F.Coordinator->FindParticipant(NewCurrent);
	if (!TestTrue(TEXT("Actor destruction immediately advances selection to a living ally"), NewCurrent != OldCurrent && Survivor && Survivor->IsAlive() && Survivor->Team == EHSRBattleParticipantTeam::Player)) return false;
	TestTrue(TEXT("Surviving ally can issue a real ordinary action"), F.Coordinator->RequestAction(F.Command(NewCurrent, TEXT("Enemy"), F.Basic)).Succeeded());
	TestEqual(TEXT("One destroyed ally does not finish battle"), F.Coordinator->GetBattleResultBroadcastCountForDevelopmentTest(), 0);
	TArray<TWeakObjectPtr<AActor>> Remaining;
	for (const auto& P : F.Coordinator->GetParticipants())
		if (P.Team == EHSRBattleParticipantTeam::Player && P.Actor.IsValid()) Remaining.Add(P.Actor);
	for (const auto& Actor : Remaining) if (Actor.IsValid()) Actor->Destroy();
	TestEqual(TEXT("Destruction of remaining team produces terminal result"), F.Coordinator->GetCurrentState(), EHSRBattleCoordinatorState::Finished);
	TestEqual(TEXT("Team destruction produces one terminal result"), F.Coordinator->GetBattleResultBroadcastCountForDevelopmentTest(), 1);
	TestTrue(TEXT("Destruction leaves no accepted request residue"), F.Coordinator->GetUltimateRequestHistory().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateResourceViewEventsTest, "HSR.Battle.Ultimate.ResourceViewEvents", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateResourceViewEventsTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this)) return false;
	int32 Publications = 0;
	FHSRBattleCommandViewState Snapshot = F.Coordinator->GetCommandViewState();
	const auto Handle = F.Coordinator->OnCommandStateReady().AddLambda([&](const FHSRBattleCommandViewState& S) { ++Publications; Snapshot = S; });
	auto* Source = F.ASC(TEXT("Player2"));
	const auto ApplyAttribute = [&](const FGameplayAttribute& Attribute, float Value)
	{
		auto* Effect = NewObject<UGameplayEffect>(F.GI); Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		auto& Modifier = Effect->Modifiers.AddDefaulted_GetRef(); Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Override; Modifier.ModifierMagnitude = FScalableFloat(Value);
		FGameplayEffectSpec Spec(Effect, Source->MakeEffectContext(), 1.f);
		return Source->ApplyGameplayEffectSpecToSelf(Spec).WasSuccessfullyApplied();
	};
	const auto Available = [&]()
	{
		const auto* View = Snapshot.UltimateSkills.FindByPredicate([](const auto& S) { return S.ActorParticipantId == FName(TEXT("Player2")); });
		return View && View->bAvailable;
	};
	int32 Before = Publications;
	TestTrue(TEXT("Real Energy GE sets below-threshold state"), ApplyAttribute(UHSRCoreAttributeSet::GetEnergyAttribute(), 99.f));
	TestTrue(TEXT("Energy change publishes event-driven UI snapshot"), Publications > Before);
	TestFalse(TEXT("Published snapshot disables insufficient-energy ultimate"), Available());
	Before = Publications;
	TestTrue(TEXT("Real Energy GE refills threshold"), ApplyAttribute(UHSRCoreAttributeSet::GetEnergyAttribute(), 100.f));
	TestTrue(TEXT("Energy readiness publishes a fresh UI event"), Publications > Before);
	TestTrue(TEXT("Published snapshot enables ready ultimate"), Available());
	Before = Publications;
	TestTrue(TEXT("Real MaxEnergy GE applies and clamps current energy"), ApplyAttribute(UHSRCoreAttributeSet::GetMaxEnergyAttribute(), 80.f));
	TestTrue(TEXT("MaxEnergy change publishes authority snapshot"), Publications > Before);
	TestFalse(TEXT("Reduced MaxEnergy immediately disables ultimate"), Available());
	const auto* Member = Snapshot.Participants.FindByPredicate([](const auto& P) { return P.ParticipantId == FName(TEXT("Player2")); });
	TestTrue(TEXT("Snapshot reports clamped authoritative Energy80/Max80"), Member && Member->Energy == 80.f && Member->MaxEnergy == 80.f);
	ApplyAttribute(UHSRCoreAttributeSet::GetMaxEnergyAttribute(), 200.f);
	ApplyAttribute(UHSRCoreAttributeSet::GetEnergyAttribute(), 100.f);
	const FGameplayTag Block = FGameplayTag::RequestGameplayTag(TEXT("State.UltimateBlocked"));
	Before = Publications; Source->AddLooseGameplayTag(Block);
	TestTrue(TEXT("Blocked tag entry publishes authority snapshot"), Publications > Before);
	TestFalse(TEXT("Blocked tag immediately disables ready ultimate"), Available());
	Before = Publications; Source->RemoveLooseGameplayTag(Block);
	TestTrue(TEXT("Blocked tag removal publishes authority snapshot"), Publications > Before);
	TestTrue(TEXT("Unblocked resource-ready ultimate re-enables immediately"), Available());
	F.Coordinator->OnCommandStateReady().Remove(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRUltimateHeadlessRegressionTest, "HSR.Battle.Ultimate.HeadlessOrdinaryRegression.U18", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRUltimateHeadlessRegressionTest::RunTest(const FString&)
{
	using namespace HSR::UltimateTests;
	FFixture F; if (!F.Initialize(*this)) return false;
	const int32 EnemyBefore = F.Coordinator->GetEnemyTurnDispatchCountForDevelopmentTest();
	const float PlayerHealthBefore = F.TeamHealth();
	const auto Ultimate = F.Command(TEXT("Player2"), TEXT("Enemy"));
	TestTrue(TEXT("Headless ultimate executes through actual GAS"), F.Coordinator->SubmitBattleCommand(Ultimate).Succeeded());
	for (int32 Step = 0; Step < 18 && F.Coordinator->GetEnemyTurnDispatchCountForDevelopmentTest() < EnemyBefore + 2; ++Step)
	{
		const FName Current = F.Coordinator->GetTurnManager()->GetCurrentParticipantId();
		const auto* Actor = F.Coordinator->FindParticipant(Current);
		if (!TestTrue(TEXT("Headless pump returns to living player selection"), Actor && Actor->IsAlive() && Actor->Team == EHSRBattleParticipantTeam::Player)) return false;
		// Start with a real Basic to generate the point needed by the next Skill;
		// this fixture correctly begins with zero shared skill points.
		const UHSRSkillDefinition* Action = Step == 1 ? F.Skill : F.Basic;
		if (!TestTrue(TEXT("U18 ordinary basic/skill executes after ultimate"), F.Coordinator->RequestAction(F.Command(Current, TEXT("Enemy"), Action)).Succeeded())) return false;
	}
	TestTrue(TEXT("U18 at least two real enemy actions dispatched without recursion deadlock"), F.Coordinator->GetEnemyTurnDispatchCountForDevelopmentTest() >= EnemyBefore + 2);
	TestTrue(TEXT("U18 real automated enemy damage reached player"), F.TeamHealth() < PlayerHealthBefore);
	TestEqual(TEXT("U18 synchronous pump core depth remains one"), F.Coordinator->GetMaxCoreExecutionDepthForDevelopmentTest(), 1);
	TestEqual(TEXT("U18 no automated enemy action rejected"), F.Coordinator->GetEnemyTurnRejectedCountForDevelopmentTest(), 0);
	return true;
}

#endif
