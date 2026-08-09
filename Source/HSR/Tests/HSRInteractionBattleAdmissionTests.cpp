#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../Character/HSRExplorationCharacter.h"
#include "../Data/Definitions/HSRCharacterCatalog.h"
#include "../Data/Definitions/HSRDropTableDefinition.h"
#include "../Data/Definitions/HSREncounterDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Exploration/HSRGrayboxInteractable.h"
#include "../Interaction/HSRInteractionComponent.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"

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
	FHSRAdmissionFixture InteractionFixture;
	if (!TestTrue(TEXT("Interaction fixture initializes"), InteractionFixture.Initialize(true)))
	{
		InteractionFixture.Shutdown();
		return false;
	}
	UHSRInteractionComponent* Interaction = InteractionFixture.Pawn->FindComponentByClass<UHSRInteractionComponent>();
	if (!TestNotNull(TEXT("Exploration Pawn owns InteractionComponent"), Interaction))
	{
		InteractionFixture.Shutdown();
		return false;
	}
	TestEqual(TEXT("No candidate remains distinct"), Interaction->TryInteract().FailureReason,
		EHSRInteractionFailureReason::NoCandidate);
	AHSRGrayboxInteractable* OutOfRange = InteractionFixture.World->SpawnActor<AHSRGrayboxInteractable>(
		InteractionFixture.Pawn->GetActorLocation() + FVector(5000.0, 0.0, 0.0), FRotator::ZeroRotator);
	const FHSRInteractionContext OutOfRangeContext(InteractionFixture.Pawn,
		InteractionFixture.Pawn->GetActorLocation());
	TestEqual(TEXT("Non-overlapping interactor is out of range"),
		OutOfRange->ExecuteInteraction_Implementation(OutOfRangeContext).FailureReason,
		EHSRInteractionFailureReason::OutOfRange);
	Interaction->RegisterCandidate(OutOfRange);
	TestEqual(TEXT("Registered candidate is observable"), Interaction->GetCurrentCandidate(),
		static_cast<AActor*>(OutOfRange));
	Interaction->UnregisterCandidate(OutOfRange);
	TestEqual(TEXT("Proper unregister returns to NoCandidate"), Interaction->TryInteract().FailureReason,
		EHSRInteractionFailureReason::NoCandidate);
	AHSRGrayboxInteractable* Unavailable = InteractionFixture.World->SpawnActor<AHSRGrayboxInteractable>();
	Unavailable->SetAvailable(false);
	Interaction->RegisterCandidate(Unavailable);
	TestEqual(TEXT("Unavailable candidate is rejected before execution"), Interaction->TryInteract().FailureReason,
		EHSRInteractionFailureReason::Unavailable);
	Interaction->UnregisterCandidate(Unavailable);
	AHSRGrayboxInteractable* Destroyed = InteractionFixture.World->SpawnActor<AHSRGrayboxInteractable>();
	Interaction->RegisterCandidate(Destroyed);
	Destroyed->Destroy();
	TestEqual(TEXT("Destroyed registered candidate is TargetInvalid"), Interaction->TryInteract().FailureReason,
		EHSRInteractionFailureReason::TargetInvalid);
	TestEqual(TEXT("Interaction failures create no admission mutation"),
		InteractionFixture.Transition->GetAutomationSnapshot(InteractionFixture.Encounter->EncounterId).AdmissionMutationCount, 0);
	InteractionFixture.Shutdown();

	FHSRAdmissionFixture RewardFixture;
	if (!TestTrue(TEXT("Reward fixture initializes"), RewardFixture.Initialize(true)))
	{
		RewardFixture.Shutdown();
		return false;
	}
	UHSRRewardSubsystem* Reward = RewardFixture.GameInstance->GetSubsystem<UHSRRewardSubsystem>();
	UHSRInventorySubsystem* Inventory = RewardFixture.GameInstance->GetSubsystem<UHSRInventorySubsystem>();
	UHSRItemDefinition* Item = NewObject<UHSRItemDefinition>(RewardFixture.GameInstance);
	Item->ItemId = TEXT("Item.Admission.Automation");
	Item->MaxStack = 10;
	UHSRDropTableDefinition* Drop = NewObject<UHSRDropTableDefinition>(RewardFixture.GameInstance);
	Drop->DropTableId = TEXT("Drop.Admission.Automation");
	FHSRDropTableEntry& DropEntry = Drop->Entries.AddDefaulted_GetRef();
	DropEntry.ItemId = Item->ItemId;
	UHSRRewardDefinition* RewardDefinition = NewObject<UHSRRewardDefinition>(RewardFixture.GameInstance);
	RewardDefinition->RewardDefinitionId = TEXT("Reward.Admission.Automation");
	RewardDefinition->DropTableId = Drop->DropTableId;
	RewardDefinition->DropRolls = 1;
	TArray<TObjectPtr<UHSRItemDefinition>> Items = { Item };
	TestEqual(TEXT("Valid reward bundle const preflight succeeds"),
		Reward->CanRegisterBundle(Items, *Drop, *RewardDefinition), EHSRRewardOperationResult::Success);
	TestFalse(TEXT("Const preflight does not register inventory metadata"), Inventory->HasDefinition(Item->ItemId));
	TestFalse(TEXT("Const preflight does not register reward metadata"), Reward->HasDefinition(RewardDefinition->RewardDefinitionId));
	RewardFixture.Encounter->RewardItemDefinitions = Items;
	RewardFixture.Encounter->VictoryRewardDefinition = RewardDefinition;
	TestEqual(TEXT("Incomplete reward bundle rejects admission"),
		RewardFixture.Transition->RequestEncounterForInteractor(RewardFixture.Encounter,
			EHSREncounterInitiative::Player, RewardFixture.Pawn).ResultType,
		EHSREncounterResultType::InvalidDefinition);
	TestEqual(TEXT("Invalid reward bundle creates no admission mutation"),
		RewardFixture.Transition->GetAutomationSnapshot(RewardFixture.Encounter->EncounterId).AdmissionMutationCount, 0);
	TestFalse(TEXT("Invalid reward bundle does not register inventory metadata"), Inventory->HasDefinition(Item->ItemId));
	TestFalse(TEXT("Invalid reward bundle does not register reward metadata"), Reward->HasDefinition(RewardDefinition->RewardDefinitionId));
	RewardFixture.Encounter->RewardDropTable = Drop;
	TestEqual(TEXT("Successful admission registers valid metadata"),
		RewardFixture.Transition->RequestEncounterForInteractor(RewardFixture.Encounter,
			EHSREncounterInitiative::Player, RewardFixture.Pawn).ResultType, EHSREncounterResultType::Success);
	TestTrue(TEXT("Successful admission registers inventory metadata"), Inventory->HasDefinition(Item->ItemId));
	TestTrue(TEXT("Successful admission registers reward metadata"), Reward->HasDefinition(RewardDefinition->RewardDefinitionId));
	TArray<FHSRRewardReceipt> Receipts;
	Reward->GetReceipts(Receipts);
	TestTrue(TEXT("Admission does not create a reward receipt"), Receipts.IsEmpty());
	RewardFixture.Shutdown();

	FHSRAdmissionFixture Fixture;
	if (!TestTrue(TEXT("Admission fixture initializes"), Fixture.Initialize(true)))
	{
		Fixture.Shutdown();
		return false;
	}
	TestEqual(TEXT("Null Definition is rejected"),
		Fixture.Transition->RequestEncounterForInteractor(nullptr,
			EHSREncounterInitiative::Player, Fixture.Pawn).ResultType,
		EHSREncounterResultType::InvalidDefinition);
	const FName ValidEncounterId = Fixture.Encounter->EncounterId;
	Fixture.Encounter->EncounterId = NAME_None;
	TestEqual(TEXT("Missing encounter id is rejected"),
		Fixture.Transition->RequestEncounterForInteractor(Fixture.Encounter,
			EHSREncounterInitiative::Player, Fixture.Pawn).ResultType,
		EHSREncounterResultType::InvalidRequest);
	Fixture.Encounter->EncounterId = ValidEncounterId;
	const FName ValidEnemyDefinitionId = Fixture.Encounter->EnemyDefinitionId;
	Fixture.Encounter->EnemyDefinitionId = NAME_None;
	TestEqual(TEXT("Missing enemy id is rejected"),
		Fixture.Transition->RequestEncounterForInteractor(Fixture.Encounter,
			EHSREncounterInitiative::Player, Fixture.Pawn).ResultType,
		EHSREncounterResultType::InvalidRequest);
	Fixture.Encounter->EnemyDefinitionId = ValidEnemyDefinitionId;
	const TSoftObjectPtr<UWorld> ValidBattleMap = Fixture.Encounter->BattleMap;
	Fixture.Encounter->BattleMap.Reset();
	TestEqual(TEXT("Missing battle map is rejected"),
		Fixture.Transition->RequestEncounterForInteractor(Fixture.Encounter,
			EHSREncounterInitiative::Player, Fixture.Pawn).ResultType,
		EHSREncounterResultType::InvalidMap);
	Fixture.Encounter->BattleMap = ValidBattleMap;
	TestEqual(TEXT("Invalid Definition fields create no admission mutation"),
		Fixture.Transition->GetAutomationSnapshot(Fixture.Encounter->EncounterId).AdmissionMutationCount, 0);
	AActor* WrongInteractor = Fixture.World->SpawnActor<AActor>();
	TestEqual(TEXT("Non-Pawn interactor is rejected"),
		Fixture.Transition->RequestEncounterForInteractor(Fixture.Encounter,
			EHSREncounterInitiative::Player, WrongInteractor).ResultType,
		EHSREncounterResultType::NoPlayerSelection);
	TestEqual(TEXT("Wrong interactor creates no admission mutation"),
		Fixture.Transition->GetAutomationSnapshot(Fixture.Encounter->EncounterId).AdmissionMutationCount, 0);
	FHSRAdmissionFixture OtherWorldFixture;
	if (!TestTrue(TEXT("Cross-World fixture initializes"), OtherWorldFixture.Initialize(true)))
	{
		OtherWorldFixture.Shutdown();
		Fixture.Shutdown();
		return false;
	}
	TestEqual(TEXT("Pawn from another World is rejected"),
		Fixture.Transition->RequestEncounterForInteractor(Fixture.Encounter,
			EHSREncounterInitiative::Player, OtherWorldFixture.Pawn).ResultType,
		EHSREncounterResultType::NoPlayerSelection);
	TestEqual(TEXT("Cross-World Pawn creates no admission mutation"),
		Fixture.Transition->GetAutomationSnapshot(Fixture.Encounter->EncounterId).AdmissionMutationCount, 0);
	OtherWorldFixture.Shutdown();

	const FHSREncounterResult First = Fixture.Transition->RequestEncounterForInteractor(
		Fixture.Encounter, EHSREncounterInitiative::Player, Fixture.Pawn);
	TestEqual(TEXT("Valid encounter admission succeeds"), First.ResultType, EHSREncounterResultType::Success);
	TestTrue(TEXT("Admission creates a request id"), First.RequestId.IsValid());
	const FHSRTransitionAutomationSnapshot FirstSnapshot = Fixture.Transition->GetAutomationSnapshot(
		Fixture.Encounter->EncounterId);
	TestEqual(TEXT("Admission captures Party slot 0 identity"), FirstSnapshot.PendingRequest.PlayerCharacterId,
		FName(TEXT("Character.A")));
	TestEqual(TEXT("Admission publishes one mutation"), FirstSnapshot.AdmissionMutationCount, 1);
	TestEqual(TEXT("Admission issues one travel"), FirstSnapshot.TravelInitiationCount, 1);

	const FHSREncounterResult Duplicate = Fixture.Transition->RequestEncounterForInteractor(
		Fixture.Encounter, EHSREncounterInitiative::Player, Fixture.Pawn);
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
	const FHSREncounterResult EmptyPartyResult = EmptyPartyFixture.Transition->RequestEncounterForInteractor(
		EmptyPartyFixture.Encounter, EHSREncounterInitiative::Player, EmptyPartyFixture.Pawn);
	TestEqual(TEXT("Empty Party rejects admission"), EmptyPartyResult.ResultType,
		EHSREncounterResultType::NoPlayerSelection);
	const FHSRTransitionAutomationSnapshot EmptySnapshot = EmptyPartyFixture.Transition->GetAutomationSnapshot(
		EmptyPartyFixture.Encounter->EncounterId);
	TestEqual(TEXT("Empty Party creates no admission mutation"), EmptySnapshot.AdmissionMutationCount, 0);
	TestEqual(TEXT("Empty Party issues no travel"), EmptySnapshot.TravelInitiationCount, 0);
	TestTrue(TEXT("Empty Party leaves request empty"), !EmptySnapshot.PendingRequest.RequestId.IsValid());
	EmptyPartyFixture.Shutdown();

	FHSRAdmissionFixture ResolvedFixture;
	if (!TestTrue(TEXT("Resolved fixture initializes"), ResolvedFixture.Initialize(true)))
	{
		ResolvedFixture.Shutdown();
		return false;
	}
	ResolvedFixture.Transition->SeedResolvedEncounterForAutomation(ResolvedFixture.Encounter->EncounterId);
	const FHSRTransitionAutomationSnapshot ResolvedBefore = ResolvedFixture.Transition->GetAutomationSnapshot(
		ResolvedFixture.Encounter->EncounterId);
	TestEqual(TEXT("Resolved encounter replay is rejected"),
		ResolvedFixture.Transition->RequestEncounterForInteractor(ResolvedFixture.Encounter,
			EHSREncounterInitiative::Player, ResolvedFixture.Pawn).ResultType,
		EHSREncounterResultType::AlreadyConsumed);
	const FHSRTransitionAutomationSnapshot ResolvedAfter = ResolvedFixture.Transition->GetAutomationSnapshot(
		ResolvedFixture.Encounter->EncounterId);
	TestEqual(TEXT("Resolved replay creates no admission mutation"), ResolvedAfter.AdmissionMutationCount,
		ResolvedBefore.AdmissionMutationCount);
	TestEqual(TEXT("Resolved replay issues no travel"), ResolvedAfter.TravelInitiationCount,
		ResolvedBefore.TravelInitiationCount);
	ResolvedFixture.Shutdown();

	FHSRAdmissionFixture FailureFixture;
	if (!TestTrue(TEXT("Travel failure fixture initializes"), FailureFixture.Initialize(true)))
	{
		FailureFixture.Shutdown();
		return false;
	}
	TestEqual(TEXT("Travel failure fixture admits first request"),
		FailureFixture.Transition->RequestEncounterForInteractor(FailureFixture.Encounter,
			EHSREncounterInitiative::Player, FailureFixture.Pawn).ResultType,
		EHSREncounterResultType::Success);
	FailureFixture.Transition->HandleTravelFailure(FailureFixture.World,
		ETravelFailure::LoadMapFailure, TEXT("matching automation failure"));
	TestEqual(TEXT("Matching failure clears the active transaction"),
		FailureFixture.Transition->GetCurrentState(), EHSREncounterState::Empty);
	TestEqual(TEXT("Matching failure permits a fresh retry"),
		FailureFixture.Transition->RequestEncounterForInteractor(FailureFixture.Encounter,
			EHSREncounterInitiative::Player, FailureFixture.Pawn).ResultType,
		EHSREncounterResultType::Success);
	FailureFixture.Shutdown();

	return true;
}

#endif
