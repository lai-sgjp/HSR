#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "../Battle/HSRBattleParticipant.h"
#include "../Battle/HSRTurnManager.h"
#include "../GAS/HSRAbilitySystemComponent.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"

/**
 * Turn-rotation behaviour when part of a team goes down.
 *
 * Defeat used to end the battle the moment the leader fell, so nothing ever exercised a battle
 * that continues past a casualty. These tests cover that gap directly: the rotation must skip a
 * downed member and keep handing turns to the survivors.
 *
 * Worth stating explicitly, because it is the subtle part: UHSRTurnManager copies the participant
 * array at Initialize and never resyncs it, while the Coordinator sets bDefeated on its own copy.
 * Eligibility still tracks reality because FHSRBattleParticipant::IsAlive reads Health off the
 * AbilitySystemComponent, which is a weak pointer shared by both copies. These tests therefore
 * down a member by zeroing Health rather than by setting bDefeated -- that is the path production
 * damage takes, and the only one the two copies agree on.
 */
namespace HSRTeamWipeAutomation
{
	static FHSRBattleParticipant MakeParticipant(UWorld* World, FName Id, float Speed, EHSRBattleParticipantTeam Team)
	{
		AActor* Actor = World ? World->SpawnActor<AActor>() : nullptr;
		UAbilitySystemComponent* ASC = Actor
			? Cast<UAbilitySystemComponent>(Actor->AddComponentByClass(UHSRAbilitySystemComponent::StaticClass(), false, FTransform::Identity, false))
			: nullptr;
		if (ASC)
		{
			ASC->InitStats(UHSRCoreAttributeSet::StaticClass(), nullptr);
			ASC->InitAbilityActorInfo(Actor, Actor);
			ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 1000.0f);
			ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 1000.0f);
			ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), Speed);
		}
		FHSRBattleParticipant Participant;
		Participant.ParticipantId = Id;
		Participant.DefinitionId = FName(*FString::Printf(TEXT("Definition.%s"), *Id.ToString()));
		Participant.Team = Team;
		Participant.Actor = Actor;
		Participant.AbilitySystemComponent = ASC;
		return Participant;
	}

	static void Down(const FHSRBattleParticipant& Participant)
	{
		if (Participant.AbilitySystemComponent.IsValid())
		{
			Participant.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 0.0f);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRTeamWipeRotationTest, "HSR.Battle.TeamWipeRotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRTeamWipeRotationTest::RunTest(const FString&)
{
	using namespace HSRTeamWipeAutomation;
	UWorld* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
	ON_SCOPE_EXIT { if (World) World->DestroyWorld(false); };
	if (!TestNotNull(TEXT("team wipe world"), World))
	{
		return false;
	}

	// Two player members plus one enemy. Distinct speeds keep the initial order deterministic.
	TArray<FHSRBattleParticipant> Participants;
	Participants.Add(MakeParticipant(World, TEXT("WipeP1"), 120.0f, EHSRBattleParticipantTeam::Player));
	Participants.Add(MakeParticipant(World, TEXT("WipeP2"), 100.0f, EHSRBattleParticipantTeam::Player));
	Participants.Add(MakeParticipant(World, TEXT("WipeE1"), 80.0f, EHSRBattleParticipantTeam::Enemy));

	UHSRTurnManager* Manager = NewObject<UHSRTurnManager>();
	if (!TestTrue(TEXT("team wipe rotation initializes"), Manager->Initialize(Participants)))
	{
		return false;
	}

	TestTrue(TEXT("all three start eligible"),
		UHSRTurnManager::IsParticipantTurnEligible(Participants[0])
		&& UHSRTurnManager::IsParticipantTurnEligible(Participants[1])
		&& UHSRTurnManager::IsParticipantTurnEligible(Participants[2]));

	// Down a member that is NOT the current actor, then let the current actor resolve normally.
	//
	// The distinction matters and is the whole reason this test exists. UHSRTurnManager only
	// re-picks an actor inside AdvanceToNextValidTurn, which runs when the current actor resolves.
	// It does not watch Health and will not vacate a turn it has already handed out, so downing
	// the current actor deadlocks the rotation by design: ResolveAction refuses a caller that is
	// no longer valid. Production never hits that because damage is resolved by the Coordinator
	// during someone else's action, which then drives the advance. Downing a non-current member
	// reproduces the production shape; downing the current one would only assert a contract the
	// manager does not hold.
	const FName FirstActor = Manager->GetCurrentParticipantId();
	TestEqual(TEXT("fastest member acts first"), FirstActor, FName(TEXT("WipeP1")));
	const int32 DownedIndex = 1;
	Down(Participants[DownedIndex]);
	TestFalse(TEXT("downed member is no longer turn eligible"), UHSRTurnManager::IsParticipantTurnEligible(Participants[DownedIndex]));
	TestTrue(TEXT("current actor stays turn eligible"), UHSRTurnManager::IsParticipantTurnEligible(Participants[0]));

	// Rotate several times and confirm the downed member never gets another turn while the
	// survivor and the enemy both keep acting.
	int32 DownedTurns = 0;
	int32 SurvivorTurns = 0;
	int32 EnemyTurns = 0;
	for (int32 Step = 0; Step < 12; ++Step)
	{
		const FName Current = Manager->GetCurrentParticipantId();
		if (Current == TEXT("WipeP2"))
		{
			++DownedTurns;
		}
		else if (Current == TEXT("WipeP1"))
		{
			++SurvivorTurns;
		}
		else if (Current == TEXT("WipeE1"))
		{
			++EnemyTurns;
		}
		if (Current.IsNone())
		{
			break;
		}
		Manager->ResolveAction(Current);
	}

	TestEqual(TEXT("downed member never acts again"), DownedTurns, 0);
	TestTrue(TEXT("survivor keeps acting"), SurvivorTurns > 0);
	TestTrue(TEXT("enemy keeps acting"), EnemyTurns > 0);
	TestNotEqual(TEXT("battle has not stalled on an empty actor"), Manager->GetCurrentParticipantId(), FName(NAME_None));
	TestNotEqual(TEXT("rotation is not finished while survivors remain"), Manager->GetState(), EHSRTurnManagerState::Finished);

	// The forecast drives the turn-order bar, so a downed member must not appear there either.
	const TArray<FHSRTurnForecastEntry> Forecast = Manager->BuildTurnForecast(6);
	bool bForecastContainsDowned = false;
	for (const FHSRTurnForecastEntry& Entry : Forecast)
	{
		if (Entry.ParticipantId == TEXT("WipeP2"))
		{
			bForecastContainsDowned = true;
		}
	}
	TestFalse(TEXT("forecast omits the downed member"), bForecastContainsDowned);
	TestTrue(TEXT("forecast still has upcoming slots"), Forecast.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRTeamWipeExhaustionTest, "HSR.Battle.TeamWipeExhaustion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRTeamWipeExhaustionTest::RunTest(const FString&)
{
	using namespace HSRTeamWipeAutomation;
	UWorld* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
	ON_SCOPE_EXIT { if (World) World->DestroyWorld(false); };
	if (!TestNotNull(TEXT("exhaustion world"), World))
	{
		return false;
	}

	TArray<FHSRBattleParticipant> Participants;
	Participants.Add(MakeParticipant(World, TEXT("ExhaustP1"), 120.0f, EHSRBattleParticipantTeam::Player));
	Participants.Add(MakeParticipant(World, TEXT("ExhaustP2"), 100.0f, EHSRBattleParticipantTeam::Player));
	Participants.Add(MakeParticipant(World, TEXT("ExhaustE1"), 80.0f, EHSRBattleParticipantTeam::Enemy));

	UHSRTurnManager* Manager = NewObject<UHSRTurnManager>();
	if (!TestTrue(TEXT("exhaustion initializes"), Manager->Initialize(Participants)))
	{
		return false;
	}

	// Down the whole player team. Eligibility must collapse to the enemy alone; the manager owns
	// no outcome decision, so the assertion is about rotation, not about a battle result.
	//
	// Order matters for the same reason as the rotation test: the current actor cannot be downed
	// in place without deadlocking ResolveAction. Down the non-current member first, let the
	// current one resolve to hand the turn on, then down it too.
	Down(Participants[1]);
	const FName Opener = Manager->GetCurrentParticipantId();
	TestEqual(TEXT("fastest member opens"), Opener, FName(TEXT("ExhaustP1")));
	TestTrue(TEXT("opener resolves before being downed"), Manager->ResolveAction(Opener));
	Down(Participants[0]);
	TestFalse(TEXT("first downed member ineligible"), UHSRTurnManager::IsParticipantTurnEligible(Participants[0]));
	TestFalse(TEXT("second downed member ineligible"), UHSRTurnManager::IsParticipantTurnEligible(Participants[1]));
	TestTrue(TEXT("enemy remains eligible"), UHSRTurnManager::IsParticipantTurnEligible(Participants[2]));

	for (int32 Step = 0; Step < 6; ++Step)
	{
		const FName Current = Manager->GetCurrentParticipantId();
		if (Current.IsNone())
		{
			break;
		}
		TestEqual(TEXT("only the enemy acts once the player team is down"), Current, FName(TEXT("ExhaustE1")));
		Manager->ResolveAction(Current);
	}

	const TArray<FHSRTurnForecastEntry> Forecast = Manager->BuildTurnForecast(4);
	for (const FHSRTurnForecastEntry& Entry : Forecast)
	{
		TestEqual(TEXT("forecast holds only the enemy"), Entry.ParticipantId, FName(TEXT("ExhaustE1")));
	}
	return true;
}

#endif
