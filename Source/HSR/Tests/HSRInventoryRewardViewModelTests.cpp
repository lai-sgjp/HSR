#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../UI/HSRInventoryRewardViewModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRInventoryRewardViewModelTest, "HSR.UI.InventoryReward.Lifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRInventoryRewardViewModelTest::RunTest(const FString&)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHSRInventorySubsystem* Inventory = NewObject<UHSRInventorySubsystem>(GameInstance);
	UHSRRewardSubsystem* Reward = NewObject<UHSRRewardSubsystem>(GameInstance);
	Reward->InitializeForAutomation(Inventory);
	UHSRItemDefinition* Item = NewObject<UHSRItemDefinition>();
	Item->ItemId = TEXT("Item.UI.Stack");
	Item->StorageKind = EHSRItemStorageKind::Stackable;
	Item->MaxStack = 99;
	TestEqual(TEXT("register item"), Inventory->RegisterDefinition(*Item), EHSRInventoryOperationResult::Success);
	UHSRRewardDefinition* Definition = NewObject<UHSRRewardDefinition>();
	Definition->RewardDefinitionId = TEXT("Reward.UI");
	Definition->FixedItems.Add({Item->ItemId, 2});
	TestEqual(TEXT("register reward"), Reward->RegisterRewardDefinition(*Definition), EHSRRewardOperationResult::Success);

	UHSRInventoryRewardViewModel* ViewModel = NewObject<UHSRInventoryRewardViewModel>();
	ViewModel->Initialize(Inventory, Reward);
	FHSRInventoryRewardSnapshot Snapshot;
	TestTrue(TEXT("initial snapshot available"), ViewModel->GetSnapshot(Snapshot));
	TestTrue(TEXT("initial inventory empty"), Snapshot.Inventory.Stacks.IsEmpty());
	int32 Broadcasts = 0;
	ViewModel->OnChanged().AddLambda([&](const FHSRInventoryRewardSnapshot&) { ++Broadcasts; });
	FHSRRewardReceipt Receipt;
	TestEqual(TEXT("reward commit"), Reward->SubmitReward({FGuid(9, 8, 7, 6), Definition->RewardDefinitionId, 1}, Receipt), EHSRRewardOperationResult::Success);
	TestEqual(TEXT("ledger-before-inventory coalesces UI refresh"), Broadcasts, 1);
	TestTrue(TEXT("updated snapshot available"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("stack visible"), Snapshot.Inventory.Stacks[0].Quantity, 2);
	TestEqual(TEXT("receipt visible"), Snapshot.Receipts.Num(), 1);
	FHSRInventorySaveData InventorySave;
	Inventory->ExportSaveData(InventorySave);
	InventorySave.Stacks[0].Quantity = 3;
	FHSRInventoryRestoreState InventoryCandidate;
	TestTrue(TEXT("same-revision inventory candidate valid"), Inventory->PrepareRestore(InventorySave, InventoryCandidate));
	Inventory->CommitRestore(MoveTemp(InventoryCandidate), true);
	TestEqual(TEXT("same-revision inventory content refreshes"), Broadcasts, 2);
	TestTrue(TEXT("same-revision inventory snapshot available"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("same-revision quantity visible"), Snapshot.Inventory.Stacks[0].Quantity, 3);
	FHSRRewardSaveData RewardSave;
	Reward->ExportSaveData(RewardSave);
	RewardSave.Receipts[0].Grants[0].Quantity = 3;
	FHSRRewardRestoreState RewardCandidate;
	TestTrue(TEXT("same-revision reward candidate valid"), Reward->PrepareRestore(RewardSave, RewardCandidate));
	Reward->CommitRestore(MoveTemp(RewardCandidate), true);
	TestEqual(TEXT("same-revision frozen grant refreshes"), Broadcasts, 3);
	TestTrue(TEXT("same-revision reward snapshot available"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("same-revision frozen quantity visible"), Snapshot.Receipts[0].Grants[0].Quantity, 3);
	ViewModel->Shutdown();
	TestEqual(TEXT("post-shutdown inventory mutation"), Inventory->AddStack(Item->ItemId, 1), EHSRInventoryOperationResult::Success);
	TestEqual(TEXT("shutdown unbound"), Broadcasts, 3);
	ViewModel->Initialize(Inventory, Reward);
	TestTrue(TEXT("rebind snapshot available"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("rebind sees latest quantity"), Snapshot.Inventory.Stacks[0].Quantity, 4);
	return true;
}

#endif
