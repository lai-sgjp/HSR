#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Equipment/HSREquipmentStatAggregator.h"
#include "../Equipment/HSREquipmentEffectBridge.h"
#include "../Battle/HSRBattleCoordinator.h"
#include "../GAS/HSRAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSREquipmentEffectContractTest,"HSR.Equipment.Effect.Contract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHSREquipmentEffectContractTest::RunTest(const FString&)
{
 FHSREquipmentInstance I; I.InstanceId=FGuid::NewGuid(); I.DefinitionId=TEXT("Weapon"); FHSREquipmentModifier M; M.Stat=EHSREquipmentStat::Attack; M.Value=3.f; I.Modifiers.Add(M); FHSREquipmentLoadout L; L.Equipment.Add(EHSREquipmentSlot::Weapon,I); FHSREquipmentAggregate A; TestTrue(TEXT("Aggregate valid"),UHSREquipmentStatAggregator::Aggregate(L,1,A)); TestEqual(TEXT("Attack aggregate"),A.Attack,3.f);
 UHSREquipmentEffectBridge* B=NewObject<UHSREquipmentEffectBridge>(); TestFalse(TEXT("Missing ASC rejected"),B->Apply(I.InstanceId,nullptr,nullptr,A)); TestTrue(TEXT("Remove unknown is idempotent"),B->Remove(I.InstanceId));
 UHSRBattleCoordinator* C=NewObject<UHSRBattleCoordinator>(); TestFalse(TEXT("Coordinator rejects absent participant"),C->ApplyEquipmentSource(TEXT("Missing"),I.InstanceId,A,1));
 UWorld* World=UWorld::CreateWorld(EWorldType::GamePreview,false); AActor* Owner=World->SpawnActor<AActor>(); UHSRAbilitySystemComponent* ASC=NewObject<UHSRAbilitySystemComponent>(Owner); Owner->AddInstanceComponent(ASC); ASC->RegisterComponent(); ASC->InitAbilityActorInfo(Owner,Owner); TSubclassOf<UGameplayEffect> EffectClass=LoadClass<UGameplayEffect>(nullptr,TEXT("/Game/GameplayEffects/GE_Equipment_P12.GE_Equipment_P12_C")); TestNotNull(TEXT("Authored equipment effect loads"),EffectClass.Get()); TestTrue(TEXT("Initial source applies"),B->Apply(I.InstanceId,ASC,EffectClass,A)); const FActiveGameplayEffectHandle FirstHandle=B->GetSourceHandleForDevelopmentTest(I.InstanceId); TestTrue(TEXT("Initial handle active"),FirstHandle.IsValid()&&ASC->GetActiveGameplayEffect(FirstHandle)); TestTrue(TEXT("External removal succeeds"),ASC->RemoveActiveGameplayEffect(FirstHandle)); A.Revision=2; TestTrue(TEXT("Stale source reapplies"),B->Apply(I.InstanceId,ASC,EffectClass,A)); const FActiveGameplayEffectHandle ReappliedHandle=B->GetSourceHandleForDevelopmentTest(I.InstanceId); TestTrue(TEXT("Reapplied handle active"),ReappliedHandle.IsValid()&&ASC->GetActiveGameplayEffect(ReappliedHandle)); TestNotEqual(TEXT("Stale handle replaced"),ReappliedHandle.ToString(),FirstHandle.ToString()); World->DestroyWorld(false);
 return true;
}
#endif
