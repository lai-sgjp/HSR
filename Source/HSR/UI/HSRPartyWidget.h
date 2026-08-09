#pragma once

#include "CoreMinimal.h"
#include "HSRScreenWidget.h"
#include "HSRPartyViewModel.h"
#include "HSRPartyWidget.generated.h"

UCLASS(Blueprintable)
class HSR_API UHSRPartyWidget : public UHSRScreenWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HSR|Party")
	void SetViewModel(UHSRPartyViewModel* InViewModel);
	UFUNCTION(BlueprintPure, Category = "HSR|Party")
	bool GetCurrentSnapshot(FHSRPartyFrontendSnapshot& OutSnapshot) const;
	UFUNCTION(BlueprintCallable, Category = "HSR|Party") EHSRPartyResult SetCandidateSlot(int32 SlotIndex, FName CharacterId);
	UFUNCTION(BlueprintCallable, Category = "HSR|Party") EHSRPartyResult ClearCandidateSlot(int32 SlotIndex);
	UFUNCTION(BlueprintCallable, Category = "HSR|Party") EHSRPartyResult SwapCandidateSlots(int32 FirstSlot, int32 SecondSlot);
	UFUNCTION(BlueprintCallable, Category = "HSR|Party") EHSRPartyResult ConfirmCandidate();
	UFUNCTION(BlueprintCallable, Category = "HSR|Party") EHSRPartyResult CancelCandidate();
	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|Party")
	void OnPartySnapshotChanged(const FHSRPartyFrontendSnapshot& Snapshot);

#if WITH_DEV_AUTOMATION_TESTS
	void AttachForAutomation() { BindAndRefresh(); }
	int32 GetBindCountForAutomation() const { return BindCount; }
	int32 GetUnbindCountForAutomation() const { return UnbindCount; }
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BindAndRefresh();
	void Unbind();
	void HandleSnapshot(const FHSRPartyFrontendSnapshot& InSnapshot);

	UPROPERTY(Transient) TObjectPtr<UHSRPartyViewModel> ViewModel;
	FDelegateHandle Subscription;
	FHSRPartyFrontendSnapshot Current;
	bool bHasSnapshot = false;
	bool bOwnsViewModel = false;
#if WITH_DEV_AUTOMATION_TESTS
	int32 BindCount = 0;
	int32 UnbindCount = 0;
#endif
};
