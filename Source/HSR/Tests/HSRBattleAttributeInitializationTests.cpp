#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "../Battle/HSRBattleAttributeInitialization.h"
#include "../Battle/HSRBattleCoordinator.h"
#include "../Battle/HSRBattleStage.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/Definitions/HSREnemyDefinition.h"
#include "../GAS/HSRAbilitySystemComponent.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "GameplayEffect.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace HSR::Presentation::AttributeTests
{
UHSRAbilitySystemComponent* MakeASC(UWorld* World, AActor*& OutActor)
{
	OutActor = World->SpawnActor<AActor>();
	auto* ASC = NewObject<UHSRAbilitySystemComponent>(OutActor);
	OutActor->AddInstanceComponent(ASC);
	ASC->RegisterComponent();
	ASC->InitAbilityActorInfo(OutActor, OutActor);
	ASC->InitStats(UHSRCoreAttributeSet::StaticClass(), nullptr);
	return ASC;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRBattleDefinitionBaseStatsTest,
	"HSR.Presentation.BattleStats.DefinitionBaseAndModifiers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRBattleDefinitionBaseStatsTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	AActor* Actor = nullptr;
	auto* ASC = HSR::Presentation::AttributeTests::MakeASC(World, Actor);
	ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 1000.f);
	ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 800.f);
	ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetAttackAttribute(), 100.f);
	auto* Definition = NewObject<UHSRCharacterDefinition>();
	Definition->BaseMaxHealth = 105.f;
	Definition->BaseMaxEnergy = 100.f;
	Definition->BaseAttack = 15.f;
	Definition->BaseDefense = 5.f;
	Definition->BaseSpeed = 98.f;
	const auto Base = FHSRBattleBaseAttributes::FromCharacter(*Definition);
	TestTrue(TEXT("Authored base replaces legacy test values"), Base.Apply(*ASC));
	TestEqual(TEXT("Definition attack becomes the base"), ASC->GetNumericAttributeBase(UHSRCoreAttributeSet::GetAttackAttribute()), 15.f);

	// Independent active modifiers stand in for the existing progression and equipment GE layers.
	auto* Bonuses = NewObject<UGameplayEffect>();
	Bonuses->DurationPolicy = EGameplayEffectDurationType::Infinite;
	for (const TPair<FGameplayAttribute, float>& Value : {
		TPair<FGameplayAttribute, float>(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 40.f),
		TPair<FGameplayAttribute, float>(UHSRCoreAttributeSet::GetAttackAttribute(), 9.f)})
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Value.Key;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value.Value));
		Bonuses->Modifiers.Add(Modifier);
	}
	const auto Handle = ASC->ApplyGameplayEffectToSelf(Bonuses, 1.f, ASC->MakeEffectContext());
	TestTrue(TEXT("Growth and equipment modifier layer applies"), Handle.WasSuccessfullyApplied());
	FHSRBattleBaseAttributes::FillHealth(*ASC);
	TestEqual(TEXT("Start health includes all max-health bonuses"), ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()), 145.f);
	TestEqual(TEXT("Final attack is authored base plus bonuses"), ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute()), 24.f);
	ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 60.f);
	TestTrue(TEXT("Base can be reassigned without absorbing bonuses"), Base.Apply(*ASC));
	TestEqual(TEXT("Repeated assignment does not double modifiers"), ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute()), 24.f);
	TestEqual(TEXT("Reassignment alone does not heal during refresh"), ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()), 60.f);
	FHSRBattleBaseAttributes Invalid = Base; Invalid.Speed = -1.f;
	TestFalse(TEXT("Invalid definition rejects before any writes"), Invalid.Apply(*ASC));
	TestEqual(TEXT("Invalid input preserves committed speed"), ASC->GetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute()), 98.f);

	auto* Enemy = NewObject<UHSREnemyDefinition>();
	TestFalse(TEXT("Legacy enemies do not opt in automatically"), Enemy->bUseAuthoredBaseStats);
	Enemy->BaseMaxHealth = 180.f; Enemy->BaseAttack = 18.f; Enemy->BaseDefense = 5.f; Enemy->BaseSpeed = 90.f;
	ASC->RemoveActiveGameplayEffect(Handle);
	TestTrue(TEXT("Enemy authored base applies"), FHSRBattleBaseAttributes::FromEnemy(*Enemy, 100.f).Apply(*ASC));
	FHSRBattleBaseAttributes::FillHealth(*ASC);
	TestEqual(TEXT("Enemy starts at its authored health"), ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()), 180.f);
	World->SpawnActor<AHSRBattleStage>();
	TestFalse(TEXT("A stage in a regression world does not enable the formal-map contract"), FHSRBattleBaseAttributes::IsFormalArena(World));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRBattleRosterEquipmentStatsTest,
	"HSR.Presentation.BattleStats.RosterEquipmentProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRBattleRosterEquipmentStatsTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* Coordinator = NewObject<UHSRBattleCoordinator>();
	const auto EquipmentEffect = LoadClass<UGameplayEffect>(nullptr, TEXT("/Game/GameplayEffects/GE_Equipment_P12.GE_Equipment_P12_C"));
	if (!TestNotNull(TEXT("Project equipment effect exists"), EquipmentEffect)) return false;
	Coordinator->SetEquipmentGameplayEffect(EquipmentEffect);
	Coordinator->SetRelicSetGameplayEffect(EquipmentEffect);
	TArray<FHSRBattleParticipant> Participants;
	FHSREquipmentRestoreMap Candidate;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		AActor* Actor = nullptr;
		auto* ASC = HSR::Presentation::AttributeTests::MakeASC(World, Actor);
		FHSRBattleBaseAttributes Base; Base.Attack = 15.f;
		Base.Apply(*ASC);
		FHSRBattleParticipant Participant;
		Participant.ParticipantId = FName(*FString::Printf(TEXT("Player.%d"), Index));
		Participant.DefinitionId = FName(*FString::Printf(TEXT("Character.StatFixture.%d"), Index));
		Participant.Actor = Actor; Participant.AbilitySystemComponent = ASC;
		Participants.Add(Participant);
		FHSREquipmentRestoreState State; State.Revision = 1;
		FHSREquipmentInstance Item; Item.InstanceId = FGuid(10, 20, 30, Index + 1);
		Item.DefinitionId = TEXT("Equipment.StatFixture");
		Item.Modifiers.Add({EHSREquipmentStat::Attack, static_cast<float>(Index + 1)});
		State.Loadout.Equipment.Add(EHSREquipmentSlot::Weapon, Item);
		Candidate.Add(HSRCharacterGuidFromProfileName(Participant.DefinitionId), State);
	}
	Coordinator->SetPlayerCharacterDefinition(Participants[0].DefinitionId, nullptr);
	Coordinator->SetParticipantsForEquipmentProjectionDevelopmentTest(Participants);
	TestTrue(TEXT("All four character loadouts project"), Coordinator->ProjectEquipmentRestore(Candidate));
	TestEqual(TEXT("One source per equipped instance"), Coordinator->GetEquipmentProjectionSourceCountForDevelopmentTest(), 4);
	for (int32 Index = 0; Index < Participants.Num(); ++Index)
		TestEqual(*FString::Printf(TEXT("Member %d receives only its own weapon"), Index),
			Participants[Index].AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute()), 16.f + Index);
	TestTrue(TEXT("Repeated equipment projection succeeds"), Coordinator->ProjectEquipmentRestore(Candidate));
	TestEqual(TEXT("Repeated projection does not add sources"), Coordinator->GetEquipmentProjectionSourceCountForDevelopmentTest(), 4);
	TestEqual(TEXT("Repeated projection does not double fourth member's bonus"),
		Participants[3].AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute()), 19.f);
	Coordinator->ProjectEquipmentRestore(FHSREquipmentRestoreMap());
	return true;
}
#endif
