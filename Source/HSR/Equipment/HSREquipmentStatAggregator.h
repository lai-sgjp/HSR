#pragma once
#include "CoreMinimal.h"
#include "HSREquipmentTypes.h"
#include "HSREquipmentStatAggregator.generated.h"

USTRUCT(BlueprintType)
struct FHSREquipmentAggregate
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) float MaxHealth=0.f;
 UPROPERTY(BlueprintReadOnly) float Attack=0.f;
 UPROPERTY(BlueprintReadOnly) float Defense=0.f;
 UPROPERTY(BlueprintReadOnly) float Speed=0.f;
 UPROPERTY(BlueprintReadOnly) int64 Revision=0;
};

UCLASS()
class HSR_API UHSREquipmentStatAggregator : public UObject
{
 GENERATED_BODY()
public:
 static bool Aggregate(const FHSREquipmentLoadout& Loadout, int64 Revision, FHSREquipmentAggregate& Out);
 static bool AddInstance(const FHSREquipmentInstance& Instance, FHSREquipmentAggregate& InOut);
};
