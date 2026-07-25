#pragma once
#include "CoreMinimal.h"
#include "../Progression/HSRCharacterProgressionTypes.h"
#include "../Party/HSRPartyTypes.h"
#include "../Equipment/HSREquipmentTypes.h"
#include "HSRSaveTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRSaveResult : uint8 { Success, InvalidArgument, SlotNotFound, CreateFailed, SaveFailed, LoadFailed, ClassMismatch, UnsupportedSchema, InvalidData };

USTRUCT(BlueprintType)
struct HSR_API FHSRSaveProfileDto { GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) FHSRCharacterRuntimeState State;
	UPROPERTY(BlueprintReadWrite) int64 RuntimeRevision = 0;
};

USTRUCT(BlueprintType)
struct HSR_API FHSREquipmentSaveDto { GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) FName DefinitionId;
	UPROPERTY(BlueprintReadWrite) FGuid InstanceId;
	UPROPERTY(BlueprintReadWrite) FGuid CharacterId;
	UPROPERTY(BlueprintReadWrite) int32 Kind = 0;
	UPROPERTY(BlueprintReadWrite) int32 Slot = 0;
	UPROPERTY(BlueprintReadWrite) int32 EnhancementLevel = 0;
	UPROPERTY(BlueprintReadWrite) TArray<FHSREquipmentModifier> Modifiers;
	UPROPERTY(BlueprintReadWrite) FName SetId;
	UPROPERTY(BlueprintReadWrite) int32 AuthorityRevision = 0;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRSaveData { GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) int32 SchemaVersion = 2;
	UPROPERTY(BlueprintReadWrite) TArray<FHSRSaveProfileDto> Profiles;
	UPROPERTY(BlueprintReadWrite) TArray<FHSRPartySlot> PartySlots;
	UPROPERTY(BlueprintReadWrite) int64 PartyRevision = 0;
	UPROPERTY(BlueprintReadWrite) TArray<FHSREquipmentSaveDto> Equipment;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRRestoreCommitInfo { GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TArray<FName> ChangedCharacterIds;
	UPROPERTY(BlueprintReadOnly) bool bPartyChanged = false;
	UPROPERTY(BlueprintReadOnly) int64 TransactionRevision = 0;
};
DECLARE_MULTICAST_DELEGATE_OneParam(FHSRRestoreCommitted,const FHSRRestoreCommitInfo&);
