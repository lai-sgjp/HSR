#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSRPreBattleCandidateViewModel.h"
#include "HSRPreBattleCandidateWidget.generated.h"

class UHSRPreBattleCandidateWidget;
class UHSRStageBuffDefinition;
class UPanelWidget;

UCLASS()
class HSR_API UHSRPreBattleChoiceBinding : public UObject
{
	GENERATED_BODY()
public:
	void Initialize(UHSRPreBattleCandidateWidget* InOwner, int32 InSlot, FName InId = NAME_None, bool bInBuff = false);
	UFUNCTION() void HandleClicked();
private:
	TWeakObjectPtr<UHSRPreBattleCandidateWidget> Owner;
	int32 Slot = INDEX_NONE;
	FName Id;
	bool bBuff = false;
};

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
	EHSRPreBattleCandidateResult ToggleBuff(FName BuffId);
	UFUNCTION(BlueprintCallable, Category = "HSR|PreBattle")
	void SelectReplacementSlot(int32 SlotIndex);
	UFUNCTION(BlueprintPure, Category = "HSR|PreBattle")
	int32 GetSelectedReplacementSlot() const { return SelectedSlot; }

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
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void HandlePartyChanged(int64 Revision);
	void RefreshSnapshot();
	void UpdateSlotTextBlocks(const FHSRPreBattleCandidateSnapshot& Snapshot);
	void RefreshChoices(const FHSRPreBattleCandidateSnapshot& Snapshot);
	void SetLabel(FName Name, const FText& Text);
	bool IsAvailableBuff(FName BuffId) const;
	EHSRPreBattleCandidateResult PresentResult(EHSRPreBattleCandidateResult Result);
	UPROPERTY(Transient) TObjectPtr<UHSRPreBattleCandidateViewModel> ViewModel;
	UPROPERTY(Transient) TArray<TObjectPtr<UHSRPreBattleChoiceBinding>> ChoiceBindings;
	UPROPERTY(EditAnywhere, Category = "Presentation") TArray<TObjectPtr<UHSRStageBuffDefinition>> AvailableBuffs;
	UPROPERTY(EditAnywhere, Category = "Presentation") TMap<FName, FText> BuffDisplayNames;
	int32 SelectedSlot = 0;
	bool bSubmitting = false;
	bool bSubmitted = false;
	FDelegateHandle PartyChangedHandle;
};
