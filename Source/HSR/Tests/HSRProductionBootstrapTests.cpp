#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "../Character/HSRExplorationCharacter.h"
#include "../Data/Definitions/HSRCharacterCatalog.h"
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
			GameInstance = NewObject<UGameInstance>();
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

	return true;
}

#endif
