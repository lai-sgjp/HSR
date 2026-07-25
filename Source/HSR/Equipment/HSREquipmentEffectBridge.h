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
 bool Apply(const FGuid& InstanceId,UAbilitySystemComponent* ASC,TSubclassOf<UGameplayEffect> EffectClass,const FHSREquipmentAggregate& Aggregate);
 bool SetSourceId(const FGuid& InstanceId,UAbilitySystemComponent* ASC,TSubclassOf<UGameplayEffect> EffectClass,const FHSREquipmentAggregate& Aggregate) { return Apply(InstanceId,ASC,EffectClass,Aggregate); }
 bool ApplySetSource(const FName& SetSourceId,UAbilitySystemComponent* ASC,TSubclassOf<UGameplayEffect> EffectClass,const FHSREquipmentAggregate& Aggregate);
 bool RemoveSetSource(const FName& SetSourceId);
 bool Remove(const FGuid& InstanceId);
 bool RemoveAll();
 int32 GetActiveSourceCount() const { return Sources.Num(); }
#if WITH_EDITOR
 FActiveGameplayEffectHandle GetSourceHandleForDevelopmentTest(const FGuid& InstanceId) const { const FSource* Source=Sources.Find(InstanceId); return Source?Source->Handle:FActiveGameplayEffectHandle(); }
#endif
private:
 struct FSource { TWeakObjectPtr<UAbilitySystemComponent> ASC; TSubclassOf<UGameplayEffect> Class; FHSREquipmentAggregate Fingerprint; FActiveGameplayEffectHandle Handle; };
 TMap<FGuid,FSource> Sources;
 TMap<FName,FGuid> SetSourceKeys;
};
