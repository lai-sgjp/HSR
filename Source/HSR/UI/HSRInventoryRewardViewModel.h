#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HSRInventoryRewardTypes.h"
#include "HSRInventoryRewardViewModel.generated.h"

class UHSRInventorySubsystem;
class UHSRRewardSubsystem;

UCLASS(BlueprintType)
class HSR_API UHSRInventoryRewardViewModel : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;
	void Initialize(UHSRInventorySubsystem* InInventory, UHSRRewardSubsystem* InReward);
	void Shutdown();
	UFUNCTION(BlueprintPure, Category = "HSR|Inventory Reward")
	bool GetSnapshot(FHSRInventoryRewardSnapshot& OutSnapshot) const;
	FHSRInventoryRewardChanged& OnChanged() { return Changed; }
	UPROPERTY(BlueprintAssignable, Category = "HSR|Inventory Reward")
	FHSRInventoryRewardBlueprintChanged OnSnapshotChanged;

private:
	void HandleInventoryChanged(int64 Revision);
	void HandleRewardCommitted(const FHSRRewardReceipt& Receipt);
	void HandleRewardRestored(int64 Revision);
	void Rebuild();

	TWeakObjectPtr<UHSRInventorySubsystem> Inventory;
	TWeakObjectPtr<UHSRRewardSubsystem> Reward;
	FDelegateHandle InventoryHandle;
	FDelegateHandle RewardCommittedHandle;
	FDelegateHandle RewardRestoredHandle;
	FHSRInventoryRewardSnapshot Snapshot;
	bool bHasSnapshot = false;
	FHSRInventoryRewardChanged Changed;
};
