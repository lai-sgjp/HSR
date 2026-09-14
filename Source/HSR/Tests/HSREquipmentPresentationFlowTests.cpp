#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "../Data/Definitions/HSREquipmentEnhancementCatalog.h"
#include "../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../UI/Relic/HSRRelicEquipmentViewModel.h"

namespace HSRPresentationEquipmentTests
{
struct FFixture
{
	UGameInstance* GI = NewObject<UGameInstance>();
	UHSRInventorySubsystem* Inventory = NewObject<UHSRInventorySubsystem>(GI);
	UHSREquipmentSubsystem* Equipment = NewObject<UHSREquipmentSubsystem>(GI);
	UHSRItemEquipmentMappingCatalog* Mapping = NewObject<UHSRItemEquipmentMappingCatalog>(GI);
	UHSREquipmentEnhancementCatalog* Enhancement = NewObject<UHSREquipmentEnhancementCatalog>(GI);
	FGuid Character = FGuid::NewGuid();
	FGuid BagId = FGuid::NewGuid();
	FName ItemId = TEXT("Item.Presentation.Head");
	FName MaterialId = TEXT("Item.Presentation.Growth");
	FName DefinitionId = TEXT("Relic.Presentation.NonConventionAssetName");

	FFixture()
	{
		auto* Item = NewObject<UHSRItemDefinition>(GI);
		Item->ItemId = ItemId;
		Item->StorageKind = EHSRItemStorageKind::Unique;
		Item->MaxStack = 1;
		Inventory->RegisterDefinition(*Item);
		Inventory->AddUnique({BagId, ItemId});
		auto* Material = NewObject<UHSRItemDefinition>(GI);
		Material->ItemId = MaterialId;
		Material->MaxStack = 99;
		Inventory->RegisterDefinition(*Material);
		Inventory->AddStack(MaterialId, 4);
		auto* Relic = NewObject<UHSRRelicDefinition>(GI);
		Relic->DefinitionId = DefinitionId;
		Relic->SetId = TEXT("Set.Presentation");
		Relic->Slot = EHSRRelicSlot::Head;
		Relic->EnhancementCap = 1;
		Relic->DefaultModifiers.Add({EHSREquipmentStat::Attack, 7.f});
		Equipment->RegisterDefinition(*Relic);
		FHSRItemEquipmentMappingEntry Map;
		Map.ItemId = ItemId;
		Map.EquipmentDefinitionId = DefinitionId;
		Map.Kind = EHSREquipmentKind::Relic;
		Map.Slot = static_cast<int32>(EHSRRelicSlot::Head);
		Mapping->AddMapping(Map);
		FHSREquipmentEnhancementRule Rule;
		Rule.DefinitionId = DefinitionId;
		Rule.Kind = EHSREquipmentKind::Relic;
		Rule.TargetLevel = 1;
		Rule.MaterialItemId = MaterialId;
		Rule.MaterialCost = 2;
		Rule.TargetModifiers.Add({EHSREquipmentStat::Attack, 14.f});
		Enhancement->AddRule(Rule);
	}

	FHSREquipmentEnhancementRequest Request() const
	{
		FHSRInventorySnapshot Bag;
		Inventory->GetSnapshot(Bag);
		FHSREquipmentLoadout Loadout;
		int32 Revision = 0;
		Equipment->GetLoadout(Character, Loadout, Revision);
		FHSREquipmentEnhancementRequest R;
		R.OperationId = FGuid::NewGuid();
		R.CharacterId = Character;
		R.InstanceId = BagId;
		R.Kind = EHSREquipmentKind::Relic;
		R.TargetLevel = 1;
		R.ExpectedInventoryRevision = Bag.Revision;
		R.ExpectedEquipmentRevision = Revision;
		R.ExpectedEnhancementLevel = 0;
		return R;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRBagEnhancementTransactionTest,
	"HSR.Equipment.Presentation.BagEnhancementTransaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRBagEnhancementTransactionTest::RunTest(const FString&)
{
	using namespace HSRPresentationEquipmentTests;
	FFixture F;
	FHSREquipmentInstance Instance;
	TestTrue(TEXT("Mapping previews registered definition independent of asset filename"),
		F.Equipment->PreviewMappedInstance(F.ItemId, F.BagId, *F.Mapping, Instance));
	TestEqual(TEXT("Preview uses authored base stat"), Instance.Modifiers[0].Value, 7.f);
	TestFalse(TEXT("Preview does not mint an instance"), F.Equipment->FindRegisteredInstance(F.BagId, Instance));
	int32 Projections = 0;
	F.Equipment->SetEnhancementProjection(
		UHSREquipmentSubsystem::FEnhancementProjectionPreflight::CreateLambda(
			[&Projections](const FHSREquipmentEnhancementRequest&, const FHSREquipmentInstance&) { ++Projections; return false; }),
		UHSREquipmentSubsystem::FEnhancementProjectionCommit());
	const auto Request = F.Request();
	const auto Result = F.Equipment->ExecuteInventoryEnhancement(Request, *F.Inventory, *F.Enhancement, *F.Mapping);
	TestEqual(TEXT("Bag enhancement succeeds without character projection"), Result.Code, EHSREquipmentEnhancementResultCode::Success);
	TestEqual(TEXT("Bag enhancement never projects equipped stats"), Projections, 0);
	TestEqual(TEXT("Bag enhancement leaves loadout revision unchanged"), Result.NewEquipmentRevision, 0);
	TestTrue(TEXT("Committed bag instance exists"), F.Equipment->FindRegisteredInstance(F.BagId, Instance));
	TestEqual(TEXT("Correct bag item level changes"), Instance.EnhancementLevel, 1);
	FHSRInventorySnapshot Bag;
	F.Inventory->GetSnapshot(Bag);
	TestEqual(TEXT("Materials charged once"), Bag.GetStackQuantity(F.MaterialId), 2);
	TestEqual(TEXT("Enhanced item stays in bag"), Bag.UniqueItems.Num(), 1);
	TestTrue(TEXT("Repeated command replays"),
		F.Equipment->ExecuteInventoryEnhancement(Request, *F.Inventory, *F.Enhancement, *F.Mapping).bReplay);
	F.Inventory->GetSnapshot(Bag);
	TestEqual(TEXT("Replay costs nothing"), Bag.GetStackQuantity(F.MaterialId), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRBagEnhancementRollbackTest,
	"HSR.Equipment.Presentation.BagEnhancementRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRBagEnhancementRollbackTest::RunTest(const FString&)
{
	using namespace HSRPresentationEquipmentTests;
	FFixture F;
	F.Inventory->RemoveStack(F.MaterialId, 4);
	const auto Result = F.Equipment->ExecuteInventoryEnhancement(F.Request(), *F.Inventory, *F.Enhancement, *F.Mapping);
	TestEqual(TEXT("Insufficient materials rejected"), Result.Code, EHSREquipmentEnhancementResultCode::InventoryRejected);
	FHSREquipmentInstance Instance;
	TestFalse(TEXT("Failed enhancement does not leave minted registry state"), F.Equipment->FindRegisteredInstance(F.BagId, Instance));
	auto Forged = F.Request();
	Forged.InstanceId = FGuid::NewGuid();
	TestEqual(TEXT("An invented bag identity cannot be enhanced"),
		F.Equipment->ExecuteInventoryEnhancement(Forged, *F.Inventory, *F.Enhancement, *F.Mapping).Code,
		EHSREquipmentEnhancementResultCode::EquipmentRejected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRRelicCandidateEnhancementFlowTest,
	"HSR.UI.RelicEquipment.CandidateEnhancementAndBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRRelicCandidateEnhancementFlowTest::RunTest(const FString&)
{
	using namespace HSRPresentationEquipmentTests;
	FFixture F;
	FHSREquipmentInstance Current;
	Current.InstanceId = FGuid::NewGuid();
	Current.DefinitionId = F.DefinitionId;
	Current.Kind = EHSREquipmentKind::Relic;
	Current.Modifiers.Add({EHSREquipmentStat::Attack, 3.f});
	F.Equipment->Equip(F.Character, Current);
	auto* VM = NewObject<UHSRRelicEquipmentViewModel>(F.GI);
	VM->Initialize(F.Equipment, F.Inventory, F.Mapping, F.Enhancement, F.Character);
	VM->SelectSlot(EHSRRelicSlot::Head);
	VM->SelectCandidate(F.BagId);
	TestEqual(TEXT("Candidate enhancement opens"), VM->OpenEnhancement(), EHSRRelicEquipmentResult::Success);
	TestEqual(TEXT("Repeated enhancement open is harmless"), VM->OpenEnhancement(), EHSRRelicEquipmentResult::Success);
	FHSRRelicEquipmentSnapshot Snapshot;
	VM->GetSnapshot(Snapshot);
	TestEqual(TEXT("Enhancement target is selected candidate, not equipped relic"), Snapshot.EnhancementInstanceId, F.BagId);
	TestEqual(TEXT("Candidate enhancement commits"), VM->CommitEnhancement(1), EHSRRelicEquipmentResult::Success);
	VM->GetSnapshot(Snapshot);
	TestTrue(TEXT("Maximum-level view stays valid"), Snapshot.bIsValid);
	TestEqual(TEXT("Maximum-level target remains visible"), Snapshot.Stage, EHSRRelicEquipmentStage::Enhancement);
	FHSREquipmentInstance Unchanged;
	F.Equipment->FindRegisteredInstance(Current.InstanceId, Unchanged);
	TestEqual(TEXT("Equipped item has not been enhanced by mistake"), Unchanged.EnhancementLevel, 0);
	TestEqual(TEXT("Back returns successfully"), VM->Back(), EHSRRelicEquipmentResult::Success);
	VM->GetSnapshot(Snapshot);
	TestEqual(TEXT("Back returns to candidate comparison"), Snapshot.Stage, EHSRRelicEquipmentStage::Comparison);
	TestEqual(TEXT("Comparison preserves selected identity"), Snapshot.SelectedCandidateId, F.BagId);
	TestEqual(TEXT("Enhanced candidate can replace current relic"), VM->CommitSelectedMovement(), EHSRRelicEquipmentResult::Success);
	TestEqual(TEXT("Equipped relic can be removed"), VM->UnequipSelectedSlot(), EHSRRelicEquipmentResult::Success);
	VM->GetSnapshot(Snapshot);
	TestFalse(TEXT("Slot is empty after removal"), Snapshot.CurrentInstanceId.IsValid());
	TestEqual(TEXT("Both relics return to bag candidates"), Snapshot.Candidates.Num(), 2);
	VM->Shutdown();
	return true;
}
#endif
