#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Curves/CurveFloat.h"
#include "../UI/Character/HSRCharacterShellViewModel.h"
#include "../UI/Character/HSRCharacterShellTypes.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Save/HSRSaveSubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Equipment/HSREquipmentTypes.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/Definitions/HSREquipmentDefinition.h"

namespace HSR::P17::CharacterShellTests
{
	static UHSRCharacterDefinition* MakeCharacter(UObject* Outer, const TCHAR* Id, const TCHAR* Name)
	{
		UHSRCharacterDefinition* Definition = NewObject<UHSRCharacterDefinition>(Outer);
		Definition->CharacterId = FName(Id);
		Definition->DisplayName = FText::FromString(Name);
		Definition->MaxLevel = 2;
		Definition->BaseMaxHealth = 500.0f;
		Definition->BaseAttack = 50.0f;
		Definition->BaseDefense = 25.0f;
		Definition->BaseSpeed = 100.0f;
		Definition->SkillMaxLevels.Add(TEXT("Skill.Basic"), 3);
		UCurveFloat* Curve = NewObject<UCurveFloat>(Definition);
		Curve->FloatCurve.AddKey(2.0f, 100.0f);
		Definition->CumulativeExperienceCurve = Curve;
		return Definition;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRCharacterShellViewModelTest,
	"HSR.UI.CharacterShell.ViewModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRCharacterShellViewModelTest::RunTest(const FString&)
{
	using namespace HSR::P17::CharacterShellTests;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHSRCharacterProfileSubsystem* Profiles = NewObject<UHSRCharacterProfileSubsystem>(GameInstance);
	UHSRSaveSubsystem* Save = NewObject<UHSRSaveSubsystem>(GameInstance);
	UHSRPartySubsystem* Party = NewObject<UHSRPartySubsystem>(GameInstance);
	UHSREquipmentSubsystem* Equipment = NewObject<UHSREquipmentSubsystem>(GameInstance);

	UHSRCharacterDefinition* Alpha = MakeCharacter(GameInstance, TEXT("Character.Alpha"), TEXT("Alpha"));
	UHSRCharacterDefinition* Beta = MakeCharacter(GameInstance, TEXT("Character.Beta"), TEXT("Beta"));
	TestEqual(TEXT("register definitions"), Profiles->RegisterDefinitions({Alpha, Beta}),
		EHSRCharacterProfileResult::Success);
	Party->InitializeForDevelopmentTest(Profiles);
	Save->InitializeForDevelopmentTest(Profiles, Party);
	Party->AddCharacter(Alpha->CharacterId);
	Party->AddCharacter(Beta->CharacterId);

	UHSREquipmentDefinition* WeaponDefinition = NewObject<UHSREquipmentDefinition>(GameInstance);
	WeaponDefinition->DefinitionId = TEXT("Equipment.Weapon.Alpha");
	WeaponDefinition->Slot = EHSREquipmentSlot::Weapon;
	WeaponDefinition->EnhancementCap = 5;
	TestEqual(TEXT("register equipment definition"), Equipment->RegisterDefinition(*WeaponDefinition),
		EHSREquipmentOperationResult::Success);
	const FGuid AlphaGuid = HSRCharacterGuidFromProfileName(Alpha->CharacterId);
	FHSREquipmentInstance Weapon;
	Weapon.InstanceId = FGuid(7, 6, 5, 4);
	Weapon.DefinitionId = WeaponDefinition->DefinitionId;
	Weapon.Kind = EHSREquipmentKind::Equipment;
	Weapon.Modifiers.Add({EHSREquipmentStat::Attack, 10.0f});
	TestEqual(TEXT("equip fixture"), Equipment->Equip(AlphaGuid, Weapon),
		EHSREquipmentOperationResult::Success);

	UHSRCharacterShellViewModel* ViewModel = NewObject<UHSRCharacterShellViewModel>();
	int32 Events = 0;
	ViewModel->OnChanged().AddLambda([&Events](const FHSRCharacterShellSnapshot&) { ++Events; });
	ViewModel->Initialize(Profiles, Save, Party, Equipment);

	TestEqual(TEXT("select alpha"), ViewModel->SelectCharacter(Alpha->CharacterId),
		EHSRCharacterShellResult::Success);
	TestEqual(TEXT("select traces"), ViewModel->SelectTab(EHSRCharacterShellTab::Traces),
		EHSRCharacterShellResult::Success);
	FHSRCharacterShellSnapshot Snapshot;
	TestTrue(TEXT("snapshot available"), ViewModel->GetSnapshot(Snapshot));
	TestTrue(TEXT("shell snapshot valid"), Snapshot.bIsValid);
	TestEqual(TEXT("selected alpha"), Snapshot.SelectedCharacterId, Alpha->CharacterId);
	TestEqual(TEXT("selected traces"), Snapshot.SelectedTab, EHSRCharacterShellTab::Traces);
	TestTrue(TEXT("traces tab available"), Snapshot.bSelectedTabAvailable);
	TestEqual(TEXT("character snapshot alpha"), Snapshot.CharacterDetail.CharacterId, Alpha->CharacterId);
	TestTrue(TEXT("character snapshot valid"), Snapshot.CharacterDetail.bIsValid);
	TestEqual(TEXT("equipment item projected"), Snapshot.EquipmentDetail.Items.Num(), 1);
	TestEqual(TEXT("equipment attack projected"), Snapshot.EquipmentDetail.Attack, 10.0f);

	TestEqual(TEXT("select beta"), ViewModel->SelectCharacter(Beta->CharacterId),
		EHSRCharacterShellResult::Success);
	TestTrue(TEXT("selection keeps tab"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("beta selected"), Snapshot.SelectedCharacterId, Beta->CharacterId);
	TestEqual(TEXT("tab retained after character change"), Snapshot.SelectedTab, EHSRCharacterShellTab::Traces);
	TestEqual(TEXT("select weapon"), ViewModel->SelectTab(EHSRCharacterShellTab::Weapon),
		EHSRCharacterShellResult::Success);
	TestEqual(TEXT("select alpha again"), ViewModel->SelectCharacter(Alpha->CharacterId),
		EHSRCharacterShellResult::Success);
	TestTrue(TEXT("selection keeps weapon tab"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("weapon tab retained after character change"), Snapshot.SelectedTab,
		EHSRCharacterShellTab::Weapon);
	TestTrue(TEXT("weapon tab available with equipment snapshot"), Snapshot.bSelectedTabAvailable);
	TestEqual(TEXT("select eidolon"), ViewModel->SelectTab(EHSRCharacterShellTab::Eidolon),
		EHSRCharacterShellResult::Success);
	TestTrue(TEXT("shell stays valid on unavailable tab"), ViewModel->GetSnapshot(Snapshot) && Snapshot.bIsValid);
	TestFalse(TEXT("eidolon tab is explicitly unavailable"), Snapshot.bSelectedTabAvailable);

	const int32 EventsBeforeInvalidSelection = Events;
	TestEqual(TEXT("invalid character rejected"), ViewModel->SelectCharacter(TEXT("Character.Missing")),
		EHSRCharacterShellResult::InvalidCharacterId);
	TestEqual(TEXT("invalid selection emits nothing"), Events, EventsBeforeInvalidSelection);
	TestTrue(TEXT("invalid selection retains old snapshot"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("old character retained"), Snapshot.SelectedCharacterId, Alpha->CharacterId);

	ViewModel->Uninitialize();
	const int32 EventsAfterUninitialize = Events;
	Profiles->GrantExperience(Alpha->CharacterId, 0);
	TestEqual(TEXT("uninitialize removes subscriptions"), Events, EventsAfterUninitialize);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRCharacterShellUnavailableTest,
	"HSR.UI.CharacterShell.Unavailable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRCharacterShellUnavailableTest::RunTest(const FString&)
{
	UHSRCharacterShellViewModel* ViewModel = NewObject<UHSRCharacterShellViewModel>();
	ViewModel->Initialize(nullptr, nullptr, nullptr, nullptr);

	FHSRCharacterShellSnapshot Snapshot;
	TestTrue(TEXT("unavailable snapshot is published"), ViewModel->GetSnapshot(Snapshot));
	TestFalse(TEXT("unavailable snapshot is not valid"), Snapshot.bIsValid);
	TestEqual(TEXT("missing authorities are typed"), Snapshot.FailureReason,
		EHSRCharacterShellResult::NotInitialized);
	TestEqual(TEXT("selecting without authority is rejected"), ViewModel->SelectCharacter(TEXT("Character.Alpha")),
		EHSRCharacterShellResult::NotInitialized);
	return true;
}

#endif
