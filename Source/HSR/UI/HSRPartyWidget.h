#pragma once

#include "CoreMinimal.h"
#include "HSRScreenWidget.h"
#include "HSRPartyViewModel.h"
#include "Components/ComboBoxString.h"
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
	UFUNCTION(BlueprintPure, Category = "HSR|Party") int32 GetSlotCount() const { return Current.Slots.Num(); }
	UFUNCTION(BlueprintPure, Category = "HSR|Party") bool GetSlotAt(int32 SlotIndex, FHSRPartySlotViewData& OutSlot) const;
	UFUNCTION(BlueprintPure, Category = "HSR|Party") bool IsSlotOccupied(int32 SlotIndex) const;
	UFUNCTION(BlueprintPure, Category = "HSR|Party") FName GetSlotCharacterId(int32 SlotIndex) const;
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
	void BindCharacterSelectors();
	void RefreshCharacterSelectors();
	void RefreshPresentation();
	EHSRPartyResult PresentActionResult(EHSRPartyResult Result, const FText& SuccessMessage);
	UFUNCTION() void HandleClear0();
	UFUNCTION() void HandleClear1();
	UFUNCTION() void HandleClear2();
	UFUNCTION() void HandleClear3();
	UFUNCTION() void HandleConfirm();
	UFUNCTION() void HandleCancel();
	FText ActionMessage;
	void HandleCharacterSelection(int32 SlotIndex, const FString& Label);
	UFUNCTION() void HandleSlot0(FString Label, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleSlot1(FString Label, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleSlot2(FString Label, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleSlot3(FString Label, ESelectInfo::Type SelectionType);
	TArray<FString> CharacterOptionLabels;
	bool bUpdatingSelectors = false;

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
