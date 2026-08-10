#include "HSRPartyWidget.h"

#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "Engine/GameInstance.h"

void UHSRPartyWidget::SetViewModel(UHSRPartyViewModel* InViewModel)
{
	Unbind();
	if (bOwnsViewModel && ViewModel) ViewModel->Shutdown();
	ViewModel = InViewModel;
	bOwnsViewModel = false;
	BindAndRefresh();
}

bool UHSRPartyWidget::GetCurrentSnapshot(FHSRPartyFrontendSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot) return false;
	OutSnapshot = Current;
	return true;
}

bool UHSRPartyWidget::GetSlotAt(int32 SlotIndex, FHSRPartySlotViewData& OutSlot) const
{
	if (!Current.Slots.IsValidIndex(SlotIndex)) return false;
	OutSlot = Current.Slots[SlotIndex];
	return true;
}

bool UHSRPartyWidget::IsSlotOccupied(int32 SlotIndex) const
{
	FHSRPartySlotViewData ViewSlot;
	return GetSlotAt(SlotIndex, ViewSlot) && ViewSlot.bOccupied;
}

FName UHSRPartyWidget::GetSlotCharacterId(int32 SlotIndex) const
{
	FHSRPartySlotViewData ViewSlot;
	return GetSlotAt(SlotIndex, ViewSlot) ? ViewSlot.CharacterId : NAME_None;
}

EHSRPartyResult UHSRPartyWidget::SetCandidateSlot(int32 SlotIndex, FName CharacterId)
{
	return ViewModel ? ViewModel->SetCandidateSlot(SlotIndex, CharacterId) : EHSRPartyResult::InvalidCandidate;
}

EHSRPartyResult UHSRPartyWidget::ClearCandidateSlot(int32 SlotIndex)
{
	return ViewModel ? ViewModel->ClearCandidateSlot(SlotIndex) : EHSRPartyResult::InvalidCandidate;
}

EHSRPartyResult UHSRPartyWidget::SwapCandidateSlots(int32 FirstSlot, int32 SecondSlot)
{
	return ViewModel ? ViewModel->SwapCandidateSlots(FirstSlot, SecondSlot) : EHSRPartyResult::InvalidCandidate;
}

EHSRPartyResult UHSRPartyWidget::ConfirmCandidate()
{
	return ViewModel ? ViewModel->ConfirmCandidate() : EHSRPartyResult::InvalidCandidate;
}

EHSRPartyResult UHSRPartyWidget::CancelCandidate()
{
	return ViewModel ? ViewModel->CancelCandidate() : EHSRPartyResult::InvalidCandidate;
}

void UHSRPartyWidget::NativeConstruct()
{
	if (!ViewModel)
	{
		UGameInstance* GameInstance = GetGameInstance();
		UHSRPartySubsystem* Party = GameInstance ? GameInstance->GetSubsystem<UHSRPartySubsystem>() : nullptr;
		UHSRCharacterProfileSubsystem* Profiles = GameInstance ? GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
		ViewModel = NewObject<UHSRPartyViewModel>(this);
		ViewModel->Initialize(Party, Profiles);
		bOwnsViewModel = true;
	}
	BindAndRefresh();
	// Initialize before Super so the Blueprint Construct event can read a valid snapshot.
	Super::NativeConstruct();
}

void UHSRPartyWidget::NativeDestruct()
{
	Unbind();
	if (bOwnsViewModel && ViewModel)
	{
		ViewModel->Shutdown();
		ViewModel = nullptr;
		bOwnsViewModel = false;
	}
	Super::NativeDestruct();
}

void UHSRPartyWidget::BindAndRefresh()
{
	if (!ViewModel || Subscription.IsValid()) return;
	Subscription = ViewModel->OnChanged().AddUObject(this, &ThisClass::HandleSnapshot);
#if WITH_DEV_AUTOMATION_TESTS
	++BindCount;
#endif
	FHSRPartyFrontendSnapshot Snapshot;
	if (ViewModel->GetSnapshot(Snapshot)) HandleSnapshot(Snapshot);
}

void UHSRPartyWidget::Unbind()
{
	if (ViewModel && Subscription.IsValid())
	{
		ViewModel->OnChanged().Remove(Subscription);
#if WITH_DEV_AUTOMATION_TESTS
		++UnbindCount;
#endif
	}
	Subscription.Reset();
	bHasSnapshot = false;
	Current = FHSRPartyFrontendSnapshot();
}

void UHSRPartyWidget::HandleSnapshot(const FHSRPartyFrontendSnapshot& InSnapshot)
{
	Current = InSnapshot;
	bHasSnapshot = true;
	OnPartySnapshotChanged(Current);
}
