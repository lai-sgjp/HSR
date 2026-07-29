#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Curves/CurveFloat.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/Definitions/HSREquipmentDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../Quest/HSRQuestSubsystem.h"
#include "../Map/HSRMapSubsystem.h"
#include "../Save/HSRSaveSubsystem.h"
#include "../Save/HSRSaveVersion.h"

/** P16-002: the validation failure is deliberately tested at LoadSnapshot,
 * before any PrepareRestore, projection, commit, revision, or delegate. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRSaveValidationPreflightTest, "HSR.Save.Validation.Preflight", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRSaveValidationPreflightTest::RunTest(const FString&)
{
	UGameInstance* GI=NewObject<UGameInstance>();auto* Profiles=NewObject<UHSRCharacterProfileSubsystem>(GI);auto* Party=NewObject<UHSRPartySubsystem>(GI);auto* Equipment=NewObject<UHSREquipmentSubsystem>(GI);auto* Inventory=NewObject<UHSRInventorySubsystem>(GI);auto* Reward=NewObject<UHSRRewardSubsystem>(GI);auto* Quest=NewObject<UHSRQuestSubsystem>(GI);auto* Map=NewObject<UHSRMapSubsystem>(GI);auto* Mapping=NewObject<UHSRItemEquipmentMappingCatalog>();auto* Save=NewObject<UHSRSaveSubsystem>(GI);
	auto* Def=NewObject<UHSRCharacterDefinition>();Def->CharacterId=TEXT("character.a");Def->MaxLevel=2;auto* Curve=NewObject<UCurveFloat>(Def);Curve->FloatCurve.AddKey(2,100);Def->CumulativeExperienceCurve=Curve;Profiles->RegisterDefinition(Def);Party->InitializeForDevelopmentTest(Profiles);Party->AddCharacter(TEXT("character.a"));
	Save->InitializeForDevelopmentTest(Profiles,Party,Equipment,Inventory,Reward,Quest,Map,Mapping);
	FHSRSaveData Baseline;TestEqual(TEXT("capture"),Save->SaveSnapshot(Baseline),EHSRSaveResult::Success);auto Canonical=[](FHSRSaveData Data){Data.SchemaVersion=6;TArray<uint8> Bytes;HSRSaveVersion::EncodeCanonicalPayload(Data,Bytes);return Bytes;};const TArray<uint8> BaselineBytes=Canonical(Baseline);const int64 Tx=Save->GetRestoreTransactionRevisionForDevelopmentTest();int32 Restores=0,ProfileEvents=0,PartyEvents=0,EquipmentEvents=0,InventoryEvents=0,RewardEvents=0,QuestEvents=0,MapEvents=0,ProjectionCalls=0;Save->OnRestoreCommitted().AddLambda([&](const FHSRRestoreCommitInfo&){++Restores;});Profiles->OnProfileChanged().AddLambda([&](FName,int64){++ProfileEvents;});Party->OnPartyChanged().AddLambda([&](int64){++PartyEvents;});Equipment->OnLoadoutChanged().AddLambda([&](const FGuid&,int32){++EquipmentEvents;});Inventory->OnInventoryChanged().AddLambda([&](int64){++InventoryEvents;});Reward->OnRewardRestored().AddLambda([&](int64){++RewardEvents;});Quest->OnQuestRestored().AddLambda([&](int64){++QuestEvents;});Map->OnMapStateChanged().AddLambda([&](const FHSRMapRuntimeSnapshot&){++MapEvents;});Equipment->SetRestoreProjection(FHSREquipmentRestoreProjection::CreateLambda([&](const FHSREquipmentRestoreMap&){++ProjectionCalls;return true;}));
	auto Reject=[&](const TCHAR* Label,FHSRSaveData Bad){TestEqual(Label,Save->LoadSnapshot(Bad),EHSRSaveResult::InvalidData);TestTrue(TEXT("complete Current preserved"),Canonical(Save->GetSnapshot())==BaselineBytes);FHSRSaveData RuntimeAfter;TestEqual(TEXT("runtime recapture succeeds"),Save->SaveSnapshot(RuntimeAfter),EHSRSaveResult::Success);TestTrue(TEXT("complete runtime preserved"),Canonical(RuntimeAfter)==BaselineBytes);TestEqual(TEXT("no restore tx"),Save->GetRestoreTransactionRevisionForDevelopmentTest(),Tx);TestEqual(TEXT("no restore delegate"),Restores,0);TestEqual(TEXT("no profile delegate"),ProfileEvents,0);TestEqual(TEXT("no party delegate"),PartyEvents,0);TestEqual(TEXT("no equipment delegate"),EquipmentEvents,0);TestEqual(TEXT("no inventory delegate"),InventoryEvents,0);TestEqual(TEXT("no reward delegate"),RewardEvents,0);TestEqual(TEXT("no quest delegate"),QuestEvents,0);TestEqual(TEXT("no map delegate"),MapEvents,0);TestEqual(TEXT("no equipment projection"),ProjectionCalls,0);};
	FHSRSaveData MissingProfile=Baseline;MissingProfile.Profiles[0].State.CharacterId=TEXT("missing.profile");Reject(TEXT("missing profile definition"),MissingProfile);
	FHSRSaveData MissingParty=Baseline;MissingParty.PartySlots[0].CharacterId=TEXT("missing.party");Reject(TEXT("missing party ownership"),MissingParty);
	FHSRSaveData MissingEquipment=Baseline;FHSREquipmentSaveDto E;E.DefinitionId=TEXT("missing.equipment");E.CharacterId=HSRCharacterGuidFromProfileName(TEXT("character.a"));E.InstanceId=FGuid(1,2,3,4);MissingEquipment.Equipment.Add(E);Reject(TEXT("missing equipment definition"),MissingEquipment);
	FHSRSaveData MissingInventory=Baseline;FHSRItemStackSnapshot S;S.ItemId=TEXT("missing.item");S.Quantity=1;MissingInventory.Inventory.Stacks.Add(S);Reject(TEXT("missing inventory definition"),MissingInventory);
	FHSRSaveData MissingReward=Baseline;FHSRRewardReceipt R;R.Request.ClaimId=FGuid(5,6,7,8);R.Request.RewardDefinitionId=TEXT("missing.reward");MissingReward.Rewards.Receipts.Add(R);Reject(TEXT("missing reward definition"),MissingReward);
	FHSRSaveData MissingQuest=Baseline;FHSRQuestRuntimeState Q;Q.QuestId=TEXT("missing.quest");MissingQuest.Quests.States.Add(Q);Reject(TEXT("missing quest definition"),MissingQuest);
	FHSRSaveData MissingMap=Baseline;MissingMap.Map.CurrentLocation.MapId=TEXT("missing.map");Reject(TEXT("missing map definition"),MissingMap);
	FHSRSaveData MissingRegion=Baseline;MissingRegion.Map.UnlockedRegionIds.Add(TEXT("missing.region"));Reject(TEXT("missing region definition"),MissingRegion);
	FHSRSaveData MissingTeleport=Baseline;MissingTeleport.Map.UnlockedTeleportIds.Add(TEXT("missing.teleport"));Reject(TEXT("missing teleport definition"),MissingTeleport);
	auto* EquipmentDef=NewObject<UHSREquipmentDefinition>();EquipmentDef->DefinitionId=TEXT("equipment.valid");EquipmentDef->Slot=EHSREquipmentSlot::Weapon;Equipment->RegisterDefinition(*EquipmentDef);auto* UniqueDef=NewObject<UHSRItemDefinition>();UniqueDef->ItemId=TEXT("item.unique.valid");UniqueDef->StorageKind=EHSRItemStorageKind::Unique;UniqueDef->MaxStack=1;Inventory->RegisterDefinition(*UniqueDef);
	FHSRItemEquipmentMappingEntry ValidMapping;ValidMapping.ItemId=UniqueDef->ItemId;ValidMapping.EquipmentDefinitionId=EquipmentDef->DefinitionId;ValidMapping.Kind=EHSREquipmentKind::Equipment;ValidMapping.Slot=static_cast<int32>(EHSREquipmentSlot::Weapon);TestTrue(TEXT("valid save mapping registered"),Mapping->AddMapping(ValidMapping));
	FHSRSaveData DuplicateOwnership=Baseline;const FGuid SharedId(9,8,7,6);FHSREquipmentSaveDto OwnedEquipment;OwnedEquipment.DefinitionId=EquipmentDef->DefinitionId;OwnedEquipment.InstanceId=SharedId;OwnedEquipment.CharacterId=HSRCharacterGuidFromProfileName(TEXT("character.a"));OwnedEquipment.Kind=static_cast<int32>(EHSREquipmentKind::Equipment);OwnedEquipment.Slot=static_cast<int32>(EHSREquipmentSlot::Weapon);DuplicateOwnership.Equipment.Add(OwnedEquipment);FHSRItemInstance OwnedInventory;OwnedInventory.InstanceId=SharedId;OwnedInventory.DefinitionId=UniqueDef->ItemId;DuplicateOwnership.Inventory.UniqueItems.Add(OwnedInventory);Reject(TEXT("equipment inventory duplicate ownership"),DuplicateOwnership);
	FHSRSaveData BaggedEquipment=Baseline;
	FHSREquipmentRegistryDto RegistryPayload;
	RegistryPayload.DefinitionId=EquipmentDef->DefinitionId;RegistryPayload.InstanceId=SharedId;
	RegistryPayload.Kind=static_cast<int32>(EHSREquipmentKind::Equipment);
	BaggedEquipment.EquipmentRegistry.Add(RegistryPayload);
	BaggedEquipment.Inventory.UniqueItems.Add(OwnedInventory);
	FHSRSaveData MismatchedBaggedEquipment=BaggedEquipment;
	MismatchedBaggedEquipment.Inventory.UniqueItems[0].DefinitionId=TEXT("item.unique.other");
	auto* OtherUniqueDef=NewObject<UHSRItemDefinition>();OtherUniqueDef->ItemId=TEXT("item.unique.other");OtherUniqueDef->StorageKind=EHSRItemStorageKind::Unique;OtherUniqueDef->MaxStack=1;Inventory->RegisterDefinition(*OtherUniqueDef);
	Reject(TEXT("schema7 bag membership must match Registry mapping"),MismatchedBaggedEquipment);
	TestEqual(TEXT("schema7 bag membership may reference Registry payload"),Save->LoadSnapshot(BaggedEquipment),EHSRSaveResult::Success);
	FHSRSaveData BaggedRoundTrip;
	TestEqual(TEXT("schema7 bagged equipment recaptures"),Save->SaveSnapshot(BaggedRoundTrip),EHSRSaveResult::Success);
	TestEqual(TEXT("schema7 bagged Registry payload retained"),BaggedRoundTrip.EquipmentRegistry.Num(),1);
	TestEqual(TEXT("schema7 bagged Inventory membership retained"),BaggedRoundTrip.Inventory.UniqueItems.Num(),1);
	if(BaggedRoundTrip.EquipmentRegistry.Num()==1&&BaggedRoundTrip.Inventory.UniqueItems.Num()==1)
	{
		TestEqual(TEXT("schema7 Registry and membership share InstanceId"),BaggedRoundTrip.EquipmentRegistry[0].InstanceId,BaggedRoundTrip.Inventory.UniqueItems[0].InstanceId);
	}
	return true;
}
#endif
