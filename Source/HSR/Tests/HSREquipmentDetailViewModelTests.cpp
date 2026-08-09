#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../UI/HSREquipmentDetailViewModel.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Data/Definitions/HSREquipmentDefinition.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../Inventory/HSRInventorySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSREquipmentDetailVMTest,"HSR.UI.EquipmentDetail.ViewModel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHSREquipmentDetailVMTest::RunTest(const FString&)
{
	UGameInstance* GI=NewObject<UGameInstance>();UHSREquipmentSubsystem* E=NewObject<UHSREquipmentSubsystem>(GI);UHSREquipmentDefinition* D=NewObject<UHSREquipmentDefinition>();D->DefinitionId=TEXT("Weapon.A");D->Slot=EHSREquipmentSlot::Weapon;D->EnhancementCap=5;TestEqual(TEXT("register"),E->RegisterDefinition(*D),EHSREquipmentOperationResult::Success);UHSRRelicDefinition* HeadDef=NewObject<UHSRRelicDefinition>();HeadDef->DefinitionId=TEXT("Relic.Head");HeadDef->SetId=TEXT("Set.A");HeadDef->Slot=EHSRRelicSlot::Head;UHSRRelicDefinition* HandsDef=NewObject<UHSRRelicDefinition>();HandsDef->DefinitionId=TEXT("Relic.Hands");HandsDef->SetId=TEXT("Set.A");HandsDef->Slot=EHSRRelicSlot::Hands;TestEqual(TEXT("register head relic"),E->RegisterDefinition(*HeadDef),EHSREquipmentOperationResult::Success);TestEqual(TEXT("register hands relic"),E->RegisterDefinition(*HandsDef),EHSREquipmentOperationResult::Success);
	const FGuid Character(0,7,0,1);UHSREquipmentDetailViewModel* VM=NewObject<UHSREquipmentDetailViewModel>();int32 Events=0;VM->OnChanged().AddLambda([&](const FHSREquipmentDetailSnapshot&){++Events;});VM->Initialize(E,Character);
	FHSREquipmentDetailSnapshot Empty;TestTrue(TEXT("empty snapshot"),VM->GetSnapshot(Empty));TestEqual(TEXT("empty state"),Empty.FailureReason,EHSREquipmentDetailResult::Empty);UHSREquipmentDetailViewModel* Invalid=NewObject<UHSREquipmentDetailViewModel>();Invalid->Initialize(nullptr,FGuid());FHSREquipmentDetailSnapshot InvalidSnapshot;TestTrue(TEXT("invalid snapshot"),Invalid->GetSnapshot(InvalidSnapshot));TestEqual(TEXT("not initialized"),InvalidSnapshot.FailureReason,EHSREquipmentDetailResult::NotInitialized);
	FHSREquipmentInstance I;I.InstanceId=FGuid(4,3,2,1);I.DefinitionId=D->DefinitionId;I.Kind=EHSREquipmentKind::Equipment;I.Modifiers.Add({EHSREquipmentStat::Attack,10.0f});TestEqual(TEXT("equip"),E->Equip(Character,I),EHSREquipmentOperationResult::Success);
	FHSREquipmentDetailSnapshot S;TestTrue(TEXT("snapshot"),VM->GetSnapshot(S));TestTrue(TEXT("valid"),S.bIsValid);TestEqual(TEXT("item"),S.Items.Num(),1);TestEqual(TEXT("source"),S.Sources.Num(),1);TestEqual(TEXT("attack"),S.Attack,10.0f);TestEqual(TEXT("zero set rows"),S.RelicSets.Num(),0);
	FHSREquipmentInstance Head;Head.InstanceId=FGuid(10,0,0,1);Head.DefinitionId=HeadDef->DefinitionId;Head.Kind=EHSREquipmentKind::Relic;TestEqual(TEXT("equip first relic"),E->Equip(Character,Head),EHSREquipmentOperationResult::Success);VM->GetSnapshot(S);TestEqual(TEXT("one set row"),S.RelicSets.Num(),1);TestEqual(TEXT("one relic count"),S.RelicSets[0].EquippedCount,1);TestFalse(TEXT("set inactive at one"),S.RelicSets[0].bActive);TestTrue(TEXT("inactive set source none"),S.RelicSets[0].SetSourceId.IsNone());
	FHSREquipmentInstance Hands;Hands.InstanceId=FGuid(10,0,0,2);Hands.DefinitionId=HandsDef->DefinitionId;Hands.Kind=EHSREquipmentKind::Relic;TestEqual(TEXT("equip second relic"),E->Equip(Character,Hands),EHSREquipmentOperationResult::Success);VM->GetSnapshot(S);TestEqual(TEXT("two relic count"),S.RelicSets[0].EquippedCount,2);TestTrue(TEXT("set active at two"),S.RelicSets[0].bActive);TestEqual(TEXT("stable set source"),S.RelicSets[0].SetSourceId,FName(TEXT("Set.A")));TestEqual(TEXT("modifier plus set source"),S.Sources.Num(),2);
	TestEqual(TEXT("unequip second relic"),E->Unequip(Character,EHSREquipmentKind::Relic,(int32)EHSRRelicSlot::Hands,Hands.InstanceId),EHSREquipmentOperationResult::Success);VM->GetSnapshot(S);TestEqual(TEXT("back to one relic"),S.RelicSets[0].EquippedCount,1);TestFalse(TEXT("set inactive again"),S.RelicSets[0].bActive);TestEqual(TEXT("set breakdown removed"),S.Sources.Num(),1);
	FHSREquipmentRestoreMap Restore;FHSREquipmentRestoreState& Restored=Restore.Add(Character);Restored.Revision=20;Restored.Loadout.Equipment.Add(EHSREquipmentSlot::Weapon,I);Restored.Loadout.Relics.Add(EHSRRelicSlot::Head,Head);Restored.Loadout.Relics.Add(EHSRRelicSlot::Hands,Hands);Restored.RelicSetCounts.Add(TEXT("Set.A"),2);const int32 BeforeRestore=Events;E->CommitRestore(Restore);E->NotifyRestored({Character});VM->GetSnapshot(S);TestTrue(TEXT("restore notification emitted"),Events>BeforeRestore);TestTrue(TEXT("restored set active"),S.RelicSets[0].bActive);TestEqual(TEXT("restored revision"),S.Revision,20);
	const int32 Before=Events;VM->Shutdown();E->SetEnhancementLevel(Character,I.InstanceId,1);TestEqual(TEXT("unbound"),Events,Before);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSREquipmentDetailMovementRefreshTest,
	"HSR.UI.EquipmentDetail.MovementRefresh",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHSREquipmentDetailMovementRefreshTest::RunTest(const FString&)
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UHSRInventorySubsystem* Inventory = NewObject<UHSRInventorySubsystem>(GI);
	UHSREquipmentSubsystem* Equipment = NewObject<UHSREquipmentSubsystem>(GI);
	UHSRItemEquipmentMappingCatalog* Catalog = NewObject<UHSRItemEquipmentMappingCatalog>();
	UHSRItemDefinition* Item = NewObject<UHSRItemDefinition>();
	Item->ItemId = TEXT("Item.Weapon.ViewModel"); Item->StorageKind = EHSRItemStorageKind::Unique; Item->MaxStack = 1;
	TestEqual(TEXT("movement refresh Inventory definition"), Inventory->RegisterDefinition(*Item), EHSRInventoryOperationResult::Success);
	UHSREquipmentDefinition* Definition = NewObject<UHSREquipmentDefinition>();
	Definition->DefinitionId = TEXT("Equipment.Weapon.ViewModel"); Definition->Slot = EHSREquipmentSlot::Weapon; Definition->EnhancementCap = 5;
	TestEqual(TEXT("movement refresh Equipment definition"), Equipment->RegisterDefinition(*Definition), EHSREquipmentOperationResult::Success);
	FHSRItemEquipmentMappingEntry Mapping;
	Mapping.ItemId = Item->ItemId; Mapping.EquipmentDefinitionId = Definition->DefinitionId;
	Mapping.Kind = EHSREquipmentKind::Equipment; Mapping.Slot = static_cast<int32>(EHSREquipmentSlot::Weapon);
	TestTrue(TEXT("movement refresh mapping"), Catalog->AddMapping(Mapping));
	const FGuid CharacterId = FGuid::NewGuid();
	const FGuid InstanceId = FGuid::NewGuid();
	TestEqual(TEXT("movement refresh membership"), Inventory->AddUnique({InstanceId, Item->ItemId}), EHSRInventoryOperationResult::Success);
	FHSREquipmentInstance Payload;
	Payload.InstanceId = InstanceId; Payload.DefinitionId = Definition->DefinitionId; Payload.Kind = EHSREquipmentKind::Equipment;
	Payload.Modifiers.Add({EHSREquipmentStat::Attack, 9.0f});
	TestEqual(TEXT("movement refresh Registry payload"), Equipment->RegisterInstance(Payload), EHSREquipmentOperationResult::Success);
	UHSREquipmentDetailViewModel* ViewModel = NewObject<UHSREquipmentDetailViewModel>();
	int32 Refreshes = 0;
	ViewModel->OnChanged().AddLambda([&Refreshes](const FHSREquipmentDetailSnapshot&) { ++Refreshes; });
	ViewModel->Initialize(Equipment, CharacterId);
	const int32 BeforeMovement = Refreshes;
	FHSRInventorySnapshot InventorySnapshot;
	Inventory->GetSnapshot(InventorySnapshot);
	FHSREquipmentMovementRequest EquipRequest;
	EquipRequest.OperationId = FGuid::NewGuid(); EquipRequest.CharacterId = CharacterId; EquipRequest.InstanceId = InstanceId;
	EquipRequest.Intent = EHSREquipmentMovementIntent::Equip; EquipRequest.Kind = EHSREquipmentKind::Equipment;
	EquipRequest.Slot = static_cast<int32>(EHSREquipmentSlot::Weapon);
	EquipRequest.ExpectedInventoryRevision = InventorySnapshot.Revision; EquipRequest.ExpectedEquipmentRevision = 0;
	const FHSREquipmentMovementResult Equipped = Equipment->ExecuteMovement(EquipRequest, *Inventory, *Catalog);
	TestEqual(TEXT("movement refresh equip committed"), Equipped.Code, EHSREquipmentMovementResultCode::Success);
	TestEqual(TEXT("movement refresh emitted once"), Refreshes, BeforeMovement + 1);
	FHSREquipmentDetailSnapshot Detail;
	TestTrue(TEXT("movement refresh snapshot available"), ViewModel->GetSnapshot(Detail));
	TestEqual(TEXT("movement refresh snapshot revision"), Detail.Revision, Equipped.NewEquipmentRevision);
	TestEqual(TEXT("movement refresh snapshot item"), Detail.Items.Num(), 1);
	TestEqual(TEXT("movement refresh snapshot aggregate"), Detail.Attack, 9.0f);
	const FHSREquipmentMovementResult Replay = Equipment->ExecuteMovement(EquipRequest, *Inventory, *Catalog);
	TestTrue(TEXT("movement refresh replay identified"), Replay.bReplay);
	TestEqual(TEXT("movement refresh replay emits nothing"), Refreshes, BeforeMovement + 1);
	FHSREquipmentMovementRequest UnequipRequest = EquipRequest;
	UnequipRequest.OperationId = FGuid::NewGuid(); UnequipRequest.Intent = EHSREquipmentMovementIntent::Unequip;
	UnequipRequest.ExpectedInventoryRevision = Equipped.NewInventoryRevision;
	UnequipRequest.ExpectedEquipmentRevision = Equipped.NewEquipmentRevision;
	const FHSREquipmentMovementResult Unequipped = Equipment->ExecuteMovement(UnequipRequest, *Inventory, *Catalog);
	TestEqual(TEXT("movement refresh unequip committed"), Unequipped.Code, EHSREquipmentMovementResultCode::Success);
	TestEqual(TEXT("movement refresh unequip emitted once"), Refreshes, BeforeMovement + 2);
	TestTrue(TEXT("movement refresh empty snapshot available"), ViewModel->GetSnapshot(Detail));
	TestEqual(TEXT("movement refresh empty state"), Detail.FailureReason, EHSREquipmentDetailResult::Success);
	TestEqual(TEXT("movement refresh empty item list"), Detail.Items.Num(), 0);
	TestEqual(TEXT("movement refresh empty revision"), Detail.Revision, Unequipped.NewEquipmentRevision);
	return true;
}
#endif
