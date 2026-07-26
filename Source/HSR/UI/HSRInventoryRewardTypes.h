#pragma once

#include "CoreMinimal.h"
#include "../Inventory/HSRItemTypes.h"
#include "../Reward/HSRRewardTypes.h"
#include "HSRInventoryRewardTypes.generated.h"

USTRUCT(BlueprintType)
struct FHSRInventoryRewardSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FHSRInventorySnapshot Inventory;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	TArray<FHSRRewardReceipt> Receipts;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRInventoryRewardChanged, const FHSRInventoryRewardSnapshot&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSRInventoryRewardBlueprintChanged, const FHSRInventoryRewardSnapshot&, Snapshot);
