#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HSRScreenStackTypes.h"
#include "HSRScreenStack.generated.h"

UCLASS()
class HSR_API UHSRScreenStack : public UObject
{
	GENERATED_BODY()

public:
	EHSRScreenStackResult SubmitRequest(const FHSRScreenRequest& Request);
	const FHSRScreenStackSnapshot& GetSnapshot() const { return Snapshot; }
	void RestoreSnapshotForTransaction(const FHSRScreenStackSnapshot& InSnapshot) { Snapshot = InSnapshot; }
	bool GetActiveEntry(FHSRScreenStackEntry& OutEntry) const;

private:
	static int32 FindActiveEntryIndex(const TArray<FHSRScreenStackEntry>& Entries);
	static EHSRScreenStackResult ValidateRequest(const FHSRScreenRequest& Request);
	static bool ContainsScreen(const TArray<FHSRScreenStackEntry>& Entries, FName ScreenId);
	EHSRScreenStackResult ApplyToCandidate(const FHSRScreenRequest& Request, TArray<FHSRScreenStackEntry>& Candidate) const;

	UPROPERTY(Transient)
	FHSRScreenStackSnapshot Snapshot;
};
