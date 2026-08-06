#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSRSaveTypes.h"
#include "HSRSaveVersion.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../Quest/HSRQuestSubsystem.h"
#include "../Map/HSRMapSubsystem.h"
#include "../Challenge/HSRChallengeProgressionSubsystem.h"
#include "HSRSaveSubsystem.generated.h"

class UHSRItemEquipmentMappingCatalog;

UCLASS()
class HSR_API UHSRSaveSubsystem : public UGameInstanceSubsystem { GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	EHSRSaveResult SaveSnapshot(FHSRSaveData& OutData);
	EHSRSaveResult LoadSnapshot(const FHSRSaveData& Candidate);
	EHSRSaveResult SaveToSlot(const FString& SlotName, int32 UserIndex = 0);
	EHSRSaveResult LoadFromSlot(const FString& SlotName, int32 UserIndex = 0);
	bool GetSlotSummary(const FString& SlotName, int32 UserIndex, FHSRSaveSlotSummary& OutSummary) const;
	const FHSRSaveLoadResult& GetLastLoadResult() const { return LastLoadResult; }
	bool HasPendingRestore() const { return PendingRestoreCandidate.IsSet(); }
	const FHSRSaveData& GetSnapshot() const { return Current; }
	FHSRRestoreCommitted& OnRestoreCommitted() { return RestoreCommitted; }
	FHSRSaveLoadCompleted& OnLoadCompleted() { return LoadCompleted; }
	EHSRSaveFailureStage GetLastWriteFailureStage() const { return LastWriteFailureStage; }
	bool HadLastWriteCleanupWarning() const { return bLastWriteCleanupWarning; }
	const FHSRSaveEnvelopeHeader& GetLastWriteHeader() const { return LastWriteHeader; }
#if WITH_DEV_AUTOMATION_TESTS
	void SetDiskFailureInjection(bool bCreate, bool bSave, bool bLoad) { bInjectCreateFailure=bCreate;bInjectSaveFailure=bSave;bInjectLoadFailure=bLoad; }
	void SetTransactionFailureInjection(EHSRSaveFailureStage InStage) { InjectedTransactionStage=InStage; }
	void SetOperationBusyForAutomation(bool bBusy) { bOperationInProgress=bBusy; }
	void SetRestoreBlockedForAutomation(bool bMapTravel, bool bBattleReturn) { bInjectMapTravelPending=bMapTravel;bInjectBattleReturnPending=bBattleReturn; }
#endif
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	void InitializeForDevelopmentTest(UHSRCharacterProfileSubsystem* InProfiles, UHSRPartySubsystem* InParty, UHSREquipmentSubsystem* InEquipment=nullptr, UHSRInventorySubsystem* InInventory=nullptr, UHSRRewardSubsystem* InReward=nullptr, UHSRQuestSubsystem* InQuest=nullptr, UHSRMapSubsystem* InMap=nullptr, UHSRItemEquipmentMappingCatalog* InMappingCatalog=nullptr);
	int64 GetRestoreTransactionRevisionForDevelopmentTest() const { return RestoreTransactionRevision; }
#endif
private:
	bool Validate(const FHSRSaveData& Candidate) const;
	bool CanPrepareSnapshot(const FHSRSaveData& Candidate) const;
	void HandleRestoreArrival(const FHSRMapArrivalCommitInfo& Info);
	void HandleRestoreTravelFailure(const FGuid& RequestId);
	UPROPERTY() FHSRSaveData Current;
	TWeakObjectPtr<UHSRCharacterProfileSubsystem> Profiles;
	TWeakObjectPtr<UHSRPartySubsystem> Party;
	TWeakObjectPtr<UHSREquipmentSubsystem> Equipment;
	TWeakObjectPtr<UHSRInventorySubsystem> Inventory;
	TWeakObjectPtr<UHSRRewardSubsystem> Reward;
	TWeakObjectPtr<UHSRQuestSubsystem> Quest;
	TWeakObjectPtr<UHSRMapSubsystem> Map;
	TWeakObjectPtr<UHSRChallengeProgressionSubsystem> ChallengeProgression;
	UPROPERTY(Transient)
	TObjectPtr<UHSRItemEquipmentMappingCatalog> MappingCatalog;
#if WITH_EDITORONLY_DATA || WITH_DEV_AUTOMATION_TESTS
	UPROPERTY(Transient)
	TObjectPtr<UHSRInventorySubsystem> DevelopmentInventory;
	UPROPERTY(Transient)
	TObjectPtr<UHSRRewardSubsystem> DevelopmentReward;
	UPROPERTY(Transient)
	TObjectPtr<UHSRQuestSubsystem> DevelopmentQuest;
	UPROPERTY(Transient)
	TObjectPtr<UHSRMapSubsystem> DevelopmentMap;
	UPROPERTY(Transient)
	TObjectPtr<UHSRChallengeProgressionSubsystem> DevelopmentChallengeProgression;
#endif
	int64 RestoreTransactionRevision=0;
	FHSRRestoreCommitted RestoreCommitted;
	FHSRSaveLoadCompleted LoadCompleted;
	FHSRSaveLoadResult LastLoadResult;
	bool bOperationInProgress=false;
	bool bCompletingRestoreTravel=false;
	TOptional<FHSRSaveData> PendingRestoreCandidate;
	FGuid PendingRestoreRequestId;
	EHSRSaveFailureStage LastWriteFailureStage=EHSRSaveFailureStage::None;
	bool bLastWriteCleanupWarning=false;
	FHSRSaveEnvelopeHeader LastWriteHeader;
#if WITH_DEV_AUTOMATION_TESTS
	bool bInjectCreateFailure=false,bInjectSaveFailure=false,bInjectLoadFailure=false;
	EHSRSaveFailureStage InjectedTransactionStage=EHSRSaveFailureStage::None;
	bool bInjectMapTravelPending=false,bInjectBattleReturnPending=false;
#endif
};
