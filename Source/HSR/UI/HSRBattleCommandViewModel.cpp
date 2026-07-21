#include "HSRBattleCommandViewModel.h"

#include "../Battle/HSRBattleCoordinator.h"
#include "../Battle/HSRBattleParticipant.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "AbilitySystemComponent.h"

void UHSRBattleCommandViewModel::SetState(const FHSRBattleCommandViewState& InState)
{
	State = InState;
	RefreshPresentationAndSelection();
	RefreshReadOnlyBattlePresentation();
	Changed.Broadcast(State);
}

void UHSRBattleCommandViewModel::BindCoordinator(UHSRBattleCoordinator* InCoordinator)
{
	UnbindCoordinator();
	Coordinator = InCoordinator;
	RebindTargetAttributes();
	RefreshReadOnlyBattlePresentation();
	UE_LOG(LogTemp, Log, TEXT("P8-005 ViewModel Bind Coordinator=%s Target=%s Bound=%d"), InCoordinator ? TEXT("valid") : TEXT("null"), *SelectedTargetId.ToString(), ObservedTargetASC.IsValid() ? 1 : 0);
}

void UHSRBattleCommandViewModel::UnbindCoordinator()
{
	if (ObservedTargetASC.IsValid())
	{
		if (ToughnessChangedHandle.IsValid()) ObservedTargetASC->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetToughnessAttribute()).Remove(ToughnessChangedHandle);
		if (MaxToughnessChangedHandle.IsValid()) ObservedTargetASC->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetMaxToughnessAttribute()).Remove(MaxToughnessChangedHandle);
	}
	ToughnessChangedHandle.Reset(); MaxToughnessChangedHandle.Reset(); ObservedTargetASC.Reset(); Coordinator.Reset();
	UE_LOG(LogTemp, Log, TEXT("P8-005 ViewModel Unbind"));
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
	RebindTargetAttributes();
}

bool UHSRBattleCommandViewModel::SelectSkill(EHSRSkillCategory Category)
{
	const FHSRBattleCommandSkillView* Skill = State.Skills.FindByPredicate([Category](const FHSRBattleCommandSkillView& Candidate) { return Candidate.Category == Category; });
	if (!Skill) return false;
	SelectedSkillId = Skill->SkillId;
	SelectedTargetId = Skill->CandidateTargetIds.Num() > 0 ? Skill->CandidateTargetIds[0] : NAME_None;
	RebindTargetAttributes(); RefreshReadOnlyBattlePresentation(); Changed.Broadcast(State);
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
	RebindTargetAttributes(); RefreshReadOnlyBattlePresentation(); Changed.Broadcast(State);
	return true;
}

void UHSRBattleCommandViewModel::RebindTargetAttributes()
{
	if (ObservedTargetASC.IsValid())
	{
		if (ToughnessChangedHandle.IsValid()) ObservedTargetASC->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetToughnessAttribute()).Remove(ToughnessChangedHandle);
		if (MaxToughnessChangedHandle.IsValid()) ObservedTargetASC->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetMaxToughnessAttribute()).Remove(MaxToughnessChangedHandle);
	}
	ToughnessChangedHandle.Reset(); MaxToughnessChangedHandle.Reset(); ObservedTargetASC.Reset();
	if (!Coordinator.IsValid()) return;
	const FHSRBattleParticipant* Target = Coordinator->GetParticipants().FindByPredicate([this](const FHSRBattleParticipant& Participant) { return Participant.ParticipantId == SelectedTargetId; });
	if (!Target || !Target->AbilitySystemComponent.IsValid()) return;
	ObservedTargetASC = Target->AbilitySystemComponent;
	ToughnessChangedHandle = ObservedTargetASC->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetToughnessAttribute()).AddUObject(this, &UHSRBattleCommandViewModel::HandleObservedToughnessChanged);
	MaxToughnessChangedHandle = ObservedTargetASC->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetMaxToughnessAttribute()).AddUObject(this, &UHSRBattleCommandViewModel::HandleObservedToughnessChanged);
}

void UHSRBattleCommandViewModel::HandleObservedToughnessChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshReadOnlyBattlePresentation();
	Changed.Broadcast(State);
}

void UHSRBattleCommandViewModel::RefreshReadOnlyBattlePresentation()
{
	WeaknessText = FText::GetEmpty(); ToughnessText = FText::GetEmpty(); BreakText = FText::GetEmpty(); DelayText = FText::GetEmpty();
	if (!Coordinator.IsValid()) return;
	const FHSRBattleParticipant* Target = Coordinator->GetParticipants().FindByPredicate([this](const FHSRBattleParticipant& Participant) { return Participant.ParticipantId == SelectedTargetId; });
	if (Target && Target->AbilitySystemComponent.IsValid())
	{
		FString Weaknesses;
		for (const FGameplayTag& Tag : Target->WeaknessTags) { if (!Weaknesses.IsEmpty()) Weaknesses += TEXT(", "); Weaknesses += Tag.ToString(); }
		WeaknessText = FText::Format(NSLOCTEXT("HSRCommand", "Weakness", "Weakness: {0}"), FText::FromString(Weaknesses.IsEmpty() ? TEXT("None") : Weaknesses));
		ToughnessText = FText::Format(NSLOCTEXT("HSRCommand", "Toughness", "Toughness: {0} / {1}"), FText::AsNumber(FMath::RoundToInt(Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetToughnessAttribute()))), FText::AsNumber(FMath::RoundToInt(Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxToughnessAttribute()))));
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
