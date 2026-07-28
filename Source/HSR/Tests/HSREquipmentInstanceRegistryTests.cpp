#if WITH_DEV_AUTOMATION_TESTS

#include "../Data/Definitions/HSREquipmentDefinition.h"
#include "../Equipment/HSREquipmentSubsystem.h"
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

#endif
