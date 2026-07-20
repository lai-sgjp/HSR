#include "HSRBattleCommandViewModel.h"

void UHSRBattleCommandViewModel::SetState(const FHSRBattleCommandViewState& InState)
{
	State = InState;
	RefreshPresentationAndSelection();
	Changed.Broadcast(State);
}

const FHSRBattleCommandSkillView* UHSRBattleCommandViewModel::FindSelectedSkill() const
{
	return State.Skills.FindByPredicate([this](const FHSRBattleCommandSkillView& Skill) { return Skill.SkillId == SelectedSkillId; });
}

void UHSRBattleCommandViewModel::RefreshPresentationAndSelection()
{
	CurrentActorText = FText::Format(NSLOCTEXT("HSRCommand", "CurrentActor", "Actor: {0}"), FText::FromName(State.CurrentActorId));
	EnergyText = FText::Format(NSLOCTEXT("HSRCommand", "Energy", "Energy: {0} / {1}"), FText::AsNumber(FMath::RoundToInt(State.Energy)), FText::AsNumber(FMath::RoundToInt(State.MaxEnergy)));
	SkillPointsText = FText::Format(NSLOCTEXT("HSRCommand", "SkillPoints", "Skill Points: {0} / {1}"), FText::AsNumber(State.SkillPoints), FText::AsNumber(State.MaxSkillPoints));
	LastResolutionText = FText::Format(NSLOCTEXT("HSRCommand", "Resolution", "Last Resolution: {0} ({1})"), FText::AsNumber(static_cast<int32>(State.LastResolution.Status)), FText::AsNumber(static_cast<int32>(State.LastResolution.FailureReason)));

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
}

bool UHSRBattleCommandViewModel::SelectSkill(EHSRSkillCategory Category)
{
	const FHSRBattleCommandSkillView* Skill = State.Skills.FindByPredicate([Category](const FHSRBattleCommandSkillView& Candidate) { return Candidate.Category == Category; });
	if (!Skill) return false;
	SelectedSkillId = Skill->SkillId;
	SelectedTargetId = Skill->CandidateTargetIds.Num() > 0 ? Skill->CandidateTargetIds[0] : NAME_None;
	return true;
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
	return true;
}
