#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Save/HSRSaveTypes.h"
#include "../Save/HSRSaveSubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "Curves/CurveFloat.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSREquipmentSaveV2Test,"HSR.Save.EquipmentV2",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHSREquipmentSaveV2Test::RunTest(const FString&)
{
	UGameInstance* GI=NewObject<UGameInstance>();UHSREquipmentSubsystem* E=NewObject<UHSREquipmentSubsystem>(GI);
	UHSRRelicDefinition* D=NewObject<UHSRRelicDefinition>();D->DefinitionId=TEXT("Relic.A");D->SetId=TEXT("Set.A");D->Slot=EHSRRelicSlot::Head;D->EnhancementCap=15;TestEqual(TEXT("register"),E->RegisterDefinition(*D),EHSREquipmentOperationResult::Success);
	const FGuid Character(0,1,0,1),Instance(1,2,3,4);FHSREquipmentInstance I;I.InstanceId=Instance;I.DefinitionId=D->DefinitionId;I.Kind=EHSREquipmentKind::Relic;I.EnhancementLevel=3;I.Modifiers.Add({EHSREquipmentStat::Attack,12.0f});TestEqual(TEXT("equip"),E->Equip(Character,I),EHSREquipmentOperationResult::Success);
	TArray<FHSREquipmentSaveDto> Saved;E->ExportSaveData(Saved);TestEqual(TEXT("one row"),Saved.Num(),1);TestEqual(TEXT("set id"),Saved[0].SetId,D->SetId);TestEqual(TEXT("revision"),Saved[0].AuthorityRevision,1);
	TMap<FGuid,FHSREquipmentRestoreState> Candidate;TestTrue(TEXT("prepare valid"),E->PrepareRestore(Saved,Candidate));E->CommitRestore(Candidate);FHSREquipmentLoadout L;int32 Revision=0;TestTrue(TEXT("restored"),E->GetLoadout(Character,L,Revision));TestEqual(TEXT("revision retained"),Revision,1);
	Saved[0].SetId=TEXT("Wrong.Set");TMap<FGuid,FHSREquipmentRestoreState> Bad;TestFalse(TEXT("bad set rejected"),E->PrepareRestore(Saved,Bad));TestTrue(TEXT("old remains"),E->GetLoadout(Character,L,Revision));TestEqual(TEXT("old revision remains"),Revision,1);
	Saved[0].SetId=D->SetId;TMap<FGuid,FHSREquipmentRestoreState> Repeat;TestTrue(TEXT("repeat prepare"),E->PrepareRestore(Saved,Repeat));E->CommitRestore(Repeat);TestTrue(TEXT("repeat restored"),E->GetLoadout(Character,L,Revision));TestEqual(TEXT("repeat revision stable"),Revision,1);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSREquipmentSaveAtomicProjectionTest,"HSR.Save.EquipmentAtomicProjection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHSREquipmentSaveAtomicProjectionTest::RunTest(const FString&)
{
	UGameInstance* GI=NewObject<UGameInstance>();auto* Profiles=NewObject<UHSRCharacterProfileSubsystem>(GI);auto* Party=NewObject<UHSRPartySubsystem>(GI);auto* Equipment=NewObject<UHSREquipmentSubsystem>(GI);auto* Save=NewObject<UHSRSaveSubsystem>(GI);auto* Character=NewObject<UHSRCharacterDefinition>();Character->CharacterId=TEXT("Character.A");Character->MaxLevel=2;auto* Curve=NewObject<UCurveFloat>(Character);Curve->FloatCurve.AddKey(2,100);Character->CumulativeExperienceCurve=Curve;Profiles->RegisterDefinition(Character);Party->InitializeForDevelopmentTest(Profiles);Party->AddCharacter(Character->CharacterId);Save->InitializeForDevelopmentTest(Profiles,Party,Equipment);
	FHSRSaveData Baseline;TestEqual(TEXT("baseline"),Save->SaveSnapshot(Baseline),EHSRSaveResult::Success);FHSRSaveData Candidate=Baseline;Candidate.Profiles[0].State.Experience=50;Equipment->SetRestoreProjection(FHSREquipmentRestoreProjection::CreateLambda([](const FHSREquipmentRestoreMap&){return false;}));TestEqual(TEXT("projection failure rejected"),Save->LoadSnapshot(Candidate),EHSRSaveResult::InvalidData);FHSRCharacterProfileSnapshot Profile;Profiles->GetProfileSnapshot(Character->CharacterId,Profile);TestEqual(TEXT("profile unchanged"),Profile.RuntimeState.Experience,0);TestEqual(TEXT("current unchanged"),Save->GetSnapshot().Profiles[0].State.Experience,0);return true;
}
#endif
