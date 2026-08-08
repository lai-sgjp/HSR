#pragma once

#include "CoreMinimal.h"
#include "HSREquipmentTypes.generated.h"

FORCEINLINE FGuid HSRCharacterGuidFromProfileName(const FName& CharacterId)
{
	const FTCHARToUTF8 Utf8(*CharacterId.ToString());
	uint32 StableHash = 2166136261u;
	for (int32 Index = 0; Index < Utf8.Length(); ++Index)
	{
		StableHash ^= static_cast<uint8>(Utf8.Get()[Index]);
		StableHash *= 16777619u;
	}
	return FGuid(0, StableHash, 0, 1);
}

UENUM(BlueprintType)
enum class EHSREquipmentKind : uint8
{
	Equipment,
	Relic
};

UENUM(BlueprintType)
enum class EHSREquipmentSlot : uint8
{
	Weapon,
	Head,
	Hands,
	Body,
	Feet
};

UENUM(BlueprintType)
enum class EHSRRelicSlot : uint8
{
	Head,
	Hands,
	Body,
	Feet,
	PlanarSphere,
	LinkRope
};

UENUM(BlueprintType)
enum class EHSREquipmentStat : uint8
{
	MaxHealth,
	Attack,
	Defense,
	Speed
};

UENUM(BlueprintType)
enum class EHSREquipmentOperationResult : uint8
{
	Success,
	NoOp,
	InvalidCharacterId,
	InvalidDefinitionId,
	InvalidInstanceId,
	DuplicateDefinitionId,
	UnknownDefinition,
	InvalidSlot,
	SlotOccupied,
	InstanceAlreadyEquipped,
	InstancePayloadConflict,
	TargetNotFound,
	InstanceMismatch,
	InvalidModifier,
	InvalidEnhancementLevel
};

UENUM(BlueprintType)
enum class EHSREquipmentMovementIntent : uint8
{
	Equip,
	Unequip,
	Replace
};

UENUM(BlueprintType)
enum class EHSREquipmentMovementResultCode : uint8
{
	Success,
	InvalidRequest,
	InventoryRevisionConflict,
	EquipmentRevisionConflict,
	MappingRejected,
	InventoryRejected,
	EquipmentRejected,
	ProjectionRejected,
	OperationIdConflict
};

UENUM(BlueprintType)
enum class EHSREquipmentEnhancementResultCode : uint8
{
	Success,
	NoOp,
	InvalidRequest,
	InventoryRevisionConflict,
	EquipmentRevisionConflict,
	EnhancementLevelConflict,
	CatalogRejected,
	EquipmentRejected,
	InventoryRejected,
	ProjectionRejected,
	OperationIdConflict
};

USTRUCT(BlueprintType)
struct FHSREquipmentMovementRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite) FGuid OperationId;
	UPROPERTY(BlueprintReadWrite) FGuid CharacterId;
	UPROPERTY(BlueprintReadWrite) FGuid InstanceId;
	UPROPERTY(BlueprintReadWrite) EHSREquipmentMovementIntent Intent = EHSREquipmentMovementIntent::Equip;
	UPROPERTY(BlueprintReadWrite) EHSREquipmentKind Kind = EHSREquipmentKind::Equipment;
	UPROPERTY(BlueprintReadWrite) int32 Slot = 0;
	UPROPERTY(BlueprintReadWrite) int64 ExpectedInventoryRevision = 0;
	UPROPERTY(BlueprintReadWrite) int32 ExpectedEquipmentRevision = 0;
};

USTRUCT(BlueprintType)
struct FHSREquipmentMovementResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FGuid OperationId;
	UPROPERTY(BlueprintReadOnly) EHSREquipmentMovementResultCode Code = EHSREquipmentMovementResultCode::InvalidRequest;
	UPROPERTY(BlueprintReadOnly) bool bCommitted = false;
	UPROPERTY(BlueprintReadOnly) bool bReplay = false;
	UPROPERTY(BlueprintReadOnly) int64 OldInventoryRevision = 0;
	UPROPERTY(BlueprintReadOnly) int64 NewInventoryRevision = 0;
	UPROPERTY(BlueprintReadOnly) int32 OldEquipmentRevision = 0;
	UPROPERTY(BlueprintReadOnly) int32 NewEquipmentRevision = 0;
	UPROPERTY(BlueprintReadOnly) FGuid DisplacedInstanceId;
};

USTRUCT(BlueprintType)
struct FHSREquipmentEnhancementRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite) FGuid OperationId;
	UPROPERTY(BlueprintReadWrite) FGuid CharacterId;
	UPROPERTY(BlueprintReadWrite) FGuid InstanceId;
	UPROPERTY(BlueprintReadWrite) EHSREquipmentKind Kind = EHSREquipmentKind::Equipment;
	UPROPERTY(BlueprintReadWrite) int64 ExpectedInventoryRevision = 0;
	UPROPERTY(BlueprintReadWrite) int32 ExpectedEquipmentRevision = 0;
	UPROPERTY(BlueprintReadWrite) int32 ExpectedEnhancementLevel = 0;
	UPROPERTY(BlueprintReadWrite) int32 TargetLevel = 0;
};

USTRUCT(BlueprintType)
struct FHSREquipmentEnhancementResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FGuid OperationId;
	UPROPERTY(BlueprintReadOnly) EHSREquipmentEnhancementResultCode Code = EHSREquipmentEnhancementResultCode::InvalidRequest;
	UPROPERTY(BlueprintReadOnly) bool bCommitted = false;
	UPROPERTY(BlueprintReadOnly) bool bReplay = false;
	UPROPERTY(BlueprintReadOnly) int64 OldInventoryRevision = 0;
	UPROPERTY(BlueprintReadOnly) int64 NewInventoryRevision = 0;
	UPROPERTY(BlueprintReadOnly) int32 OldEquipmentRevision = 0;
	UPROPERTY(BlueprintReadOnly) int32 NewEquipmentRevision = 0;
	UPROPERTY(BlueprintReadOnly) int32 OldEnhancementLevel = 0;
	UPROPERTY(BlueprintReadOnly) int32 NewEnhancementLevel = 0;
};

USTRUCT(BlueprintType)
struct FHSREquipmentModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EHSREquipmentStat Stat = EHSREquipmentStat::Attack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Value = 0.0f;
};

USTRUCT(BlueprintType)
struct FHSREquipmentInstance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGuid InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName DefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EHSREquipmentKind Kind = EHSREquipmentKind::Equipment;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 EnhancementLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FHSREquipmentModifier> Modifiers;
};

USTRUCT(BlueprintType)
struct FHSREquipmentLoadout
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<EHSREquipmentSlot, FHSREquipmentInstance> Equipment;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<EHSRRelicSlot, FHSREquipmentInstance> Relics;
};
