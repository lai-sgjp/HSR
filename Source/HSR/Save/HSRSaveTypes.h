#pragma once
#include "CoreMinimal.h"
#include "../Progression/HSRCharacterProgressionTypes.h"
#include "../Party/HSRPartyTypes.h"
#include "HSRSaveTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRSaveResult : uint8 { Success, InvalidArgument, SlotNotFound, CreateFailed, SaveFailed, LoadFailed, ClassMismatch, UnsupportedSchema, InvalidData };

USTRUCT(BlueprintType)
struct HSR_API FHSRSaveProfileDto { GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) FHSRCharacterRuntimeState State;
	UPROPERTY(BlueprintReadWrite) int64 RuntimeRevision = 0;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRSaveData { GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) int32 SchemaVersion = 1;
	UPROPERTY(BlueprintReadWrite) TArray<FHSRSaveProfileDto> Profiles;
	UPROPERTY(BlueprintReadWrite) TArray<FHSRPartySlot> PartySlots;
	UPROPERTY(BlueprintReadWrite) int64 PartyRevision = 0;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRRestoreCommitInfo { GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TArray<FName> ChangedCharacterIds;
	UPROPERTY(BlueprintReadOnly) bool bPartyChanged = false;
	UPROPERTY(BlueprintReadOnly) int64 TransactionRevision = 0;
};
DECLARE_MULTICAST_DELEGATE_OneParam(FHSRRestoreCommitted,const FHSRRestoreCommitInfo&);
