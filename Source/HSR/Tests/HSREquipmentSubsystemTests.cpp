#if WITH_DEV_AUTOMATION_TESTS

#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Data/Definitions/HSREquipmentDefinition.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Data/Definitions/HSRRelicSetDefinition.h"
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"

#include <limits>

namespace HSR::Equipment::Tests
{
	static FGuid Id(uint32 Seed)
	{
		return FGuid(Seed, Seed + 1, Seed + 2, Seed + 3);
	}

	static FHSREquipmentInstance MakeInstance(FName DefinitionId, EHSREquipmentKind Kind, uint32 Seed, int32 Level = 0)
	{
		FHSREquipmentInstance Instance;
		Instance.InstanceId = Id(Seed);
		Instance.DefinitionId = DefinitionId;
		Instance.Kind = Kind;
		Instance.EnhancementLevel = Level;
		return Instance;
	}

	static UHSREquipmentSubsystem* MakeSubsystem()
	{
		UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
		return NewObject<UHSREquipmentSubsystem>(GameInstance);
	}

	static void RegisterStandardDefinitions(FAutomationTestBase& Test, UHSREquipmentSubsystem& Subsystem)
	{
		UHSREquipmentDefinition* Weapon = NewObject<UHSREquipmentDefinition>();
		Weapon->DefinitionId = TEXT("WeaponA");
		Weapon->Slot = EHSREquipmentSlot::Weapon;
		Weapon->EnhancementCap = 5;
		Test.TestEqual(TEXT("Register weapon"), Subsystem.RegisterDefinition(*Weapon), EHSREquipmentOperationResult::Success);

		UHSRRelicDefinition* Relic = NewObject<UHSRRelicDefinition>();
		Relic->DefinitionId = TEXT("RelicA");
		Relic->Slot = EHSRRelicSlot::Head;
		Relic->EnhancementCap = 3;
		Test.TestEqual(TEXT("Register relic"), Subsystem.RegisterDefinition(*Relic), EHSREquipmentOperationResult::Success);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSREquipmentTransactionsTest, "HSR.Equipment.Transactions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSREquipmentTransactionsTest::RunTest(const FString& Parameters)
{
	using namespace HSR::Equipment::Tests;
	UHSREquipmentSubsystem* Subsystem = MakeSubsystem();
	RegisterStandardDefinitions(*this, *Subsystem);
	const FGuid Character = Id(100);
	const FHSREquipmentInstance First = MakeInstance(TEXT("WeaponA"), EHSREquipmentKind::Equipment, 200);
	const FHSREquipmentInstance Replacement = MakeInstance(TEXT("WeaponA"), EHSREquipmentKind::Equipment, 300, 2);

	TestEqual(TEXT("Equip succeeds"), Subsystem->Equip(Character, First), EHSREquipmentOperationResult::Success);
	FHSREquipmentLoadout Loadout;
	int32 Revision = 0;
	TestTrue(TEXT("Loadout exists"), Subsystem->GetLoadout(Character, Loadout, Revision));
	TestEqual(TEXT("Initial revision"), Revision, 1);
	TestEqual(TEXT("Occupied slot rejects equip"), Subsystem->Equip(Character, Replacement), EHSREquipmentOperationResult::SlotOccupied);
	TestTrue(TEXT("Failure leaves revision unchanged"), Subsystem->GetLoadout(Character, Loadout, Revision));
	TestEqual(TEXT("Failure revision"), Revision, 1);

	TestEqual(TEXT("Replace succeeds"), Subsystem->Replace(Character, Replacement), EHSREquipmentOperationResult::Success);
	TestTrue(TEXT("Replacement loadout exists"), Subsystem->GetLoadout(Character, Loadout, Revision));
	TestEqual(TEXT("Replace revision"), Revision, 2);
	TestEqual(TEXT("Replacement retained"), Loadout.Equipment[EHSREquipmentSlot::Weapon].InstanceId, Replacement.InstanceId);
	TestEqual(TEXT("Enhancement no-op"), Subsystem->SetEnhancementLevel(Character, Replacement.InstanceId, 2), EHSREquipmentOperationResult::NoOp);
	TestTrue(TEXT("No-op loadout exists"), Subsystem->GetLoadout(Character, Loadout, Revision));
	TestEqual(TEXT("No-op revision"), Revision, 2);
	TestEqual(TEXT("Enhancement succeeds"), Subsystem->SetEnhancementLevel(Character, Replacement.InstanceId, 3), EHSREquipmentOperationResult::Success);
	TestEqual(TEXT("Unequip mismatch"), Subsystem->Unequip(Character, EHSREquipmentKind::Equipment, static_cast<int32>(EHSREquipmentSlot::Weapon), First.InstanceId), EHSREquipmentOperationResult::InstanceMismatch);
	TestEqual(TEXT("Unequip succeeds"), Subsystem->Unequip(Character, EHSREquipmentKind::Equipment, static_cast<int32>(EHSREquipmentSlot::Weapon), Replacement.InstanceId), EHSREquipmentOperationResult::Success);
	TestEqual(TEXT("Unequip missing target"), Subsystem->Unequip(Character, EHSREquipmentKind::Equipment, static_cast<int32>(EHSREquipmentSlot::Weapon), Replacement.InstanceId), EHSREquipmentOperationResult::TargetNotFound);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSREquipmentValidationTest, "HSR.Equipment.Validation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSREquipmentValidationTest::RunTest(const FString& Parameters)
{
	using namespace HSR::Equipment::Tests;
	UHSREquipmentSubsystem* Subsystem = MakeSubsystem();
	UHSREquipmentDefinition* Empty = NewObject<UHSREquipmentDefinition>();
	TestEqual(TEXT("Empty definition id"), Subsystem->RegisterDefinition(*Empty), EHSREquipmentOperationResult::InvalidDefinitionId);
	RegisterStandardDefinitions(*this, *Subsystem);
	UHSREquipmentDefinition* Duplicate = NewObject<UHSREquipmentDefinition>();
	Duplicate->DefinitionId = TEXT("WeaponA");
	TestEqual(TEXT("Duplicate definition id"), Subsystem->RegisterDefinition(*Duplicate), EHSREquipmentOperationResult::DuplicateDefinitionId);

	const FGuid CharacterA = Id(400);
	const FGuid CharacterB = Id(500);
	FHSREquipmentInstance InvalidId = MakeInstance(TEXT("WeaponA"), EHSREquipmentKind::Equipment, 600);
	InvalidId.InstanceId.Invalidate();
	TestEqual(TEXT("Empty instance id"), Subsystem->Equip(CharacterA, InvalidId), EHSREquipmentOperationResult::InvalidInstanceId);
	TestEqual(TEXT("Unknown definition"), Subsystem->Equip(CharacterA, MakeInstance(TEXT("Missing"), EHSREquipmentKind::Equipment, 601)), EHSREquipmentOperationResult::UnknownDefinition);
	TestEqual(TEXT("Kind mismatch"), Subsystem->Equip(CharacterA, MakeInstance(TEXT("WeaponA"), EHSREquipmentKind::Relic, 602)), EHSREquipmentOperationResult::InvalidSlot);
	FHSREquipmentInstance BadModifier = MakeInstance(TEXT("WeaponA"), EHSREquipmentKind::Equipment, 603);
	BadModifier.Modifiers.Add({ EHSREquipmentStat::Attack, -1.0f });
	TestEqual(TEXT("Negative modifier"), Subsystem->Equip(CharacterA, BadModifier), EHSREquipmentOperationResult::InvalidModifier);
	BadModifier.Modifiers[0].Value = std::numeric_limits<float>::infinity();
	TestEqual(TEXT("Infinite modifier"), Subsystem->Equip(CharacterA, BadModifier), EHSREquipmentOperationResult::InvalidModifier);
	BadModifier.Modifiers = { { EHSREquipmentStat::Attack, MAX_flt }, { EHSREquipmentStat::Attack, MAX_flt } };
	TestEqual(TEXT("Modifier aggregate overflow"), Subsystem->Equip(CharacterA, BadModifier), EHSREquipmentOperationResult::InvalidModifier);
	TestEqual(TEXT("Negative enhancement"), Subsystem->Equip(CharacterA, MakeInstance(TEXT("WeaponA"), EHSREquipmentKind::Equipment, 604, -1)), EHSREquipmentOperationResult::InvalidEnhancementLevel);
	TestEqual(TEXT("Above cap enhancement"), Subsystem->Equip(CharacterA, MakeInstance(TEXT("WeaponA"), EHSREquipmentKind::Equipment, 605, 6)), EHSREquipmentOperationResult::InvalidEnhancementLevel);

	const FHSREquipmentInstance Equipped = MakeInstance(TEXT("WeaponA"), EHSREquipmentKind::Equipment, 606);
	TestEqual(TEXT("Equip owner"), Subsystem->Equip(CharacterA, Equipped), EHSREquipmentOperationResult::Success);
	TestEqual(TEXT("Cross-character duplicate instance"), Subsystem->Equip(CharacterB, Equipped), EHSREquipmentOperationResult::InstanceAlreadyEquipped);
	TestEqual(TEXT("Replace absent target"), Subsystem->Replace(CharacterB, MakeInstance(TEXT("WeaponA"), EHSREquipmentKind::Equipment, 607)), EHSREquipmentOperationResult::TargetNotFound);
	TestEqual(TEXT("Cross-character enhancement"), Subsystem->SetEnhancementLevel(CharacterB, Equipped.InstanceId, 1), EHSREquipmentOperationResult::InstanceMismatch);
	TestEqual(TEXT("Invalid enhancement request"), Subsystem->SetEnhancementLevel(CharacterA, Equipped.InstanceId, 6), EHSREquipmentOperationResult::InvalidEnhancementLevel);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSREquipmentSetThresholdTest, "HSR.Equipment.SetThreshold", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSREquipmentSetThresholdTest::RunTest(const FString&)
{
	using namespace HSR::Equipment::Tests;
	UHSREquipmentSubsystem* Subsystem = MakeSubsystem();

	// Unregistered sets keep the historical two-piece rule.
	TestEqual(TEXT("Unknown set falls back to default"), Subsystem->GetSetThreshold(TEXT("Set.Unknown")), 2);

	UHSRRelicSetDefinition* FourPiece = NewObject<UHSRRelicSetDefinition>();
	FourPiece->SetId = TEXT("Set.Four");
	FourPiece->Threshold = 4;
	TestEqual(TEXT("Register four-piece set"), Subsystem->RegisterSetDefinition(*FourPiece),
		EHSREquipmentOperationResult::Success);
	TestEqual(TEXT("Authored threshold is honoured"), Subsystem->GetSetThreshold(TEXT("Set.Four")), 4);
	TestEqual(TEXT("Duplicate set rejected"), Subsystem->RegisterSetDefinition(*FourPiece),
		EHSREquipmentOperationResult::DuplicateDefinitionId);

	UHSRRelicSetDefinition* Unnamed = NewObject<UHSRRelicSetDefinition>();
	Unnamed->Threshold = 2;
	TestEqual(TEXT("Missing set id rejected"), Subsystem->RegisterSetDefinition(*Unnamed),
		EHSREquipmentOperationResult::InvalidDefinitionId);

	UHSRRelicSetDefinition* NonPositive = NewObject<UHSRRelicSetDefinition>();
	NonPositive->SetId = TEXT("Set.Zero");
	NonPositive->Threshold = 0;
	TestEqual(TEXT("Non-positive threshold rejected"), Subsystem->RegisterSetDefinition(*NonPositive),
		EHSREquipmentOperationResult::InvalidEnhancementLevel);

	// Two equipped pieces of a four-piece set must not activate.
	const FGuid CharacterId = Id(400);
	UHSRRelicDefinition* Head = NewObject<UHSRRelicDefinition>();
	Head->DefinitionId = TEXT("Relic.Four.Head");
	Head->Slot = EHSRRelicSlot::Head;
	Head->SetId = TEXT("Set.Four");
	UHSRRelicDefinition* Hands = NewObject<UHSRRelicDefinition>();
	Hands->DefinitionId = TEXT("Relic.Four.Hands");
	Hands->Slot = EHSRRelicSlot::Hands;
	Hands->SetId = TEXT("Set.Four");
	TestEqual(TEXT("Register set head"), Subsystem->RegisterDefinition(*Head), EHSREquipmentOperationResult::Success);
	TestEqual(TEXT("Register set hands"), Subsystem->RegisterDefinition(*Hands), EHSREquipmentOperationResult::Success);
	TestEqual(TEXT("Equip head"),
		Subsystem->Equip(CharacterId, MakeInstance(TEXT("Relic.Four.Head"), EHSREquipmentKind::Relic, 410)),
		EHSREquipmentOperationResult::Success);
	TestEqual(TEXT("Equip hands"),
		Subsystem->Equip(CharacterId, MakeInstance(TEXT("Relic.Four.Hands"), EHSREquipmentKind::Relic, 420)),
		EHSREquipmentOperationResult::Success);

	TArray<FHSRRelicSetSnapshot> Snapshots;
	Subsystem->GetRelicSetSnapshots(CharacterId, Snapshots);
	const FHSRRelicSetSnapshot* Row = Snapshots.FindByPredicate(
		[](const FHSRRelicSetSnapshot& Candidate) { return Candidate.SetId == TEXT("Set.Four"); });
	if (TestNotNull(TEXT("Set row present"), Row))
	{
		TestEqual(TEXT("Snapshot reports authored threshold"), Row->Threshold, 4);
		TestEqual(TEXT("Two pieces counted"), Row->EquippedCount, 2);
		TestFalse(TEXT("Two of four does not activate"), Row->bActive);
	}
	return true;
}

#endif
