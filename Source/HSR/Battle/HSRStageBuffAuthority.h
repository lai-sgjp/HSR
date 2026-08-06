#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HSRStageBuffAuthority.generated.h"

class UHSRStageBuffDefinition;

USTRUCT()
struct FHSRStageBuffEncounterRegistry
{
	GENERATED_BODY()

	UPROPERTY()
	FName EncounterId;

	UPROPERTY()
	TArray<TObjectPtr<UHSRStageBuffDefinition>> Definitions;
};

UCLASS()
class HSR_API UHSRStageBuffAuthority : public UObject
{
	GENERATED_BODY()

public:
	bool RegisterEncounterBuffs(FName EncounterId, const TArray<UHSRStageBuffDefinition*>& Definitions);
	bool ValidateBuffIds(FName EncounterId, const TArray<FName>& BuffIds) const;
	const UHSRStageBuffDefinition* FindBuff(FName EncounterId, FName BuffId) const;
	void Reset();

private:
	UPROPERTY()
	TArray<FHSRStageBuffEncounterRegistry> EncounterRegistries;
};
