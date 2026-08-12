#include "HSRQuestViewModel.h"

#include "../Quest/HSRQuestSubsystem.h"

// 销毁前先关闭（解绑任务子系统事件），避免子系统在 VM 销毁后再回调它。
void UHSRQuestViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

// 初始化：数据源是任务子系统（UHSRQuestSubsystem）。
// 订阅“任务变化”与“任务恢复”两类事件，任一事件都会触发 Rebuild 重建快照并广播。
void UHSRQuestViewModel::Initialize(UHSRQuestSubsystem* InQuest)
{
	// 重新初始化前先清理旧的订阅。
	Shutdown();
	Quest = InQuest;
	if (InQuest)
	{
		QuestChangedHandle = InQuest->OnQuestChanged().AddUObject(this, &ThisClass::HandleQuestChanged);
		QuestRestoredHandle = InQuest->OnQuestRestored().AddUObject(this, &ThisClass::HandleQuestRestored);
	}
	// 立即构建一次初始快照。
	Rebuild();
}

// 关闭：移除所有事件订阅、清空子系统引用与快照状态。
void UHSRQuestViewModel::Shutdown()
{
	if (Quest.IsValid())
	{
		Quest->OnQuestChanged().Remove(QuestChangedHandle);
		Quest->OnQuestRestored().Remove(QuestRestoredHandle);
	}
	Quest.Reset();
	QuestChangedHandle.Reset();
	QuestRestoredHandle.Reset();
	Snapshot = FHSRQuestFrontendSnapshot();
}

// 取快照：把当前快照副本写入出参。
bool UHSRQuestViewModel::GetSnapshot(FHSRQuestFrontendSnapshot& OutSnapshot) const
{
	OutSnapshot = Snapshot;
	return true;
}

// 任务变化事件：重建快照并广播。
void UHSRQuestViewModel::HandleQuestChanged(const FHSRQuestRuntimeState&)
{
	Rebuild();
}

// 任务恢复事件（如存档读档后）：重建快照并广播。
void UHSRQuestViewModel::HandleQuestRestored(int64)
{
	Rebuild();
}

// 重建快照：从任务子系统拉取全部任务运行时状态，转成前端展示所需的数据视图
// （FHSRQuestViewData），并汇总各任务的最大修订号作为快照修订号。
// 任务子系统不可用时，快照状态置为 Unavailable。
void UHSRQuestViewModel::Rebuild()
{
	FHSRQuestFrontendSnapshot Next;
	if (!Quest.IsValid())
	{
		Next.Status = EHSRQuestFrontendStatus::Unavailable;
	}
	else
	{
		TArray<FHSRQuestRuntimeState> States;
		Quest->GetQuestStates(States);
		// 无任务时状态为 Empty，否则为 Ready。
		Next.Status = States.IsEmpty() ? EHSRQuestFrontendStatus::Empty : EHSRQuestFrontendStatus::Ready;
		// 把每个任务的运行时状态映射成展示视图，同时展开其目标列表。
		for (const FHSRQuestRuntimeState& State : States)
		{
			FHSRQuestViewData& View = Next.Quests.AddDefaulted_GetRef();
			View.QuestId = State.QuestId;
			View.State = State.State;
			View.bRewardClaimed = State.bRewardClaimed;
			// 任务定义是否存在也纳入视图（用于前端区分“有定义/已失效”的任务）。
			View.bDefinitionAvailable = Quest->HasDefinition(State.QuestId);
			View.Revision = State.Revision;
			Next.Revision = FMath::Max(Next.Revision, State.Revision);
			for (const FHSRQuestRuntimeObjective& Objective : State.Objectives)
			{
				FHSRQuestObjectiveViewData& ObjectiveView = View.Objectives.AddDefaulted_GetRef();
				ObjectiveView.ObjectiveId = Objective.ObjectiveId;
				ObjectiveView.CurrentCount = Objective.CurrentCount;
				ObjectiveView.RequiredCount = Objective.RequiredCount;
				ObjectiveView.bCompleted = Objective.bCompleted;
			}
		}
	}
	// 移动新快照入位并广播。
	Snapshot = MoveTemp(Next);
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}
