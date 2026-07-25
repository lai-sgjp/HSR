#pragma once
#include "CoreMinimal.h"
#include "../Equipment/HSREquipmentTypes.h"
#include "HSREquipmentDetailTypes.generated.h"
UENUM(BlueprintType) enum class EHSREquipmentDetailResult:uint8 { Success, Empty, NotInitialized, RestoreFailed, InvalidSnapshot };
USTRUCT(BlueprintType) struct HSR_API FHSREquipmentSourceRow { GENERATED_BODY() UPROPERTY(BlueprintReadOnly) FName SourceId; UPROPERTY(BlueprintReadOnly) FName DefinitionId; UPROPERTY(BlueprintReadOnly) EHSREquipmentStat Stat=EHSREquipmentStat::Attack; UPROPERTY(BlueprintReadOnly) float AuthoredValue=0; UPROPERTY(BlueprintReadOnly) float EffectiveValue=0; };
USTRUCT(BlueprintType) struct HSR_API FHSRRelicSetDetailRow { GENERATED_BODY() UPROPERTY(BlueprintReadOnly) FName SetId; UPROPERTY(BlueprintReadOnly) FName SetSourceId; UPROPERTY(BlueprintReadOnly) int32 EquippedCount=0; UPROPERTY(BlueprintReadOnly) int32 Threshold=2; UPROPERTY(BlueprintReadOnly) bool bActive=false; };
USTRUCT(BlueprintType) struct HSR_API FHSREquipmentDetailSnapshot { GENERATED_BODY() UPROPERTY(BlueprintReadOnly) FGuid CharacterId; UPROPERTY(BlueprintReadOnly) TArray<FHSREquipmentInstance> Items; UPROPERTY(BlueprintReadOnly) TArray<FHSREquipmentSourceRow> Sources; UPROPERTY(BlueprintReadOnly) TArray<FHSRRelicSetDetailRow> RelicSets; UPROPERTY(BlueprintReadOnly) float MaxHealth=0; UPROPERTY(BlueprintReadOnly) float Attack=0; UPROPERTY(BlueprintReadOnly) float Defense=0; UPROPERTY(BlueprintReadOnly) float Speed=0; UPROPERTY(BlueprintReadOnly) int32 Revision=0; UPROPERTY(BlueprintReadOnly) bool bRestoring=false; UPROPERTY(BlueprintReadOnly) bool bIsValid=false; UPROPERTY(BlueprintReadOnly) EHSREquipmentDetailResult FailureReason=EHSREquipmentDetailResult::InvalidSnapshot; };
DECLARE_MULTICAST_DELEGATE_OneParam(FHSREquipmentDetailChanged,const FHSREquipmentDetailSnapshot&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSREquipmentDetailBlueprintChanged,const FHSREquipmentDetailSnapshot&,Snapshot);
