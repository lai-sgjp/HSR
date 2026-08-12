#include "HSRQuestSubsystem.h"

#include "../Data/Definitions/HSRQuestDefinition.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "Misc/Crc.h"

// 任务子系统初始化：缓存奖励子系统的引用。
// 单独缓存而非每次都查，是因为后续登记/领取任务时要频繁使用；初始化时 GameInstance 已经就绪。
void UHSRQuestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Reward = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRRewardSubsystem>() : nullptr;
}

#if WITH_DEV_AUTOMATION_TESTS
// 自动化测试专用：注入一个伪造的奖励子系统，避免测试依赖真实的 GameInstance 子系统注册。
void UHSRQuestSubsystem::InitializeForAutomation(UHSRRewardSubsystem* InReward)
{
	Reward = InReward;
}
#endif

#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
// 编辑器/开发测试专用：同样允许外部注入奖励子系统。
void UHSRQuestSubsystem::InitializeForDevelopmentTest(UHSRRewardSubsystem* InReward)
{
	Reward = InReward;
}
#endif

// 登记一份任务定义。先做完整校验，通过后才转成内部规则并存入表。
EHSRQuestOperationResult UHSRQuestSubsystem::RegisterQuestDefinition(const UHSRQuestDefinition& Definition)
{
	const EHSRQuestOperationResult Validation = CanRegisterQuestDefinition(Definition);
	if (Validation != EHSRQuestOperationResult::Success)
	{
		return Validation;
	}
	// 只拷贝运行时需要的字段，避免长期持有 DataAsset 引用。
	FQuestRule Rule;
	Rule.QuestId = Definition.QuestId;
	Rule.Objectives = Definition.Objectives;
	Rule.RewardDefinitionId = Definition.RewardDefinitionId;
	Rule.RewardSeed = Definition.RewardSeed;
	Rule.bAutoClaimReward = Definition.bAutoClaimReward;
	QuestDefinitions.Add(Rule.QuestId, MoveTemp(Rule));
	return EHSRQuestOperationResult::Success;
}

// 校验任务定义是否可以登记。规则：ID 非空、至少一个目标、目标 ID/事件 ID/计数合法且不重复；
// 已登记过的同 ID 任务必须内容完全一致，否则视为冲突。
EHSRQuestOperationResult UHSRQuestSubsystem::CanRegisterQuestDefinition(const UHSRQuestDefinition& Definition) const
{
	if (Definition.QuestId.IsNone())
	{
		return EHSRQuestOperationResult::InvalidDefinitionId;
	}
	if (Definition.Objectives.IsEmpty())
	{
		return EHSRQuestOperationResult::InvalidDefinition;
	}
	TSet<FName> ObjectiveIds;
	for (const FHSRQuestObjectiveDefinition& Objective : Definition.Objectives)
	{
		if (Objective.ObjectiveId.IsNone() || Objective.EventId.IsNone() || Objective.RequiredCount <= 0 || ObjectiveIds.Contains(Objective.ObjectiveId))
		{
			return EHSRQuestOperationResult::InvalidDefinition;
		}
		ObjectiveIds.Add(Objective.ObjectiveId);
	}
	// 同 ID 再次登记：逐项比较，任何字段不一致都视为“重复定义但内容不同”。
	if (const FQuestRule* Existing = QuestDefinitions.Find(Definition.QuestId))
	{
		if (Existing->RewardDefinitionId != Definition.RewardDefinitionId || Existing->RewardSeed != Definition.RewardSeed || Existing->bAutoClaimReward != Definition.bAutoClaimReward || Existing->Objectives.Num() != Definition.Objectives.Num())
		{
			return EHSRQuestOperationResult::DuplicateDefinitionId;
		}
		for (int32 Index = 0; Index < Existing->Objectives.Num(); ++Index)
		{
			const FHSRQuestObjectiveDefinition& A = Existing->Objectives[Index];
			const FHSRQuestObjectiveDefinition& B = Definition.Objectives[Index];
			if (A.ObjectiveId != B.ObjectiveId || A.EventId != B.EventId || A.RequiredCount != B.RequiredCount)
			{
				return EHSRQuestOperationResult::DuplicateDefinitionId;
			}
		}
		return EHSRQuestOperationResult::NoOp;
	}
	return EHSRQuestOperationResult::Success;
}

// 根据规则构建任务的初始运行时状态：目标全部计数为 0，奖励领取 ID 由任务 ID 派生。
bool UHSRQuestSubsystem::BuildInitialState(const FQuestRule& Rule, FHSRQuestRuntimeState& OutState) const
{
	// 初始化：任务从 Active 开始，领取 ID 先按规则派生，修订号沿用子系统下一个版本号。
	OutState = FHSRQuestRuntimeState();
	OutState.QuestId = Rule.QuestId;
	OutState.State = EHSRQuestState::Active;
	OutState.RewardClaimId = MakeQuestRewardClaimId(Rule.QuestId);
	OutState.Revision = Revision + 1;
	// 把规则里的目标复制成运行时目标，计数从 0 开始。
	for (const FHSRQuestObjectiveDefinition& Objective : Rule.Objectives)
	{
		FHSRQuestRuntimeObjective RuntimeObjective;
		RuntimeObjective.ObjectiveId = Objective.ObjectiveId;
		RuntimeObjective.RequiredCount = Objective.RequiredCount;
		OutState.Objectives.Add(RuntimeObjective);
	}
	return !OutState.Objectives.IsEmpty();
}

// 启动一个任务。任务已存在则直接返回其当前状态（幂等），否则构建初始状态并广播。
EHSRQuestOperationResult UHSRQuestSubsystem::StartQuest(FName QuestId, FHSRQuestRuntimeState& OutState)
{
	const FQuestRule* Rule = QuestDefinitions.Find(QuestId);
	if (!Rule)
	{
		return EHSRQuestOperationResult::UnknownQuestDefinition;
	}
	// 任务已存在：直接把当前状态回填给调用方，不重复创建。
	if (const FHSRQuestRuntimeState* Existing = QuestStates.Find(QuestId))
	{
		OutState = *Existing;
		return EHSRQuestOperationResult::NoOp;
	}
	// 构建失败说明规则数据异常（例如没有目标）。
	if (!BuildInitialState(*Rule, OutState))
	{
		return EHSRQuestOperationResult::InvalidDefinition;
	}
	OutState.Revision = ++Revision;
	QuestStates.Add(QuestId, OutState);
	QuestChanged.Broadcast(OutState);
	return EHSRQuestOperationResult::Success;
}

// 提交一个领域事件，推进所有“正在监听该事件”的进行中任务目标。
// 返回聚合结果：任何任务有实质变化返回 Success；匹配到事件但没变化返回 NoOp；完全没匹配返回 InvalidEvent。
EHSRQuestOperationResult UHSRQuestSubsystem::SubmitEvent(const FHSRQuestDomainEvent& Event, TArray<FHSRQuestRuntimeState>& OutChangedStates)
{
	OutChangedStates.Reset();
	// 事件必须有 ID 且计数为正，否则无从推进目标。
	if (Event.EventId.IsNone() || Event.Count <= 0)
	{
		return EHSRQuestOperationResult::InvalidEvent;
	}
	bool bMatched = false;
	// 遍历所有进行中的任务，让监听该事件的目标累加进度。
	for (TPair<FName, FHSRQuestRuntimeState>& Entry : QuestStates)
	{
		FHSRQuestRuntimeState& State = Entry.Value;
		// 只推进 Active 任务；已完成/已暂停的任务不接受事件。
		if (State.State != EHSRQuestState::Active)
		{
			continue;
		}
		const FQuestRule* Rule = QuestDefinitions.Find(State.QuestId);
		if (!Rule)
		{
			continue;
		}
		bool bChanged = false;
		// 规则与运行时目标按下标一一对应，必须长度一致（登记时已保证）。
		for (int32 Index = 0; Index < Rule->Objectives.Num() && Index < State.Objectives.Num(); ++Index)
		{
			const FHSRQuestObjectiveDefinition& ObjectiveRule = Rule->Objectives[Index];
			FHSRQuestRuntimeObjective& ObjectiveState = State.Objectives[Index];
			if (ObjectiveRule.EventId == Event.EventId)
			{
				bMatched = true;
				// 已完成的目标不再重复累计。
				if (ObjectiveState.bCompleted)
				{
					continue;
				}
				// 累加并夹在 [0, RequiredCount]，防止越界；达到要求即标记完成。
				ObjectiveState.CurrentCount = FMath::Clamp(ObjectiveState.CurrentCount + Event.Count, 0, ObjectiveState.RequiredCount);
				ObjectiveState.bCompleted = ObjectiveState.CurrentCount >= ObjectiveState.RequiredCount;
				bChanged = true;
			}
		}
		// 所有目标都完成后，任务整体进入 Completed。
		if (bChanged && IsComplete(State))
		{
			State.State = EHSRQuestState::Completed;
		}
		if (bChanged)
		{
			State.Revision = ++Revision;
			OutChangedStates.Add(State);
			QuestChanged.Broadcast(State);
			// 任务完成且定义了自动领奖：立即领取，并用领奖后的最新状态覆盖返回列表里的那份。
			if (State.State == EHSRQuestState::Completed && Rule->bAutoClaimReward)
			{
				FHSRQuestRewardClaimResult ClaimResult;
				ClaimQuestReward(State.QuestId, ClaimResult);
				FHSRQuestRuntimeState Latest;
				if (GetQuestState(State.QuestId, Latest))
				{
					OutChangedStates.Last() = Latest;
				}
			}
		}
	}
	return OutChangedStates.IsEmpty() ? (bMatched ? EHSRQuestOperationResult::NoOp : EHSRQuestOperationResult::InvalidEvent) : EHSRQuestOperationResult::Success;
}

// 领取任务奖励。只允许 Completed 且未领过奖的任务；领取结果写入 OutResult。
// 领取后立刻把任务标记为已领奖并广播，保证 UI/存档看到一致状态。
EHSRQuestOperationResult UHSRQuestSubsystem::ClaimQuestReward(FName QuestId, FHSRQuestRewardClaimResult& OutResult)
{
	OutResult = FHSRQuestRewardClaimResult();
	FHSRQuestRuntimeState* State = QuestStates.Find(QuestId);
	const FQuestRule* Rule = QuestDefinitions.Find(QuestId);
	if (!State || !Rule)
	{
		OutResult.QuestResult = EHSRQuestOperationResult::UnknownQuestDefinition;
		return OutResult.QuestResult;
	}
	if (State->State != EHSRQuestState::Completed)
	{
		OutResult.QuestResult = EHSRQuestOperationResult::InvalidState;
		return OutResult.QuestResult;
	}
	// 已领过奖，或该任务根本没配奖励，视为无事可做。
	if (State->bRewardClaimed || Rule->RewardDefinitionId.IsNone())
	{
		OutResult.QuestResult = EHSRQuestOperationResult::NoOp;
		return OutResult.QuestResult;
	}
	if (!Reward.IsValid())
	{
		OutResult.QuestResult = EHSRQuestOperationResult::RewardRejected;
		return OutResult.QuestResult;
	}
	// 组装奖励请求：领取 ID 优先用已有的（保证幂等），否则由任务 ID 派生。
	FHSRRewardRequest Request;
	Request.ClaimId = State->RewardClaimId.IsValid() ? State->RewardClaimId : MakeQuestRewardClaimId(QuestId);
	Request.RewardDefinitionId = Rule->RewardDefinitionId;
	Request.Seed = Rule->RewardSeed;
	OutResult.RewardResult = Reward->SubmitReward(Request, OutResult.Receipt);
	// 奖励子系统拒绝时，任务侧也视为“领取被拒”，不落任何状态。
	if (OutResult.RewardResult != EHSRRewardOperationResult::Success && OutResult.RewardResult != EHSRRewardOperationResult::NoOp)
	{
		OutResult.QuestResult = EHSRQuestOperationResult::RewardRejected;
		return OutResult.QuestResult;
	}
	State->bRewardClaimed = true;
	State->RewardClaimId = Request.ClaimId;
	State->Revision = ++Revision;
	OutResult.QuestResult = OutResult.RewardResult == EHSRRewardOperationResult::Success ? EHSRQuestOperationResult::Success : EHSRQuestOperationResult::NoOp;
	QuestChanged.Broadcast(*State);
	return OutResult.QuestResult;
}

// 查询单个任务状态；任务不存在返回 false。
bool UHSRQuestSubsystem::GetQuestState(FName QuestId, FHSRQuestRuntimeState& OutState) const
{
	const FHSRQuestRuntimeState* State = QuestStates.Find(QuestId);
	if (!State)
	{
		return false;
	}
	OutState = *State;
	return true;
}

// 导出存档数据：按任务 ID 字典序排列所有任务状态，保证存档内容稳定可比较。
void UHSRQuestSubsystem::GetQuestStates(TArray<FHSRQuestRuntimeState>& OutStates) const
{
	QuestStates.GenerateValueArray(OutStates);
	OutStates.Sort([](const FHSRQuestRuntimeState& A, const FHSRQuestRuntimeState& B)
	{
		return A.QuestId.LexicalLess(B.QuestId);
	});
}

// 导出存档数据：任务状态 + 全局修订号。
void UHSRQuestSubsystem::ExportSaveData(FHSRQuestSaveData& OutData) const
{
	GetQuestStates(OutData.States);
	OutData.Revision = Revision;
}

// 校验并准备一份任务存档恢复候选。规则：
// 每个任务都必须仍被注册、不重复、状态自洽（目标与规则一一对应、Completed 状态与目标全完成一致、
// 已领奖的任务必须是 Completed、领取 ID 与派生规则一致），且各状态修订号不超过存档修订号。
bool UHSRQuestSubsystem::PrepareRestore(const FHSRQuestSaveData& Data, FHSRQuestRestoreState& OutCandidate) const
{
	if (Data.Revision < 0)
	{
		return false;
	}
	FHSRQuestRestoreState Candidate;
	Candidate.Revision = Data.Revision;
	int64 MaxStateRevision = 0;
	for (const FHSRQuestRuntimeState& State : Data.States)
	{
		// 任务规则必须仍存在；同一任务只允许出现一次；任务必须已是 Active/Completed；
		// 修订号必须为正且不超过存档修订号；目标数量必须与规则一致。
		const FQuestRule* Rule = QuestDefinitions.Find(State.QuestId);
		if (!Rule || Candidate.States.Contains(State.QuestId) || State.State == EHSRQuestState::NotStarted || State.Revision <= 0 || State.Revision > Data.Revision || State.Objectives.Num() != Rule->Objectives.Num())
		{
			return false;
		}
		// 逐目标核对：ID 与规则一致、计数合法且与完成标记一致。
		bool bAllComplete = true;
		for (int32 Index = 0; Index < Rule->Objectives.Num(); ++Index)
		{
			const FHSRQuestObjectiveDefinition& RuleObjective = Rule->Objectives[Index];
			const FHSRQuestRuntimeObjective& SavedObjective = State.Objectives[Index];
			if (SavedObjective.ObjectiveId != RuleObjective.ObjectiveId || SavedObjective.RequiredCount != RuleObjective.RequiredCount || SavedObjective.CurrentCount < 0 || SavedObjective.CurrentCount > SavedObjective.RequiredCount || SavedObjective.bCompleted != (SavedObjective.CurrentCount >= SavedObjective.RequiredCount))
			{
				return false;
			}
			bAllComplete &= SavedObjective.bCompleted;
		}
		// 任务整体状态必须与“目标是否全完成”一致，防止存档把半完成状态标成 Completed。
		if ((State.State == EHSRQuestState::Completed) != bAllComplete)
		{
			return false;
		}
		// 已领奖的任务不可能是未完成状态。
		if (State.bRewardClaimed && State.State != EHSRQuestState::Completed)
		{
			return false;
		}
		// 领取 ID 必须是有效的且符合派生规则（防止手改存档）。
		if (!State.RewardClaimId.IsValid() || State.RewardClaimId != MakeQuestRewardClaimId(State.QuestId))
		{
			return false;
		}
		MaxStateRevision = FMath::Max(MaxStateRevision, State.Revision);
		Candidate.States.Add(State.QuestId, State);
	}
	// 修订号一致性兜底：空存档必须修订号为 0；非空存档里任何任务修订号都不能超过存档修订号。
	if ((Data.States.IsEmpty() && Data.Revision != 0) || (!Data.States.IsEmpty() && MaxStateRevision > Data.Revision))
	{
		return false;
	}
	OutCandidate = MoveTemp(Candidate);
	return true;
}

// 判断恢复候选与当前运行时任务状态是否不同（用于决定是否需要提交）。
bool UHSRQuestSubsystem::IsRestoreDifferent(const FHSRQuestRestoreState& Candidate) const
{
	if (Revision != Candidate.Revision || QuestStates.Num() != Candidate.States.Num())
	{
		return true;
	}
	// 逐任务逐字段比较；任一不一致即视为不同。
	for (const TPair<FName, FHSRQuestRuntimeState>& Entry : QuestStates)
	{
		const FHSRQuestRuntimeState* Other = Candidate.States.Find(Entry.Key);
		if (!Other || Other->State != Entry.Value.State || Other->bRewardClaimed != Entry.Value.bRewardClaimed || Other->RewardClaimId != Entry.Value.RewardClaimId || Other->Revision != Entry.Value.Revision || Other->Objectives.Num() != Entry.Value.Objectives.Num())
		{
			return true;
		}
		for (int32 Index = 0; Index < Entry.Value.Objectives.Num(); ++Index)
		{
			const FHSRQuestRuntimeObjective& A = Entry.Value.Objectives[Index];
			const FHSRQuestRuntimeObjective& B = Other->Objectives[Index];
			if (A.ObjectiveId != B.ObjectiveId || A.CurrentCount != B.CurrentCount || A.RequiredCount != B.RequiredCount || A.bCompleted != B.bCompleted)
			{
				return true;
			}
		}
	}
	return false;
}

// 提交恢复：整体替换任务表与修订号，可选广播“任务已恢复”。
void UHSRQuestSubsystem::CommitRestore(FHSRQuestRestoreState&& Candidate, bool bNotify)
{
	QuestStates = MoveTemp(Candidate.States);
	Revision = Candidate.Revision;
	if (bNotify)
	{
		QuestRestored.Broadcast(Revision);
	}
}

// 判断任务是否全部目标完成。空目标列表视为未完成（登记时已保证非空，这里是防御）。
bool UHSRQuestSubsystem::IsComplete(const FHSRQuestRuntimeState& State) const
{
	if (State.Objectives.IsEmpty())
	{
		return false;
	}
	for (const FHSRQuestRuntimeObjective& Objective : State.Objectives)
	{
		if (!Objective.bCompleted)
		{
			return false;
		}
	}
	return true;
}

// 由任务 ID 派生一个确定性的奖励领取 GUID。
// 用 CRC 散列填充 GUID 的高位段，并用固定魔数混淆；同一任务 ID 永远得到同一 GUID，
// 这样“领取 ID”在跨存档/跨会话间稳定，幂等校验才有意义。
FGuid UHSRQuestSubsystem::MakeQuestRewardClaimId(FName QuestId)
{
	const uint32 Hash = FCrc::StrCrc32(*QuestId.ToString());
	FGuid Result(0x51455354u, Hash, Hash ^ 0xA14E13u, Hash ^ 0xC1A1Du);
	if (!Result.IsValid())
	{
		Result.D = 1;
	}
	return Result;
}
