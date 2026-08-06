#pragma once
#include "CoreMinimal.h"
#include "../Progression/HSRCharacterProgressionTypes.h"
#include "../Party/HSRPartyTypes.h"
#include "../Equipment/HSREquipmentTypes.h"
#include "../Inventory/HSRItemTypes.h"
#include "../Reward/HSRRewardTypes.h"
#include "../Quest/HSRQuestTypes.h"
#include "../Map/HSRMapTypes.h"
#include "../Challenge/HSRChallengeProgressionTypes.h"
#include "HSRSaveTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRSaveResult : uint8 { Success, InvalidArgument, SlotNotFound, CreateFailed, SaveFailed, LoadFailed, ClassMismatch, UnsupportedSchema, InvalidData, InvalidEnvelope, IntegrityFailed, SlotIdentityMismatch, LegacyData };

UENUM(BlueprintType)
enum class EHSRSaveSlotState : uint8
{
	Empty,
	Ready,
	Recoverable,
	Unavailable
};

UENUM(BlueprintType)
enum class EHSRSaveFailureStage : uint8 { None, Capture, Encode, StagingWrite, StagingReadback, BackupWrite, BackupReadback, PrimaryWrite, PrimaryReadback, Cleanup };

enum class EHSRSaveLoadSource : uint8 { None, Primary, Backup, LegacyPrimary };
enum class EHSRSaveLoadReason : uint8 { None, Missing, InvalidArgument, Busy, TravelPending, LegacyInvalid, PrepareFailed, ProjectionFailed, LineageMismatch, InvalidGeneration, DecodeFailure };

USTRUCT(BlueprintType)
struct HSR_API FHSRSaveSlotSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString SlotName;

	UPROPERTY(BlueprintReadOnly)
	EHSRSaveSlotState State = EHSRSaveSlotState::Empty;

	UPROPERTY(BlueprintReadOnly)
	EHSRSaveResult Result = EHSRSaveResult::SlotNotFound;

	UPROPERTY(BlueprintReadOnly)
	int64 Generation = 0;

	UPROPERTY(BlueprintReadOnly)
	int64 UtcUnixMilliseconds = 0;

	UPROPERTY(BlueprintReadOnly)
	FName MapId = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	int32 PartyMemberCount = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 CompletedChallengeCount = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bPrimaryPresent = false;

	UPROPERTY(BlueprintReadOnly)
	bool bBackupPresent = false;

	UPROPERTY(BlueprintReadOnly)
	bool bPrimaryTrusted = false;

	UPROPERTY(BlueprintReadOnly)
	bool bRecoveredFromBackup = false;
};

struct FHSRSaveLoadResult
{
	EHSRSaveResult Result = EHSRSaveResult::LoadFailed;
	EHSRSaveLoadSource Source = EHSRSaveLoadSource::None;
	uint8 PrimaryReason = 0;
	uint8 BackupReason = 0;
	EHSRSaveLoadReason PrimaryStageReason = EHSRSaveLoadReason::None;
	EHSRSaveLoadReason BackupStageReason = EHSRSaveLoadReason::None;
	FGuid SaveId;
	uint64 Generation = 0;
	bool bPrimaryHeaderTrusted = false;
	bool bRecoveredFromBackup = false;
	bool bPrimaryUntrusted = false;
	bool bRuntimeChanged = false;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRSaveLoadCompleted, const FHSRSaveLoadResult&);

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
struct HSR_API FHSREquipmentRegistryDto { GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) FName DefinitionId;
	UPROPERTY(BlueprintReadWrite) FGuid InstanceId;
	UPROPERTY(BlueprintReadWrite) int32 Kind = 0;
	UPROPERTY(BlueprintReadWrite) int32 EnhancementLevel = 0;
	UPROPERTY(BlueprintReadWrite) TArray<FHSREquipmentModifier> Modifiers;
	UPROPERTY(BlueprintReadWrite) FName SetId;
};

USTRUCT(BlueprintType)
struct HSR_API FHSREquipmentPlacementDto { GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) FGuid InstanceId;
	UPROPERTY(BlueprintReadWrite) FGuid CharacterId;
	UPROPERTY(BlueprintReadWrite) int32 Kind = 0;
	UPROPERTY(BlueprintReadWrite) int32 Slot = 0;
	UPROPERTY(BlueprintReadWrite) int32 AuthorityRevision = 0;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRSaveData { GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) int32 SchemaVersion = 8;
	UPROPERTY(BlueprintReadWrite) TArray<FHSRSaveProfileDto> Profiles;
	UPROPERTY(BlueprintReadWrite) TArray<FHSRPartySlot> PartySlots;
	UPROPERTY(BlueprintReadWrite) int64 PartyRevision = 0;
	UPROPERTY(BlueprintReadWrite) TArray<FHSREquipmentSaveDto> Equipment;
	UPROPERTY(BlueprintReadWrite) TArray<FHSREquipmentRegistryDto> EquipmentRegistry;
	UPROPERTY(BlueprintReadWrite) TArray<FHSREquipmentPlacementDto> EquipmentPlacements;
	UPROPERTY(BlueprintReadWrite) FHSRInventorySaveData Inventory;
	UPROPERTY(BlueprintReadWrite) FHSRRewardSaveData Rewards;
	UPROPERTY(BlueprintReadWrite) FHSRQuestSaveData Quests;
	UPROPERTY(BlueprintReadWrite) FHSRMapSaveData Map;
	UPROPERTY(BlueprintReadWrite) FHSRChallengeProgressionSaveData ChallengeProgression;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRRestoreCommitInfo { GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TArray<FName> ChangedCharacterIds;
	UPROPERTY(BlueprintReadOnly) bool bPartyChanged = false;
	UPROPERTY(BlueprintReadOnly) bool bInventoryChanged = false;
	UPROPERTY(BlueprintReadOnly) bool bRewardsChanged = false;
	UPROPERTY(BlueprintReadOnly) bool bQuestsChanged = false;
	UPROPERTY(BlueprintReadOnly) bool bMapChanged = false;
	UPROPERTY(BlueprintReadOnly) bool bChallengeProgressionChanged = false;
	UPROPERTY(BlueprintReadOnly) int64 TransactionRevision = 0;
};
DECLARE_MULTICAST_DELEGATE_OneParam(FHSRRestoreCommitted,const FHSRRestoreCommitInfo&);
