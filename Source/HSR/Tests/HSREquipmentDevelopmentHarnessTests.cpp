#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Equipment/HSREquipmentDevelopmentHarness.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Inventory/HSRInventorySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSREquipmentDevelopmentHarnessTest,"HSR.Equipment.DevelopmentHarness",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHSREquipmentDevelopmentHarnessTest::RunTest(const FString&)
{
	UGameInstance* GI=NewObject<UGameInstance>();UHSREquipmentSubsystem* Equipment=NewObject<UHSREquipmentSubsystem>(GI);TestNotNull(TEXT("equipment subsystem"),Equipment);TestFalse(TEXT("null subsystem rejected"),FHSREquipmentDevelopmentHarness::SetupFixedLoadoutForTest(nullptr));
	TestTrue(TEXT("setup fixed loadout"),FHSREquipmentDevelopmentHarness::SetupFixedLoadoutForTest(Equipment));FHSREquipmentLoadout Loadout;int32 Revision=0;const FGuid CharacterId=HSRCharacterGuidFromProfileName(TEXT("Character.A"));TestTrue(TEXT("fixed loadout exists"),Equipment->GetLoadout(CharacterId,Loadout,Revision));const int32 SetupRevision=Revision;TestEqual(TEXT("one weapon"),Loadout.Equipment.Num(),1);TestEqual(TEXT("two relics"),Loadout.Relics.Num(),2);const FGuid WeaponId=Loadout.Equipment.FindChecked(EHSREquipmentSlot::Weapon).InstanceId;const FGuid HeadId=Loadout.Relics.FindChecked(EHSRRelicSlot::Head).InstanceId;const FGuid HandsId=Loadout.Relics.FindChecked(EHSRRelicSlot::Hands).InstanceId;
	TestTrue(TEXT("remove second relic"),FHSREquipmentDevelopmentHarness::RemoveSecondRelicForTest(Equipment));Equipment->GetLoadout(CharacterId,Loadout,Revision);const int32 RemoveRevision=Revision;TestTrue(TEXT("remove revision increases"),RemoveRevision>SetupRevision);TestEqual(TEXT("one relic"),Loadout.Relics.Num(),1);
	TestTrue(TEXT("restore second relic"),FHSREquipmentDevelopmentHarness::SetupFixedLoadoutForTest(Equipment));Equipment->GetLoadout(CharacterId,Loadout,Revision);const int32 RestoreRevision=Revision;TestTrue(TEXT("restore revision increases"),RestoreRevision>RemoveRevision);TestEqual(TEXT("two relics restored"),Loadout.Relics.Num(),2);TestEqual(TEXT("weapon id stable"),Loadout.Equipment.FindChecked(EHSREquipmentSlot::Weapon).InstanceId,WeaponId);TestEqual(TEXT("head id stable"),Loadout.Relics.FindChecked(EHSRRelicSlot::Head).InstanceId,HeadId);TestEqual(TEXT("hands id stable"),Loadout.Relics.FindChecked(EHSRRelicSlot::Hands).InstanceId,HandsId);
	TestTrue(TEXT("clear loadout"),FHSREquipmentDevelopmentHarness::ClearLoadoutForTest(Equipment));TestFalse(TEXT("clear removes authority loadout"),Equipment->GetLoadout(CharacterId,Loadout,Revision));TestTrue(TEXT("setup after clear"),FHSREquipmentDevelopmentHarness::SetupFixedLoadoutForTest(Equipment));Equipment->GetLoadout(CharacterId,Loadout,Revision);TestTrue(TEXT("post-clear setup revision increases"),Revision>RestoreRevision);
	TestTrue(TEXT("cleanup is idempotent"),FHSREquipmentDevelopmentHarness::CleanupSave());
	UHSRInventorySubsystem* Inventory=NewObject<UHSRInventorySubsystem>(GI);TestTrue(TEXT("P17 movement audit completes"),FHSREquipmentDevelopmentHarness::RunP17MovementAuditForTest(Equipment,Inventory));
	return true;
}
#endif
