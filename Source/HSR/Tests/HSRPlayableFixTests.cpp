#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Curves/CurveFloat.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../UI/HSRCharacterDetailViewModel.h"
#include "../Battle/HSREnemyTargetPolicy.h"
#include "../GAS/HSRAbilitySystemComponent.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "../Equipment/HSREquipmentEffectBridge.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "../Map/HSRWorldMarkers.h"
#include "../Exploration/HSRRewardChest.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Engine/SkeletalMesh.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSREquipmentDetailRevisionTest, "HSR.PlayableFix.EquipmentDetailRevision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSREquipmentDetailRevisionTest::RunTest(const FString&)
{
	auto* GI = NewObject<UGameInstance>();
	auto* Profiles = NewObject<UHSRCharacterProfileSubsystem>(GI);
	auto* Equipment = NewObject<UHSREquipmentSubsystem>(GI);
	auto* Character = NewObject<UHSRCharacterDefinition>(GI);
	Character->CharacterId = TEXT("Revision.Character");
	Character->MaxLevel = 2;
	Character->BaseMaxHealth = 500.f;
	auto* Curve = NewObject<UCurveFloat>(Character);
	Curve->FloatCurve.AddKey(2, 100);
	Character->CumulativeExperienceCurve = Curve;
	Profiles->RegisterDefinitions({Character});
	auto* Relic = NewObject<UHSRRelicDefinition>(GI);
	Relic->DefinitionId = TEXT("Revision.Relic");
	Relic->SetId = TEXT("Revision.Set");
	Relic->Slot = EHSRRelicSlot::Head;
	Relic->DefaultModifiers.Add({EHSREquipmentStat::MaxHealth, 40.f});
	Equipment->RegisterDefinition(*Relic);
	FHSREquipmentInstance Instance;
	Instance.InstanceId = FGuid::NewGuid();
	Instance.DefinitionId = Relic->DefinitionId;
	Instance.Kind = EHSREquipmentKind::Relic;
	Instance.Modifiers = Relic->DefaultModifiers;
	Equipment->RegisterInstance(Instance);
	auto* VM = NewObject<UHSRCharacterDetailViewModel>(GI);
	VM->Initialize(Profiles, nullptr, nullptr, Equipment);
	TestEqual(TEXT("Select character"), VM->SelectCharacter(Character->CharacterId), EHSRCharacterDetailResult::Success);
	FHSRCharacterDetailSnapshot Before, Equipped, Removed;
	VM->GetSnapshot(Before);
	const FGuid Id = HSRCharacterGuidFromProfileName(Character->CharacterId);
	TestEqual(TEXT("Equip"), Equipment->EquipById(Id, Instance.InstanceId), EHSREquipmentOperationResult::Success);
	VM->GetSnapshot(Equipped);
	TestEqual(TEXT("Growth revision unchanged"), Equipped.RuntimeRevision, Before.RuntimeRevision);
	TestEqual(TEXT("Equipment refresh is not suppressed"), Equipped.DerivedStats.MaxHealth, 540.f);
	TestTrue(TEXT("Equipment revision advances"), Equipped.EquipmentRevision > Before.EquipmentRevision);
	TestEqual(TEXT("Unequip"), Equipment->Unequip(Id, EHSREquipmentKind::Relic, static_cast<int32>(EHSRRelicSlot::Head), Instance.InstanceId), EHSREquipmentOperationResult::Success);
	VM->GetSnapshot(Removed);
	TestEqual(TEXT("Removal refresh"), Removed.DerivedStats.MaxHealth, 500.f);
	VM->Uninitialize();
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSREnemyTargetWeightsTest, "HSR.PlayableFix.EnemyTargetWeights", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSREnemyTargetWeightsTest::RunTest(const FString&)
{
	TestEqual(TEXT("Ordinary health independent"), HSREnemyTargetPolicy::Weight(10,100,false,false), 1.f);
	TestEqual(TEXT("Ordinary targets stay uniformly weighted after a repeat"), HSREnemyTargetPolicy::Weight(10,100,false,true), 1.f);
	TestEqual(TEXT("Elite wounded weight"), HSREnemyTargetPolicy::Weight(25,100,true,false), 2.5f);
	TestEqual(TEXT("Repeat penalty"), HSREnemyTargetPolicy::Weight(25,100,true,true), 1.25f);
	FRandomStream A(77), B(77);
	TArray<int32> Counts{0,0,0,0};
	for (int32 I=0; I<2000; ++I)
	{
		const int32 Pick = HSREnemyTargetPolicy::Choose({1,1,1,1}, A);
		if (!TestTrue(TEXT("Valid candidate"), Counts.IsValidIndex(Pick))) return false;
		++Counts[Pick];
		if (!TestEqual(TEXT("Seed reproducible"), Pick, HSREnemyTargetPolicy::Choose({1,1,1,1}, B))) return false;
	}
	for (int32 Count : Counts) TestTrue(TEXT("Every slot receives ordinary attacks"), Count > 350 && Count < 650);
	TestEqual(TEXT("No candidates"), HSREnemyTargetPolicy::Choose({}, A), INDEX_NONE);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSREquipmentHealthTest, "HSR.PlayableFix.EquipmentMaximumHealth", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSREquipmentHealthTest::RunTest(const FString&)
{
	auto* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
	auto* Owner = World->SpawnActor<AActor>();
	auto* ASC = NewObject<UHSRAbilitySystemComponent>(Owner);
	Owner->AddInstanceComponent(ASC);
	ASC->RegisterComponent();
	ASC->InitAbilityActorInfo(Owner, Owner);
	auto* Attributes = NewObject<UHSRCoreAttributeSet>(Owner);
	ASC->AddAttributeSetSubobject(Attributes);
	ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 100.f);
	ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 75.f);
	auto* Bridge = NewObject<UHSREquipmentEffectBridge>(Owner);
	TSubclassOf<UGameplayEffect> Effect = LoadClass<UGameplayEffect>(nullptr, TEXT("/Game/GameplayEffects/GE_Equipment_P12.GE_Equipment_P12_C"));
	const FGuid Id = FGuid::NewGuid();
	FHSREquipmentAggregate Bonus; Bonus.MaxHealth = 40.f; Bonus.Revision = 1;
	TestTrue(TEXT("HP relic applies"), Bridge->Apply(Id, ASC, Effect, Bonus));
	TestEqual(TEXT("Maximum increases"), Attributes->GetMaxHealth(), 140.f);
	TestEqual(TEXT("Equip does not heal"), Attributes->GetHealth(), 75.f);
	ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 130.f);
	Bonus.MaxHealth = 60.f; Bonus.Revision = 2;
	TestTrue(TEXT("Enhancement applies"), Bridge->Apply(Id, ASC, Effect, Bonus));
	TestEqual(TEXT("Enhanced maximum"), Attributes->GetMaxHealth(), 160.f);
	TestEqual(TEXT("Enhancement preserves current HP"), Attributes->GetHealth(), 130.f);
	TestTrue(TEXT("Unequip applies"), Bridge->Remove(Id));
	TestEqual(TEXT("Maximum restored"), Attributes->GetMaxHealth(), 100.f);
	TestEqual(TEXT("Current HP clamps on removal"), Attributes->GetHealth(), 100.f);
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRCharacterPresentationRefsTest, "HSR.PlayableFix.FourCharacterReferences", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRCharacterPresentationRefsTest::RunTest(const FString&)
{
	TSet<USkeletalMesh*> Meshes;
	for (const TCHAR* Name : {TEXT("Huohua"), TEXT("Remiel"), TEXT("EvernightMoon"), TEXT("Verina")})
	{
		const FString Path = FString(TEXT("/Game/Data/VerticalSlice/Characters/DA_Character_")) + Name;
		const auto* Definition = LoadObject<UHSRCharacterDefinition>(nullptr, *Path);
		if (!TestNotNull(Path, Definition)) return false;
		auto* Mesh = Definition->CharacterMesh.LoadSynchronous();
		if (!TestNotNull(TEXT("Character mesh"), Mesh)) return false;
		Meshes.Add(Mesh);
		const auto* Interface = IAnimClassInterface::GetFromClass(Definition->AnimationClass.LoadSynchronous());
		if (!TestNotNull(TEXT("Animation class interface"), Interface)) return false;
		TestTrue(TEXT("ABP skeleton matches actual mesh"), Interface->GetTargetSkeleton() == Mesh->GetSkeleton());
		for (const auto& Reference : {Definition->AttackAnimation, Definition->SkillAnimation, Definition->HitAnimation, Definition->DefeatAnimation})
		{
			const auto* Clip = Reference.LoadSynchronous();
			if (!TestNotNull(TEXT("Combat clip loads"), Clip)) return false;
			TestTrue(TEXT("Clip skeleton matches actual mesh"), Clip->GetSkeleton() == Mesh->GetSkeleton());
			TestTrue(TEXT("Clip has duration"), Clip->GetPlayLength() > .1f);
		}
	}
	TestEqual(TEXT("Four distinct character meshes"), Meshes.Num(), 4);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRMarkerVisibilityTest, "HSR.PlayableFix.SharedChestMarkers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRMarkerVisibilityTest::RunTest(const FString&)
{
	auto* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
	auto* GI = NewObject<UGameInstance>();
	World->SetGameInstance(GI);
	auto* Chest = World->SpawnActor<AHSRRewardChest>();
	TestEqual(TEXT("Available chest visible in shared projection"), HSRWorldMarkers::Collect(World).Num(), 1);
	Chest->SetActorHiddenInGame(true);
	TestEqual(TEXT("Hidden chest removed from shared projection"), HSRWorldMarkers::Collect(World).Num(), 0);
	World->DestroyWorld(false);
	return true;
}
#endif
