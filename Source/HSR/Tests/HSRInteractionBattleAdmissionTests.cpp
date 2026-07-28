#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../Character/HSRExplorationCharacter.h"
#include "../Data/Definitions/HSRCharacterCatalog.h"
#include "../Data/Definitions/HSREncounterDefinition.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"

namespace
{
	struct FHSRAdmissionFixture
	{
		UGameInstance* GameInstance = nullptr;
		UWorld* World = nullptr;
		APlayerController* Controller = nullptr;
		AHSRExplorationCharacter* Pawn = nullptr;
		UHSREncounterDefinition* Encounter = nullptr;
		UHSRBattleTransitionSubsystem* Transition = nullptr;

		bool Initialize(bool bSeedParty)
		{
			if (!GEngine)
			{
				return false;
			}

			GameInstance = NewObject<UGameInstance>(GEngine);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone(FName(*FString::Printf(TEXT("HSRAdmission_%s"),
				*FGuid::NewGuid().ToString(EGuidFormats::Digits))));
			World = GameInstance->GetWorld();
			if (!World)
			{
				return false;
			}

			Controller = World->SpawnActor<APlayerController>();
			Pawn = World->SpawnActor<AHSRExplorationCharacter>();
			if (!Controller || !Pawn)
			{
				return false;
			}
			Controller->Possess(Pawn);

			UHSRCharacterCatalog* Catalog = LoadObject<UHSRCharacterCatalog>(nullptr,
				TEXT("/Game/Data/Progression/DA_CharacterCatalog_P11.DA_CharacterCatalog_P11"));
			UHSRCharacterProfileSubsystem* Profiles = GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>();
			UHSRPartySubsystem* Party = GameInstance->GetSubsystem<UHSRPartySubsystem>();
			if (!Catalog || !Profiles || !Party)
			{
				return false;
			}
			if (Profiles->RegisterLoadedCatalog(Catalog) != EHSRCharacterProfileResult::Success)
			{
				return false;
			}
			if (bSeedParty && Party->AddCharacter(TEXT("Character.A"), 0) != EHSRPartyResult::Success)
			{
				return false;
			}

			Encounter = NewObject<UHSREncounterDefinition>(GameInstance);
			Encounter->EncounterId = TEXT("Encounter.Admission.Automation");
			Encounter->EnemyDefinitionId = TEXT("Enemy.Phase5Test");
			Encounter->BattleMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Map_Battle.Map_Battle")));
			Transition = GameInstance->GetSubsystem<UHSRBattleTransitionSubsystem>();
			if (!Transition)
			{
				return false;
			}
			Transition->SetTravelSuppressedForAutomation(true);
			return true;
		}

		void Shutdown() const
		{
			if (GameInstance)
			{
				GameInstance->Shutdown();
				if (World)
				{
					World->DestroyWorld(false);
					GEngine->DestroyWorldContext(World);
				}
				GameInstance->RemoveFromRoot();
			}
		}
	};

	bool SameRequest(const FHSREncounterRequest& Left, const FHSREncounterRequest& Right)
	{
		return Left.RequestId == Right.RequestId
			&& Left.PlayerCharacterId == Right.PlayerCharacterId
			&& Left.EncounterId == Right.EncounterId
			&& Left.EnemyDefinitionId == Right.EnemyDefinitionId
			&& Left.Initiative == Right.Initiative
			&& Left.BattleMapPath == Right.BattleMapPath
			&& Left.ReturnTransform.Equals(Right.ReturnTransform)
			&& Left.ExplorationMapPath == Right.ExplorationMapPath
			&& Left.RewardDefinitionId == Right.RewardDefinitionId
			&& Left.RewardSeed == Right.RewardSeed;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRInteractionBattleAdmissionTest,
	"HSR.InteractionBattle.Admission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRInteractionBattleAdmissionTest::RunTest(const FString& Parameters)
{
	FHSRAdmissionFixture Fixture;
	if (!TestTrue(TEXT("Admission fixture initializes"), Fixture.Initialize(true)))
	{
		Fixture.Shutdown();
		return false;
	}

	const FHSREncounterResult First = Fixture.Transition->RequestEncounter(
		Fixture.Encounter, EHSREncounterInitiative::Player);
	TestEqual(TEXT("Valid encounter admission succeeds"), First.ResultType, EHSREncounterResultType::Success);
	TestTrue(TEXT("Admission creates a request id"), First.RequestId.IsValid());
	const FHSRTransitionAutomationSnapshot FirstSnapshot = Fixture.Transition->GetAutomationSnapshot(
		Fixture.Encounter->EncounterId);
	TestEqual(TEXT("Admission captures Party slot 0 identity"), FirstSnapshot.PendingRequest.PlayerCharacterId,
		FName(TEXT("Character.A")));
	TestEqual(TEXT("Admission publishes one mutation"), FirstSnapshot.AdmissionMutationCount, 1);
	TestEqual(TEXT("Admission issues one travel"), FirstSnapshot.TravelInitiationCount, 1);

	const FHSREncounterResult Duplicate = Fixture.Transition->RequestEncounter(
		Fixture.Encounter, EHSREncounterInitiative::Player);
	const FHSRTransitionAutomationSnapshot DuplicateSnapshot = Fixture.Transition->GetAutomationSnapshot(
		Fixture.Encounter->EncounterId);
	TestEqual(TEXT("Duplicate admission is rejected"), Duplicate.ResultType, EHSREncounterResultType::AlreadyPending);
	TestTrue(TEXT("Duplicate preserves the first request"), SameRequest(
		DuplicateSnapshot.PendingRequest, FirstSnapshot.PendingRequest));
	TestEqual(TEXT("Duplicate creates no mutation"), DuplicateSnapshot.AdmissionMutationCount,
		FirstSnapshot.AdmissionMutationCount);
	TestEqual(TEXT("Duplicate issues no travel"), DuplicateSnapshot.TravelInitiationCount,
		FirstSnapshot.TravelInitiationCount);

	Fixture.Transition->HandleTravelFailure(nullptr, ETravelFailure::LoadMapFailure, TEXT("uncorrelatable"));
	const FHSRTransitionAutomationSnapshot AfterNullFailure = Fixture.Transition->GetAutomationSnapshot(
		Fixture.Encounter->EncounterId);
	TestTrue(TEXT("Null-World failure preserves the active request"), SameRequest(
		AfterNullFailure.PendingRequest, FirstSnapshot.PendingRequest));

	const FHSREncounterResult Consumed = Fixture.Transition->ConsumePendingEncounter();
	TestEqual(TEXT("Battle consumes the request once"), Consumed.ResultType, EHSREncounterResultType::Success);
	TestTrue(TEXT("Consumed request is identical"), SameRequest(Consumed.ConsumedRequest, FirstSnapshot.PendingRequest));
	TestEqual(TEXT("Second consume is rejected"), Fixture.Transition->ConsumePendingEncounter().ResultType,
		EHSREncounterResultType::AlreadyConsumed);
	Fixture.Shutdown();

	FHSRAdmissionFixture EmptyPartyFixture;
	if (!TestTrue(TEXT("Empty Party fixture initializes"), EmptyPartyFixture.Initialize(false)))
	{
		EmptyPartyFixture.Shutdown();
		return false;
	}
	const FHSREncounterResult EmptyPartyResult = EmptyPartyFixture.Transition->RequestEncounter(
		EmptyPartyFixture.Encounter, EHSREncounterInitiative::Player);
	TestEqual(TEXT("Empty Party rejects admission"), EmptyPartyResult.ResultType,
		EHSREncounterResultType::NoPlayerSelection);
	const FHSRTransitionAutomationSnapshot EmptySnapshot = EmptyPartyFixture.Transition->GetAutomationSnapshot(
		EmptyPartyFixture.Encounter->EncounterId);
	TestEqual(TEXT("Empty Party creates no admission mutation"), EmptySnapshot.AdmissionMutationCount, 0);
	TestEqual(TEXT("Empty Party issues no travel"), EmptySnapshot.TravelInitiationCount, 0);
	TestTrue(TEXT("Empty Party leaves request empty"), !EmptySnapshot.PendingRequest.RequestId.IsValid());
	EmptyPartyFixture.Shutdown();

	return true;
}

#endif
