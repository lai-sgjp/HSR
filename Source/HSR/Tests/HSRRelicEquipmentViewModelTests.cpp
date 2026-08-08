#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../Data/Definitions/HSREquipmentEnhancementCatalog.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../UI/Relic/HSRRelicEquipmentViewModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSRRelicEquipmentViewModelFlowTest,
	"HSR.UI.RelicEquipment.ViewModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRRelicEquipmentViewModelFlowTest::RunTest(const FString&)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UHSRInventorySubsystem* Inventory = NewObject<UHSRInventorySubsystem>(GameInstance);
	UHSREquipmentSubsystem* Equipment = NewObject<UHSREquipmentSubsystem>(GameInstance);
	UHSRItemEquipmentMappingCatalog* MappingCatalog = NewObject<UHSRItemEquipmentMappingCatalog>(GameInstance);
	UHSREquipmentEnhancementCatalog* EnhancementCatalog = NewObject<UHSREquipmentEnhancementCatalog>(GameInstance);
	const FGuid CharacterId = FGuid::NewGuid();
	const FGuid CurrentInstanceId = FGuid::NewGuid();
	const FGuid CandidateInstanceId = FGuid::NewGuid();

	UHSRItemDefinition* Item = NewObject<UHSRItemDefinition>(GameInstance);
	Item->ItemId = TEXT("Item.Relic.Head.Test");
	Item->StorageKind = EHSRItemStorageKind::Unique;
	Item->MaxStack = 1;
	TestEqual(TEXT("ViewModel item definition"), Inventory->RegisterDefinition(*Item),
		EHSRInventoryOperationResult::Success);
	UHSRRelicDefinition* Relic = NewObject<UHSRRelicDefinition>(GameInstance);
	Relic->DefinitionId = TEXT("Relic.Head.ViewModel");
	Relic->SetId = TEXT("Set.ViewModel");
	Relic->Slot = EHSRRelicSlot::Head;
	Relic->EnhancementCap = 2;
	TestEqual(TEXT("ViewModel relic definition"), Equipment->RegisterDefinition(*Relic),
		EHSREquipmentOperationResult::Success);
	FHSRItemEquipmentMappingEntry Mapping;
	Mapping.ItemId = Item->ItemId;
	Mapping.EquipmentDefinitionId = Relic->DefinitionId;
	Mapping.Kind = EHSREquipmentKind::Relic;
	Mapping.Slot = static_cast<int32>(EHSRRelicSlot::Head);
	TestTrue(TEXT("ViewModel mapping"), MappingCatalog->AddMapping(Mapping));
	FHSREquipmentEnhancementRule Rule;
	Rule.DefinitionId = Relic->DefinitionId;
	Rule.Kind = EHSREquipmentKind::Relic;
	Rule.TargetLevel = 1;
	Rule.MaterialItemId = TEXT("Item.Material.ViewModel");
	Rule.MaterialCost = 1;
	Rule.TargetModifiers.Add({EHSREquipmentStat::Attack, 12.0f});
	TestTrue(TEXT("ViewModel enhancement rule"), EnhancementCatalog->AddRule(Rule));
	UHSRItemDefinition* Material = NewObject<UHSRItemDefinition>(GameInstance);
	Material->ItemId = Rule.MaterialItemId;
	Material->StorageKind = EHSRItemStorageKind::Stackable;
	Material->MaxStack = 5;
	TestEqual(TEXT("ViewModel material definition"), Inventory->RegisterDefinition(*Material),
		EHSRInventoryOperationResult::Success);
	TestEqual(TEXT("ViewModel material stack"), Inventory->AddStack(Material->ItemId, 2),
		EHSRInventoryOperationResult::Success);

	FHSREquipmentInstance Current;
	Current.InstanceId = CurrentInstanceId;
	Current.DefinitionId = Relic->DefinitionId;
	Current.Kind = EHSREquipmentKind::Relic;
	Current.EnhancementLevel = 0;
	Current.Modifiers.Add({EHSREquipmentStat::Attack, 5.0f});
	TestEqual(TEXT("ViewModel current relic"), Equipment->Equip(CharacterId, Current),
		EHSREquipmentOperationResult::Success);
	TestEqual(TEXT("ViewModel candidate membership"), Inventory->AddUnique({CandidateInstanceId, Item->ItemId}),
		EHSRInventoryOperationResult::Success);
	FHSREquipmentInstance Candidate = Current;
	Candidate.InstanceId = CandidateInstanceId;
	Candidate.Modifiers[0].Value = 9.0f;
	TestEqual(TEXT("ViewModel candidate registry"), Equipment->RegisterInstance(Candidate),
		EHSREquipmentOperationResult::Success);

	UHSRRelicEquipmentViewModel* ViewModel = NewObject<UHSRRelicEquipmentViewModel>(GameInstance);
	int32 ChangeEvents = 0;
	ViewModel->OnChanged().AddLambda([&ChangeEvents](const FHSRRelicEquipmentSnapshot&) { ++ChangeEvents; });
	ViewModel->Initialize(Equipment, Inventory, MappingCatalog, EnhancementCatalog, CharacterId);
	FHSRRelicEquipmentSnapshot Snapshot;
	TestTrue(TEXT("ViewModel initial snapshot"), ViewModel->GetSnapshot(Snapshot));
	TestTrue(TEXT("ViewModel initial snapshot valid"), Snapshot.bIsValid);
	TestEqual(TEXT("ViewModel starts at slot stage"), Snapshot.Stage, EHSRRelicEquipmentStage::SlotSelection);

	TestEqual(TEXT("Select relic slot succeeds"), ViewModel->SelectSlot(EHSRRelicSlot::Head),
		EHSRRelicEquipmentResult::Success);
	TestTrue(TEXT("Candidate list snapshot available"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("Candidate list stage"), Snapshot.Stage, EHSRRelicEquipmentStage::CandidateSelection);
	TestEqual(TEXT("One candidate is visible"), Snapshot.Candidates.Num(), 1);
	TestEqual(TEXT("Current relic is exposed"), Snapshot.CurrentInstanceId, CurrentInstanceId);

	TestEqual(TEXT("Candidate selection succeeds"), ViewModel->SelectCandidate(CandidateInstanceId),
		EHSRRelicEquipmentResult::Success);
	TestTrue(TEXT("Comparison snapshot available"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("Comparison stage"), Snapshot.Stage, EHSRRelicEquipmentStage::Comparison);
	TestTrue(TEXT("Comparison is valid"), Snapshot.Comparison.bIsValid);
	TestEqual(TEXT("Comparison candidate is selected"), Snapshot.Comparison.CandidateInstanceId,
		CandidateInstanceId);
	TestEqual(TEXT("Comparison delta is pure value"), Snapshot.Comparison.StatDeltas[1].Delta, 4.0f);

	TestEqual(TEXT("Replace intent commits"), ViewModel->CommitSelectedMovement(),
		EHSRRelicEquipmentResult::Success);
	TestTrue(TEXT("Committed replacement refreshes snapshot"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("Replacement becomes current"), Snapshot.CurrentInstanceId, CandidateInstanceId);
	TestEqual(TEXT("Replacement leaves candidate stage"), Snapshot.Stage,
		EHSRRelicEquipmentStage::CandidateSelection);

	TestEqual(TEXT("Enhancement stage opens"), ViewModel->OpenEnhancement(),
		EHSRRelicEquipmentResult::Success);
	TestTrue(TEXT("Enhancement options are visible"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("Enhancement stage"), Snapshot.Stage, EHSRRelicEquipmentStage::Enhancement);
	TestEqual(TEXT("One enhancement option is visible"), Snapshot.EnhancementOptions.Num(), 1);
	TestEqual(TEXT("Enhancement commits"), ViewModel->CommitEnhancement(1),
		EHSRRelicEquipmentResult::Success);
	TestTrue(TEXT("Enhancement refreshes committed snapshot"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("Enhanced current level"), Snapshot.CurrentEnhancementLevel, 1);
	TestEqual(TEXT("Enhancement notification is emitted"), ChangeEvents > 0, true);

	const int32 EventsBeforeShutdown = ChangeEvents;
	ViewModel->Shutdown();
	TestFalse(TEXT("Shutdown clears snapshot"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("Shutdown unbinds callbacks"), ChangeEvents, EventsBeforeShutdown);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSRRelicEnhancementMaterialGrantTest,
	"HSR.UI.RelicEquipment.MaterialGrantRespectsStackCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRRelicEnhancementMaterialGrantTest::RunTest(const FString&)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UHSRInventorySubsystem* Inventory = NewObject<UHSRInventorySubsystem>(GameInstance);
	static const FName MaterialId(TEXT("Item.Material.CapTest"));

	UHSRItemDefinition* Material = NewObject<UHSRItemDefinition>(GameInstance);
	Material->ItemId = MaterialId;
	Material->StorageKind = EHSRItemStorageKind::Stackable;
	Material->MaxStack = 99;
	TestEqual(TEXT("Material definition registers"), Inventory->RegisterDefinition(*Material),
		EHSRInventoryOperationResult::Success);

	TestEqual(TEXT("Granting past the stack cap is rejected"), Inventory->AddStack(MaterialId, 999),
		EHSRInventoryOperationResult::StackLimitExceeded);

	FHSRInventorySnapshot RejectedSnapshot;
	Inventory->GetSnapshot(RejectedSnapshot);
	TestEqual(TEXT("Rejected grant leaves no material"), RejectedSnapshot.Stacks.Num(), 0);

	EHSRItemStorageKind StorageKind = EHSRItemStorageKind::Unique;
	int32 MaxStack = 0;
	TestTrue(TEXT("Definition info is readable"),
		Inventory->GetDefinitionInfo(MaterialId, StorageKind, MaxStack));
	TestEqual(TEXT("Definition reports the authored cap"), MaxStack, 99);

	TestEqual(TEXT("Granting up to the cap succeeds"),
		Inventory->AddStack(MaterialId, FMath::Min(999, MaxStack)),
		EHSRInventoryOperationResult::Success);

	FHSRInventorySnapshot GrantedSnapshot;
	Inventory->GetSnapshot(GrantedSnapshot);
	TestEqual(TEXT("Capped grant produces one stack"), GrantedSnapshot.Stacks.Num(), 1);
	TestEqual(TEXT("Capped grant covers the highest rule cost"),
		GrantedSnapshot.Stacks[0].Quantity >= 16, true);
	return true;
}

#endif
