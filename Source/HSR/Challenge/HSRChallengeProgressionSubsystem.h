#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSRChallengeProgressionTypes.h"
#include "HSRChallengeProgressionSubsystem.generated.h"

UCLASS()
class HSR_API UHSRChallengeProgressionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	EHSRChallengeProgressionResult CompleteEncounter(FName EncounterId);
	bool IsCompleted(FName EncounterId) const;
	bool IsValidEncounterId(FName EncounterId) const { return !EncounterId.IsNone(); }
	const FHSRChallengeProgressionSnapshot& GetSnapshot() const { return Snapshot; }
	FHSRChallengeProgressionChanged& OnProgressionChanged() { return ProgressionChanged; }
	void ExportSaveData(FHSRChallengeProgressionSaveData& OutData) const;

	static bool ValidateSaveData(const FHSRChallengeProgressionSaveData& Data);

private:
	friend class UHSRSaveSubsystem;

	bool PrepareRestore(const FHSRChallengeProgressionSaveData& Saved,
		FHSRChallengeProgressionSaveData& OutCandidate) const;
	bool IsRestoreDifferent(const FHSRChallengeProgressionSaveData& Candidate) const;
	void CommitRestore(FHSRChallengeProgressionSaveData&& Candidate, bool bNotify);
	void RebuildSnapshot();

	TSet<FName> CompletedEncounterIds;
	FHSRChallengeProgressionSnapshot Snapshot;
	FHSRChallengeProgressionChanged ProgressionChanged;
};
