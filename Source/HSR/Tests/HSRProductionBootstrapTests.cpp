#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "../Character/HSRExplorationCharacter.h"
#include "../Data/Definitions/HSRCharacterCatalog.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/Definitions/HSRMapCatalog.h"
#include "../Map/HSRMapSubsystem.h"
#include "../Framework/HSRGameModeBase.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../UI/HSRCharacterDetailViewModel.h"

namespace
{
	struct FHSRBootstrapFixture
	{
		UGameInstance* GameInstance = nullptr;
		UWorld* World = nullptr;
		AHSRGameModeBase* GameMode = nullptr;
		APlayerController* Controller = nullptr;
		AHSRExplorationCharacter* Pawn = nullptr;
		UHSRCharacterCatalog* Catalog = nullptr;

		bool Initialize()
		{
			if (!GEngine)
			{
				return false;
			}
			GameInstance = NewObject<UGameInstance>(GEngine);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone(FName(*FString::Printf(TEXT("HSRBootstrap_%s"),
				*FGuid::NewGuid().ToString(EGuidFormats::Digits))));
			World = GameInstance->GetWorld();
			Catalog = LoadObject<UHSRCharacterCatalog>(nullptr,
				TEXT("/Game/Data/Progression/DA_CharacterCatalog_P11.DA_CharacterCatalog_P11"));
			if (!World || !Catalog)
			{
				return false;
			}

			GameMode = World->SpawnActor<AHSRGameModeBase>();
			Controller = World->SpawnActor<APlayerController>();
			Pawn = World->SpawnActor<AHSRExplorationCharacter>();
			if (!GameMode || !Controller || !Pawn)
			{
				return false;
			}

			Controller->Possess(Pawn);
			GameMode->ConfigureCharacterBootstrapForAutomation(Catalog, TEXT("Character.A"), Controller);
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRProductionBootstrapCharacterIdentityTest,
	"HSR.ProductionBootstrap.CharacterIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRProductionBootstrapCharacterIdentityTest::RunTest(const FString& Parameters)
{
	FHSRBootstrapFixture Fixture;
	if (!TestTrue(TEXT("Fixture initializes with the authored catalog"), Fixture.Initialize()))
	{
		Fixture.Shutdown();
		return false;
	}

	TestEqual(TEXT("New game bootstrap succeeds"),
		Fixture.GameMode->BootstrapCharacterIdentity(EHSRCharacterBootstrapMode::NewGameDefaults),
		EHSRCharacterBootstrapResult::Success);
	TestEqual(TEXT("GameMode resolves Character.A"), Fixture.GameMode->GetResolvedCharacterId(), FName(TEXT("Character.A")));
	TestEqual(TEXT("Pawn projects Character.A"), Fixture.Pawn->GetProjectedCharacterId(), FName(TEXT("Character.A")));

	UHSRPartySubsystem* Party = Fixture.GameInstance->GetSubsystem<UHSRPartySubsystem>();
	UHSRCharacterProfileSubsystem* Profiles = Fixture.GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>();
	FHSRPartySnapshot PartyAfterFirst;
	TestTrue(TEXT("Party snapshot available"), Party && Party->GetSnapshot(PartyAfterFirst));
	TestEqual(TEXT("Party slot 0 owns Character.A"), PartyAfterFirst.Slots[0].CharacterId, FName(TEXT("Character.A")));
	FHSRCharacterProfileSnapshot Profile;
	TestTrue(TEXT("Character.A profile exists"), Profiles && Profiles->GetProfileSnapshot(TEXT("Character.A"), Profile));

	UHSRCharacterDetailViewModel* ViewModel = NewObject<UHSRCharacterDetailViewModel>();
	ViewModel->Initialize(Profiles, nullptr, Party);
	TestEqual(TEXT("Character UI selects committed Party slot 0"), ViewModel->SelectPartySlot0(), EHSRCharacterDetailResult::Success);
	FHSRCharacterDetailSnapshot Detail;
	TestTrue(TEXT("Character UI snapshot exists"), ViewModel->GetSnapshot(Detail));
	TestEqual(TEXT("Character UI identity matches Party"), Detail.CharacterId, PartyAfterFirst.Slots[0].CharacterId);

	TestEqual(TEXT("Repeated bootstrap is a no-op"),
		Fixture.GameMode->BootstrapCharacterIdentity(EHSRCharacterBootstrapMode::NewGameDefaults),
		EHSRCharacterBootstrapResult::NoOp);
	FHSRPartySnapshot PartyAfterRepeat;
	Party->GetSnapshot(PartyAfterRepeat);
	TestEqual(TEXT("Repeated bootstrap preserves Party revision"), PartyAfterRepeat.Revision, PartyAfterFirst.Revision);

	TestEqual(TEXT("Existing committed selection changes to Character.B"),
		Party->ReplaceCharacter(0, TEXT("Character.B")), EHSRPartyResult::Success);
	FHSRPartySnapshot PartyWithB;
	Party->GetSnapshot(PartyWithB);
	TestEqual(TEXT("New-game defaults preserve an existing committed selection"),
		Fixture.GameMode->BootstrapCharacterIdentity(EHSRCharacterBootstrapMode::NewGameDefaults),
		EHSRCharacterBootstrapResult::NoOp);
	FHSRPartySnapshot PartyAfterExisting;
	Party->GetSnapshot(PartyAfterExisting);
	TestEqual(TEXT("Existing selection remains Character.B"), PartyAfterExisting.Slots[0].CharacterId, FName(TEXT("Character.B")));
	TestEqual(TEXT("Existing selection bootstrap preserves Party revision"), PartyAfterExisting.Revision, PartyWithB.Revision);
	TestEqual(TEXT("Pawn follows the committed Character.B selection"), Fixture.Pawn->GetProjectedCharacterId(), FName(TEXT("Character.B")));
	Fixture.Shutdown();

	FHSRBootstrapFixture EmptyFixture;
	if (!TestTrue(TEXT("Empty-state fixture initializes"), EmptyFixture.Initialize()))
	{
		EmptyFixture.Shutdown();
		return false;
	}
	TestEqual(TEXT("Committed-runtime mode rejects empty Party"),
		EmptyFixture.GameMode->BootstrapCharacterIdentity(EHSRCharacterBootstrapMode::UseCommittedRuntime),
		EHSRCharacterBootstrapResult::NoCommittedSelection);
	TestTrue(TEXT("Failed committed-runtime bootstrap leaves Pawn identity empty"), EmptyFixture.Pawn->GetProjectedCharacterId().IsNone());
	FHSRPartySnapshot EmptyParty;
	EmptyFixture.GameInstance->GetSubsystem<UHSRPartySubsystem>()->GetSnapshot(EmptyParty);
	TestEqual(TEXT("Failed committed-runtime bootstrap leaves Party revision zero"), EmptyParty.Revision, static_cast<int64>(0));
	EmptyFixture.Shutdown();

	FHSRBootstrapFixture NonPrimarySlotFixture;
	if (!TestTrue(TEXT("Non-primary-slot fixture initializes"), NonPrimarySlotFixture.Initialize()))
	{
		NonPrimarySlotFixture.Shutdown();
		return false;
	}
	UHSRCharacterProfileSubsystem* NonPrimaryProfiles =
		NonPrimarySlotFixture.GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>();
	UHSRPartySubsystem* NonPrimaryParty = NonPrimarySlotFixture.GameInstance->GetSubsystem<UHSRPartySubsystem>();
	TestEqual(TEXT("Non-primary-slot fixture registers Catalog"),
		NonPrimaryProfiles->RegisterLoadedCatalog(NonPrimarySlotFixture.Catalog), EHSRCharacterProfileResult::Success);
	TestEqual(TEXT("Committed Character.B occupies slot 1"),
		NonPrimaryParty->AddCharacter(TEXT("Character.B"), 1), EHSRPartyResult::Success);
	FHSRPartySnapshot NonPrimaryBefore;
	NonPrimaryParty->GetSnapshot(NonPrimaryBefore);
	TestEqual(TEXT("New-game defaults reject a non-empty Party with empty slot 0"),
		NonPrimarySlotFixture.GameMode->BootstrapCharacterIdentity(EHSRCharacterBootstrapMode::NewGameDefaults),
		EHSRCharacterBootstrapResult::NoCommittedSelection);
	FHSRPartySnapshot NonPrimaryAfter;
	NonPrimaryParty->GetSnapshot(NonPrimaryAfter);
	TestTrue(TEXT("Non-primary selection rejection preserves all Party slots"),
		NonPrimaryAfter.Slots[0].IsEmpty() && NonPrimaryAfter.Slots[1].CharacterId == TEXT("Character.B"));
	TestEqual(TEXT("Non-primary selection rejection preserves Party revision"),
		NonPrimaryAfter.Revision, NonPrimaryBefore.Revision);
	TestTrue(TEXT("Non-primary selection rejection leaves Pawn identity empty"),
		NonPrimarySlotFixture.Pawn->GetProjectedCharacterId().IsNone());
	NonPrimarySlotFixture.Shutdown();

	FHSRBootstrapFixture InvalidIdFixture;
	if (!TestTrue(TEXT("Invalid-id fixture initializes"), InvalidIdFixture.Initialize()))
	{
		InvalidIdFixture.Shutdown();
		return false;
	}
	InvalidIdFixture.GameMode->ConfigureCharacterBootstrapForAutomation(
		InvalidIdFixture.Catalog, TEXT("Character.Missing"), InvalidIdFixture.Controller);
	TestEqual(TEXT("Missing initial CharacterId is rejected"),
		InvalidIdFixture.GameMode->BootstrapCharacterIdentity(EHSRCharacterBootstrapMode::NewGameDefaults),
		EHSRCharacterBootstrapResult::InvalidInitialCharacter);
	FHSRPartySnapshot InvalidIdParty;
	InvalidIdFixture.GameInstance->GetSubsystem<UHSRPartySubsystem>()->GetSnapshot(InvalidIdParty);
	TestEqual(TEXT("Invalid initial ID leaves Party revision zero"), InvalidIdParty.Revision, static_cast<int64>(0));
	TestTrue(TEXT("Invalid initial ID leaves Pawn identity empty"), InvalidIdFixture.Pawn->GetProjectedCharacterId().IsNone());
	InvalidIdFixture.Shutdown();

	FHSRBootstrapFixture MissingCatalogFixture;
	if (!TestTrue(TEXT("Missing-catalog fixture initializes"), MissingCatalogFixture.Initialize()))
	{
		MissingCatalogFixture.Shutdown();
		return false;
	}
	MissingCatalogFixture.GameMode->ConfigureCharacterBootstrapForAutomation(
		nullptr, TEXT("Character.A"), MissingCatalogFixture.Controller);
	TestEqual(TEXT("Missing catalog is rejected"),
		MissingCatalogFixture.GameMode->BootstrapCharacterIdentity(EHSRCharacterBootstrapMode::NewGameDefaults),
		EHSRCharacterBootstrapResult::MissingCatalog);
	FHSRCharacterProfileSnapshot MissingCatalogProfile;
	TestFalse(TEXT("Missing catalog creates no profile"), MissingCatalogFixture.GameInstance
		->GetSubsystem<UHSRCharacterProfileSubsystem>()->GetProfileSnapshot(TEXT("Character.A"), MissingCatalogProfile));
	MissingCatalogFixture.Shutdown();

	FHSRBootstrapFixture ConflictFixture;
	if (!TestTrue(TEXT("Catalog-conflict fixture initializes"), ConflictFixture.Initialize()))
	{
		ConflictFixture.Shutdown();
		return false;
	}
	const UHSRCharacterDefinition* FirstDefinition = ConflictFixture.Catalog->Characters[0]
		? ConflictFixture.Catalog->Characters[0]->GetDefaultObject<UHSRCharacterDefinition>() : nullptr;
	UHSRCharacterProfileSubsystem* ConflictProfiles = ConflictFixture.GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>();
	if (!TestNotNull(TEXT("Catalog contains a valid first Definition"), FirstDefinition))
	{
		ConflictFixture.Shutdown();
		return false;
	}
	TestEqual(TEXT("Partial registration setup succeeds"), ConflictProfiles->RegisterDefinition(FirstDefinition),
		EHSRCharacterProfileResult::Success);
	TestEqual(TEXT("Partially registered catalog is rejected"),
		ConflictFixture.GameMode->BootstrapCharacterIdentity(EHSRCharacterBootstrapMode::NewGameDefaults),
		EHSRCharacterBootstrapResult::CatalogConflict);
	FHSRPartySnapshot ConflictParty;
	ConflictFixture.GameInstance->GetSubsystem<UHSRPartySubsystem>()->GetSnapshot(ConflictParty);
	TestEqual(TEXT("Catalog conflict leaves Party revision zero"), ConflictParty.Revision, static_cast<int64>(0));
	ConflictFixture.Shutdown();

	FHSRBootstrapFixture WrongPawnFixture;
	if (!TestTrue(TEXT("Wrong-pawn fixture initializes"), WrongPawnFixture.Initialize()))
	{
		WrongPawnFixture.Shutdown();
		return false;
	}
	ADefaultPawn* WrongPawn = WrongPawnFixture.World->SpawnActor<ADefaultPawn>();
	WrongPawnFixture.Controller->Possess(WrongPawn);
	TestEqual(TEXT("Wrong Pawn type is rejected before Domain mutation"),
		WrongPawnFixture.GameMode->BootstrapCharacterIdentity(EHSRCharacterBootstrapMode::NewGameDefaults),
		EHSRCharacterBootstrapResult::PawnProjectionFailed);
	FHSRCharacterProfileSnapshot WrongPawnProfile;
	TestFalse(TEXT("Wrong Pawn creates no profile"), WrongPawnFixture.GameInstance
		->GetSubsystem<UHSRCharacterProfileSubsystem>()->GetProfileSnapshot(TEXT("Character.A"), WrongPawnProfile));
	WrongPawnFixture.Shutdown();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRProductionBootstrapMapCatalogTest,
	"HSR.ProductionBootstrap.MapCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRProductionBootstrapMapCatalogTest::RunTest(const FString& Parameters)
{
	FHSRBootstrapFixture Fixture;
	if (!TestTrue(TEXT("Map-catalog fixture initializes"), Fixture.Initialize()))
	{
		Fixture.Shutdown();
		return false;
	}

	UHSRMapCatalog* Catalog = LoadObject<UHSRMapCatalog>(nullptr,
		TEXT("/Game/Data/Maps/DA_MapCatalog.DA_MapCatalog"));
	if (!TestNotNull(TEXT("Authored map catalog loads"), Catalog))
	{
		Fixture.Shutdown();
		return false;
	}
	Fixture.GameMode->ConfigureMapBootstrapForAutomation(Catalog, TEXT("Map.ObservationCar"));

	UHSRMapSubsystem* Maps = Fixture.GameInstance->GetSubsystem<UHSRMapSubsystem>();
	TestEqual(TEXT("Map bootstrap succeeds"),
		Fixture.GameMode->BootstrapMapDefinitions(), EHSRMapBootstrapResult::Success);
	TestEqual(TEXT("All three maps registered"), Maps->HasMapDefinition(TEXT("Map.ObservationCar"))
		&& Maps->HasMapDefinition(TEXT("Map.NewEriduSixthStreet"))
		&& Maps->HasMapDefinition(TEXT("Map.HertaSupportSection")), true);
	TestEqual(TEXT("All six teleports registered"), Maps->HasTeleportDefinition(TEXT("Teleport.ObservationCarToSixthStreet"))
		&& Maps->HasTeleportDefinition(TEXT("Teleport.SixthStreetToObservationCar"))
		&& Maps->HasTeleportDefinition(TEXT("Teleport.ObservationCarToHerta"))
		&& Maps->HasTeleportDefinition(TEXT("Teleport.HertaToObservationCar"))
		&& Maps->HasTeleportDefinition(TEXT("Teleport.SixthStreetToHerta"))
		&& Maps->HasTeleportDefinition(TEXT("Teleport.HertaToSixthStreet")), true);
	TestEqual(TEXT("Initial location set"), Maps->GetSnapshot().CurrentLocation.MapId, FName(TEXT("Map.ObservationCar")));
	TestEqual(TEXT("Herta display name resolved"), Maps->GetMapDisplayName(TEXT("Map.HertaSupportSection")),
		FText::FromString(TEXT("黑塔空间站支援舱段")));

	// All catalog regions are unlocked so every authored teleport is usable.
	TestEqual(TEXT("All catalog regions unlocked"), Maps->IsRegionUnlocked(TEXT("Region.A"))
		&& Maps->IsRegionUnlocked(TEXT("Region.B"))
		&& Maps->IsRegionUnlocked(TEXT("Region.C")), true);
	TArray<FHSRTeleportProjection> Projections;
	Maps->GetAvailableTeleports(Projections);
	int32 Reachable = 0;
	for (const FHSRTeleportProjection& Projection : Projections)
	{
		if (Projection.bUsable)
		{
			++Reachable;
		}
	}
	TestEqual(TEXT("Two teleports reachable from observation car"), Reachable, 2);

	// Repeated bootstrap is a no-op and does not conflict.
	TestEqual(TEXT("Repeated map bootstrap no-op"),
		Fixture.GameMode->BootstrapMapDefinitions(), EHSRMapBootstrapResult::Success);

	Fixture.Shutdown();

	FHSRBootstrapFixture MissingCatalogFixture;
	if (!TestTrue(TEXT("Missing-catalog fixture initializes"), MissingCatalogFixture.Initialize()))
	{
		MissingCatalogFixture.Shutdown();
		return false;
	}
	MissingCatalogFixture.GameMode->ConfigureMapBootstrapForAutomation(nullptr, TEXT("Map.ObservationCar"));
	TestEqual(TEXT("Missing map catalog is rejected"),
		MissingCatalogFixture.GameMode->BootstrapMapDefinitions(), EHSRMapBootstrapResult::MissingCatalog);
	MissingCatalogFixture.Shutdown();

	return true;
}

#endif
