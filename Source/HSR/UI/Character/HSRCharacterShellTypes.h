#pragma once

#include "CoreMinimal.h"
#include "../HSRCharacterDetailTypes.h"
#include "../HSREquipmentDetailTypes.h"
#include "HSRCharacterShellTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRCharacterShellTab : uint8
{
	Detail,
	Weapon,
	Traces,
	Relics,
	Eidolon,
	Information,
	Outfit
};

UENUM(BlueprintType)
enum class EHSRCharacterShellResult : uint8
{
	Success,
	NotInitialized,
	EmptyList,
	InvalidCharacterId,
	CharacterUnavailable,
	InvalidTab
};

USTRUCT(BlueprintType)
struct HSR_API FHSRCharacterShellEntrySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName CharacterId;
	UPROPERTY(BlueprintReadOnly) FText DisplayName;
	UPROPERTY(BlueprintReadOnly) bool bIsSelected = false;
	UPROPERTY(BlueprintReadOnly) bool bIsAvailable = false;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRCharacterShellSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) TArray<FHSRCharacterShellEntrySnapshot> CharacterEntries;
	UPROPERTY(BlueprintReadOnly) FName SelectedCharacterId;
	UPROPERTY(BlueprintReadOnly) EHSRCharacterShellTab SelectedTab = EHSRCharacterShellTab::Detail;
	UPROPERTY(BlueprintReadOnly) FHSRCharacterDetailSnapshot CharacterDetail;
	UPROPERTY(BlueprintReadOnly) FHSREquipmentDetailSnapshot EquipmentDetail;
	UPROPERTY(BlueprintReadOnly) bool bIsValid = false;
	UPROPERTY(BlueprintReadOnly) EHSRCharacterShellResult FailureReason = EHSRCharacterShellResult::NotInitialized;
	UPROPERTY(BlueprintReadOnly) bool bSelectedTabAvailable = false;
	UPROPERTY(BlueprintReadOnly) EHSRCharacterShellResult SelectedTabFailureReason = EHSRCharacterShellResult::NotInitialized;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRCharacterShellChanged, const FHSRCharacterShellSnapshot&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSRCharacterShellBlueprintChanged,
	const FHSRCharacterShellSnapshot&, Snapshot);
