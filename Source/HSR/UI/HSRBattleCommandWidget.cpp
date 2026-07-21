#include "HSRBattleCommandWidget.h"

#include "HSRBattleCommandViewModel.h"
#include "../Battle/HSRBattleCoordinator.h"
#include "../Battle/HSRBattleGameMode.h"

void UHSRBattleCommandWidget::NativeConstruct()
{
	Super::NativeConstruct();

	AHSRBattleGameMode* BattleGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHSRBattleGameMode>() : nullptr;
	BindViewModel(BattleGameMode ? BattleGameMode->GetCommandViewModel() : nullptr, BattleGameMode ? BattleGameMode->GetCoordinator() : nullptr);
	UE_LOG(LogTemp, Log, TEXT("P6-004A Widget NativeConstruct Widget=%s Generation=%d Bound=%d"), *GetName(), BindGeneration, StateChangedHandle.IsValid() ? 1 : 0);
}

void UHSRBattleCommandWidget::BindViewModel(UHSRBattleCommandViewModel* InViewModel, UHSRBattleCoordinator* InCoordinator)
{
	UnbindViewModel();
	ViewModel = InViewModel;
	Coordinator = InCoordinator;
	if (!InViewModel || !InCoordinator)
	{
		UE_LOG(LogTemp, Warning, TEXT("P6-004A Widget Bind Result=FAILED Widget=%s ViewModel=%s Coordinator=%s"), *GetName(), InViewModel ? TEXT("valid") : TEXT("null"), InCoordinator ? TEXT("valid") : TEXT("null"));
		return;
	}

	++BindGeneration;
	StateChangedHandle = InViewModel->OnChanged().AddUObject(this, &UHSRBattleCommandWidget::HandleViewStateChanged);
	UE_LOG(LogTemp, Log, TEXT("P6-004A Widget Bind Result=SUCCESS Widget=%s Generation=%d ActiveBindings=1"), *GetName(), BindGeneration);
	HandleViewStateChanged(InViewModel->GetState());
}

FHSRBattleCommandViewState UHSRBattleCommandWidget::GetCurrentViewState() const
{
	return ViewModel.IsValid() ? ViewModel->GetStateCopy() : FHSRBattleCommandViewState();
}

FText UHSRBattleCommandWidget::GetCurrentActorText() const { return ViewModel.IsValid() ? ViewModel->GetCurrentActorText() : FText::GetEmpty(); }
FText UHSRBattleCommandWidget::GetEnergyText() const { return ViewModel.IsValid() ? ViewModel->GetEnergyText() : FText::GetEmpty(); }
FText UHSRBattleCommandWidget::GetSkillPointsText() const { return ViewModel.IsValid() ? ViewModel->GetSkillPointsText() : FText::GetEmpty(); }
FText UHSRBattleCommandWidget::GetLastResolutionText() const { return ViewModel.IsValid() ? ViewModel->GetLastResolutionText() : FText::GetEmpty(); }
FText UHSRBattleCommandWidget::GetWeaknessText() const { return ViewModel.IsValid() ? ViewModel->GetWeaknessText() : FText::GetEmpty(); }
FText UHSRBattleCommandWidget::GetToughnessText() const { return ViewModel.IsValid() ? ViewModel->GetToughnessText() : FText::GetEmpty(); }
FText UHSRBattleCommandWidget::GetBreakText() const { return ViewModel.IsValid() ? ViewModel->GetBreakText() : FText::GetEmpty(); }
FText UHSRBattleCommandWidget::GetDelayText() const { return ViewModel.IsValid() ? ViewModel->GetDelayText() : FText::GetEmpty(); }
bool UHSRBattleCommandWidget::SelectSkill(EHSRSkillCategory Category) { return ViewModel.IsValid() && ViewModel->SelectSkill(Category); }
bool UHSRBattleCommandWidget::SelectTarget(FName TargetId) { return ViewModel.IsValid() && ViewModel->SelectTarget(TargetId); }
FName UHSRBattleCommandWidget::GetSelectedSkillId() const { return ViewModel.IsValid() ? ViewModel->GetSelectedSkillId() : NAME_None; }
FName UHSRBattleCommandWidget::GetSelectedTargetId() const { return ViewModel.IsValid() ? ViewModel->GetSelectedTargetId() : NAME_None; }
TArray<FName> UHSRBattleCommandWidget::GetTargetOptions() const { return ViewModel.IsValid() ? ViewModel->GetTargetOptions() : TArray<FName>(); }

FHSRAbilityResolution UHSRBattleCommandWidget::SubmitCommand(FGuid ActionId, FName ActorParticipantId, FName SkillId, FName TargetParticipantId)
{
	FHSRAbilityResolution Resolution;
	Resolution.ActionId = ActionId;
	Resolution.ActorParticipantId = ActorParticipantId;
	Resolution.SkillId = SkillId;
	if (!Coordinator.IsValid())
	{
		Resolution.Status = EHSRAbilityResolutionStatus::Rejected;
		Resolution.FailureReason = EHSRAbilityFailureReason::InvalidBattle;
		return Resolution;
	}

	FHSRBattleActionCommand Command;
	Command.ActionId = ActionId;
	Command.BattleId = Coordinator->GetCurrentRequestId();
	Command.ActorParticipantId = ActorParticipantId;
	Command.SkillId = SkillId;
	Command.TargetParticipantIds.Add(TargetParticipantId);
	Resolution = Coordinator->RequestAction(Command);
	++SubmitCount;
	UE_LOG(LogTemp, Log, TEXT("P6-004A Widget Submit Count=%d ActionId=%s ActorId=%s SkillId=%s TargetId=%s Status=%d Reason=%d"), SubmitCount, *ActionId.ToString(), *ActorParticipantId.ToString(), *SkillId.ToString(), *TargetParticipantId.ToString(), static_cast<int32>(Resolution.Status), static_cast<int32>(Resolution.FailureReason));
	return Resolution;
}

FHSRAbilityResolution UHSRBattleCommandWidget::SubmitSelectedSkill()
{
	const FHSRBattleCommandViewState State = GetCurrentViewState();
	return SubmitCommand(FGuid::NewGuid(), State.CurrentActorId, GetSelectedSkillId(), GetSelectedTargetId());
}

void UHSRBattleCommandWidget::HandleViewStateChanged(const FHSRBattleCommandViewState& State)
{
	UE_LOG(LogTemp, Verbose, TEXT("P6-004A Widget Snapshot Widget=%s Generation=%d Actor=%s Skills=%d Energy=%.2f/%.2f SP=%d/%d"), *GetName(), BindGeneration, *State.CurrentActorId.ToString(), State.Skills.Num(), State.Energy, State.MaxEnergy, State.SkillPoints, State.MaxSkillPoints);
	OnCommandViewStateChanged(State);
}

void UHSRBattleCommandWidget::UnbindViewModel()
{
	if (ViewModel.IsValid() && StateChangedHandle.IsValid())
	{
		ViewModel->OnChanged().Remove(StateChangedHandle);
		UE_LOG(LogTemp, Log, TEXT("P6-004A Widget Unbind Result=SUCCESS Widget=%s Generation=%d ActiveBindings=0"), *GetName(), BindGeneration);
	}
	StateChangedHandle.Reset();
	ViewModel.Reset();
	Coordinator.Reset();
}

void UHSRBattleCommandWidget::NativeDestruct()
{
	UnbindViewModel();
	UE_LOG(LogTemp, Log, TEXT("P6-004A Widget NativeDestruct Widget=%s Generation=%d SubmitCount=%d"), *GetName(), BindGeneration, SubmitCount);
	Super::NativeDestruct();
}
