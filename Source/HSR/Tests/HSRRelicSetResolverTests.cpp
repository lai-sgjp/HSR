#include "Misc/AutomationTest.h"
#include "../Equipment/HSRRelicSetResolver.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Data/Definitions/HSRRelicSetDefinition.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRRelicSetResolverTest, "HSR.Equipment.RelicSetResolver", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRRelicSetResolverTest::RunTest(const FString&)
{
	TArray<UHSRRelicDefinition*> Definitions;
	TArray<UHSRRelicSetDefinition*> Sets;
	TestTrue(TEXT("Empty input resolves empty"), FHSRRelicSetResolver::Resolve(FHSREquipmentLoadout(), Definitions, Sets).IsEmpty());

	UHSRRelicSetDefinition* SetA = NewObject<UHSRRelicSetDefinition>(); SetA->SetId = TEXT("A"); SetA->SetGameplayEffectClass = UGameplayEffect::StaticClass();
	UHSRRelicSetDefinition* Duplicate = NewObject<UHSRRelicSetDefinition>(); Duplicate->SetId = TEXT("A"); Duplicate->SetGameplayEffectClass = UGameplayEffect::StaticClass();
	Sets = { SetA, Duplicate };
	UHSRRelicDefinition* Def1 = NewObject<UHSRRelicDefinition>(); Def1->DefinitionId = TEXT("D1"); Def1->SetId = TEXT("A");
	UHSRRelicDefinition* Def2 = NewObject<UHSRRelicDefinition>(); Def2->DefinitionId = TEXT("D2"); Def2->SetId = TEXT("A");
	UHSRRelicDefinition* DefOther = NewObject<UHSRRelicDefinition>(); DefOther->DefinitionId = TEXT("D3"); DefOther->SetId = TEXT("B");
	Definitions = { Def1, Def2, DefOther };
	FHSREquipmentLoadout Loadout;
	FHSREquipmentInstance I1; I1.DefinitionId = TEXT("D1"); Loadout.Relics.Add(EHSRRelicSlot::Head, I1);
	FHSRRelicSetResolution R = FHSRRelicSetResolver::Resolve(Loadout, Definitions, Sets).FindRef(TEXT("A"));
	TestEqual(TEXT("One piece inactive"), R.Count, 1); TestFalse(TEXT("One piece inactive"), R.bActive); TestTrue(TEXT("Inactive source empty"), R.SetSourceId.IsNone());
	FHSREquipmentInstance I2; I2.DefinitionId = TEXT("D2"); Loadout.Relics.Add(EHSRRelicSlot::Hands, I2);
	R = FHSRRelicSetResolver::Resolve(Loadout, Definitions, Sets).FindRef(TEXT("A"));
	TestEqual(TEXT("Two pieces active"), R.Count, 2); TestTrue(TEXT("Threshold activates"), R.bActive); TestEqual(TEXT("Source id stable"), R.SetSourceId, FName(TEXT("A")));
	Loadout.Relics.Remove(EHSRRelicSlot::Hands);
	const auto First = FHSRRelicSetResolver::Resolve(Loadout, Definitions, Sets);
	const auto Second = FHSRRelicSetResolver::Resolve(Loadout, Definitions, Sets);
	TestEqual(TEXT("Repeated resolve is stable"), First.FindRef(TEXT("A")).Count, Second.FindRef(TEXT("A")).Count);
	return true;
}
