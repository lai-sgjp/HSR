#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
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
#include "../Save/HSRSaveVersion.h"
#include "../Map/HSRMapSubsystem.h"
#include "../Quest/HSRQuestSubsystem.h"
#include "../Data/Definitions/HSRMapDefinition.h"
#include "../Data/Definitions/HSRTeleportDefinition.h"
#include "../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../Data/Definitions/HSRRelicSetDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

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
		UHSRQuestSubsystem* Quest = nullptr;
		UHSRMapSubsystem* Map = nullptr;
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
		F.Quest = NewObject<UHSRQuestSubsystem>(F.GameInstance);
		F.Map = NewObject<UHSRMapSubsystem>(F.GameInstance);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRProductionSaveDefinitionsColdBootstrapTest,
	"HSR.Save.ProductionDefinitions.ColdBootstrap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRProductionSaveDefinitionsColdBootstrapTest::RunTest(const FString&)
{
	// 验证全新的 GameInstance 在任何奖励 Actor 注册前即可完成生产定义冷启动。
	if (!TestNotNull(TEXT("Engine is available"), GEngine))
	{
		return false;
	}

	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->AddToRoot();
	GameInstance->InitializeStandalone(FName(*FString::Printf(TEXT("HSRProductionDefinitions_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits))));

	UHSRInventorySubsystem* Inventory = GameInstance->GetSubsystem<UHSRInventorySubsystem>();
	UHSRRewardSubsystem* Reward = GameInstance->GetSubsystem<UHSRRewardSubsystem>();
	TestNotNull(TEXT("cold GameInstance creates Inventory"), Inventory);
	TestNotNull(TEXT("cold GameInstance creates Reward"), Reward);
	if (Inventory && Reward)
	{
		TestTrue(TEXT("cold Inventory knows LumenShard"),
			Inventory->HasDefinition(TEXT("Item.Material.LumenShard")));
		TestTrue(TEXT("cold Inventory knows ArchiveToken"),
			Inventory->HasDefinition(TEXT("Item.Unique.ArchiveToken")));
		TestTrue(TEXT("cold Reward knows Standard reward"),
			Reward->HasDefinition(TEXT("Reward.P13.Standard")));
		// Demo reward bundles (chest / encounter VictoryRewardDefinition) are the same
		// shape as P13: a reward definition plus its fixed relic items. A receipt stored in a
		// save must survive a cold boot, so these must be registered without any Actor
		// side effect, exactly like DemoItemBootstrap registers the item definitions.
		TestTrue(TEXT("cold Reward knows demo chest reward"),
			Reward->HasDefinition(TEXT("Demo.Reward.WangXiaYiTong")));
		TestTrue(TEXT("cold Reward knows demo boss reward"),
			Reward->HasDefinition(TEXT("Demo.Reward.Laigushi")));
		TestTrue(TEXT("cold Reward knows demo inspector reward"),
			Reward->HasDefinition(TEXT("Demo.Reward.SupportSectionInspector")));
	}

	UWorld* World = GameInstance->GetWorld();
	GameInstance->Shutdown();
	if (World)
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	GameInstance->RemoveFromRoot();
	return true;
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
	TestEqual(TEXT("schema is current"), Captured.SchemaVersion, HSRSaveVersion::CurrentSchema);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDemoRewardSaveDiskRoundtripTest,
	"HSR.Save.DemoReward.DiskRoundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDemoRewardSaveDiskRoundtripTest::RunTest(const FString&)
{
	// Regression for the "cannot load/save any save" report: a receipt whose reward definition
	// was only ever registered by a chest/encounter Actor made Validate reject the whole blob on
	// the next cold boot (the reward registry was empty before any Actor existed). The fix
	// registers demo reward definitions in HSRRewardSubsystem::Initialize, so a disk roundtrip
	// that stores a demo receipt must survive decode -> validate -> restore.
	using namespace HSR::P13::SaveTests;
	FFixture F = MakeFixture(*this);
	UHSRRewardDefinition* DemoReward = NewObject<UHSRRewardDefinition>();
	DemoReward->RewardDefinitionId = TEXT("Demo.Reward.WangXiaYiTong");
	UHSRItemDefinition* DemoRelic = NewObject<UHSRItemDefinition>();
	DemoRelic->ItemId = TEXT("Demo.Relic.HeavenLiveRoom.PlanarSphere");
	DemoRelic->StorageKind = EHSRItemStorageKind::Unique;
	DemoRelic->MaxStack = 1;
	TestEqual(TEXT("register demo relic item"), F.Inventory->RegisterDefinition(*DemoRelic), EHSRInventoryOperationResult::Success);
	DemoReward->FixedItems.Add({DemoRelic->ItemId, 1});
	F.Reward->InitializeForAutomation(F.Inventory);
	TestEqual(TEXT("register demo reward"), F.Reward->RegisterRewardDefinition(*DemoReward), EHSRRewardOperationResult::Success);
	FHSRRewardReceipt Receipt;
	const FGuid Claim(77, 1, 2, 3);
	TestEqual(TEXT("claim demo reward"), F.Reward->SubmitReward({Claim, DemoReward->RewardDefinitionId, 1}, Receipt), EHSRRewardOperationResult::Success);

	// Equip a relic so the blob carries an equipment placement. Character identity GUID is
	// hashed case-insensitively, so the placement authored against the live "Character.A"
	// profile must still match the profile row after a decode (which reconstructs the name).
	// MakeFixture already registered Relic.P13.Save.
	FHSREquipmentInstance Equipped;
	const FGuid EquipId(13, 4, 9, 9);
	Equipped.InstanceId = EquipId;
	Equipped.DefinitionId = TEXT("Relic.P13.Save");
	Equipped.Kind = EHSREquipmentKind::Relic;
	Equipped.EnhancementLevel = 0;
	TestEqual(TEXT("equip relic"), F.Equipment->Equip(HSRCharacterGuidFromProfileName(TEXT("Character.A")), Equipped), EHSREquipmentOperationResult::Success);

	const FString Slot = FString::Printf(TEXT("HSR_DemoReward_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	ON_SCOPE_EXIT { UGameplayStatics::DeleteGameInSlot(Slot, 0); };
	TestEqual(TEXT("disk save"), F.Save->SaveToSlot(Slot, 0), EHSRSaveResult::Success);

	// A fresh fixture is a second cold boot: only startup-registered definitions exist here.
	FFixture Reload = MakeFixture(*this);
	TestEqual(TEXT("reload register demo relic item"), Reload.Inventory->RegisterDefinition(*DemoRelic), EHSRInventoryOperationResult::Success);
	Reload.Reward->InitializeForAutomation(Reload.Inventory);
	TestEqual(TEXT("reload register demo reward"), Reload.Reward->RegisterRewardDefinition(*DemoReward), EHSRRewardOperationResult::Success);
	TestEqual(TEXT("disk load"), Reload.Save->LoadFromSlot(Slot, 0), EHSRSaveResult::Success);
	FHSRRewardReceipt Restored;
	TestTrue(TEXT("demo receipt restored"), Reload.Reward->GetReceipt(Claim, Restored));
	return true;
}

#endif
