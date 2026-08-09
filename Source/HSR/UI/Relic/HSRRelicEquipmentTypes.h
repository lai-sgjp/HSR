#pragma once

#include "CoreMinimal.h"
#include "../../Equipment/HSREquipmentTypes.h"
#include "HSRRelicEquipmentTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRRelicEquipmentStage : uint8
{
	SlotSelection,
	CandidateSelection,
	Comparison,
	Enhancement
};

UENUM(BlueprintType)
enum class EHSRRelicEquipmentResult : uint8
{
	Success,
	AtRoot,
	NotInitialized,
	InvalidCharacterId,
	InvalidSlot,
	Empty,
	CatalogUnavailable,
	CandidateUnavailable,
	ComparisonUnavailable,
	NoEnhancementOption,
	InvalidTargetLevel,
	AuthorityRejected,
	StaleSnapshot,
	// Appended, not inserted: Blueprints serialize these by value.
	InsufficientMaterial,
	InvalidRequest
};

USTRUCT(BlueprintType)
struct HSR_API FHSRRelicSlotRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) EHSRRelicSlot Slot = EHSRRelicSlot::Head;
	UPROPERTY(BlueprintReadOnly) bool bHasEquipped = false;
	UPROPERTY(BlueprintReadOnly) FGuid EquippedInstanceId;
	UPROPERTY(BlueprintReadOnly) FHSREquipmentInstance EquippedInstance;
	UPROPERTY(BlueprintReadOnly) bool bIsSelected = false;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRRelicCandidateRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FGuid InstanceId;
	UPROPERTY(BlueprintReadOnly) FName ItemId;
	UPROPERTY(BlueprintReadOnly) FName DefinitionId;
	UPROPERTY(BlueprintReadOnly) EHSRRelicSlot Slot = EHSRRelicSlot::Head;
	UPROPERTY(BlueprintReadOnly) FHSREquipmentInstance Instance;
	UPROPERTY(BlueprintReadOnly) bool bIsSelected = false;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRRelicStatDeltaRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) EHSREquipmentStat Stat = EHSREquipmentStat::Attack;
	UPROPERTY(BlueprintReadOnly) float CurrentValue = 0.0f;
	UPROPERTY(BlueprintReadOnly) float CandidateValue = 0.0f;
	UPROPERTY(BlueprintReadOnly) float Delta = 0.0f;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRRelicComparisonSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) bool bIsValid = false;
	UPROPERTY(BlueprintReadOnly) FGuid CurrentInstanceId;
	UPROPERTY(BlueprintReadOnly) FGuid CandidateInstanceId;
	UPROPERTY(BlueprintReadOnly) FHSREquipmentInstance CurrentInstance;
	UPROPERTY(BlueprintReadOnly) FHSREquipmentInstance CandidateInstance;
	UPROPERTY(BlueprintReadOnly) TArray<FHSRRelicStatDeltaRow> StatDeltas;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRRelicEnhancementOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 TargetLevel = 0;
	UPROPERTY(BlueprintReadOnly) FName MaterialItemId;
	UPROPERTY(BlueprintReadOnly) int32 MaterialCost = 0;
	UPROPERTY(BlueprintReadOnly) TArray<FHSREquipmentModifier> TargetModifiers;
	UPROPERTY(BlueprintReadOnly) bool bAffordable = false;
	UPROPERTY(BlueprintReadOnly) bool bAvailable = false;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRRelicEquipmentSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FGuid CharacterId;
	UPROPERTY(BlueprintReadOnly) EHSRRelicEquipmentStage Stage = EHSRRelicEquipmentStage::SlotSelection;
	UPROPERTY(BlueprintReadOnly) EHSRRelicSlot SelectedSlot = EHSRRelicSlot::Head;
	UPROPERTY(BlueprintReadOnly) FGuid SelectedCandidateId;
	UPROPERTY(BlueprintReadOnly) FGuid CurrentInstanceId;
	UPROPERTY(BlueprintReadOnly) int32 CurrentEnhancementLevel = 0;
	UPROPERTY(BlueprintReadOnly) int64 InventoryRevision = 0;
	UPROPERTY(BlueprintReadOnly) int32 EquipmentRevision = 0;
	UPROPERTY(BlueprintReadOnly) TArray<FHSRRelicSlotRow> Slots;
	UPROPERTY(BlueprintReadOnly) TArray<FHSRRelicCandidateRow> Candidates;
	UPROPERTY(BlueprintReadOnly) FHSRRelicComparisonSnapshot Comparison;
	UPROPERTY(BlueprintReadOnly) TArray<FHSRRelicEnhancementOption> EnhancementOptions;
	UPROPERTY(BlueprintReadOnly) bool bIsValid = false;
	UPROPERTY(BlueprintReadOnly) EHSRRelicEquipmentResult FailureReason = EHSRRelicEquipmentResult::NotInitialized;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRRelicEquipmentChanged, const FHSRRelicEquipmentSnapshot&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSRRelicEquipmentBlueprintChanged,
	const FHSRRelicEquipmentSnapshot&, Snapshot);
