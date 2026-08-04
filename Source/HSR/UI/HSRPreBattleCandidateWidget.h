#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSRPreBattleCandidateViewModel.h"
#include "HSRPreBattleCandidateWidget.generated.h"

UCLASS(Blueprintable)
class HSR_API UHSRPreBattleCandidateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HSR|PreBattle")
	void InitializeCandidate(const FHSREncounterRequest& Template);

	UFUNCTION(BlueprintCallable, Category = "HSR|PreBattle")
	EHSRPreBattleCandidateResult SetCandidateSlot(int32 SlotIndex, FName CharacterId);

	UFUNCTION(BlueprintCallable, Category = "HSR|PreBattle")
	EHSRPreBattleCandidateResult SetBuff(FName BuffId);

	UFUNCTION(BlueprintCallable, Category = "HSR|PreBattle")
	EHSRPreBattleCandidateResult ConfirmCandidate(UPARAM(ref) FHSREncounterRequest& OutRequest);

	/** Confirms the local candidate and submits the resulting request to BattleTransition. */
	UFUNCTION(BlueprintCallable, Category = "HSR|PreBattle")
	FHSREncounterResult ConfirmAndSubmitEncounter(UPARAM(ref) FHSREncounterRequest& OutRequest);

	UFUNCTION(BlueprintCallable, Category = "HSR|PreBattle")
	EHSRPreBattleCandidateResult CancelCandidate();

	UFUNCTION(BlueprintPure, Category = "HSR|PreBattle")
	FHSRPreBattleCandidateSnapshot GetCandidateSnapshot() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|PreBattle")
	void OnCandidateSnapshotChanged(const FHSRPreBattleCandidateSnapshot& Snapshot);

protected:
	virtual void NativeDestruct() override;

private:
	void HandlePartyChanged(int64 Revision);
	void RefreshSnapshot();
	UPROPERTY(Transient) TObjectPtr<UHSRPreBattleCandidateViewModel> ViewModel;
	FDelegateHandle PartyChangedHandle;
};
