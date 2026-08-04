#pragma once

#include "CoreMinimal.h"
#include "../Battle/HSREncounterTypes.h"
#include "HSRPreBattleCandidateTypes.h"
#include "HSRPreBattleCandidateViewModel.generated.h"

class UHSRPartySubsystem;
class UHSRCharacterProfileSubsystem;

UCLASS(BlueprintType)
class HSR_API UHSRPreBattleCandidateViewModel : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UHSRPartySubsystem* InParty, UHSRCharacterProfileSubsystem* InProfiles,
		const FHSREncounterRequest& InTemplate);

	UFUNCTION(BlueprintPure, Category = "HSR|PreBattle")
	FHSRPreBattleCandidateSnapshot GetSnapshot() const { return Snapshot; }

	UFUNCTION(BlueprintCallable, Category = "HSR|PreBattle")
	EHSRPreBattleCandidateResult SetCandidateSlot(int32 SlotIndex, FName CharacterId);

	UFUNCTION(BlueprintCallable, Category = "HSR|PreBattle")
	EHSRPreBattleCandidateResult SetBuff(FName BuffId);

	UFUNCTION(BlueprintCallable, Category = "HSR|PreBattle")
	EHSRPreBattleCandidateResult ConfirmCandidate(UPARAM(ref) FHSREncounterRequest& OutRequest);

	UFUNCTION(BlueprintCallable, Category = "HSR|PreBattle")
	EHSRPreBattleCandidateResult CancelCandidate();

private:
	bool IsKnownProfile(FName CharacterId) const;
	bool ContainsCandidate(FName CharacterId, int32 IgnoreSlot = INDEX_NONE) const;
	void RebuildSnapshot();

	TWeakObjectPtr<UHSRPartySubsystem> Party;
	TWeakObjectPtr<UHSRCharacterProfileSubsystem> Profiles;
	FHSREncounterRequest Template;
	TArray<FName> CandidateCharacterIds;
	TArray<FName> BuffIds;
	FHSRPreBattleCandidateSnapshot Snapshot;
};
