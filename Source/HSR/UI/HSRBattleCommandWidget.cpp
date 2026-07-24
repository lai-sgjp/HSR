#include "HSRBattleCommandWidget.h"

#include "HSRBattleCommandViewModel.h"
#include "../Battle/HSRBattleCoordinator.h"
#include "../Battle/HSRBattleGameMode.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

UHSRBattleCommandWidget::UHSRBattleCommandWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UHSRBattleCommandWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	BindDesignerEvents();
	AHSRBattleGameMode* BattleGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHSRBattleGameMode>() : nullptr;
	BindViewModel(BattleGameMode ? BattleGameMode->GetCommandViewModel() : nullptr, BattleGameMode ? BattleGameMode->GetCoordinator() : nullptr);
	UE_LOG(LogTemp, Log, TEXT("P6-004A Widget NativeConstruct Widget=%s Generation=%d Bound=%d"), *GetName(), BindGeneration, StateChangedHandle.IsValid() ? 1 : 0);
}

FReply UHSRBattleCommandWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (GetCurrentViewState().ResultViewState.bVisible && (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom))
	{
		const bool bAccepted = ConfirmBattleResult();
		UE_LOG(LogTemp, Log, TEXT("P10-004 ResultConfirm Key=%s Result=%s"), *Key.ToString(), bAccepted ? TEXT("SUCCESS") : TEXT("REJECTED"));
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
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
FText UHSRBattleCommandWidget::GetTurnOrderText() const { return ViewModel.IsValid() ? ViewModel->GetTurnOrderText() : FText::GetEmpty(); }
FText UHSRBattleCommandWidget::GetParticipantsText() const { return ViewModel.IsValid() ? ViewModel->GetParticipantsText() : FText::GetEmpty(); }
FText UHSRBattleCommandWidget::GetPresentationText() const { return ViewModel.IsValid() ? ViewModel->GetPresentationText() : FText::GetEmpty(); }
FText UHSRBattleCommandWidget::GetStatusText() const { return ViewModel.IsValid() ? ViewModel->GetStatusText() : FText::GetEmpty(); }
FText UHSRBattleCommandWidget::GetStatusOperationText() const { return ViewModel.IsValid() ? ViewModel->GetStatusOperationText() : FText::GetEmpty(); }
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
	if (ActionId.IsValid() && ActionId == LastSubmittedActionId)
	{
		++SubmitCount;
		return LastSubmittedResolution;
	}
	const FHSRBattleCommandViewState VerifiedSnapshot = GetCurrentViewState();
	if (!Coordinator.IsValid())
	{
		Resolution.Status = EHSRAbilityResolutionStatus::Rejected;
		Resolution.FailureReason = EHSRAbilityFailureReason::InvalidBattle;
		return Resolution;
	}
	if (!ViewModel.IsValid() || !ViewModel->BeginCommandSubmit(ActionId, ActorParticipantId, SkillId, TargetParticipantId))
	{
		Resolution.Status = EHSRAbilityResolutionStatus::Rejected;
		Resolution.FailureReason = EHSRAbilityFailureReason::AbilityUnavailable;
		return Resolution;
	}

	FHSRBattleActionCommand Command;
	Command.ActionId = ActionId;
	// Use the same snapshot that passed BeginCommandSubmit.  Reading the live
	// Coordinator id after locking could route a stale UI command into a reset battle.
	Command.BattleId = VerifiedSnapshot.BattleId;
	Command.ActorParticipantId = ActorParticipantId;
	Command.SkillId = SkillId;
	Command.TargetParticipantIds.Add(TargetParticipantId);
	if (Command.BattleId != Coordinator->GetCurrentRequestId())
	{
		Resolution.Status = EHSRAbilityResolutionStatus::Rejected;
		Resolution.FailureReason = EHSRAbilityFailureReason::InvalidBattle;
		ViewModel->ResolveCommandSubmit(Command.BattleId, Resolution);
		return Resolution;
	}
	Resolution = Coordinator->RequestAction(Command);
	LastSubmittedActionId = ActionId;
	LastSubmittedResolution = Resolution;
	ViewModel->ResolveCommandSubmit(Command.BattleId, Resolution);
	++SubmitCount;
	UE_LOG(LogTemp, Log, TEXT("P6-004A Widget Submit Count=%d ActionId=%s ActorId=%s SkillId=%s TargetId=%s Status=%d Reason=%d"), SubmitCount, *ActionId.ToString(), *ActorParticipantId.ToString(), *SkillId.ToString(), *TargetParticipantId.ToString(), static_cast<int32>(Resolution.Status), static_cast<int32>(Resolution.FailureReason));
	return Resolution;
}

FHSRAbilityResolution UHSRBattleCommandWidget::SubmitSelectedSkill()
{
	const FHSRBattleCommandViewState State = GetCurrentViewState();
	return SubmitCommand(FGuid::NewGuid(), State.CurrentActorId, GetSelectedSkillId(), GetSelectedTargetId());
}

bool UHSRBattleCommandWidget::ConfirmBattleResult()
{
	return ViewModel.IsValid() && ViewModel->RequestBattleResultConfirm();
}

void UHSRBattleCommandWidget::HandleViewStateChanged(const FHSRBattleCommandViewState& State)
{
	UE_LOG(LogTemp, Verbose, TEXT("P6-004A Widget Snapshot Widget=%s Generation=%d Actor=%s Skills=%d Energy=%.2f/%.2f SP=%d/%d"), *GetName(), BindGeneration, *State.CurrentActorId.ToString(), State.Skills.Num(), State.Energy, State.MaxEnergy, State.SkillPoints, State.MaxSkillPoints);
	OnCommandViewStateChanged(State);
	RefreshDesignerControls(State);
}

const FHSRBattleCommandSkillView* UHSRBattleCommandWidget::FindSkillView(const FHSRBattleCommandViewState& State, EHSRSkillCategory Category) const
{
	return State.Skills.FindByPredicate([Category](const FHSRBattleCommandSkillView& Skill) { return Skill.Category == Category; });
}

void UHSRBattleCommandWidget::RefreshSkillControls(const FHSRBattleCommandViewState& State, EHSRSkillCategory Category, UButton* Button, UTextBlock* NameText, UTextBlock* DescriptionText, UTextBlock* CostText)
{
	const FHSRBattleCommandSkillView* Skill = FindSkillView(State, Category);
	const bool bUnlockedPlayerTurn = !State.ResultViewState.bVisible && State.bCurrentActorPlayerControlled && !State.bCommandPending && !State.bPresentationLocked;
	if (Button) Button->SetIsEnabled(Skill && Skill->bAvailable && bUnlockedPlayerTurn);
	if (NameText) NameText->SetText(Skill ? Skill->DisplayName : FText::FromString(TEXT("Unavailable")));
	// Long authored descriptions belong to the future detail screen, not command buttons.
	if (DescriptionText) DescriptionText->SetVisibility(ESlateVisibility::Collapsed);
	if (!CostText) return;

	FText Cost = FText::GetEmpty();
	if (Skill)
	{
		switch (Category)
		{
		case EHSRSkillCategory::BasicAttack: Cost = NSLOCTEXT("HSRCommand", "BasicCost", "SP +1"); break;
		case EHSRSkillCategory::Skill: Cost = FText::Format(NSLOCTEXT("HSRCommand", "SkillCost", "SP -{0}"), FText::AsNumber(Skill->SkillPointCost)); break;
		case EHSRSkillCategory::Ultimate:
			Cost = Skill->bEnergyCostIsKnown
				? FText::Format(NSLOCTEXT("HSRCommand", "UltimateCost", "Energy -{0}"), FText::AsNumber(Skill->EnergyCost))
				: NSLOCTEXT("HSRCommand", "UnknownUltimateCost", "Energy ?");
			break;
		case EHSRSkillCategory::Heal: Cost = NSLOCTEXT("HSRCommand", "HealCost", "Cost --"); break;
		default: break;
		}
	}
	CostText->SetText(Cost);
}

void UHSRBattleCommandWidget::RefreshDesignerControls(const FHSRBattleCommandViewState& State)
{
	bRefreshingDesignerControls = true;
	if (TXT_CurrentActor) TXT_CurrentActor->SetText(GetCurrentActorText());
	if (TXT_Energy) TXT_Energy->SetText(GetEnergyText());
	if (TXT_SkillPoints) TXT_SkillPoints->SetText(GetSkillPointsText());
	if (TXT_Last_Resolution) TXT_Last_Resolution->SetText(GetLastResolutionText());
	if (TXT_Weakness) TXT_Weakness->SetText(GetWeaknessText());
	if (TXT_Toughness) TXT_Toughness->SetText(GetToughnessText());
	if (TXT_Break) TXT_Break->SetText(GetBreakText());
	if (TXT_Delay) TXT_Delay->SetText(GetDelayText());
	if (TXT_Statuses) TXT_Statuses->SetText(GetStatusText());
	if (TXT_StatisOperation) TXT_StatisOperation->SetText(GetStatusOperationText());
	if (TXT_TurnOrder) TXT_TurnOrder->SetText(GetTurnOrderText());
	if (TXT_Participants) TXT_Participants->SetText(GetParticipantsText());
	if (TXT_Presentation) TXT_Presentation->SetText(GetPresentationText());
	if (TXT_Result)
	{
		const FHSRBattleResultViewState& Result = State.ResultViewState;
		TXT_Result->SetText(Result.bVisible
			? (Result.Outcome == EHSRBattleOutcome::PlayerVictory ? NSLOCTEXT("HSRResult", "Victory", "Victory") : NSLOCTEXT("HSRResult", "Defeat", "Defeat"))
			: FText::GetEmpty());
	}
	if (BTN_ResultConfirm)
	{
		BTN_ResultConfirm->SetVisibility(State.ResultViewState.bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		BTN_ResultConfirm->SetIsEnabled(State.ResultViewState.bVisible && !State.ResultViewState.bConfirmPending);
	}
	if (State.ResultViewState.bVisible) FocusResultConfirm();

	RefreshSkillControls(State, EHSRSkillCategory::BasicAttack, BTN_Basic, TXT_BasicName, TXT_BasicDescription, TXT_BasicCost);
	RefreshSkillControls(State, EHSRSkillCategory::Skill, BTN_Skill, TXT_SkillName, TXT_SkillDescription, TXT_SkillCost);
	RefreshSkillControls(State, EHSRSkillCategory::Ultimate, BTN_Ultimate, TXT_UltimateName, TXT_UltimateDescription, TXT_UltimateCost);
	RefreshSkillControls(State, EHSRSkillCategory::Heal, Button_Heal, TXT_HealName, TXT_HealDescription, TXT_HealCost);

	const FHSRBattleCommandSkillView* SelectedSkill = State.Skills.FindByPredicate([&State](const FHSRBattleCommandSkillView& Skill) { return Skill.SkillId == State.SelectedSkillId; });
	if (TXT_DisabledReason)
	{
		FText Reason = FText::GetEmpty();
		if (!State.bCurrentActorPlayerControlled) Reason = NSLOCTEXT("HSRCommand", "EnemyTurn", "Waiting for enemy action");
		else if (State.bCommandPending) Reason = NSLOCTEXT("HSRCommand", "Pending", "Command pending");
		else if (State.bPresentationLocked) Reason = NSLOCTEXT("HSRCommand", "PresentationLocked", "Presentation locked");
		else if (SelectedSkill && !SelectedSkill->bAvailable)
		{
			if (const UEnum* FailureEnum = StaticEnum<EHSRAbilityFailureReason>()) Reason = FailureEnum->GetDisplayNameTextByValue(static_cast<int64>(SelectedSkill->DisabledReason));
		}
		TXT_DisabledReason->SetText(Reason);
	}
	if (PendingOverlay)
	{
		PendingOverlay->SetText(State.bCommandPending ? NSLOCTEXT("HSRCommand", "PendingOverlay", "Command Pending...") : FText::GetEmpty());
		PendingOverlay->SetVisibility((State.bCommandPending || State.bPresentationLocked) ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
	if (BTN_Execute) BTN_Execute->SetIsEnabled(State.bCanSubmit);

	if (CB_Target)
	{
		CB_Target->ClearOptions();
		if (SelectedSkill)
		{
			for (const FName TargetId : SelectedSkill->CandidateTargetIds) CB_Target->AddOption(TargetId.ToString());
		}
		if (!State.SelectedTargetId.IsNone()) CB_Target->SetSelectedOption(State.SelectedTargetId.ToString());
		CB_Target->SetIsEnabled(!State.ResultViewState.bVisible && State.bCurrentActorPlayerControlled && !State.bCommandPending && !State.bPresentationLocked && SelectedSkill && SelectedSkill->CandidateTargetIds.Num() > 1);
	}
	bRefreshingDesignerControls = false;
}

void UHSRBattleCommandWidget::BindDesignerEvents()
{
	UnbindDesignerEvents();
	if (BTN_Basic) BTN_Basic->OnClicked.AddDynamic(this, &UHSRBattleCommandWidget::HandleBasicClicked);
	if (BTN_Skill) BTN_Skill->OnClicked.AddDynamic(this, &UHSRBattleCommandWidget::HandleSkillClicked);
	if (BTN_Ultimate) BTN_Ultimate->OnClicked.AddDynamic(this, &UHSRBattleCommandWidget::HandleUltimateClicked);
	if (Button_Heal) Button_Heal->OnClicked.AddDynamic(this, &UHSRBattleCommandWidget::HandleHealClicked);
	if (BTN_Execute) BTN_Execute->OnClicked.AddDynamic(this, &UHSRBattleCommandWidget::HandleExecuteClicked);
	if (BTN_ResultConfirm) BTN_ResultConfirm->OnClicked.AddDynamic(this, &UHSRBattleCommandWidget::HandleResultConfirmClicked);
}

void UHSRBattleCommandWidget::UnbindDesignerEvents()
{
	if (BTN_Basic) BTN_Basic->OnClicked.RemoveDynamic(this, &UHSRBattleCommandWidget::HandleBasicClicked);
	if (BTN_Skill) BTN_Skill->OnClicked.RemoveDynamic(this, &UHSRBattleCommandWidget::HandleSkillClicked);
	if (BTN_Ultimate) BTN_Ultimate->OnClicked.RemoveDynamic(this, &UHSRBattleCommandWidget::HandleUltimateClicked);
	if (Button_Heal) Button_Heal->OnClicked.RemoveDynamic(this, &UHSRBattleCommandWidget::HandleHealClicked);
	if (BTN_Execute) BTN_Execute->OnClicked.RemoveDynamic(this, &UHSRBattleCommandWidget::HandleExecuteClicked);
	if (BTN_ResultConfirm) BTN_ResultConfirm->OnClicked.RemoveDynamic(this, &UHSRBattleCommandWidget::HandleResultConfirmClicked);
}

void UHSRBattleCommandWidget::HandleBasicClicked() { SelectSkill(EHSRSkillCategory::BasicAttack); }
void UHSRBattleCommandWidget::HandleSkillClicked() { SelectSkill(EHSRSkillCategory::Skill); }
void UHSRBattleCommandWidget::HandleUltimateClicked() { SelectSkill(EHSRSkillCategory::Ultimate); }
void UHSRBattleCommandWidget::HandleHealClicked() { SelectSkill(EHSRSkillCategory::Heal); }
void UHSRBattleCommandWidget::HandleExecuteClicked() { SubmitSelectedSkill(); }
void UHSRBattleCommandWidget::HandleResultConfirmClicked() { ConfirmBattleResult(); }

void UHSRBattleCommandWidget::FocusResultConfirm()
{
	if (BTN_ResultConfirm && BTN_ResultConfirm->GetIsEnabled())
	{
		BTN_ResultConfirm->SetKeyboardFocus();
		UE_LOG(LogTemp, Log, TEXT("P10-004 ResultFocus Target=ConfirmButton Result=%s"), BTN_ResultConfirm->HasKeyboardFocus() ? TEXT("SUCCESS") : TEXT("FAILED"));
	}
	else
	{
		SetKeyboardFocus();
		UE_LOG(LogTemp, Log, TEXT("P10-004 ResultFocus Target=WidgetFallback Result=%s"), HasKeyboardFocus() ? TEXT("SUCCESS") : TEXT("FAILED"));
	}
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
	LastSubmittedActionId.Invalidate();
	LastSubmittedResolution = FHSRAbilityResolution();
}

void UHSRBattleCommandWidget::NativeDestruct()
{
	UnbindDesignerEvents();
	UnbindViewModel();
	UE_LOG(LogTemp, Log, TEXT("P6-004A Widget NativeDestruct Widget=%s Generation=%d SubmitCount=%d"), *GetName(), BindGeneration, SubmitCount);
	Super::NativeDestruct();
}
