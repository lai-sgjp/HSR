#include "HSRQuestViewModel.h"

#include "../Quest/HSRQuestSubsystem.h"

void UHSRQuestViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UHSRQuestViewModel::Initialize(UHSRQuestSubsystem* InQuest)
{
	Shutdown();
	Quest = InQuest;
	if (InQuest)
	{
		QuestChangedHandle = InQuest->OnQuestChanged().AddUObject(this, &ThisClass::HandleQuestChanged);
		QuestRestoredHandle = InQuest->OnQuestRestored().AddUObject(this, &ThisClass::HandleQuestRestored);
	}
	Rebuild();
}

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

bool UHSRQuestViewModel::GetSnapshot(FHSRQuestFrontendSnapshot& OutSnapshot) const
{
	OutSnapshot = Snapshot;
	return true;
}

void UHSRQuestViewModel::HandleQuestChanged(const FHSRQuestRuntimeState&)
{
	Rebuild();
}

void UHSRQuestViewModel::HandleQuestRestored(int64)
{
	Rebuild();
}

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
		Next.Status = States.IsEmpty() ? EHSRQuestFrontendStatus::Empty : EHSRQuestFrontendStatus::Ready;
		for (const FHSRQuestRuntimeState& State : States)
		{
			FHSRQuestViewData& View = Next.Quests.AddDefaulted_GetRef();
			View.QuestId = State.QuestId;
			View.State = State.State;
			View.bRewardClaimed = State.bRewardClaimed;
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
	Snapshot = MoveTemp(Next);
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}
