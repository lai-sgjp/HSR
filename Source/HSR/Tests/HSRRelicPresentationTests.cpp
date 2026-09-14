#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../UI/Relic/HSRRelicEquipmentWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRRelicNativePresentationTest,
	"HSR.UI.RelicEquipment.NativePresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRRelicNativePresentationTest::RunTest(const FString&)
{
	UHSRRelicEquipmentWidget* Widget = NewObject<UHSRRelicEquipmentWidget>();
	Widget->WidgetTree = NewObject<UWidgetTree>(Widget);
	UVerticalBox* Root = Widget->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
	Widget->WidgetTree->RootWidget = Root;
	for (const TCHAR* Name : { TEXT("SlotListHost"), TEXT("CandidateListHost"), TEXT("EnhancementOptionsHost"), TEXT("ComparisonBodyHost") })
		Root->AddChild(Widget->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), Name));
	for (const TCHAR* Name : { TEXT("BTN_Equip"), TEXT("BTN_Unequip"), TEXT("BTN_Enhance"), TEXT("BTN_ConfirmEnhance") })
		Root->AddChild(Widget->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name));
	for (const TCHAR* Name : { TEXT("TXT_Status"), TEXT("TXT_ComparisonBody"), TEXT("TXT_EnhanceBody") })
		Root->AddChild(Widget->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name));
	const auto Enabled = [Widget](const TCHAR* Name) { return Widget->WidgetTree->FindWidget<UButton>(Name)->GetIsEnabled(); };
	FHSRRelicEquipmentSnapshot Snapshot;
	Snapshot.bIsValid = true;
	Snapshot.FailureReason = EHSRRelicEquipmentResult::Success;
	Snapshot.Stage = EHSRRelicEquipmentStage::CandidateSelection;
	Snapshot.Slots.AddDefaulted();
	Widget->PresentSnapshotForAutomation(Snapshot);
	TestFalse(TEXT("Empty slot cannot equip without a candidate"), Enabled(TEXT("BTN_Equip")));
	TestFalse(TEXT("Empty slot cannot unequip"), Enabled(TEXT("BTN_Unequip")));
	TestFalse(TEXT("Empty slot cannot enhance"), Enabled(TEXT("BTN_Enhance")));
	TArray<UWidget*> AllWidgets;
	Widget->WidgetTree->GetAllWidgets(AllWidgets);
	bool bLocalizedSlotFound = false;
	for (UWidget* Child : AllWidgets)
		if (const UTextBlock* Text = Cast<UTextBlock>(Child))
		{
			const FString Label = Text->GetText().ToString();
			bLocalizedSlotFound |= Label.Contains(TEXT("头部")) && Label.Contains(TEXT("未装备"));
			TestFalse(TEXT("No internal enum in native rows"), Label.Contains(TEXT("EHSR")));
		}
	TestTrue(TEXT("Native slot row renders localized empty slot"), bLocalizedSlotFound);
	Snapshot.Stage = EHSRRelicEquipmentStage::Comparison;
	Snapshot.SelectedCandidateId = FGuid::NewGuid();
	FHSRRelicCandidateRow& Candidate = Snapshot.Candidates.AddDefaulted_GetRef();
	Candidate.InstanceId = Snapshot.SelectedCandidateId;
	Snapshot.Comparison.bIsValid = true;
	Snapshot.Comparison.CandidateInstanceId = Snapshot.SelectedCandidateId;
	FHSRRelicStatDeltaRow& Delta = Snapshot.Comparison.StatDeltas.AddDefaulted_GetRef();
	Delta.Stat = EHSREquipmentStat::Attack;
	Delta.CurrentValue = 3.f;
	Delta.CandidateValue = 7.f;
	Delta.Delta = 4.f;
	Widget->PresentSnapshotForAutomation(Snapshot);
	TestTrue(TEXT("Valid comparison enables equip"), Enabled(TEXT("BTN_Equip")));
	const FString Comparison = Widget->WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_ComparisonBody"))->GetText().ToString();
	TestTrue(TEXT("Comparison localizes stat and uses actual delta"), Comparison.Contains(TEXT("攻击力")) && Comparison.Contains(TEXT("+4")));
	Snapshot.Comparison.CandidateInstanceId = FGuid::NewGuid();
	Widget->PresentSnapshotForAutomation(Snapshot);
	TestFalse(TEXT("Mismatched comparison cannot equip another item"), Enabled(TEXT("BTN_Equip")));
	Snapshot.Stage = EHSRRelicEquipmentStage::Enhancement;
	Snapshot.CurrentInstanceId = FGuid::NewGuid();
	Snapshot.EnhancementInstanceId = Snapshot.SelectedCandidateId;
	FHSRRelicEnhancementOption& Option = Snapshot.EnhancementOptions.AddDefaulted_GetRef();
	Option.TargetLevel = 1;
	Option.bAvailable = true;
	Option.bAffordable = false;
	Widget->PresentSnapshotForAutomation(Snapshot);
	Widget->SelectEnhancementLevel(1);
	TestFalse(TEXT("Insufficient materials disable confirmation"), Enabled(TEXT("BTN_ConfirmEnhance")));
	TestFalse(TEXT("Cannot unequip during enhancement preview"), Enabled(TEXT("BTN_Unequip")));
	Option.bAffordable = true;
	Widget->PresentSnapshotForAutomation(Snapshot);
	TestTrue(TEXT("Selected affordable level enables explicit confirmation"), Enabled(TEXT("BTN_ConfirmEnhance")));
	Option.bAvailable = false;
	Widget->PresentSnapshotForAutomation(Snapshot);
	TestFalse(TEXT("Unavailable level remains disabled even when affordable"), Enabled(TEXT("BTN_ConfirmEnhance")));
	return true;
}
#endif
