#if WITH_DEV_AUTOMATION_TESTS

#include "../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSRItemEquipmentMappingContractTest,
	"HSR.Equipment.Movement.MappingContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRItemEquipmentMappingContractTest::RunTest(const FString& Parameters)
{
	UHSRItemEquipmentMappingCatalog* Catalog = NewObject<UHSRItemEquipmentMappingCatalog>();
	FHSRItemEquipmentMappingEntry Entry;
	Entry.ItemId = TEXT("Item.Equipment.Test");
	Entry.EquipmentDefinitionId = TEXT("Equipment.Test");
	Entry.Kind = EHSREquipmentKind::Equipment;
	Entry.Slot = static_cast<int32>(EHSREquipmentSlot::Weapon);
	TestTrue(TEXT("Valid mapping accepted"), Catalog->AddMapping(Entry));
	TestFalse(TEXT("Duplicate ItemId rejected"), Catalog->AddMapping(Entry));
	TestTrue(TEXT("Mapping resolves by explicit ItemId"), Catalog->Resolve(TEXT("Item.Equipment.Test"), Entry));
	TestEqual(TEXT("Resolved target is explicit"), Entry.EquipmentDefinitionId, FName(TEXT("Equipment.Test")));
	return true;
}

#endif
