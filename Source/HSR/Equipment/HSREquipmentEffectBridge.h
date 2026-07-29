#pragma once
#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "HSREquipmentStatAggregator.h"
#include "HSREquipmentEffectBridge.generated.h"
class UAbilitySystemComponent; class UGameplayEffect;
UCLASS()
class HSR_API UHSREquipmentEffectBridge : public UObject
{
 GENERATED_BODY()
public:
 bool CanApply(UAbilitySystemComponent* ASC,TSubclassOf<UGameplayEffect> EffectClass,const FHSREquipmentAggregate& Aggregate) const;
 bool CanRemove(const FGuid& InstanceId) const;
 bool Apply(const FGuid& InstanceId,UAbilitySystemComponent* ASC,TSubclassOf<UGameplayEffect> EffectClass,const FHSREquipmentAggregate& Aggregate);
 bool SetSourceId(const FGuid& InstanceId,UAbilitySystemComponent* ASC,TSubclassOf<UGameplayEffect> EffectClass,const FHSREquipmentAggregate& Aggregate) { return Apply(InstanceId,ASC,EffectClass,Aggregate); }
 bool ApplySetSource(const FName& SetSourceId,UAbilitySystemComponent* ASC,TSubclassOf<UGameplayEffect> EffectClass,const FHSREquipmentAggregate& Aggregate);
 bool RemoveSetSource(const FName& SetSourceId);
 bool Remove(const FGuid& InstanceId);
 bool RemoveAll();
 int32 GetActiveSourceCount() const { return Sources.Num(); }
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
 FActiveGameplayEffectHandle GetSourceHandleForDevelopmentTest(const FGuid& InstanceId) const { const FSource* Source=Sources.Find(InstanceId); return Source?Source->Handle:FActiveGameplayEffectHandle(); }
 void SetPreflightFailureForDevelopmentTest(bool bApply,bool bRemove) { bForceCanApplyFailure=bApply;bForceCanRemoveFailure=bRemove; }
#endif
private:
 struct FSource { TWeakObjectPtr<UAbilitySystemComponent> ASC; TSubclassOf<UGameplayEffect> Class; FHSREquipmentAggregate Fingerprint; FActiveGameplayEffectHandle Handle; };
 TMap<FGuid,FSource> Sources;
 TMap<FName,FGuid> SetSourceKeys;
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
 bool bForceCanApplyFailure=false;
 bool bForceCanRemoveFailure=false;
#endif
};
