#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSRSaveTypes.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../Quest/HSRQuestSubsystem.h"
#include "HSRSaveSubsystem.generated.h"

UCLASS()
class HSR_API UHSRSaveSubsystem : public UGameInstanceSubsystem { GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	EHSRSaveResult SaveSnapshot(FHSRSaveData& OutData);
	EHSRSaveResult LoadSnapshot(const FHSRSaveData& Candidate);
	EHSRSaveResult SaveToSlot(const FString& SlotName, int32 UserIndex = 0);
	EHSRSaveResult LoadFromSlot(const FString& SlotName, int32 UserIndex = 0);
	const FHSRSaveData& GetSnapshot() const { return Current; }
	FHSRRestoreCommitted& OnRestoreCommitted() { return RestoreCommitted; }
#if WITH_DEV_AUTOMATION_TESTS
	void SetDiskFailureInjection(bool bCreate, bool bSave, bool bLoad) { bInjectCreateFailure=bCreate;bInjectSaveFailure=bSave;bInjectLoadFailure=bLoad; }
#endif
#if WITH_EDITOR
	void InitializeForDevelopmentTest(UHSRCharacterProfileSubsystem* InProfiles, UHSRPartySubsystem* InParty, UHSREquipmentSubsystem* InEquipment=nullptr, UHSRInventorySubsystem* InInventory=nullptr, UHSRRewardSubsystem* InReward=nullptr, UHSRQuestSubsystem* InQuest=nullptr);
	int64 GetRestoreTransactionRevisionForDevelopmentTest() const { return RestoreTransactionRevision; }
#endif
private:
	bool Validate(const FHSRSaveData& Candidate) const;
	UPROPERTY() FHSRSaveData Current;
	TWeakObjectPtr<UHSRCharacterProfileSubsystem> Profiles;
	TWeakObjectPtr<UHSRPartySubsystem> Party;
	TWeakObjectPtr<UHSREquipmentSubsystem> Equipment;
	TWeakObjectPtr<UHSRInventorySubsystem> Inventory;
	TWeakObjectPtr<UHSRRewardSubsystem> Reward;
	TWeakObjectPtr<UHSRQuestSubsystem> Quest;
#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TObjectPtr<UHSRInventorySubsystem> DevelopmentInventory;
	UPROPERTY(Transient)
	TObjectPtr<UHSRRewardSubsystem> DevelopmentReward;
	UPROPERTY(Transient)
	TObjectPtr<UHSRQuestSubsystem> DevelopmentQuest;
#endif
	int64 RestoreTransactionRevision=0;
	FHSRRestoreCommitted RestoreCommitted;
#if WITH_DEV_AUTOMATION_TESTS
	bool bInjectCreateFailure=false,bInjectSaveFailure=false,bInjectLoadFailure=false;
#endif
};
