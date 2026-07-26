#pragma once

#include "CoreMinimal.h"
#include "HSRRewardTypes.h"

class HSR_API FHSRRewardResolver
{
public:
	static constexpr int32 MaxDropRolls = 100;
	static constexpr int32 MaxDefinitionEntries = 128;
	static constexpr int32 MaxResolvedGrants = 128;
	static bool Resolve(const FHSRRewardDefinitionRule& Reward, const FHSRDropTableRule* DropTable, int32 Seed, TArray<FHSRRewardItemEntry>& OutItems);
};
