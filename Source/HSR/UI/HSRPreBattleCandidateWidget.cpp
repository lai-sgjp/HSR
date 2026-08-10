#include "HSRPreBattleCandidateWidget.h"

#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
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

FHSREncounterResult UHSRPreBattleCandidateWidget::ConfirmAndSubmitEncounter(FHSREncounterRequest& OutRequest)
{
	if (!ViewModel)
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidRequest,
			FText::FromString(TEXT("Pre-battle candidate is not initialized.")));
	}

	if (ConfirmCandidate(OutRequest) != EHSRPreBattleCandidateResult::Success)
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidRequest,
			FText::FromString(TEXT("Pre-battle candidate validation failed.")));
	}

	UHSRBattleTransitionSubsystem* Transition = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>() : nullptr;
	return Transition
		? Transition->SubmitEncounterRequestFromUI(OutRequest)
		: FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidRequest,
			FText::FromString(TEXT("Battle transition is unavailable.")));
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
	if (ViewModel)
	{
		const FHSRPreBattleCandidateSnapshot Snapshot = ViewModel->GetSnapshot();
		UpdateSlotTextBlocks(Snapshot);
		OnCandidateSnapshotChanged(Snapshot);
	}
}

void UHSRPreBattleCandidateWidget::UpdateSlotTextBlocks(const FHSRPreBattleCandidateSnapshot& Snapshot)
{
	if (!WidgetTree) return;
	// The panel renders up to four candidate slots.  Resolving by name keeps the C++ side
	// independent of how many slot widgets the Blueprint actually places, and lets an authored
	// panel show all committed members without hardcoding slot indices in the graph.
	const FName SlotNames[] = { TEXT("Text_Slot0_Character"), TEXT("Text_Slot1_Character"),
		TEXT("Text_Slot2_Character"), TEXT("Text_Slot3_Character") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SlotNames); ++Index)
	{
		UTextBlock* TextBlock = WidgetTree->FindWidget<UTextBlock>(SlotNames[Index]);
		if (!TextBlock) continue;
		if (Snapshot.CandidateCharacterIds.IsValidIndex(Index) && !Snapshot.CandidateCharacterIds[Index].IsNone())
		{
			TextBlock->SetText(FText::FromName(Snapshot.CandidateCharacterIds[Index]));
		}
		else
		{
			TextBlock->SetText(FText::FromString(TEXT("Empty")));
		}
	}
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
