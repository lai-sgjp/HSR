#if WITH_DEV_AUTOMATION_TESTS

#include "../Data/Definitions/HSREquipmentDefinition.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Save/HSRSaveTypes.h"
#include "../Save/HSRSaveVersion.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

namespace HSR::Equipment::RegistryTests
{
	static FGuid Id(uint32 Seed)
	{
		return FGuid(Seed, Seed + 1, Seed + 2, Seed + 3);
	}

	static UHSREquipmentSubsystem* MakeSubsystem()
	{
		UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
		UHSREquipmentSubsystem* Subsystem = NewObject<UHSREquipmentSubsystem>(GameInstance);

		UHSREquipmentDefinition* Weapon = NewObject<UHSREquipmentDefinition>();
		Weapon->DefinitionId = TEXT("Weapon.Registry");
		Weapon->Slot = EHSREquipmentSlot::Weapon;
		Weapon->EnhancementCap = 5;
		Subsystem->RegisterDefinition(*Weapon);
		return Subsystem;
	}

	static FHSREquipmentInstance MakeWeapon(uint32 Seed, int32 Level = 0)
	{
		FHSREquipmentInstance Instance;
		Instance.InstanceId = Id(Seed);
		Instance.DefinitionId = TEXT("Weapon.Registry");
		Instance.Kind = EHSREquipmentKind::Equipment;
		Instance.EnhancementLevel = Level;
		Instance.Modifiers.Add({ EHSREquipmentStat::Attack, 12.5f });
		return Instance;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSREquipmentRegistryOwnershipTest,
	"HSR.Equipment.Registry.Ownership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSREquipmentRegistryOwnershipTest::RunTest(const FString& Parameters)
{
	using namespace HSR::Equipment::RegistryTests;
	UHSREquipmentSubsystem* Subsystem = MakeSubsystem();
	const FHSREquipmentInstance First = MakeWeapon(100, 2);

	TestEqual(TEXT("First registration succeeds"), Subsystem->RegisterInstance(First), EHSREquipmentOperationResult::Success);
	TestEqual(TEXT("Matching registration is idempotent"), Subsystem->RegisterInstance(First), EHSREquipmentOperationResult::NoOp);

	FHSREquipmentInstance ChangedPayload = First;
	ChangedPayload.EnhancementLevel = 3;
	TestEqual(TEXT("Changed payload conflicts"), Subsystem->RegisterInstance(ChangedPayload), EHSREquipmentOperationResult::InstancePayloadConflict);

	FHSREquipmentInstance Registered;
	TestTrue(TEXT("Registry resolves complete payload"), Subsystem->FindRegisteredInstance(First.InstanceId, Registered));
	TestEqual(TEXT("Registry preserves enhancement"), Registered.EnhancementLevel, 2);
	TestEqual(TEXT("Registry preserves modifiers"), Registered.Modifiers.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSREquipmentRegistryPlacementTest,
	"HSR.Equipment.Registry.Placement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSREquipmentRegistryPlacementTest::RunTest(const FString& Parameters)
{
	using namespace HSR::Equipment::RegistryTests;
	UHSREquipmentSubsystem* Subsystem = MakeSubsystem();
	const FGuid CharacterA = Id(200);
	const FGuid CharacterB = Id(300);
	const FHSREquipmentInstance First = MakeWeapon(400, 1);
	const FHSREquipmentInstance Second = MakeWeapon(500, 2);
	TestEqual(TEXT("Register first"), Subsystem->RegisterInstance(First), EHSREquipmentOperationResult::Success);
	TestEqual(TEXT("Register second"), Subsystem->RegisterInstance(Second), EHSREquipmentOperationResult::Success);

	TestEqual(TEXT("Place by id"), Subsystem->EquipById(CharacterA, First.InstanceId), EHSREquipmentOperationResult::Success);
	TestEqual(TEXT("Same instance cannot have second placement"), Subsystem->EquipById(CharacterB, First.InstanceId), EHSREquipmentOperationResult::InstanceAlreadyEquipped);

	FHSREquipmentLoadout Loadout;
	int32 Revision = 0;
	TestTrue(TEXT("Resolved loadout exists"), Subsystem->GetLoadout(CharacterA, Loadout, Revision));
	TestEqual(TEXT("Resolved payload comes from registry"), Loadout.Equipment[EHSREquipmentSlot::Weapon].EnhancementLevel, 1);

	TestEqual(TEXT("Unequip removes placement"), Subsystem->Unequip(CharacterA, EHSREquipmentKind::Equipment, static_cast<int32>(EHSREquipmentSlot::Weapon), First.InstanceId), EHSREquipmentOperationResult::Success);
	FHSREquipmentInstance Retained;
	TestTrue(TEXT("Unequip retains registry payload"), Subsystem->FindRegisteredInstance(First.InstanceId, Retained));
	TestEqual(TEXT("Unequip retains modifiers"), Retained.Modifiers.Num(), 1);

	TestEqual(TEXT("Place first again"), Subsystem->EquipById(CharacterA, First.InstanceId), EHSREquipmentOperationResult::Success);
	TestEqual(TEXT("Replace by id"), Subsystem->ReplaceById(CharacterA, Second.InstanceId), EHSREquipmentOperationResult::Success);
	TestTrue(TEXT("Replaced instance remains registered"), Subsystem->FindRegisteredInstance(First.InstanceId, Retained));
	TestTrue(TEXT("Replacement remains registered"), Subsystem->FindRegisteredInstance(Second.InstanceId, Retained));

	TestEqual(TEXT("Enhance authoritative instance"), Subsystem->SetEnhancementLevel(CharacterA, Second.InstanceId, 4), EHSREquipmentOperationResult::Success);
	TestTrue(TEXT("Enhanced loadout resolves"), Subsystem->GetLoadout(CharacterA, Loadout, Revision));
	TestEqual(TEXT("Resolved enhancement updated"), Loadout.Equipment[EHSREquipmentSlot::Weapon].EnhancementLevel, 4);
	TestTrue(TEXT("Registry resolves enhanced instance"), Subsystem->FindRegisteredInstance(Second.InstanceId, Retained));
	TestEqual(TEXT("Registry enhancement updated once"), Retained.EnhancementLevel, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSREquipmentRegistryPersistenceTest,
	"HSR.Equipment.Registry.Persistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSREquipmentRegistryPersistenceTest::RunTest(const FString& Parameters)
{
	using namespace HSR::Equipment::RegistryTests;
	UHSREquipmentSubsystem* Source = MakeSubsystem();
	const FGuid Character = Id(600);
	const FHSREquipmentInstance Placed = MakeWeapon(700, 2);
	const FHSREquipmentInstance Unplaced = MakeWeapon(800, 3);
	Source->RegisterInstance(Placed);
	Source->RegisterInstance(Unplaced);
	Source->EquipById(Character, Placed.InstanceId);

	TArray<FHSREquipmentRegistryDto> Registry;
	TArray<FHSREquipmentPlacementDto> Placements;
	Source->ExportSaveData(Registry, Placements);
	TestEqual(TEXT("Both registry records export"), Registry.Num(), 2);
	TestEqual(TEXT("Only equipped instance has placement"), Placements.Num(), 1);

	UHSREquipmentSubsystem* Restored = MakeSubsystem();
	FHSREquipmentRegistryRestoreState Candidate;
	TestTrue(TEXT("Schema 7 rows prepare"), Restored->PrepareRestore(Registry, Placements, Candidate));
	Restored->CommitRestore(Candidate);
	FHSREquipmentInstance RoundTripped;
	TestTrue(TEXT("Unplaced payload survives restore"), Restored->FindRegisteredInstance(Unplaced.InstanceId, RoundTripped));
	TestEqual(TEXT("Unplaced enhancement survives"), RoundTripped.EnhancementLevel, 3);

	FHSRSaveData Schema6;
	Schema6.SchemaVersion = 6;
	FHSREquipmentSaveDto Legacy;
	Legacy.DefinitionId = Placed.DefinitionId;
	Legacy.InstanceId = Placed.InstanceId;
	Legacy.CharacterId = Character;
	Legacy.Kind = 0;
	Legacy.Slot = static_cast<int32>(EHSREquipmentSlot::Weapon);
	Legacy.EnhancementLevel = Placed.EnhancementLevel;
	Legacy.Modifiers = Placed.Modifiers;
	Legacy.AuthorityRevision = 1;
	Schema6.Equipment.Add(Legacy);
	FHSRItemInstance InventoryOnly;
	InventoryOnly.InstanceId = Id(900);
	InventoryOnly.DefinitionId = TEXT("Inventory.Unique");
	Schema6.Inventory.UniqueItems.Add(InventoryOnly);
	TestEqual(TEXT("Schema 6 migrates"), HSRSaveVersion::MigrateToCurrent(Schema6), EHSRSaveDecodeResult::Success);
	TestEqual(TEXT("Migration creates one registry row"), Schema6.EquipmentRegistry.Num(), 1);
	TestEqual(TEXT("Migration creates one placement row"), Schema6.EquipmentPlacements.Num(), 1);
	TestEqual(TEXT("Inventory unique item remains inventory only"), Schema6.Inventory.UniqueItems.Num(), 1);
	TestEqual(TEXT("Inventory id is not invented in registry"), Schema6.EquipmentRegistry[0].InstanceId, Placed.InstanceId);
	return true;
}

#endif
