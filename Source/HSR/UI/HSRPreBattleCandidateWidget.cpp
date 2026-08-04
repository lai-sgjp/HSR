#include "HSRPreBattleCandidateWidget.h"

#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "Engine/GameInstance.h"

void UHSRPreBattleCandidateWidget::InitializeCandidate(const FHSREncounterRequest& Template)
{
	UGameInstance* GameInstance = GetGameInstance();
	UHSRPartySubsystem* Party = GameInstance ? GameInstance->GetSubsystem<UHSRPartySubsystem>() : nullptr;
	UHSRCharacterProfileSubsystem* Profiles = GameInstance ? GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
	if (Party && PartyChangedHandle.IsValid())
	{
		Party->OnPartyChanged().Remove(PartyChangedHandle);
		PartyChangedHandle.Reset();
	}
	if (!ViewModel) ViewModel = NewObject<UHSRPreBattleCandidateViewModel>(this);
	ViewModel->Initialize(Party, Profiles, Template);
	if (Party)
	{
		PartyChangedHandle = Party->OnPartyChanged().AddUObject(this, &ThisClass::HandlePartyChanged);
	}
	RefreshSnapshot();
}

EHSRPreBattleCandidateResult UHSRPreBattleCandidateWidget::SetCandidateSlot(int32 SlotIndex, FName CharacterId)
{
	const EHSRPreBattleCandidateResult Result = ViewModel ? ViewModel->SetCandidateSlot(SlotIndex, CharacterId) : EHSRPreBattleCandidateResult::InvalidCandidate;
	RefreshSnapshot();
	return Result;
}

EHSRPreBattleCandidateResult UHSRPreBattleCandidateWidget::SetBuff(FName BuffId)
{
	const EHSRPreBattleCandidateResult Result = ViewModel ? ViewModel->SetBuff(BuffId) : EHSRPreBattleCandidateResult::InvalidCandidate;
	RefreshSnapshot();
	return Result;
}

EHSRPreBattleCandidateResult UHSRPreBattleCandidateWidget::ConfirmCandidate(FHSREncounterRequest& OutRequest)
{
	return ViewModel ? ViewModel->ConfirmCandidate(OutRequest) : EHSRPreBattleCandidateResult::InvalidCandidate;
}

EHSRPreBattleCandidateResult UHSRPreBattleCandidateWidget::CancelCandidate()
{
	const EHSRPreBattleCandidateResult Result = ViewModel ? ViewModel->CancelCandidate() : EHSRPreBattleCandidateResult::InvalidCandidate;
	RefreshSnapshot();
	return Result;
}

FHSRPreBattleCandidateSnapshot UHSRPreBattleCandidateWidget::GetCandidateSnapshot() const
{
	return ViewModel ? ViewModel->GetSnapshot() : FHSRPreBattleCandidateSnapshot();
}

void UHSRPreBattleCandidateWidget::HandlePartyChanged(int64)
{
	RefreshSnapshot();
}

void UHSRPreBattleCandidateWidget::RefreshSnapshot()
{
	if (ViewModel) OnCandidateSnapshotChanged(ViewModel->GetSnapshot());
}

void UHSRPreBattleCandidateWidget::NativeDestruct()
{
	if (ViewModel)
	{
		if (UHSRPartySubsystem* Party = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRPartySubsystem>() : nullptr)
			Party->OnPartyChanged().Remove(PartyChangedHandle);
	}
	PartyChangedHandle.Reset();
	ViewModel = nullptr;
	Super::NativeDestruct();
}
