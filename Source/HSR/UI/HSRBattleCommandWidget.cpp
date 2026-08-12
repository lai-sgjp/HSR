#include "HSRBattleCommandWidget.h"

#include "HSRBattleCommandViewModel.h"
#include "HSRSkillButtonWidget.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

UHSRBattleCommandWidget::UHSRBattleCommandWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 命令面板需要接收键盘输入（如回车确认结果），因此默认设为可聚焦。
	SetIsFocusable(true);
}

void UHSRBattleCommandWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	BindDesignerEvents();
	// No GetAuthGameMode reach-back: the owner calls BindViewModel. This widget used to fetch its own
	// ViewModel and Coordinator here as well, which meant a battle-map GameMode was a hard
	// requirement just to construct the widget, and duplicated the push the owner already performs.
	// 设计约束：本 Widget 不再自己反向获取 GameMode，而是由外部（协调器/UI 管理器）在创建后
	// 主动调用 BindViewModel 注入 VM。早期版本在这里自行查找 ViewModel 与 Coordinator，
	// 导致“构造该 Widget 就必须有战斗地图 GameMode”，而且与外部注入重复。
	UE_LOG(LogTemp, Log, TEXT("P6-004A Widget NativeConstruct Widget=%s Generation=%d Bound=%d"), *GetName(), BindGeneration, StateChangedHandle.IsValid() ? 1 : 0);
}

// 键盘按键处理：战斗结果界面可见时，回车/空格/手柄确认键都视为“确认结果”。
// 处理完按键后返回 Handled，表示本 Widget 已消费该输入，不再向上冒泡。
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

// 绑定 ViewModel 与命令汇（CommandSink）。命令汇必须实现 IHSRBattleCommandSink 接口，
// 否则视为接线错误直接拒绝绑定——宁可整组不工作，也不允许“界面渲染正常但命令全部丢失”。
// 绑定成功后立即推送一次当前状态，让界面在首次显示时就反映最新战斗数据。
void UHSRBattleCommandWidget::BindViewModel(UHSRBattleCommandViewModel* InViewModel, UObject* InCommandSink)
{
	UnbindViewModel();
	ViewModel = InViewModel;
	// A non-null object that does not implement the sink interface is a wiring mistake, not a
	// degraded mode, so it fails the bind the same way a null one does rather than silently
	// producing a widget that renders state but drops every command.
	// 传入的对象非空但未实现 sink 接口，属于接线错误而非降级模式，
	// 因此与传入空指针一样让绑定失败，避免“界面正常显示、命令却被静默丢弃”的假象。
	IHSRBattleCommandSink* Sink = Cast<IHSRBattleCommandSink>(InCommandSink);
	CommandSink = Sink;
	if (!InViewModel || !Sink)
	{
		UE_LOG(LogTemp, Warning, TEXT("P6-004A Widget Bind Result=FAILED Widget=%s ViewModel=%s Sink=%s"), *GetName(), InViewModel ? TEXT("valid") : TEXT("null"), Sink ? TEXT("valid") : (InCommandSink ? TEXT("not-a-sink") : TEXT("null")));
		return;
	}

	// 绑定 VM 的 OnChanged 事件：每次战斗状态快照变化都会回调 HandleViewStateChanged。
	++BindGeneration;
	StateChangedHandle = InViewModel->OnChanged().AddUObject(this, &UHSRBattleCommandWidget::HandleViewStateChanged);
	UE_LOG(LogTemp, Log, TEXT("P6-004A Widget Bind Result=SUCCESS Widget=%s Generation=%d ActiveBindings=1"), *GetName(), BindGeneration);
	HandleViewStateChanged(InViewModel->GetState());
}

// 返回当前 ViewModel 的状态快照副本；VM 无效时返回空快照，保证调用方永远拿到安全值。
FHSRBattleCommandViewState UHSRBattleCommandWidget::GetCurrentViewState() const
{
	if (ViewModel.IsValid())
	{
		return ViewModel->GetStateCopy();
	}
	return FHSRBattleCommandViewState();
}

// 以下一组 Getter 都是“薄访问器”：把 VM 中已生成好的显示文本原样读出。
// 它们不回算、不读战斗对象，纯粹是 Widget 向 VM 取值的最小接口。
FText UHSRBattleCommandWidget::GetCurrentActorText() const
{
	return ViewModel.IsValid() ? ViewModel->GetCurrentActorText() : FText::GetEmpty();
}

FText UHSRBattleCommandWidget::GetEnergyText() const
{
	return ViewModel.IsValid() ? ViewModel->GetEnergyText() : FText::GetEmpty();
}

FText UHSRBattleCommandWidget::GetSkillPointsText() const
{
	return ViewModel.IsValid() ? ViewModel->GetSkillPointsText() : FText::GetEmpty();
}

FText UHSRBattleCommandWidget::GetLastResolutionText() const
{
	return ViewModel.IsValid() ? ViewModel->GetLastResolutionText() : FText::GetEmpty();
}

FText UHSRBattleCommandWidget::GetWeaknessText() const
{
	return ViewModel.IsValid() ? ViewModel->GetWeaknessText() : FText::GetEmpty();
}

FText UHSRBattleCommandWidget::GetToughnessText() const
{
	return ViewModel.IsValid() ? ViewModel->GetToughnessText() : FText::GetEmpty();
}

FText UHSRBattleCommandWidget::GetBreakText() const
{
	return ViewModel.IsValid() ? ViewModel->GetBreakText() : FText::GetEmpty();
}

FText UHSRBattleCommandWidget::GetDelayText() const
{
	return ViewModel.IsValid() ? ViewModel->GetDelayText() : FText::GetEmpty();
}

FText UHSRBattleCommandWidget::GetTurnOrderText() const
{
	return ViewModel.IsValid() ? ViewModel->GetTurnOrderText() : FText::GetEmpty();
}

FText UHSRBattleCommandWidget::GetParticipantsText() const
{
	return ViewModel.IsValid() ? ViewModel->GetParticipantsText() : FText::GetEmpty();
}

FText UHSRBattleCommandWidget::GetPresentationText() const
{
	return ViewModel.IsValid() ? ViewModel->GetPresentationText() : FText::GetEmpty();
}

FText UHSRBattleCommandWidget::GetStatusText() const
{
	return ViewModel.IsValid() ? ViewModel->GetStatusText() : FText::GetEmpty();
}

FText UHSRBattleCommandWidget::GetStatusOperationText() const
{
	return ViewModel.IsValid() ? ViewModel->GetStatusOperationText() : FText::GetEmpty();
}

// 用户选择技能/目标等操作同样全部转发给 VM，由 VM 统一校验并广播新状态。
bool UHSRBattleCommandWidget::SelectSkill(EHSRSkillCategory Category)
{
	return ViewModel.IsValid() && ViewModel->SelectSkill(Category);
}

bool UHSRBattleCommandWidget::SelectSkillById(FName SkillId)
{
	return ViewModel.IsValid() && ViewModel->SelectSkillById(SkillId);
}

// 是否启用“技能列表”模式：只有同时配置了容器与条目类才使用列表，否则回退到固定按钮布局。
bool UHSRBattleCommandWidget::UsesSkillList() const
{
	return SkillListContainer != nullptr && SkillEntryClass != nullptr;
}

// One entry per authored skill. Entries are reused across refreshes and only created/destroyed when
// the skill count changes, so a loadout swap between participants does not churn the whole panel.
// 技能列表采用“按需增删、尽量复用”的策略：每个技能一个条目，条目在刷新间复用，
// 只在技能数量变化时才创建/销毁。这样切换参战者导致技能组变更时，不会重建整个面板。
void UHSRBattleCommandWidget::RefreshSkillList(const FHSRBattleCommandViewState& State)
{
	if (!UsesSkillList())
	{
		return;
	}

	const int32 DesiredCount = State.Skills.Num();

	// 现有条目多于所需：从尾部逐个移除（先解绑点击事件再移出面板）。
	while (SkillEntries.Num() > DesiredCount)
	{
		if (UHSRSkillButtonWidget* Removed = SkillEntries.Pop())
		{
			Removed->OnSkillClicked.Unbind();
			Removed->RemoveFromParent();
		}
	}

	// 现有条目少于所需：逐个创建新条目并绑定点击回调。
	while (SkillEntries.Num() < DesiredCount)
	{
		UHSRSkillButtonWidget* Entry = CreateWidget<UHSRSkillButtonWidget>(this, SkillEntryClass);
		if (!Entry)
		{
			break;
		}
		Entry->OnSkillClicked.BindUObject(this, &UHSRBattleCommandWidget::HandleSkillEntryClicked);
		SkillListContainer->AddChild(Entry);
		SkillEntries.Add(Entry);
	}

	// 回合级锁：只要不是玩家回合、或命令挂起、或展示锁定，任何技能都不可点击。
	const bool bUnlockedPlayerTurn = !State.ResultViewState.bVisible && State.bCurrentActorPlayerControlled
		&& !State.bCommandPending && !State.bPresentationLocked;

	// 把每个技能的最新视图数据推给对应条目，并标出当前选中项。
	for (int32 Index = 0; Index < SkillEntries.Num(); ++Index)
	{
		if (UHSRSkillButtonWidget* Entry = SkillEntries[Index])
		{
			FHSRBattleCommandSkillView EntryView = State.Skills[Index];
			// Turn-level locks gate every entry, on top of each skill's own availability.
			// 每个条目的可用性 = 技能自身可用性 && 回合级解锁状态。
			EntryView.bAvailable = EntryView.bAvailable && bUnlockedPlayerTurn;
			Entry->SetSkillView(EntryView, EntryView.SkillId == State.SelectedSkillId);
		}
	}
}

// 技能条目被点击：跳过设计期控件的误触，转发给按 ID 选择技能。
void UHSRBattleCommandWidget::HandleSkillEntryClicked(FName SkillId)
{
	if (bRefreshingDesignerControls)
	{
		return;
	}
	SelectSkillById(SkillId);
}

// 选择目标：转发给 VM 校验并广播。
bool UHSRBattleCommandWidget::SelectTarget(FName TargetId)
{
	return ViewModel.IsValid() && ViewModel->SelectTarget(TargetId);
}

// 以下 Getter 返回 VM 中的选择状态；VM 无效时返回安全默认值。
FName UHSRBattleCommandWidget::GetSelectedSkillId() const
{
	return ViewModel.IsValid() ? ViewModel->GetSelectedSkillId() : NAME_None;
}

FName UHSRBattleCommandWidget::GetSelectedTargetId() const
{
	return ViewModel.IsValid() ? ViewModel->GetSelectedTargetId() : NAME_None;
}

TArray<FName> UHSRBattleCommandWidget::GetTargetOptions() const
{
	if (ViewModel.IsValid())
	{
		return ViewModel->GetTargetOptions();
	}
	return TArray<FName>();
}

// 提交一条战斗命令：这是 Widget 把玩家操作送到战斗系统的核心出口。
// 流程：1) 相同 ActionId 去重（返回上一次结果）；2) 校验命令汇有效；
// 3) 让 VM 做提交前置校验（BeginCommandSubmit）；4) 确认命令中的战斗 ID 仍是当前战斗；
// 5) 交给命令汇真正提交，并记录结果供 ResolveCommandSubmit 收尾。
FHSRAbilityResolution UHSRBattleCommandWidget::SubmitCommand(FGuid ActionId, FName ActorParticipantId, FName SkillId, FName TargetParticipantId)
{
	FHSRAbilityResolution Resolution;
	Resolution.ActionId = ActionId;
	Resolution.ActorParticipantId = ActorParticipantId;
	Resolution.SkillId = SkillId;
	// 相同 ActionId 的重复提交：直接返回上次结果，避免重复执行同一命令。
	if (ActionId.IsValid() && ActionId == LastSubmittedActionId)
	{
		++SubmitCount;
		return LastSubmittedResolution;
	}
	// 先取一份提交时的状态快照，用于之后校验战斗 ID（见下方注释）。
	const FHSRBattleCommandViewState VerifiedSnapshot = GetCurrentViewState();
	// 命令汇无效：无法把命令送达战斗系统，按“无效战斗”拒绝。
	if (!CommandSink.IsValid())
	{
		Resolution.Status = EHSRAbilityResolutionStatus::Rejected;
		Resolution.FailureReason = EHSRAbilityFailureReason::InvalidBattle;
		return Resolution;
	}
	// VM 前置校验（战斗进行中、玩家回合、技能/目标一致等）不通过则拒绝。
	if (!ViewModel.IsValid() || !ViewModel->BeginCommandSubmit(ActionId, ActorParticipantId, SkillId, TargetParticipantId))
	{
		Resolution.Status = EHSRAbilityResolutionStatus::Rejected;
		Resolution.FailureReason = EHSRAbilityFailureReason::AbilityUnavailable;
		return Resolution;
	}

	FHSRBattleActionCommand Command;
	Command.ActionId = ActionId;
	// Use the same snapshot that passed BeginCommandSubmit.  Reading the live
	// sink id after locking could route a stale UI command into a reset battle.
	// 使用与 BeginCommandSubmit 相同的快照：锁定后若再读实时战斗 ID，
	// 可能把陈旧的 UI 命令送进一场已被重置的战斗。
	Command.BattleId = VerifiedSnapshot.BattleId;
	Command.ActorParticipantId = ActorParticipantId;
	Command.SkillId = SkillId;
	Command.TargetParticipantIds.Add(TargetParticipantId);
	// 提交瞬间战斗 ID 与命令汇当前战斗不一致：说明战斗已切换，拒绝并让 VM 收尾挂起状态。
	if (Command.BattleId != CommandSink->GetActiveBattleId())
	{
		Resolution.Status = EHSRAbilityResolutionStatus::Rejected;
		Resolution.FailureReason = EHSRAbilityFailureReason::InvalidBattle;
		ViewModel->ResolveCommandSubmit(Command.BattleId, Resolution);
		return Resolution;
	}
	// 正式提交到命令汇，并记录本次提交的 ActionId 与结果，供去重和解析使用。
	Resolution = CommandSink->SubmitBattleCommand(Command);
	LastSubmittedActionId = ActionId;
	LastSubmittedResolution = Resolution;
	ViewModel->ResolveCommandSubmit(Command.BattleId, Resolution);
	++SubmitCount;
	UE_LOG(LogTemp, Log, TEXT("P6-004A Widget Submit Count=%d ActionId=%s ActorId=%s SkillId=%s TargetId=%s Status=%d Reason=%d"), SubmitCount, *ActionId.ToString(), *ActorParticipantId.ToString(), *SkillId.ToString(), *TargetParticipantId.ToString(), static_cast<int32>(Resolution.Status), static_cast<int32>(Resolution.FailureReason));
	return Resolution;
}

// 提交当前选中的技能：生成新的 ActionId，并以当前行动者/技能/目标构造命令。
FHSRAbilityResolution UHSRBattleCommandWidget::SubmitSelectedSkill()
{
	const FHSRBattleCommandViewState State = GetCurrentViewState();
	return SubmitCommand(FGuid::NewGuid(), State.CurrentActorId, GetSelectedSkillId(), GetSelectedTargetId());
}

// 确认战斗结果：转发给 VM 触发确认流程。
bool UHSRBattleCommandWidget::ConfirmBattleResult()
{
	return ViewModel.IsValid() && ViewModel->RequestBattleResultConfirm();
}

// VM 状态变化回调：每次战斗快照更新都会进入这里，先通知蓝图事件，
// 再刷新本 Widget 的所有受控控件。这是 Widget 响应数据变化的唯一入口。
void UHSRBattleCommandWidget::HandleViewStateChanged(const FHSRBattleCommandViewState& State)
{
	UE_LOG(LogTemp, Verbose, TEXT("P6-004A Widget Snapshot Widget=%s Generation=%d Actor=%s Skills=%d Energy=%.2f/%.2f SP=%d/%d"), *GetName(), BindGeneration, *State.CurrentActorId.ToString(), State.Skills.Num(), State.Energy, State.MaxEnergy, State.SkillPoints, State.MaxSkillPoints);
	OnCommandViewStateChanged(State);
	RefreshDesignerControls(State);
}

// 按类别查找技能视图：直接委托给状态的查询方法。
const FHSRBattleCommandSkillView* UHSRBattleCommandWidget::FindSkillView(const FHSRBattleCommandViewState& State, EHSRSkillCategory Category) const
{
	return State.FindSkillByCategory(Category);
}

// 刷新固定按钮布局的单个技能控件（名称/描述/花费/可用性）。
// 描述文本一律折叠：长段的作者描述属于未来的详情界面，不适合塞进命令按钮。
void UHSRBattleCommandWidget::RefreshSkillControls(const FHSRBattleCommandViewState& State, EHSRSkillCategory Category, UButton* Button, UTextBlock* NameText, UTextBlock* DescriptionText, UTextBlock* CostText)
{
	const FHSRBattleCommandSkillView* Skill = FindSkillView(State, Category);
	const bool bUnlockedPlayerTurn = !State.ResultViewState.bVisible && State.bCurrentActorPlayerControlled && !State.bCommandPending && !State.bPresentationLocked;
	if (Button)
	{
		Button->SetIsEnabled(Skill && Skill->bAvailable && bUnlockedPlayerTurn);
	}
	if (NameText)
	{
		NameText->SetText(Skill ? Skill->DisplayName : FText::FromString(TEXT("Unavailable")));
	}
	// Long authored descriptions belong to the future detail screen, not command buttons.
	// 长描述属于未来的详情界面，命令按钮上不展示，直接折叠。
	if (DescriptionText)
	{
		DescriptionText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (!CostText)
	{
		return;
	}

	// Shared with the skill-list entries so both surfaces render one cost rule. The old chain here
	// hardcoded "SP +1" for BasicAttack, which silently misreported any other authored delta.
	// 花费文本与技能列表共用同一套“构建花费文本”逻辑，保证两处显示一致。
	// 旧实现曾对普攻硬编码“SP +1”，遇到其它作者配置的消耗会静默显示错误。
	CostText->SetText(Skill ? Skill->BuildCostText() : FText::GetEmpty());
}

// 设计期控件刷新总入口：把所有受控的 UMG 控件（文本/按钮/下拉框/覆盖层）
// 一次性同步为当前状态。bRefreshingDesignerControls 标志用于防止刷新引发的
// 控件事件（如下拉框选项变化）反过来触发选择逻辑造成循环。
void UHSRBattleCommandWidget::RefreshDesignerControls(const FHSRBattleCommandViewState& State)
{
	bRefreshingDesignerControls = true;
	if (TXT_CurrentActor)
	{
		TXT_CurrentActor->SetText(GetCurrentActorText());
	}
	if (TXT_Energy)
	{
		TXT_Energy->SetText(GetEnergyText());
	}
	if (TXT_SkillPoints)
	{
		TXT_SkillPoints->SetText(GetSkillPointsText());
	}
	if (TXT_Last_Resolution)
	{
		TXT_Last_Resolution->SetText(GetLastResolutionText());
	}
	if (TXT_Weakness)
	{
		TXT_Weakness->SetText(GetWeaknessText());
	}
	if (TXT_Toughness)
	{
		TXT_Toughness->SetText(GetToughnessText());
	}
	if (TXT_Break)
	{
		TXT_Break->SetText(GetBreakText());
	}
	if (TXT_Delay)
	{
		TXT_Delay->SetText(GetDelayText());
	}
	if (TXT_Statuses)
	{
		TXT_Statuses->SetText(GetStatusText());
	}
	if (TXT_StatisOperation)
	{
		TXT_StatisOperation->SetText(GetStatusOperationText());
	}
	if (TXT_TurnOrder)
	{
		TXT_TurnOrder->SetText(GetTurnOrderText());
	}
	if (TXT_Participants)
	{
		TXT_Participants->SetText(GetParticipantsText());
	}
	if (TXT_Presentation)
	{
		TXT_Presentation->SetText(GetPresentationText());
	}
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
	// 结果界面可见时，把焦点移到确认按钮上，方便手柄/键盘直接确认。
	if (State.ResultViewState.bVisible)
	{
		FocusResultConfirm();
	}

	// 两种技能呈现方式二选一：技能列表模式或固定按钮模式。
	if (UsesSkillList())
	{
		RefreshSkillList(State);
	}
	else
	{
		RefreshSkillControls(State, EHSRSkillCategory::BasicAttack, BTN_Basic, TXT_BasicName, TXT_BasicDescription, TXT_BasicCost);
		RefreshSkillControls(State, EHSRSkillCategory::Skill, BTN_Skill, TXT_SkillName, TXT_SkillDescription, TXT_SkillCost);
		RefreshSkillControls(State, EHSRSkillCategory::Ultimate, BTN_Ultimate, TXT_UltimateName, TXT_UltimateDescription, TXT_UltimateCost);
		RefreshSkillControls(State, EHSRSkillCategory::Heal, Button_Heal, TXT_HealName, TXT_HealDescription, TXT_HealCost);
	}

	// “禁用原因”文本：按优先级显示当前为什么不能行动。
	const FHSRBattleCommandSkillView* SelectedSkill = State.FindSelectedSkill();
	if (TXT_DisabledReason)
	{
		FText Reason = FText::GetEmpty();
		if (!State.bCurrentActorPlayerControlled)
		{
			Reason = NSLOCTEXT("HSRCommand", "EnemyTurn", "Waiting for enemy action");
		}
		else if (State.bCommandPending)
		{
			Reason = NSLOCTEXT("HSRCommand", "Pending", "Command pending");
		}
		else if (State.bPresentationLocked)
		{
			Reason = NSLOCTEXT("HSRCommand", "PresentationLocked", "Presentation locked");
		}
		else if (SelectedSkill && !SelectedSkill->bAvailable)
		{
			// 技能自身不可用时，用枚举的显示名给出具体原因。
			if (const UEnum* FailureEnum = StaticEnum<EHSRAbilityFailureReason>())
			{
				Reason = FailureEnum->GetDisplayNameTextByValue(static_cast<int64>(SelectedSkill->DisabledReason));
			}
		}
		TXT_DisabledReason->SetText(Reason);
	}
	if (PendingOverlay)
	{
		PendingOverlay->SetText(State.bCommandPending ? NSLOCTEXT("HSRCommand", "PendingOverlay", "Command Pending...") : FText::GetEmpty());
		PendingOverlay->SetVisibility((State.bCommandPending || State.bPresentationLocked) ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
	if (BTN_Execute)
	{
		BTN_Execute->SetIsEnabled(State.bCanSubmit);
	}

	// 目标下拉框：重建选项列表并选中当前目标；只有一个候选目标时禁用下拉。
	if (CB_Target)
	{
		CB_Target->ClearOptions();
		if (SelectedSkill)
		{
			for (const FName TargetId : SelectedSkill->CandidateTargetIds)
			{
				CB_Target->AddOption(TargetId.ToString());
			}
		}
		if (!State.SelectedTargetId.IsNone())
		{
			CB_Target->SetSelectedOption(State.SelectedTargetId.ToString());
		}
		CB_Target->SetIsEnabled(!State.ResultViewState.bVisible && State.bCurrentActorPlayerControlled && !State.bCommandPending && !State.bPresentationLocked && SelectedSkill && SelectedSkill->CandidateTargetIds.Num() > 1);
	}
	bRefreshingDesignerControls = false;
}

// 绑定各设计期按钮的点击事件。每次刷新前先解绑，避免重复绑定。
void UHSRBattleCommandWidget::BindDesignerEvents()
{
	UnbindDesignerEvents();
	if (BTN_Basic)
	{
		BTN_Basic->OnClicked.AddDynamic(this, &UHSRBattleCommandWidget::HandleBasicClicked);
	}
	if (BTN_Skill)
	{
		BTN_Skill->OnClicked.AddDynamic(this, &UHSRBattleCommandWidget::HandleSkillClicked);
	}
	if (BTN_Ultimate)
	{
		BTN_Ultimate->OnClicked.AddDynamic(this, &UHSRBattleCommandWidget::HandleUltimateClicked);
	}
	if (Button_Heal)
	{
		Button_Heal->OnClicked.AddDynamic(this, &UHSRBattleCommandWidget::HandleHealClicked);
	}
	if (BTN_Execute)
	{
		BTN_Execute->OnClicked.AddDynamic(this, &UHSRBattleCommandWidget::HandleExecuteClicked);
	}
	if (BTN_ResultConfirm)
	{
		BTN_ResultConfirm->OnClicked.AddDynamic(this, &UHSRBattleCommandWidget::HandleResultConfirmClicked);
	}
}

// 解除所有按钮的点击事件绑定。
void UHSRBattleCommandWidget::UnbindDesignerEvents()
{
	if (BTN_Basic)
	{
		BTN_Basic->OnClicked.RemoveDynamic(this, &UHSRBattleCommandWidget::HandleBasicClicked);
	}
	if (BTN_Skill)
	{
		BTN_Skill->OnClicked.RemoveDynamic(this, &UHSRBattleCommandWidget::HandleSkillClicked);
	}
	if (BTN_Ultimate)
	{
		BTN_Ultimate->OnClicked.RemoveDynamic(this, &UHSRBattleCommandWidget::HandleUltimateClicked);
	}
	if (Button_Heal)
	{
		Button_Heal->OnClicked.RemoveDynamic(this, &UHSRBattleCommandWidget::HandleHealClicked);
	}
	if (BTN_Execute)
	{
		BTN_Execute->OnClicked.RemoveDynamic(this, &UHSRBattleCommandWidget::HandleExecuteClicked);
	}
	if (BTN_ResultConfirm)
	{
		BTN_ResultConfirm->OnClicked.RemoveDynamic(this, &UHSRBattleCommandWidget::HandleResultConfirmClicked);
	}
}

// 各按钮点击处理器：只做一件事——把操作转成对应的技能选择/提交/确认调用。
void UHSRBattleCommandWidget::HandleBasicClicked()
{
	SelectSkill(EHSRSkillCategory::BasicAttack);
}

void UHSRBattleCommandWidget::HandleSkillClicked()
{
	SelectSkill(EHSRSkillCategory::Skill);
}

void UHSRBattleCommandWidget::HandleUltimateClicked()
{
	SelectSkill(EHSRSkillCategory::Ultimate);
}

void UHSRBattleCommandWidget::HandleHealClicked()
{
	SelectSkill(EHSRSkillCategory::Heal);
}

void UHSRBattleCommandWidget::HandleExecuteClicked()
{
	SubmitSelectedSkill();
}

void UHSRBattleCommandWidget::HandleResultConfirmClicked()
{
	ConfirmBattleResult();
}

// 把焦点给到“确认结果”按钮；按钮不可用时退回 Widget 自身兜底。
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

// 解除与 ViewModel 的全部关联：移除事件订阅、清空引用、复位提交去重状态。
void UHSRBattleCommandWidget::UnbindViewModel()
{
	if (ViewModel.IsValid() && StateChangedHandle.IsValid())
	{
		ViewModel->OnChanged().Remove(StateChangedHandle);
		UE_LOG(LogTemp, Log, TEXT("P6-004A Widget Unbind Result=SUCCESS Widget=%s Generation=%d ActiveBindings=0"), *GetName(), BindGeneration);
	}
	StateChangedHandle.Reset();
	ViewModel.Reset();
	CommandSink.Reset();
	LastSubmittedActionId.Invalidate();
	LastSubmittedResolution = FHSRAbilityResolution();
}

// 销毁前解绑所有事件与 VM，保证 Widget 生命周期结束后不再接收任何回调。
void UHSRBattleCommandWidget::NativeDestruct()
{
	UnbindDesignerEvents();
	UnbindViewModel();
	UE_LOG(LogTemp, Log, TEXT("P6-004A Widget NativeDestruct Widget=%s Generation=%d SubmitCount=%d"), *GetName(), BindGeneration, SubmitCount);
	Super::NativeDestruct();
}
