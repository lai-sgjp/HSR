#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Curves/CurveFloat.h"
#include "Kismet/GameplayStatics.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Data/Definitions/HSRQuestDefinition.h"
#include "../Data/Definitions/HSRMapDefinition.h"
#include "../Data/Definitions/HSRTeleportDefinition.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../Quest/HSRQuestSubsystem.h"
#include "../Map/HSRMapSubsystem.h"
#include "../Save/HSRSaveSubsystem.h"

namespace HSR::P16::Cold
{
constexpr const TCHAR* Slot=TEXT("p16_cold_authority_v1");
FString Staging(){return FString(Slot)+TEXT(".__hsr_staging_v1");}
FString Backup(){return FString(Slot)+TEXT(".__hsr_backup_v1");}
bool ExistsAny(){return UGameplayStatics::DoesSaveGameExist(Slot,0)||UGameplayStatics::DoesSaveGameExist(Staging(),0)||UGameplayStatics::DoesSaveGameExist(Backup(),0);}
struct FFixture
{
	UGameInstance* GI=NewObject<UGameInstance>();UHSRCharacterProfileSubsystem* Profiles=NewObject<UHSRCharacterProfileSubsystem>(GI);UHSRPartySubsystem* Party=NewObject<UHSRPartySubsystem>(GI);UHSREquipmentSubsystem* Equipment=NewObject<UHSREquipmentSubsystem>(GI);UHSRInventorySubsystem* Inventory=NewObject<UHSRInventorySubsystem>(GI);UHSRRewardSubsystem* Reward=NewObject<UHSRRewardSubsystem>(GI);UHSRQuestSubsystem* Quest=NewObject<UHSRQuestSubsystem>(GI);UHSRMapSubsystem* Map=NewObject<UHSRMapSubsystem>(GI);UHSRSaveSubsystem* Save=NewObject<UHSRSaveSubsystem>(GI);
	const FGuid EquipmentId=FGuid(16,5,1,1);
	FFixture()
	{
		auto MakeCharacter=[&](FName Id){UHSRCharacterDefinition* D=NewObject<UHSRCharacterDefinition>();D->CharacterId=Id;D->MaxLevel=2;UCurveFloat* C=NewObject<UCurveFloat>(D);C->FloatCurve.AddKey(2,100);D->CumulativeExperienceCurve=C;Profiles->RegisterDefinition(D);};MakeCharacter(TEXT("character.p16.a"));MakeCharacter(TEXT("character.p16.b"));Party->InitializeForDevelopmentTest(Profiles);
		UHSRRelicDefinition* Relic=NewObject<UHSRRelicDefinition>();Relic->DefinitionId=TEXT("relic.p16.cold");Relic->SetId=TEXT("set.p16.cold");Relic->Slot=EHSRRelicSlot::Head;Relic->EnhancementCap=15;Equipment->RegisterDefinition(*Relic);
		UHSRItemDefinition* Item=NewObject<UHSRItemDefinition>();Item->ItemId=TEXT("item.p16.quest");Item->StorageKind=EHSRItemStorageKind::Stackable;Item->MaxStack=99;Inventory->RegisterDefinition(*Item);
		Reward->InitializeForAutomation(Inventory);UHSRRewardDefinition* RewardDef=NewObject<UHSRRewardDefinition>();RewardDef->RewardDefinitionId=TEXT("reward.p16.quest");RewardDef->FixedItems.Add({Item->ItemId,2});Reward->RegisterRewardDefinition(*RewardDef);
		Quest->InitializeForAutomation(Reward);UHSRQuestDefinition* QuestDef=NewObject<UHSRQuestDefinition>();QuestDef->QuestId=TEXT("quest.p16.cold");QuestDef->Objectives.Add({TEXT("objective.p16.cold"),TEXT("event.p16.cold"),1});QuestDef->RewardDefinitionId=RewardDef->RewardDefinitionId;QuestDef->RewardSeed=1605;QuestDef->bAutoClaimReward=true;Quest->RegisterQuestDefinition(*QuestDef);
		auto MakeMap=[&](FName Id,FName Region,FName Arrival,const TCHAR* Path){UHSRMapDefinition* D=NewObject<UHSRMapDefinition>();D->MapId=Id;D->RegionId=Region;D->DefaultArrivalId=Arrival;D->World=TSoftObjectPtr<UWorld>(FSoftObjectPath(Path));Map->RegisterMapDefinition(*D);};MakeMap(TEXT("map.p16.a"),TEXT("region.p16.a"),TEXT("arrival.p16.a"),TEXT("/Game/Maps/Map_A.Map_A"));MakeMap(TEXT("map.p16.b"),TEXT("region.p16.b"),TEXT("arrival.p16.b"),TEXT("/Game/Maps/Map_B.Map_B"));UHSRTeleportDefinition* Teleport=NewObject<UHSRTeleportDefinition>();Teleport->TeleportId=TEXT("teleport.p16.ab");Teleport->SourceMapId=TEXT("map.p16.a");Teleport->DestinationMapId=TEXT("map.p16.b");Teleport->DestinationArrivalId=TEXT("arrival.p16.froma");Map->RegisterTeleportDefinition(*Teleport);
		Save->InitializeForDevelopmentTest(Profiles,Party,Equipment,Inventory,Reward,Quest,Map);
	}
	void MakeS1()
	{
		Party->AddCharacter(TEXT("character.p16.a"));FHSREquipmentInstance Relic;Relic.InstanceId=EquipmentId;Relic.DefinitionId=TEXT("relic.p16.cold");Relic.Kind=EHSREquipmentKind::Relic;Equipment->Equip(HSRCharacterGuidFromProfileName(TEXT("character.p16.a")),Relic);Equipment->SetEnhancementLevel(HSRCharacterGuidFromProfileName(TEXT("character.p16.a")),EquipmentId,3);FHSRQuestRuntimeState State;Quest->StartQuest(TEXT("quest.p16.cold"),State);Map->SetCurrentLocation(TEXT("map.p16.a"));Map->UnlockRegion(TEXT("region.p16.a"));Map->SetExplorationFlag(TEXT("flag.p16.s1"));
	}
	void MakeS2()
	{
		Profiles->GrantExperience(TEXT("character.p16.a"),100);Party->AddCharacter(TEXT("character.p16.b"));Equipment->SetEnhancementLevel(HSRCharacterGuidFromProfileName(TEXT("character.p16.a")),EquipmentId,4);TArray<FHSRQuestRuntimeState> Changed;Quest->SubmitEvent({TEXT("event.p16.cold"),1},Changed);Map->SetCurrentLocation(TEXT("map.p16.b"));Map->UnlockRegion(TEXT("region.p16.b"));Map->UnlockTeleport(TEXT("teleport.p16.ab"));Map->SetExplorationFlag(TEXT("flag.p16.s2"));
	}
};
void AssertState(FAutomationTestBase& T,const FHSRSaveData& D,bool bS2)
{
	T.TestEqual(TEXT("two profiles"),D.Profiles.Num(),2);const FHSRSaveProfileDto* A=D.Profiles.FindByPredicate([](const auto& P){return P.State.CharacterId==TEXT("character.p16.a");});T.TestTrue(TEXT("profile A exists"),A!=nullptr);if(A){T.TestEqual(TEXT("profile A level"),A->State.Level,bS2?2:1);T.TestEqual(TEXT("profile A revision"),A->RuntimeRevision,static_cast<int64>(bS2?1:0));}
	T.TestEqual(TEXT("party revision"),D.PartyRevision,static_cast<int64>(bS2?2:1));T.TestEqual(TEXT("two party slots"),D.PartySlots.Num(),2);if(D.PartySlots.Num()>=2){T.TestEqual(TEXT("party A"),D.PartySlots[0].CharacterId,FName(TEXT("character.p16.a")));T.TestEqual(TEXT("party B state"),D.PartySlots[1].CharacterId,bS2?FName(TEXT("character.p16.b")):NAME_None);}
	T.TestEqual(TEXT("one equipment"),D.Equipment.Num(),1);if(!D.Equipment.IsEmpty()){T.TestEqual(TEXT("equipment id"),D.Equipment[0].InstanceId,FGuid(16,5,1,1));T.TestEqual(TEXT("equipment enhancement"),D.Equipment[0].EnhancementLevel,bS2?4:3);T.TestEqual(TEXT("equipment revision"),D.Equipment[0].AuthorityRevision,bS2?3:2);}
	T.TestEqual(TEXT("inventory stacks"),D.Inventory.Stacks.Num(),bS2?1:0);T.TestEqual(TEXT("inventory revision"),D.Inventory.Revision,static_cast<int64>(bS2?1:0));if(bS2&&!D.Inventory.Stacks.IsEmpty())T.TestEqual(TEXT("inventory quantity"),D.Inventory.Stacks[0].Quantity,2);
	T.TestEqual(TEXT("reward receipts"),D.Rewards.Receipts.Num(),bS2?1:0);T.TestEqual(TEXT("reward revision"),D.Rewards.Revision,static_cast<int64>(bS2?1:0));T.TestEqual(TEXT("quest states"),D.Quests.States.Num(),1);if(!D.Quests.States.IsEmpty()){T.TestEqual(TEXT("quest complete state"),D.Quests.States[0].State,bS2?EHSRQuestState::Completed:EHSRQuestState::Active);T.TestEqual(TEXT("quest reward claimed"),D.Quests.States[0].bRewardClaimed,bS2);}
	T.TestEqual(TEXT("map location"),D.Map.CurrentLocation.MapId,bS2?FName(TEXT("map.p16.b")):FName(TEXT("map.p16.a")));T.TestTrue(TEXT("S1 flag"),D.Map.ExplorationFlags.Contains(TEXT("flag.p16.s1")));T.TestEqual(TEXT("S2 flag"),D.Map.ExplorationFlags.Contains(TEXT("flag.p16.s2")),bS2);T.TestEqual(TEXT("map revision"),D.Map.Revision,static_cast<int64>(bS2?7:3));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRColdSeed,"HSR.ColdSave.Seed",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHSRColdSeed::RunTest(const FString&){using namespace HSR::P16::Cold;TestFalse(TEXT("cold slot must be clean before seed"),ExistsAny());if(ExistsAny())return false;FFixture F;F.MakeS1();TestEqual(TEXT("save S1"),F.Save->SaveToSlot(Slot),EHSRSaveResult::Success);F.MakeS2();TestEqual(TEXT("save S2"),F.Save->SaveToSlot(Slot),EHSRSaveResult::Success);TArray<uint8>P,B;FHSRSaveData PD,BD;FHSRSaveEnvelopeHeader PH,BH;UGameplayStatics::LoadDataFromSlot(P,Slot,0);UGameplayStatics::LoadDataFromSlot(B,Backup(),0);TestEqual(TEXT("primary decode"),HSRSaveVersion::DecodeEnvelope(P,Slot,0,PD,&PH),EHSRSaveDecodeResult::Success);TestEqual(TEXT("backup decode"),HSRSaveVersion::DecodeEnvelope(B,Slot,0,BD,&BH),EHSRSaveDecodeResult::Success);TestTrue(TEXT("same lineage"),PH.SaveId==BH.SaveId);TestEqual(TEXT("primary gen2"),PH.Generation,static_cast<uint64>(2));TestEqual(TEXT("backup gen1"),BH.Generation,static_cast<uint64>(1));AssertState(*this,PD,true);AssertState(*this,BD,false);TestFalse(TEXT("no staging"),UGameplayStatics::DoesSaveGameExist(Staging(),0));return true;}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRColdVerifyPrimary,"HSR.ColdSave.VerifyPrimary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHSRColdVerifyPrimary::RunTest(const FString&){using namespace HSR::P16::Cold;FFixture F;F.MakeS1();int32 Commits=0;F.Save->OnRestoreCommitted().AddLambda([&](const FHSRRestoreCommitInfo&){++Commits;});const int64 Before=F.Save->GetRestoreTransactionRevisionForDevelopmentTest();const EHSRSaveResult Result=F.Save->LoadFromSlot(Slot);const FHSRSaveLoadResult& Load=F.Save->GetLastLoadResult();AddInfo(FString::Printf(TEXT("cold primary diagnostics result=%d source=%d primaryReason=%d primaryStage=%d backupReason=%d backupStage=%d"),static_cast<int32>(Result),static_cast<int32>(Load.Source),Load.PrimaryReason,static_cast<int32>(Load.PrimaryStageReason),Load.BackupReason,static_cast<int32>(Load.BackupStageReason)));TestEqual(TEXT("cold primary load"),Result,EHSRSaveResult::Success);if(Result!=EHSRSaveResult::Success)return false;TestEqual(TEXT("primary source"),Load.Source,EHSRSaveLoadSource::Primary);TestEqual(TEXT("primary gen2"),Load.Generation,static_cast<uint64>(2));AssertState(*this,F.Save->GetSnapshot(),true);TestEqual(TEXT("one transaction"),F.Save->GetRestoreTransactionRevisionForDevelopmentTest(),Before+1);TestEqual(TEXT("one commit event"),Commits,1);TestEqual(TEXT("repeat primary"),F.Save->LoadFromSlot(Slot),EHSRSaveResult::Success);TestEqual(TEXT("repeat tx stable"),F.Save->GetRestoreTransactionRevisionForDevelopmentTest(),Before+1);TestEqual(TEXT("repeat event stable"),Commits,1);TestFalse(TEXT("load never stages"),UGameplayStatics::DoesSaveGameExist(Staging(),0));return true;}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRColdCorruptPrimary,"HSR.ColdSave.CorruptPrimary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHSRColdCorruptPrimary::RunTest(const FString&){using namespace HSR::P16::Cold;TArray<uint8>P,B,After,B2;TestTrue(TEXT("read primary"),UGameplayStatics::LoadDataFromSlot(P,Slot,0));TestTrue(TEXT("read backup"),UGameplayStatics::LoadDataFromSlot(B,Backup(),0));FHSRSaveData D;TestEqual(TEXT("primary valid before"),HSRSaveVersion::DecodeEnvelope(P,Slot,0,D),EHSRSaveDecodeResult::Success);TestTrue(TEXT("payload exists"),P.Num()>FHSRSaveEnvelopeHeader::HeaderBytes);if(P.Num()<=FHSRSaveEnvelopeHeader::HeaderBytes)return false;const int32 Offset=FHSRSaveEnvelopeHeader::HeaderBytes;P[Offset]^=1;TestTrue(TEXT("write only corrupt primary"),UGameplayStatics::SaveDataToSlot(P,Slot,0));UGameplayStatics::LoadDataFromSlot(After,Slot,0);UGameplayStatics::LoadDataFromSlot(B2,Backup(),0);TestTrue(TEXT("primary exact controlled bytes"),After==P);TestTrue(TEXT("backup byte exact"),B2==B);TestEqual(TEXT("primary checksum mismatch"),HSRSaveVersion::DecodeEnvelope(After,Slot,0,D),EHSRSaveDecodeResult::ChecksumMismatch);TestEqual(TEXT("backup still valid"),HSRSaveVersion::DecodeEnvelope(B2,Slot,0,D),EHSRSaveDecodeResult::Success);TestFalse(TEXT("no staging"),UGameplayStatics::DoesSaveGameExist(Staging(),0));return true;}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRColdVerifyBackup,"HSR.ColdSave.VerifyBackup",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHSRColdVerifyBackup::RunTest(const FString&){using namespace HSR::P16::Cold;FFixture F;F.MakeS1();F.MakeS2();int32 Commits=0;F.Save->OnRestoreCommitted().AddLambda([&](const FHSRRestoreCommitInfo&){++Commits;});const int64 Before=F.Save->GetRestoreTransactionRevisionForDevelopmentTest();TArray<uint8>P,B,P0,B0;UGameplayStatics::LoadDataFromSlot(P0,Slot,0);UGameplayStatics::LoadDataFromSlot(B0,Backup(),0);TestEqual(TEXT("cold backup load"),F.Save->LoadFromSlot(Slot),EHSRSaveResult::Success);TestEqual(TEXT("backup source"),F.Save->GetLastLoadResult().Source,EHSRSaveLoadSource::Backup);TestTrue(TEXT("recovered flag"),F.Save->GetLastLoadResult().bRecoveredFromBackup);TestTrue(TEXT("primary untrusted"),F.Save->GetLastLoadResult().bPrimaryUntrusted);TestEqual(TEXT("backup gen1"),F.Save->GetLastLoadResult().Generation,static_cast<uint64>(1));AssertState(*this,F.Save->GetSnapshot(),false);TestEqual(TEXT("one backup transaction"),F.Save->GetRestoreTransactionRevisionForDevelopmentTest(),Before+1);TestEqual(TEXT("one backup commit"),Commits,1);TestEqual(TEXT("repeat backup"),F.Save->LoadFromSlot(Slot),EHSRSaveResult::Success);TestEqual(TEXT("repeat backup tx stable"),F.Save->GetRestoreTransactionRevisionForDevelopmentTest(),Before+1);TestEqual(TEXT("repeat backup event stable"),Commits,1);UGameplayStatics::LoadDataFromSlot(P,Slot,0);UGameplayStatics::LoadDataFromSlot(B,Backup(),0);TestTrue(TEXT("backup recovery primary read-only"),P==P0);TestTrue(TEXT("backup recovery backup read-only"),B==B0);TestFalse(TEXT("backup recovery no staging"),UGameplayStatics::DoesSaveGameExist(Staging(),0));return true;}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRColdCleanup,"HSR.ColdSave.Cleanup",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHSRColdCleanup::RunTest(const FString&){using namespace HSR::P16::Cold;if(UGameplayStatics::DoesSaveGameExist(Slot,0))TestTrue(TEXT("delete primary"),UGameplayStatics::DeleteGameInSlot(Slot,0));if(UGameplayStatics::DoesSaveGameExist(Backup(),0))TestTrue(TEXT("delete backup"),UGameplayStatics::DeleteGameInSlot(Backup(),0));if(UGameplayStatics::DoesSaveGameExist(Staging(),0))TestTrue(TEXT("delete staging"),UGameplayStatics::DeleteGameInSlot(Staging(),0));TestFalse(TEXT("primary absent"),UGameplayStatics::DoesSaveGameExist(Slot,0));TestFalse(TEXT("backup absent"),UGameplayStatics::DoesSaveGameExist(Backup(),0));TestFalse(TEXT("staging absent"),UGameplayStatics::DoesSaveGameExist(Staging(),0));return true;}
#endif
