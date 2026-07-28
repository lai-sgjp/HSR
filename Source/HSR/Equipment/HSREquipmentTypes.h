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
