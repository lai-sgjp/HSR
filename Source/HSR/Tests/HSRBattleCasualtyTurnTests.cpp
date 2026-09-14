#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemComponent.h"
#include "../Battle/HSRBattleCoordinator.h"
#include "../Battle/HSRBattleGameMode.h"
#include "../Battle/HSRTurnManager.h"
#include "../Data/HSRSkillDefinition.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"

namespace HSR::CasualtyTurnTests
{
	struct FFixture
	{
		UGameInstance* GI = nullptr;
		UWorld* World = nullptr;
		UHSRBattleCoordinator* Coordinator = nullptr;
		~FFixture()
		{
			if (Coordinator) Coordinator->Reset();
			if (!GI) return;
			GI->Shutdown();
			if (World) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
			GI->RemoveFromRoot();
		}
		bool Initialize(FAutomationTestBase& Test, int32 PlayerCount, int32 EnemyCount)
		{
			if (!GEngine) return false;
			GI = NewObject<UGameInstance>(GEngine);
			GI->AddToRoot();
			GI->InitializeStandalone(FName(*FString::Printf(TEXT("HSRCasualty_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))));
			World = GI->GetWorld();
			auto GameMode = LoadClass<AHSRBattleGameMode>(nullptr, TEXT("/Game/Blueprints/Framework/BP_HSRBattleGameMode.BP_HSRBattleGameMode_C"));
			FText Failure;
			Coordinator = AHSRBattleGameMode::CreateRepeatableBreakAutomationFixture(GI, World, GameMode, Failure);
			if (!Test.TestNotNull(*FString::Printf(TEXT("Production damage fixture: %s"), *Failure.ToString()), Coordinator)) return false;
			TArray<FHSRBattleRosterEntry> Roster = Coordinator->GetPlayerRoster();
			auto* Profiles = GI->GetSubsystem<UHSRCharacterProfileSubsystem>();
			const UHSRCharacterDefinition* Original = nullptr;
			if (Roster.IsEmpty() || !Profiles->GetDefinition(Roster[0].CharacterId, Original)) return false;
			for (int32 Index = 1; Index < PlayerCount; ++Index)
			{
				auto* Definition = DuplicateObject<UHSRCharacterDefinition>(Original, GI);
				Definition->CharacterId = FName(*FString::Printf(TEXT("Automation.Casualty.Character%d"), Index));
				if (!Test.TestEqual(TEXT("Ally profile registered"), Profiles->RegisterDefinition(Definition), EHSRCharacterProfileResult::Success)) return false;
				FHSRCharacterProgressionContext Context;
				if (!Profiles->GetProgressionContext(Definition->CharacterId, Context)) return false;
				Coordinator->SetCharacterProgressionContext(FName(*FString::Printf(TEXT("Player%d"), Index + 1)), Context);
				Roster.Add({Definition->CharacterId, Roster[0].PawnClass});
			}
			const FName EnemyDefinitionId = Coordinator->FindFirstOfTeam(EHSRBattleParticipantTeam::Enemy)->DefinitionId;
			TArray<FHSRBattleRosterEntry> Enemies;
			for (int32 Index = 0; Index < EnemyCount; ++Index) Enemies.Add({EnemyDefinitionId, APawn::StaticClass()});
			Coordinator->SetPlayerRoster(Roster);
			Coordinator->SetEnemyRoster(Enemies);
			if (!Test.TestTrue(TEXT("Multiple participant production fixture builds"), Coordinator->ResetAndRebuildForDevelopmentTest(World).IsSuccess())) return false;
			for (const auto& Participant : Coordinator->GetParticipants())
			{
				// Isolate casualty/retargeting from the fixture's repeatable Break status.
				// Four fast allies would otherwise repeatedly delay the boss beyond this
				// test's bounded command loop before it can take its next legal turn.
				Coordinator->FindParticipant(Participant.ParticipantId)->WeaknessTags.Reset();
				auto* ASC = Participant.AbilitySystemComponent.Get();
				ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 10000.f);
				ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 10000.f);
				ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetAttackAttribute(), 100.f);
				ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetDefenseAttribute(), 0.f);
				ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), Participant.Team == EHSRBattleParticipantTeam::Player ? 200.f : 100.f);
			}
			return Test.TestTrue(TEXT("Deterministic action order initialized"), Coordinator->GetTurnManager()->Initialize(Coordinator->GetParticipants()));
		}
		void SetHealth(FName Id, float Health)
		{
			Coordinator->FindParticipant(Id)->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), Health);
		}
		float Health(FName Id) const
		{
			return Coordinator->FindParticipant(Id)->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
		}
		FHSRBattleActionCommand Command(FName Target) const
		{
			FHSRBattleActionCommand Result;
			Result.ActionId = FGuid::NewGuid(); Result.BattleId = Coordinator->GetCurrentRequestId();
			Result.ActorParticipantId = Coordinator->GetTurnManager()->GetCurrentParticipantId();
			Result.SkillId = Coordinator->GetBasicAttackDefinition()->SkillId;
			Result.TargetParticipantIds.Add(Target);
			return Result;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRBossPartialCasualtyTurnTest,
	"HSR.Battle.CasualtyTurn.EnemyKillAdvancesAndRetargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRBossPartialCasualtyTurnTest::RunTest(const FString&)
{
	HSR::CasualtyTurnTests::FFixture F;
	if (!F.Initialize(*this, 4, 1)) return false;
	F.SetHealth(TEXT("Player"), 1.f);
	const int32 RejectedBefore = F.Coordinator->GetEnemyTurnRejectedCountForDevelopmentTest();
	for (int32 Step = 0; Step < 8 && F.Coordinator->FindParticipant(TEXT("Player"))->IsAlive(); ++Step)
	{
		if (!TestTrue(TEXT("Player command and automated response complete"), F.Coordinator->RequestAction(F.Command(TEXT("Enemy"))).Succeeded())) return false;
	}
	TestFalse(TEXT("Boss killed the leader through GAS"), F.Coordinator->FindParticipant(TEXT("Player"))->IsAlive());
	TestEqual(TEXT("Surviving allies keep battle active"), F.Coordinator->GetCurrentState(), EHSRBattleCoordinatorState::Spawned);
	const auto* Current = F.Coordinator->FindParticipant(F.Coordinator->GetTurnManager()->GetCurrentParticipantId());
	if (!TestTrue(TEXT("Lethal enemy action hands control to living ally"), Current && Current->Team == EHSRBattleParticipantTeam::Player && Current->IsAlive())) return false;
	TestEqual(TEXT("Partial casualty produces no battle result"), F.Coordinator->GetBattleResultBroadcastCountForDevelopmentTest(), 0);
	const float Before = F.Health(TEXT("Player2"));
	for (int32 Step = 0; Step < 8 && F.Health(TEXT("Player2")) == Before; ++Step)
	{
		if (!TestTrue(TEXT("Surviving allies continue issuing commands"), F.Coordinator->RequestAction(F.Command(TEXT("Enemy"))).Succeeded())) return false;
	}
	TestTrue(TEXT("Next enemy turn chooses a living teammate"), F.Health(TEXT("Player2")) < Before);
	TestEqual(TEXT("Automated enemy commands were not rejected"), F.Coordinator->GetEnemyTurnRejectedCountForDevelopmentTest(), RejectedBefore);
	TestEqual(TEXT("Action execution remains non-recursive"), F.Coordinator->GetMaxCoreExecutionDepthForDevelopmentTest(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRMultiEnemyCasualtyTurnTest,
	"HSR.Battle.CasualtyTurn.PlayerKillReplayAndTerminalWipe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRMultiEnemyCasualtyTurnTest::RunTest(const FString&)
{
	HSR::CasualtyTurnTests::FFixture F;
	if (!F.Initialize(*this, 1, 2)) return false;
	F.SetHealth(TEXT("Enemy"), 1.f);
	const auto Command = F.Command(TEXT("Enemy"));
	const int32 DispatchesBefore = F.Coordinator->GetEnemyTurnDispatchCountForDevelopmentTest();
	const uint64 BeforeSequence = F.Coordinator->GetTurnManager()->GetTurnSequence();
	if (!TestTrue(TEXT("Lethal player action commits"), F.Coordinator->RequestAction(Command).Succeeded())) return false;
	TestFalse(TEXT("First enemy is dead"), F.Coordinator->FindParticipant(TEXT("Enemy"))->IsAlive());
	TestTrue(TEXT("Partial enemy defeat advances turns"), F.Coordinator->GetTurnManager()->GetTurnSequence() > BeforeSequence);
	TestEqual(TEXT("Surviving enemy gets an automated action"), F.Coordinator->GetEnemyTurnDispatchCountForDevelopmentTest(), DispatchesBefore + 1);
	const uint64 AfterSequence = F.Coordinator->GetTurnManager()->GetTurnSequence();
	const float AfterHealth = F.Health(TEXT("Player"));
	TestTrue(TEXT("Same action ID replays cached success"), F.Coordinator->RequestAction(Command).Succeeded());
	TestEqual(TEXT("Replay does not advance turn again"), F.Coordinator->GetTurnManager()->GetTurnSequence(), AfterSequence);
	TestEqual(TEXT("Replay does not dispatch another enemy action"), F.Health(TEXT("Player")), AfterHealth);
	F.SetHealth(TEXT("Enemy2"), 1.f);
	const uint64 BeforeTerminal = F.Coordinator->GetTurnManager()->GetTurnSequence();
	TestTrue(TEXT("Last enemy kill succeeds"), F.Coordinator->RequestAction(F.Command(TEXT("Enemy2"))).Succeeded());
	TestEqual(TEXT("Team wipe finishes battle"), F.Coordinator->GetCurrentState(), EHSRBattleCoordinatorState::Finished);
	TestEqual(TEXT("Terminal kill does not start an extra turn"), F.Coordinator->GetTurnManager()->GetTurnSequence(), BeforeTerminal);
	TestEqual(TEXT("Terminal result emitted once"), F.Coordinator->GetBattleResultBroadcastCountForDevelopmentTest(), 1);
	return true;
}

#endif
