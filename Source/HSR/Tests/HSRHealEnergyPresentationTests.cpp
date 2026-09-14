#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "../Data/HSRSkillDefinition.h"
#include "../GAS/HSRAbilitySystemComponent.h"
#include "../GAS/Ability/HSRHealAbility.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"

namespace HSRHealEnergyPresentationTests
{
	// Real ASC instances and authored instant effects; no direct ActivateAbility calls or mocked commits.
	struct FFixture
	{
		UWorld* World = nullptr;
		UAbilitySystemComponent* Source = nullptr;
		UAbilitySystemComponent* Target = nullptr;
		UHSRHealAbility* Ability = nullptr;
		UHSRSkillDefinition* Definition = nullptr;
		FGameplayAbilitySpecHandle Handle;
		UClass* HealClass = nullptr;
		UClass* CostClass = nullptr;
		UClass* RefundClass = nullptr;

		~FFixture()
		{
			if (Source) { Source->GameplayEffectApplicationQueries.Reset(); Source->ClearAllAbilities(); }
			if (Target) Target->GameplayEffectApplicationQueries.Reset();
			if (World) World->DestroyWorld(false);
		}

		bool Initialize(FAutomationTestBase& Test, bool bCostsEnergy, float InitialEnergy = 100.f)
		{
			HealClass = LoadClass<UGameplayEffect>(nullptr, TEXT("/Game/GameplayEffects/BP_GE_P6_Heal.BP_GE_P6_Heal_C"));
			CostClass = LoadClass<UGameplayEffect>(nullptr, TEXT("/Game/GameplayEffects/BP_GE_P6_UltimateEnergyCost.BP_GE_P6_UltimateEnergyCost_C"));
			RefundClass = LoadClass<UGameplayEffect>(nullptr, TEXT("/Game/GameplayEffects/BP_GE_P7_UltimateEnergyRefund.BP_GE_P7_UltimateEnergyRefund_C"));
			if (!Test.TestNotNull(TEXT("Authored heal effect"), HealClass)
				|| !Test.TestNotNull(TEXT("Authored energy cost"), CostClass)
				|| !Test.TestNotNull(TEXT("Authored energy refund"), RefundClass)) return false;
			World = UWorld::CreateWorld(EWorldType::GamePreview, false);
			if (!Test.TestNotNull(TEXT("Transient gameplay world"), World)) return false;
			const auto MakeASC = [this]() -> UAbilitySystemComponent*
			{
				AActor* Actor = World->SpawnActor<AActor>();
				if (!Actor) return nullptr;
				auto* ASC = Cast<UAbilitySystemComponent>(Actor->AddComponentByClass(
					UHSRAbilitySystemComponent::StaticClass(), false, FTransform::Identity, false));
				if (!ASC) return nullptr;
				ASC->InitStats(UHSRCoreAttributeSet::StaticClass(), nullptr);
				ASC->InitAbilityActorInfo(Actor, Actor);
				ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 1000.f);
				ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 100.f);
				ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxEnergyAttribute(), 200.f);
				return ASC;
			};
			Source = MakeASC(); Target = MakeASC();
			if (!Test.TestNotNull(TEXT("Source ASC"), Source) || !Test.TestNotNull(TEXT("Target ASC"), Target)) return false;
			Source->SetNumericAttributeBase(UHSRCoreAttributeSet::GetEnergyAttribute(), InitialEnergy);
			Handle = Source->GiveAbility(FGameplayAbilitySpec(UHSRHealAbility::StaticClass(), 1));
			FGameplayAbilitySpec* Spec = Source->FindAbilitySpecFromHandle(Handle);
			Ability = Spec ? Cast<UHSRHealAbility>(Spec->GetPrimaryInstance()) : nullptr;
			if (!Test.TestNotNull(TEXT("Granted instanced heal ability"), Ability)) return false;
			Definition = NewObject<UHSRSkillDefinition>(Source);
			Definition->SkillId = TEXT("Skill.Presentation.HealEnergy");
			Definition->Category = EHSRSkillCategory::Heal;
			Definition->TargetType = EHSRTargetType::SingleAlly;
			Definition->AbilityClass = UHSRHealAbility::StaticClass();
			Definition->EffectGameplayEffectClass = HealClass;
			if (bCostsEnergy)
			{
				Definition->CostGameplayEffectClass = CostClass;
				Definition->EnergyRefundGameplayEffectClass = RefundClass;
			}
			return Test.TestTrue(TEXT("Valid heal configuration accepted"), Ability->ConfigureFromSkillDefinition(*Definition));
		}

		float Energy() const { return Source->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute()); }
		float Health() const { return Target->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()); }
		bool Activate() { Ability->SetPendingTarget(Target); return Source->TryActivateAbility(Handle); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSROrdinaryHealEnergyTest,
	"HSR.GAS.HealPresentation.OrdinaryHealPreservesEnergy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSROrdinaryHealEnergyTest::RunTest(const FString&)
{
	HSRHealEnergyPresentationTests::FFixture F;
	if (!F.Initialize(*this, false, 37.f)) return false;
	TestTrue(TEXT("ASC accepts ordinary heal"), F.Activate());
	TestTrue(TEXT("Ordinary heal reports success"), F.Ability->DidLastActivationSucceed());
	TestTrue(TEXT("Target really receives health"), F.Health() > 100.f);
	TestEqual(TEXT("Ordinary heal does not spend energy"), F.Energy(), 37.f);
	TestEqual(TEXT("Successful activation has no failure reason"), F.Ability->GetLastFailureReason(), EHSRAbilityFailureReason::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRFinisherHealEnergyTest,
	"HSR.GAS.HealPresentation.FinisherSpends100Energy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRFinisherHealEnergyTest::RunTest(const FString&)
{
	HSRHealEnergyPresentationTests::FFixture F;
	if (!F.Initialize(*this, true)) return false;
	TestTrue(TEXT("ASC accepts fully charged finisher"), F.Activate());
	TestTrue(TEXT("Charged finisher reports success"), F.Ability->DidLastActivationSucceed());
	TestTrue(TEXT("Finisher really heals target"), F.Health() > 100.f);
	TestEqual(TEXT("Finisher spends exactly 100 energy"), F.Energy(), 0.f);
	TestEqual(TEXT("Successful finisher has no failure reason"), F.Ability->GetLastFailureReason(), EHSRAbilityFailureReason::None);
	const float HealedHealth = F.Health();
	TestFalse(TEXT("Repeated finisher cannot activate at zero energy"), F.Activate());
	TestFalse(TEXT("Repeated rejection does not retain previous success"), F.Ability->DidLastActivationSucceed());
	TestEqual(TEXT("Repeated rejected finisher cannot heal twice"), F.Health(), HealedHealth);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRInsufficientHealEnergyTest,
	"HSR.GAS.HealPresentation.Energy99CannotHeal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRInsufficientHealEnergyTest::RunTest(const FString&)
{
	HSRHealEnergyPresentationTests::FFixture F;
	if (!F.Initialize(*this, true, 99.f)) return false;
	TestEqual(TEXT("Availability explains insufficient energy"), F.Ability->GetPreActivationFailureReason(
		F.Handle, F.Source->AbilityActorInfo.Get()), EHSRAbilityFailureReason::InsufficientEnergy);
	TestFalse(TEXT("ASC refuses energy 99 finisher"), F.Activate());
	TestFalse(TEXT("Refused finisher is not reported as successful"), F.Ability->DidLastActivationSucceed());
	TestEqual(TEXT("Rejected activation leaves target health unchanged"), F.Health(), 100.f);
	TestEqual(TEXT("Rejected activation spends nothing"), F.Energy(), 99.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRRejectedHealRefundTest,
	"HSR.GAS.HealPresentation.TargetRejectionRefundsEnergy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRRejectedHealRefundTest::RunTest(const FString&)
{
	HSRHealEnergyPresentationTests::FFixture F;
	if (!F.Initialize(*this, true)) return false;
	int32 TargetQueries = 0;
	F.Target->GameplayEffectApplicationQueries.Add(FGameplayEffectApplicationQuery::CreateLambda(
		[&TargetQueries](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec&) { ++TargetQueries; return false; }));
	TArray<float> ObservedEnergy;
	F.Source->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetEnergyAttribute()).AddLambda(
		[&ObservedEnergy](const FOnAttributeChangeData& Data) { ObservedEnergy.Add(Data.NewValue); });
	TestTrue(TEXT("ASC enters activation before target rejection"), F.Activate());
	TestEqual(TEXT("Target really rejected effect application"), TargetQueries, 1);
	TestFalse(TEXT("Rejected heal reports failure"), F.Ability->DidLastActivationSucceed());
	TestEqual(TEXT("Rejected heal identifies effect failure"), F.Ability->GetLastFailureReason(), EHSRAbilityFailureReason::EffectFailed);
	TestEqual(TEXT("Target has not been healed"), F.Health(), 100.f);
	TestTrue(TEXT("Cost was applied before compensation"), ObservedEnergy.Contains(0.f));
	TestEqual(TEXT("Refund restores all 100 energy"), F.Energy(), 100.f);
	F.Source->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetEnergyAttribute()).Clear();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRRejectedHealCostTest,
	"HSR.GAS.HealPresentation.CostRejectionCannotGrantFreeHeal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRRejectedHealCostTest::RunTest(const FString&)
{
	HSRHealEnergyPresentationTests::FFixture F;
	if (!F.Initialize(*this, true)) return false;
	F.Source->GameplayEffectApplicationQueries.Add(FGameplayEffectApplicationQuery::CreateLambda(
		[](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec&) { return false; }));
	TestTrue(TEXT("ASC starts ability with sufficient energy"), F.Activate());
	TestFalse(TEXT("Refused cost does not count as successful heal"), F.Ability->DidLastActivationSucceed());
	TestEqual(TEXT("Refused cost reports commit failure"), F.Ability->GetLastFailureReason(), EHSRAbilityFailureReason::CommitFailed);
	TestEqual(TEXT("Failed cost leaves energy unchanged"), F.Energy(), 100.f);
	TestEqual(TEXT("Failed cost cannot provide free healing"), F.Health(), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRInvalidHealConfigurationTest,
	"HSR.GAS.HealPresentation.InvalidConfigurationFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRInvalidHealConfigurationTest::RunTest(const FString&)
{
	HSRHealEnergyPresentationTests::FFixture F;
	if (!F.Initialize(*this, false)) return false;
	F.Definition->CostGameplayEffectClass = F.CostClass;
	TestFalse(TEXT("Cost without refund rejected"), F.Ability->ConfigureFromSkillDefinition(*F.Definition));
	TestFalse(TEXT("Invalid reconfiguration cannot run old free heal"), F.Activate());
	F.Definition->EnergyRefundGameplayEffectClass = F.CostClass;
	TestFalse(TEXT("Negative refund rejected"), F.Ability->ConfigureFromSkillDefinition(*F.Definition));
	F.Definition->CostGameplayEffectClass = F.HealClass;
	F.Definition->EnergyRefundGameplayEffectClass = F.RefundClass;
	TestFalse(TEXT("Non-energy cost rejected"), F.Ability->ConfigureFromSkillDefinition(*F.Definition));
	F.Definition->CostGameplayEffectClass.Reset();
	TestFalse(TEXT("Refund without matching cost rejected"), F.Ability->ConfigureFromSkillDefinition(*F.Definition));
	TestEqual(TEXT("Invalid config cannot change energy"), F.Energy(), 100.f);
	TestEqual(TEXT("Invalid config cannot heal"), F.Health(), 100.f);
	F.Definition->CostGameplayEffectClass = F.CostClass;
	TestTrue(TEXT("Valid replacement configuration recovers"), F.Ability->ConfigureFromSkillDefinition(*F.Definition));
	TestTrue(TEXT("Recovered ability activates"), F.Activate());
	TestTrue(TEXT("Recovered ability really succeeds"), F.Ability->DidLastActivationSucceed());
	TestEqual(TEXT("Recovered configuration retains cost"), F.Energy(), 0.f);
	return true;
}

#endif
