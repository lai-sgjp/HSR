#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Curves/CurveFloat.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/Definitions/HSRDropTableDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../Save/HSRSaveSubsystem.h"

namespace HSR::P13::SaveTests
{
	struct FFixture
	{
		UGameInstance* GameInstance = nullptr;
		UHSRCharacterProfileSubsystem* Profiles = nullptr;
		UHSRPartySubsystem* Party = nullptr;
		UHSREquipmentSubsystem* Equipment = nullptr;
		UHSRInventorySubsystem* Inventory = nullptr;
		UHSRRewardSubsystem* Reward = nullptr;
		UHSRSaveSubsystem* Save = nullptr;
	};

	static FFixture MakeFixture(FAutomationTestBase& Test)
	{
		FFixture F;
		F.GameInstance = NewObject<UGameInstance>();
		F.Profiles = NewObject<UHSRCharacterProfileSubsystem>(F.GameInstance);
		F.Party = NewObject<UHSRPartySubsystem>(F.GameInstance);
		F.Equipment = NewObject<UHSREquipmentSubsystem>(F.GameInstance);
		F.Inventory = NewObject<UHSRInventorySubsystem>(F.GameInstance);
		F.Reward = NewObject<UHSRRewardSubsystem>(F.GameInstance);
		F.Save = NewObject<UHSRSaveSubsystem>(F.GameInstance);

		UHSRCharacterDefinition* Character = NewObject<UHSRCharacterDefinition>();
		Character->CharacterId = TEXT("Character.A");
		Character->MaxLevel = 2;
		UCurveFloat* Curve = NewObject<UCurveFloat>(Character);
		Curve->FloatCurve.AddKey(2, 100);
		Character->CumulativeExperienceCurve = Curve;
		F.Profiles->RegisterDefinition(Character);
		F.Party->InitializeForDevelopmentTest(F.Profiles);
		F.Party->AddCharacter(Character->CharacterId);

		UHSRItemDefinition* Stack = NewObject<UHSRItemDefinition>();
		Stack->ItemId = TEXT("Item.Material.LumenShard");
		Stack->StorageKind = EHSRItemStorageKind::Stackable;
		Stack->MaxStack = 99;
		Test.TestEqual(TEXT("register stack"), F.Inventory->RegisterDefinition(*Stack), EHSRInventoryOperationResult::Success);
		UHSRItemDefinition* Unique = NewObject<UHSRItemDefinition>();
		Unique->ItemId = TEXT("Item.Unique.ArchiveToken");
		Unique->StorageKind = EHSRItemStorageKind::Unique;
		Unique->MaxStack = 1;
		Test.TestEqual(TEXT("register unique"), F.Inventory->RegisterDefinition(*Unique), EHSRInventoryOperationResult::Success);
		UHSRRelicDefinition* Relic = NewObject<UHSRRelicDefinition>();
		Relic->DefinitionId = TEXT("Relic.P13.Save");
		Relic->SetId = TEXT("Set.P13.Save");
		Relic->Slot = EHSRRelicSlot::Head;
		Relic->EnhancementCap = 15;
		Test.TestEqual(TEXT("register relic"), F.Equipment->RegisterDefinition(*Relic), EHSREquipmentOperationResult::Success);

		UHSRDropTableDefinition* Drop = NewObject<UHSRDropTableDefinition>();
		Drop->DropTableId = TEXT("Drop.P13.Save");
		Drop->Entries.Add({Unique->ItemId, 1, 1, 1});
		Test.TestEqual(TEXT("register drop"), F.Reward->RegisterDropTable(*Drop), EHSRRewardOperationResult::Success);
		UHSRRewardDefinition* RewardDefinition = NewObject<UHSRRewardDefinition>();
		RewardDefinition->RewardDefinitionId = TEXT("Reward.P13.Save");
		RewardDefinition->FixedItems.Add({Stack->ItemId, 2});
		RewardDefinition->DropTableId = Drop->DropTableId;
		RewardDefinition->DropRolls = 1;
		F.Reward->InitializeForAutomation(F.Inventory);
		Test.TestEqual(TEXT("register reward"), F.Reward->RegisterRewardDefinition(*RewardDefinition), EHSRRewardOperationResult::Success);
		F.Save->InitializeForDevelopmentTest(F.Profiles, F.Party, F.Equipment, F.Inventory, F.Reward);
		return F;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRInventoryRewardSaveV3Test, "HSR.Save.InventoryRewardV3", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRInventoryRewardSaveV3Test::RunTest(const FString&)
{
	using namespace HSR::P13::SaveTests;
	FFixture Source = MakeFixture(*this);
	const FGuid EquipmentInstanceId(13, 4, 9, 9);
	FHSREquipmentInstance Equipped;
	Equipped.InstanceId = EquipmentInstanceId;
	Equipped.DefinitionId = TEXT("Relic.P13.Save");
	Equipped.Kind = EHSREquipmentKind::Relic;
	TestEqual(TEXT("equip source relic"), Source.Equipment->Equip(HSRCharacterGuidFromProfileName(TEXT("Character.A")), Equipped), EHSREquipmentOperationResult::Success);
	FHSRRewardReceipt Receipt;
	const FGuid Claim(13, 4, 1, 1);
	TestEqual(TEXT("claim source reward"), Source.Reward->SubmitReward({Claim, TEXT("Reward.P13.Save"), 777}, Receipt), EHSRRewardOperationResult::Success);
	FHSRSaveData Captured;
	TestEqual(TEXT("capture v3"), Source.Save->SaveSnapshot(Captured), EHSRSaveResult::Success);
	TestEqual(TEXT("schema v7"), Captured.SchemaVersion, 7);
	TestEqual(TEXT("saved stack"), Captured.Inventory.Stacks.Num(), 1);
	TestEqual(TEXT("saved unique"), Captured.Inventory.UniqueItems.Num(), 1);
	TestEqual(TEXT("saved receipt"), Captured.Rewards.Receipts.Num(), 1);

	FFixture Target = MakeFixture(*this);
	int32 InventoryEvents = 0, RewardEvents = 0, RestoreEvents = 0;
	Target.Inventory->OnInventoryChanged().AddLambda([&](int64) { ++InventoryEvents; });
	Target.Reward->OnRewardRestored().AddLambda([&](int64) { ++RewardEvents; });
	Target.Save->OnRestoreCommitted().AddLambda([&](const FHSRRestoreCommitInfo&) { ++RestoreEvents; });
	TestEqual(TEXT("load v3"), Target.Save->LoadSnapshot(Captured), EHSRSaveResult::Success);
	FHSRInventorySnapshot Snapshot;
	Target.Inventory->GetSnapshot(Snapshot);
	TestEqual(TEXT("restored stack quantity"), Snapshot.Stacks[0].Quantity, 2);
	TestEqual(TEXT("restored unique id"), Snapshot.UniqueItems[0].InstanceId, Captured.Inventory.UniqueItems[0].InstanceId);
	FHSRRewardReceipt RestoredReceipt;
	TestTrue(TEXT("restored claim ledger"), Target.Reward->GetReceipt(Claim, RestoredReceipt));
	TestEqual(TEXT("one inventory restore event"), InventoryEvents, 1);
	TestEqual(TEXT("one reward restore event"), RewardEvents, 1);
	TestEqual(TEXT("one aggregate restore event"), RestoreEvents, 1);
	TestEqual(TEXT("repeat load succeeds"), Target.Save->LoadSnapshot(Captured), EHSRSaveResult::Success);
	TestEqual(TEXT("repeat inventory silent"), InventoryEvents, 1);
	TestEqual(TEXT("repeat reward silent"), RewardEvents, 1);
	TestEqual(TEXT("repeat aggregate silent"), RestoreEvents, 1);
	int32 ProjectionCalls = 0;
	Target.Equipment->SetRestoreProjection(FHSREquipmentRestoreProjection::CreateLambda([&](const FHSREquipmentRestoreMap&)
	{
		++ProjectionCalls;
		return true;
	}));

	FHSRSaveData Bad = Captured;
	const FHSRItemStackSnapshot DuplicateStack = Bad.Inventory.Stacks[0];
	Bad.Inventory.Stacks.Add(DuplicateStack);
	TestEqual(TEXT("duplicate stack rejected"), Target.Save->LoadSnapshot(Bad), EHSRSaveResult::InvalidData);
	TestEqual(TEXT("late invalid inventory does not project equipment"), ProjectionCalls, 0);
	Bad = Captured;
	const FHSRRewardReceipt DuplicateReceipt = Bad.Rewards.Receipts[0];
	Bad.Rewards.Receipts.Add(DuplicateReceipt);
	TestEqual(TEXT("duplicate claim rejected"), Target.Save->LoadSnapshot(Bad), EHSRSaveResult::InvalidData);
	TestEqual(TEXT("late invalid reward does not project equipment"), ProjectionCalls, 0);
	Bad = Captured;
	FHSRInventoryGrant DuplicateUniqueGrant = Bad.Rewards.Receipts[0].Grants[1];
	Bad.Rewards.Receipts[0].Grants.Add(DuplicateUniqueGrant);
	TestEqual(TEXT("ledger-global duplicate unique rejected"), Target.Save->LoadSnapshot(Bad), EHSRSaveResult::InvalidData);
	Bad = Captured;
	++Bad.Rewards.Revision;
	TestEqual(TEXT("non-contiguous receipt revision rejected"), Target.Save->LoadSnapshot(Bad), EHSRSaveResult::InvalidData);
	Bad = Captured;
	Bad.Inventory.UniqueItems[0].InstanceId = EquipmentInstanceId;
	TestEqual(TEXT("inventory equipment instance collision rejected"), Target.Save->LoadSnapshot(Bad), EHSRSaveResult::InvalidData);
	TestEqual(TEXT("bad candidates emit nothing"), RestoreEvents, 1);

	FHSRSaveData Legacy = Captured;
	Legacy.SchemaVersion = 2;
	Legacy.Inventory = FHSRInventorySaveData();
	Legacy.Rewards = FHSRRewardSaveData();
	TestEqual(TEXT("v2 compatibility load"), Target.Save->LoadSnapshot(Legacy), EHSRSaveResult::Success);
	TestEqual(TEXT("valid candidate projects equipment once"), ProjectionCalls, 1);
	Target.Inventory->GetSnapshot(Snapshot);
	TestTrue(TEXT("v2 migrates empty inventory"), Snapshot.Stacks.IsEmpty() && Snapshot.UniqueItems.IsEmpty());
	TestFalse(TEXT("v2 migrates empty ledger"), Target.Reward->GetReceipt(Claim, RestoredReceipt));
	return true;
}

#endif
