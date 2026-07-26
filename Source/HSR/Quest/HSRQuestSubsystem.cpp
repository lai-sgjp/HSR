#include "HSRQuestSubsystem.h"

#include "../Data/Definitions/HSRQuestDefinition.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "Misc/Crc.h"

void UHSRQuestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Reward = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRRewardSubsystem>() : nullptr;
}

#if WITH_DEV_AUTOMATION_TESTS
void UHSRQuestSubsystem::InitializeForAutomation(UHSRRewardSubsystem* InReward)
{
	Reward = InReward;
}
#endif

#if WITH_EDITOR
void UHSRQuestSubsystem::InitializeForDevelopmentTest(UHSRRewardSubsystem* InReward)
{
	Reward = InReward;
}
#endif

EHSRQuestOperationResult UHSRQuestSubsystem::RegisterQuestDefinition(const UHSRQuestDefinition& Definition)
{
	const EHSRQuestOperationResult Validation = CanRegisterQuestDefinition(Definition);
	if (Validation != EHSRQuestOperationResult::Success)
	{
		return Validation;
	}
	FQuestRule Rule;
	Rule.QuestId = Definition.QuestId;
	Rule.Objectives = Definition.Objectives;
	Rule.RewardDefinitionId = Definition.RewardDefinitionId;
	Rule.RewardSeed = Definition.RewardSeed;
	Rule.bAutoClaimReward = Definition.bAutoClaimReward;
	QuestDefinitions.Add(Rule.QuestId, MoveTemp(Rule));
	return EHSRQuestOperationResult::Success;
}

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

bool UHSRQuestSubsystem::BuildInitialState(const FQuestRule& Rule, FHSRQuestRuntimeState& OutState) const
{
	OutState = FHSRQuestRuntimeState();
	OutState.QuestId = Rule.QuestId;
	OutState.State = EHSRQuestState::Active;
	OutState.RewardClaimId = MakeQuestRewardClaimId(Rule.QuestId);
	OutState.Revision = Revision + 1;
	for (const FHSRQuestObjectiveDefinition& Objective : Rule.Objectives)
	{
		FHSRQuestRuntimeObjective RuntimeObjective;
		RuntimeObjective.ObjectiveId = Objective.ObjectiveId;
		RuntimeObjective.RequiredCount = Objective.RequiredCount;
		OutState.Objectives.Add(RuntimeObjective);
	}
	return !OutState.Objectives.IsEmpty();
}

EHSRQuestOperationResult UHSRQuestSubsystem::StartQuest(FName QuestId, FHSRQuestRuntimeState& OutState)
{
	const FQuestRule* Rule = QuestDefinitions.Find(QuestId);
	if (!Rule)
	{
		return EHSRQuestOperationResult::UnknownQuestDefinition;
	}
	if (const FHSRQuestRuntimeState* Existing = QuestStates.Find(QuestId))
	{
		OutState = *Existing;
		return EHSRQuestOperationResult::NoOp;
	}
	if (!BuildInitialState(*Rule, OutState))
	{
		return EHSRQuestOperationResult::InvalidDefinition;
	}
	OutState.Revision = ++Revision;
	QuestStates.Add(QuestId, OutState);
	QuestChanged.Broadcast(OutState);
	return EHSRQuestOperationResult::Success;
}

EHSRQuestOperationResult UHSRQuestSubsystem::SubmitEvent(const FHSRQuestDomainEvent& Event, TArray<FHSRQuestRuntimeState>& OutChangedStates)
{
	OutChangedStates.Reset();
	if (Event.EventId.IsNone() || Event.Count <= 0)
	{
		return EHSRQuestOperationResult::InvalidEvent;
	}
	bool bMatched = false;
	for (TPair<FName, FHSRQuestRuntimeState>& Entry : QuestStates)
	{
		FHSRQuestRuntimeState& State = Entry.Value;
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
		for (int32 Index = 0; Index < Rule->Objectives.Num() && Index < State.Objectives.Num(); ++Index)
		{
			const FHSRQuestObjectiveDefinition& ObjectiveRule = Rule->Objectives[Index];
			FHSRQuestRuntimeObjective& ObjectiveState = State.Objectives[Index];
			if (ObjectiveRule.EventId == Event.EventId)
			{
				bMatched = true;
				if (ObjectiveState.bCompleted)
				{
					continue;
				}
				ObjectiveState.CurrentCount = FMath::Clamp(ObjectiveState.CurrentCount + Event.Count, 0, ObjectiveState.RequiredCount);
				ObjectiveState.bCompleted = ObjectiveState.CurrentCount >= ObjectiveState.RequiredCount;
				bChanged = true;
			}
		}
		if (bChanged && IsComplete(State))
		{
			State.State = EHSRQuestState::Completed;
		}
		if (bChanged)
		{
			State.Revision = ++Revision;
			OutChangedStates.Add(State);
			QuestChanged.Broadcast(State);
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
	FHSRRewardRequest Request;
	Request.ClaimId = State->RewardClaimId.IsValid() ? State->RewardClaimId : MakeQuestRewardClaimId(QuestId);
	Request.RewardDefinitionId = Rule->RewardDefinitionId;
	Request.Seed = Rule->RewardSeed;
	OutResult.RewardResult = Reward->SubmitReward(Request, OutResult.Receipt);
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

void UHSRQuestSubsystem::GetQuestStates(TArray<FHSRQuestRuntimeState>& OutStates) const
{
	QuestStates.GenerateValueArray(OutStates);
	OutStates.Sort([](const FHSRQuestRuntimeState& A, const FHSRQuestRuntimeState& B)
	{
		return A.QuestId.LexicalLess(B.QuestId);
	});
}

void UHSRQuestSubsystem::ExportSaveData(FHSRQuestSaveData& OutData) const
{
	GetQuestStates(OutData.States);
	OutData.Revision = Revision;
}

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
		const FQuestRule* Rule = QuestDefinitions.Find(State.QuestId);
		if (!Rule || Candidate.States.Contains(State.QuestId) || State.State == EHSRQuestState::NotStarted || State.Revision <= 0 || State.Revision > Data.Revision || State.Objectives.Num() != Rule->Objectives.Num())
		{
			return false;
		}
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
		if ((State.State == EHSRQuestState::Completed) != bAllComplete)
		{
			return false;
		}
		if (State.bRewardClaimed && State.State != EHSRQuestState::Completed)
		{
			return false;
		}
		if (!State.RewardClaimId.IsValid() || State.RewardClaimId != MakeQuestRewardClaimId(State.QuestId))
		{
			return false;
		}
		MaxStateRevision = FMath::Max(MaxStateRevision, State.Revision);
		Candidate.States.Add(State.QuestId, State);
	}
	if ((Data.States.IsEmpty() && Data.Revision != 0) || (!Data.States.IsEmpty() && MaxStateRevision > Data.Revision))
	{
		return false;
	}
	OutCandidate = MoveTemp(Candidate);
	return true;
}

bool UHSRQuestSubsystem::IsRestoreDifferent(const FHSRQuestRestoreState& Candidate) const
{
	if (Revision != Candidate.Revision || QuestStates.Num() != Candidate.States.Num())
	{
		return true;
	}
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

void UHSRQuestSubsystem::CommitRestore(FHSRQuestRestoreState&& Candidate, bool bNotify)
{
	QuestStates = MoveTemp(Candidate.States);
	Revision = Candidate.Revision;
	if (bNotify)
	{
		QuestRestored.Broadcast(Revision);
	}
}

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
