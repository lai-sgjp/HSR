#include "HSRBattleCommandViewModel.h"

#include "../Battle/HSRBattleCoordinator.h"
#include "../Battle/HSRBattleParticipant.h"
#include "../Battle/HSRBattleTypes.h"

void UHSRBattleCommandViewModel::BeginDestroy()
{
	UnbindCoordinator();
	Super::BeginDestroy();
}

void UHSRBattleCommandViewModel::SetState(const FHSRBattleCommandViewState& InState)
{
	State = InState;
	if (bCommandPending && (!PendingBattleId.IsValid() || State.BattleId != PendingBattleId || !State.BattleId.IsValid()
		|| (State.LastResolution.ActionId == PendingActionId && PendingActionId.IsValid())))
	{
		bCommandPending = false;
		PendingBattleId.Invalidate();
		PendingActionId.Invalidate();
	}
	RefreshPresentationAndSelection();
	RefreshCommandState();
	RefreshReadOnlyBattlePresentation();
	Changed.Broadcast(State);
}

void UHSRBattleCommandViewModel::BindCoordinator(UHSRBattleCoordinator* InCoordinator)
{
	UnbindCoordinator();
	Coordinator = InCoordinator;
	RefreshReadOnlyBattlePresentation();
	UE_LOG(LogTemp, Log, TEXT("P8-005 ViewModel Bind Coordinator=%s Target=%s"), InCoordinator ? TEXT("valid") : TEXT("null"), *SelectedTargetId.ToString());
}

void UHSRBattleCommandViewModel::UnbindCoordinator()
{
	Coordinator.Reset();
	ClearCommandLocks();
	UE_LOG(LogTemp, Log, TEXT("P8-005 ViewModel Unbind"));
}

const FHSRBattleCommandSkillView* UHSRBattleCommandViewModel::FindSelectedSkill() const
{
	return State.FindSkill(SelectedSkillId);
}

void UHSRBattleCommandViewModel::RefreshPresentationAndSelection()
{
	CurrentActorText = FText::Format(NSLOCTEXT("HSRCommand", "CurrentActor", "Actor: {0}"), FText::FromName(State.CurrentActorId));
	EnergyText = FText::Format(NSLOCTEXT("HSRCommand", "Energy", "Energy: {0} / {1}"), FText::AsNumber(FMath::RoundToInt(State.Energy)), FText::AsNumber(FMath::RoundToInt(State.MaxEnergy)));
	SkillPointsText = FText::Format(NSLOCTEXT("HSRCommand", "SkillPoints", "Skill Points: {0} / {1}"), FText::AsNumber(State.SkillPoints), FText::AsNumber(State.MaxSkillPoints));
	LastResolutionText = FText::Format(NSLOCTEXT("HSRCommand", "Resolution", "Last Resolution: {0} ({1})"), FText::AsNumber(static_cast<int32>(State.LastResolution.Status)), FText::AsNumber(static_cast<int32>(State.LastResolution.FailureReason)));
	TArray<FString> StatusLines;
	for (const FHSRStatusPublicSnapshot& Status : State.Statuses)
	{
		StatusLines.Add(FString::Printf(TEXT("%s | %s | %s | %s | x%d | %d"), *Status.TargetParticipantId.ToString(), *Status.StatusId.ToString(), *Status.DisplayName.ToString(),
			Status.Classification == EHSRStatusClassification::Buff ? TEXT("Buff") : TEXT("Debuff"), Status.Stacks, Status.RemainingTurns));
	}
	StatusText = FText::FromString(FString::Join(StatusLines, TEXT("\n")));
	const FHSRStatusPublicOperationEvent& Operation = State.LastStatusOperation;
	StatusOperationText = Operation.Sequence > 0
		? FText::FromString(FString::Printf(TEXT("%s | %s | op=%d | result=%d | #%lld"), *Operation.TargetParticipantId.ToString(), *Operation.StatusId.ToString(), static_cast<int32>(Operation.Operation), static_cast<int32>(Operation.Result), Operation.Sequence))
		: FText::GetEmpty();
	TArray<FString> OrderLines;
	for (const FName ParticipantId : State.TurnOrderParticipantIds) OrderLines.Add(ParticipantId.ToString());
	TurnOrderText = FText::FromString(FString::Join(OrderLines, TEXT(" -> ")));
	TArray<FString> ParticipantLines;
	for (const FHSRBattleParticipantView& Participant : State.Participants)
	{
		TArray<FString> WeaknessNames;
		for (const FGameplayTag& WeaknessTag : Participant.WeaknessTags)
		{
			WeaknessNames.Add(WeaknessTag.ToString());
		}
		WeaknessNames.Sort();
		const FString ParticipantWeaknessText = WeaknessNames.IsEmpty() ? TEXT("None") : FString::Join(WeaknessNames, TEXT(", "));
		ParticipantLines.Add(FString::Printf(TEXT("%s | HP %.0f/%.0f | Energy %.0f/%.0f | Toughness %.0f/%.0f | Weakness %s%s"),
			*Participant.ParticipantId.ToString(), Participant.Health, Participant.MaxHealth, Participant.Energy, Participant.MaxEnergy,
			Participant.Toughness, Participant.MaxToughness, *ParticipantWeaknessText, Participant.bDefeated ? TEXT(" | Defeated") : TEXT("")));
	}
	ParticipantsText = FText::FromString(FString::Join(ParticipantLines, TEXT("\n")));
	TArray<FString> PresentationLines;
	for (const FHSRBattlePresentationEvent& Event : State.PresentationEvents)
	{
		const TCHAR* Label = Event.EventType == EHSRPresentationEventType::Damage ? TEXT("Damage") : Event.EventType == EHSRPresentationEventType::Toughness ? TEXT("Toughness") : Event.EventType == EHSRPresentationEventType::Break ? TEXT("Break") : TEXT("Heal");
		PresentationLines.Add(FString::Printf(TEXT("%s -> %s | %s %.0f%s%s"), *Event.SourceParticipantId.ToString(), *Event.TargetParticipantId.ToString(), Label, Event.Value, Event.bCritical ? TEXT(" | Critical") : TEXT(""), Event.bBreak ? TEXT(" | Break") : TEXT("")));
	}
	PresentationText = FText::FromString(FString::Join(PresentationLines, TEXT("\n")));

	const FHSRBattleCommandSkillView* SelectedSkill = FindSelectedSkill();
	if (!SelectedSkill)
	{
		SelectedSkillId = State.Skills.Num() > 0 ? State.Skills[0].SkillId : NAME_None;
		SelectedSkill = FindSelectedSkill();
	}
	if (!SelectedSkill || !SelectedSkill->CandidateTargetIds.Contains(SelectedTargetId))
	{
		SelectedTargetId = SelectedSkill && SelectedSkill->CandidateTargetIds.Num() > 0 ? SelectedSkill->CandidateTargetIds[0] : NAME_None;
	}
	RefreshReadOnlyBattlePresentation();
	RefreshCommandState();
}

bool UHSRBattleCommandViewModel::SelectSkillById(FName SkillId)
{
	const FHSRBattleCommandSkillView* Skill = State.FindSkill(SkillId);
	if (!Skill)
	{
		return false;
	}

	SelectedSkillId = Skill->SkillId;
	SelectedTargetId = Skill->CandidateTargetIds.Num() > 0 ? Skill->CandidateTargetIds[0] : NAME_None;

	RefreshReadOnlyBattlePresentation();
	RefreshCommandState();
	Changed.Broadcast(State);
	return true;
}

// Category-keyed selection is first-match-wins, so it cannot reach a second skill sharing a
// category. Kept as a shim for existing callers; new code should use SelectSkillById.
bool UHSRBattleCommandViewModel::SelectSkill(EHSRSkillCategory Category)
{
	const FHSRBattleCommandSkillView* Skill = State.FindSkillByCategory(Category);
	return Skill ? SelectSkillById(Skill->SkillId) : false;
}

TArray<FName> UHSRBattleCommandViewModel::GetTargetOptions() const
{
	const FHSRBattleCommandSkillView* Skill = FindSelectedSkill();
	return Skill ? Skill->CandidateTargetIds : TArray<FName>();
}

bool UHSRBattleCommandViewModel::SelectTarget(FName TargetId)
{
	if (!GetTargetOptions().Contains(TargetId)) return false;
	SelectedTargetId = TargetId;
	RefreshReadOnlyBattlePresentation();
	RefreshCommandState();
	Changed.Broadcast(State);
	return true;
}

bool UHSRBattleCommandViewModel::BeginCommandSubmit(const FGuid& ActionId, FName ActorParticipantId, FName SkillId, FName TargetParticipantId)
{
	const FHSRBattleCommandSkillView* Skill = State.FindSkill(SkillId);
	if (!ActionId.IsValid() || !State.BattleId.IsValid() || !State.bCurrentActorPlayerControlled || bCommandPending || bPresentationLocked || ActorParticipantId != State.CurrentActorId
		|| !Skill || !Skill->bAvailable || SkillId != SelectedSkillId || TargetParticipantId != SelectedTargetId || !Skill->CandidateTargetIds.Contains(TargetParticipantId)) return false;
	PendingBattleId = State.BattleId;
	PendingActionId = ActionId;
	bCommandPending = true;
	RefreshCommandState();
	Changed.Broadcast(State);
	return true;
}

void UHSRBattleCommandViewModel::ResolveCommandSubmit(const FGuid& BattleId, const FHSRAbilityResolution& Resolution)
{
	if (bCommandPending && BattleId == PendingBattleId && Resolution.ActionId == PendingActionId)
	{
		bCommandPending = false;
		PendingBattleId.Invalidate();
		PendingActionId.Invalidate();
		RefreshCommandState();
		Changed.Broadcast(State);
	}
}

void UHSRBattleCommandViewModel::ClearCommandLocks()
{
	bCommandPending = false; bPresentationLocked = false; PendingBattleId.Invalidate(); PendingActionId.Invalidate();
	RefreshCommandState();
}

bool UHSRBattleCommandViewModel::ShowBattleResult(const FHSRBattleResult& Result)
{
	if (!Result.IsValid() || State.BattleId != Result.RequestId || State.ResultViewState.bVisible || State.ResultViewState.RequestId.IsValid()) return false;
	State.ResultViewState.RequestId = Result.RequestId;
	State.ResultViewState.Outcome = Result.Outcome;
	State.ResultViewState.DefeatedParticipantId = Result.DefeatedParticipantId;
	State.ResultViewState.bVisible = true;
	State.ResultViewState.bConfirmPending = false;
	ClearCommandLocks();
	RefreshCommandState();
	Changed.Broadcast(State);
	return true;
}

bool UHSRBattleCommandViewModel::RequestBattleResultConfirm()
{
	if (!State.ResultViewState.bVisible || State.ResultViewState.bConfirmPending || !State.ResultViewState.RequestId.IsValid()) return false;
	State.ResultViewState.bConfirmPending = true;
	RefreshCommandState();
	Changed.Broadcast(State);
	ResultConfirmRequested.Broadcast(State.ResultViewState.RequestId);
	return true;
}

void UHSRBattleCommandViewModel::RejectBattleResultConfirm(const FGuid& RequestId)
{
	if (!State.ResultViewState.bVisible || !State.ResultViewState.bConfirmPending || State.ResultViewState.RequestId != RequestId) return;
	State.ResultViewState.bConfirmPending = false;
	RefreshCommandState();
	Changed.Broadcast(State);
}

void UHSRBattleCommandViewModel::ClearBattleResult()
{
	State.ResultViewState = FHSRBattleResultViewState();
	RefreshCommandState();
}

void UHSRBattleCommandViewModel::RefreshCommandState()
{
	State.SelectedSkillId = SelectedSkillId;
	State.SelectedTargetId = SelectedTargetId;
	State.bCommandPending = bCommandPending;
	State.bPresentationLocked = bPresentationLocked;
	State.PendingActionId = PendingActionId;
	const FHSRBattleCommandSkillView* Skill = FindSelectedSkill();
	State.bCanSubmit = !State.ResultViewState.bVisible && !bCommandPending && !bPresentationLocked && Coordinator.IsValid() && State.BattleId.IsValid() && State.bCurrentActorPlayerControlled
		&& Skill && Skill->bAvailable && Skill->CandidateTargetIds.Contains(SelectedTargetId);
}

void UHSRBattleCommandViewModel::RefreshReadOnlyBattlePresentation()
{
	WeaknessText = FText::GetEmpty();
	ToughnessText = FText::GetEmpty();
	BreakText = FText::GetEmpty();
	DelayText = FText::GetEmpty();

	// Toughness and weaknesses come from the participant view the Coordinator already baked from the
	// ASC, so the UI never observes a live ASC of its own. bHasAttributes distinguishes "never read"
	// from a genuine zero, which is why an ASC-less participant still renders no text at all.
	const FHSRBattleParticipantView* TargetView = State.Participants.FindByPredicate(
		[this](const FHSRBattleParticipantView& Candidate)
		{
			return Candidate.ParticipantId == SelectedTargetId;
		});
	if (TargetView && TargetView->bHasAttributes)
	{
		FString Weaknesses;
		for (const FGameplayTag& Tag : TargetView->WeaknessTags)
		{
			if (!Weaknesses.IsEmpty())
			{
				Weaknesses += TEXT(", ");
			}
			Weaknesses += Tag.ToString();
		}
		WeaknessText = FText::Format(
			NSLOCTEXT("HSRCommand", "Weakness", "Weakness: {0}"),
			FText::FromString(Weaknesses.IsEmpty() ? TEXT("None") : Weaknesses));
		ToughnessText = FText::Format(
			NSLOCTEXT("HSRCommand", "Toughness", "Toughness: {0} / {1}"),
			FText::AsNumber(FMath::RoundToInt(TargetView->Toughness)),
			FText::AsNumber(FMath::RoundToInt(TargetView->MaxToughness)));
	}

	if (!Coordinator.IsValid())
	{
		return;
	}
	if (State.LastResolution.bHasBreakResult)
	{
		const FHSRBreakResult& Break = State.LastResolution.BreakResult;
		BreakText = FText::Format(NSLOCTEXT("HSRCommand", "Break", "Break: {0}"), FText::FromString(Break.bTriggered ? TEXT("Triggered") : TEXT("Not triggered")));
		const bool bDelayRegistered = Break.bTriggered && Coordinator->GetLastBreakDelayActionId() == Break.ActionId && Coordinator->WasLastBreakDelayRegistered();
		DelayText = FText::Format(NSLOCTEXT("HSRCommand", "Delay", "Delay: {0}"), FText::FromString(Break.bTriggered ? (bDelayRegistered ? TEXT("Registered") : TEXT("Rejected")) : TEXT("Not requested")));
	}
	UE_LOG(LogTemp, Log, TEXT("P8-005 View Target=%s Weakness=%s Toughness=%s Break=%s Delay=%s"), *SelectedTargetId.ToString(), *WeaknessText.ToString(), *ToughnessText.ToString(), *BreakText.ToString(), *DelayText.ToString());
}
