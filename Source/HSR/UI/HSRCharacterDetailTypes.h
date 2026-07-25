#pragma once
#include "CoreMinimal.h"
#include "../Progression/HSRCharacterDerivedStats.h"
#include "HSRCharacterDetailTypes.generated.h"

UENUM(BlueprintType) enum class EHSRCharacterDetailResult:uint8 { Success, NotInitialized, InvalidCharacterId, ProfileNotFound, DefinitionNotFound, InvalidSnapshot, PartySlotEmpty };
USTRUCT(BlueprintType) struct HSR_API FHSRCharacterDetailSkill { GENERATED_BODY() UPROPERTY(BlueprintReadOnly) FName SkillId; UPROPERTY(BlueprintReadOnly) int32 Level=0; UPROPERTY(BlueprintReadOnly) int32 MaxLevel=0; };
USTRUCT(BlueprintType) struct HSR_API FHSRCharacterDetailSnapshot { GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName CharacterId; UPROPERTY(BlueprintReadOnly) FText DisplayName; UPROPERTY(BlueprintReadOnly) int32 Level=1; UPROPERTY(BlueprintReadOnly) int32 MaxLevel=1; UPROPERTY(BlueprintReadOnly) int32 Experience=0;
	/** Cumulative experience thresholds for the current level and its next level. */
	UPROPERTY(BlueprintReadOnly) int32 ExperienceForCurrentLevel=0; UPROPERTY(BlueprintReadOnly) int32 ExperienceForNextLevel=0; UPROPERTY(BlueprintReadOnly) bool bAtMaxLevel=false;
	UPROPERTY(BlueprintReadOnly) int32 Ascension=0; UPROPERTY(BlueprintReadOnly) int64 RuntimeRevision=0;
	/** Authored base layer, additive progression layer, and final aggregated display values. */
	UPROPERTY(BlueprintReadOnly) FHSRCharacterDerivedStats BaseStats; UPROPERTY(BlueprintReadOnly) FHSRCharacterDerivedStats ProgressionBonuses; UPROPERTY(BlueprintReadOnly) FHSRCharacterDerivedStats DerivedStats;
	UPROPERTY(BlueprintReadOnly) FSoftObjectPath PortraitPath; UPROPERTY(BlueprintReadOnly) bool bHasPortrait=false;
	UPROPERTY(BlueprintReadOnly) bool bIsValid=false; UPROPERTY(BlueprintReadOnly) EHSRCharacterDetailResult FailureReason=EHSRCharacterDetailResult::InvalidSnapshot;
	UPROPERTY(BlueprintReadOnly) TArray<FHSRCharacterDetailSkill> Skills;
};
DECLARE_MULTICAST_DELEGATE_OneParam(FHSRCharacterDetailChanged,const FHSRCharacterDetailSnapshot&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSRCharacterDetailBlueprintChanged,const FHSRCharacterDetailSnapshot&,Snapshot);
