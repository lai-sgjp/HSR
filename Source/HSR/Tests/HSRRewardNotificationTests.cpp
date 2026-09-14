#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../UI/HSRInventoryRewardWidget.h"
#include "../UI/HSRInventoryRewardViewModel.h"
#include "../Data/Definitions/HSRItemDefinition.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRRewardNotificationHistoryTest, "HSR.UI.RewardNotification.HistoryAndDuplicateReceipts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRRewardNotificationHistoryTest::RunTest(const FString&)
{
	UClass* WidgetClass = LoadClass<UHSRRewardSummaryWidget>(nullptr,
		TEXT("/Game/UI/WBP_RewardSummary_P13.WBP_RewardSummary_P13_C"));
	if (!TestNotNull(TEXT("real authored reward widget loads"), WidgetClass)) return false;
	UHSRRewardSummaryWidget* Widget = NewObject<UHSRRewardSummaryWidget>(GetTransientPackage(), WidgetClass);
	Widget->Initialize();
	Widget->TakeWidget();
	UHSRInventoryRewardViewModel* ViewModel = NewObject<UHSRInventoryRewardViewModel>();
	Widget->SetViewModel(ViewModel);
	Widget->AttachForAutomation();
	Widget->AttachForAutomation();

	UHSRItemDefinition* Item = NewObject<UHSRItemDefinition>();
	Item->ItemId = TEXT("Test.RewardToast.InternalItem");
	Item->DisplayName = FText::FromString(TEXT("回响碎片"));
	FHSRInventoryRewardSnapshot Snapshot;
	FHSRRewardReceipt Historic;
	Historic.Request.ClaimId = FGuid(1, 2, 3, 4);
	Historic.Grants.Add({Item->ItemId, 10, {}});
	Snapshot.Receipts.Add(Historic);
	ViewModel->OnChanged().Broadcast(Snapshot);
	TestTrue(TEXT("first binding does not replay history"), Widget->GetRewardNotificationText().IsEmpty());

	FHSRRewardReceipt Fresh = Historic;
	Fresh.Request.ClaimId = FGuid(5, 6, 7, 8);
	Fresh.Grants[0].Quantity = 3;
	Snapshot.Receipts.Add(Fresh);
	ViewModel->OnChanged().Broadcast(Snapshot);
	FString Message = Widget->GetRewardNotificationText().ToString();
	TestTrue(TEXT("new grant displays definition name and quantity"), Message.Contains(TEXT("回响碎片 × 3")));
	TestFalse(TEXT("notification never displays internal ID"), Message.Contains(TEXT("InternalItem")));
	ViewModel->OnChanged().Broadcast(Snapshot);
	TestEqual(TEXT("duplicate snapshot neither doubles nor replaces quantity"), Widget->GetRewardNotificationText().ToString(), Message);
	Widget->ExpireForAutomation();
	TestTrue(TEXT("notification expires"), Widget->GetRewardNotificationText().IsEmpty());
	ViewModel->OnChanged().Broadcast(Snapshot);
	TestTrue(TEXT("duplicate receipt cannot reopen expired notification"), Widget->GetRewardNotificationText().IsEmpty());
	FHSRRewardReceipt QueuedA = Fresh, QueuedB = Fresh;
	QueuedA.Request.ClaimId = FGuid(13, 14, 15, 16);
	QueuedB.Request.ClaimId = FGuid(17, 18, 19, 20);
	QueuedA.Grants[0].Quantity = 4;
	QueuedB.Grants[0].Quantity = 7;
	Snapshot.Receipts.Append({QueuedA, QueuedB});
	ViewModel->OnChanged().Broadcast(Snapshot);
	TestTrue(TEXT("first queued receipt displays alone"), Widget->GetRewardNotificationText().ToString().Contains(TEXT("× 4")));
	Widget->ExpireForAutomation();
	TestTrue(TEXT("next receipt displays after expiration"), Widget->GetRewardNotificationText().ToString().Contains(TEXT("× 7")));
	Widget->ExpireForAutomation();
	TestTrue(TEXT("queue drains"), Widget->GetRewardNotificationText().IsEmpty());

	Widget->SetViewModel(nullptr);
	Fresh.Request.ClaimId = FGuid(9, 10, 11, 12);
	Snapshot.Receipts.Add(Fresh);
	ViewModel->OnChanged().Broadcast(Snapshot);
	TestTrue(TEXT("detached widget no longer receives notifications"), Widget->GetRewardNotificationText().IsEmpty());
	return true;
}
#endif
