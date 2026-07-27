#include "HSRScreenStack.h"

namespace
{
	int32 LayerPriority(const EHSRUIScreenLayer Layer)
	{
		return static_cast<int32>(Layer);
	}
}

EHSRScreenStackResult UHSRScreenStack::SubmitRequest(const FHSRScreenRequest& Request)
{
	if (Request.RequestToken <= 0)
	{
		return EHSRScreenStackResult::InvalidRequest;
	}
	if (Request.RequestToken == Snapshot.LastProcessedRequestToken)
	{
		return EHSRScreenStackResult::AlreadyProcessed;
	}
	if (Request.RequestToken < Snapshot.LastProcessedRequestToken)
	{
		return EHSRScreenStackResult::StaleRequest;
	}
	const EHSRScreenStackResult ValidationResult = ValidateRequest(Request);
	if (ValidationResult != EHSRScreenStackResult::Success)
	{
		return ValidationResult;
	}

	TArray<FHSRScreenStackEntry> Candidate = Snapshot.Entries;
	const EHSRScreenStackResult Result = ApplyToCandidate(Request, Candidate);
	if (Result != EHSRScreenStackResult::Success && Result != EHSRScreenStackResult::NoOp)
	{
		return Result;
	}

	Snapshot.Entries = MoveTemp(Candidate);
	Snapshot.LastProcessedRequestToken = Request.RequestToken;
	return Result;
}

bool UHSRScreenStack::GetActiveEntry(FHSRScreenStackEntry& OutEntry) const
{
	const int32 Index = FindActiveEntryIndex(Snapshot.Entries);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	OutEntry = Snapshot.Entries[Index];
	return true;
}

int32 UHSRScreenStack::FindActiveEntryIndex(const TArray<FHSRScreenStackEntry>& Entries)
{
	int32 ActiveIndex = INDEX_NONE;
	int32 ActivePriority = INDEX_NONE;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const int32 Priority = LayerPriority(Entries[Index].Layer);
		if (Priority >= ActivePriority)
		{
			ActivePriority = Priority;
			ActiveIndex = Index;
		}
	}
	return ActiveIndex;
}

EHSRScreenStackResult UHSRScreenStack::ValidateRequest(const FHSRScreenRequest& Request)
{
	if (Request.Layer != EHSRUIScreenLayer::HUD
		&& Request.Layer != EHSRUIScreenLayer::Menu
		&& Request.Layer != EHSRUIScreenLayer::Modal)
	{
		return EHSRScreenStackResult::InvalidRequest;
	}
	if (Request.InputIntent != EHSRUIInputIntent::GameOnly
		&& Request.InputIntent != EHSRUIInputIntent::GameAndUI
		&& Request.InputIntent != EHSRUIInputIntent::UIOnly)
	{
		return EHSRScreenStackResult::InvalidRequest;
	}

	switch (Request.Operation)
	{
	case EHSRScreenStackOperation::Push:
	case EHSRScreenStackOperation::Replace:
		return Request.ScreenId.IsNone() ? EHSRScreenStackResult::InvalidScreenId : EHSRScreenStackResult::Success;
	case EHSRScreenStackOperation::Pop:
	case EHSRScreenStackOperation::CloseToRoot:
		return Request.ScreenId.IsNone() ? EHSRScreenStackResult::Success : EHSRScreenStackResult::InvalidRequest;
	default:
		return EHSRScreenStackResult::InvalidRequest;
	}
}

bool UHSRScreenStack::ContainsScreen(const TArray<FHSRScreenStackEntry>& Entries, const FName ScreenId)
{
	return Entries.ContainsByPredicate([ScreenId](const FHSRScreenStackEntry& Entry)
	{
		return Entry.ScreenId == ScreenId;
	});
}

EHSRScreenStackResult UHSRScreenStack::ApplyToCandidate(const FHSRScreenRequest& Request, TArray<FHSRScreenStackEntry>& Candidate) const
{
	if (Request.Operation == EHSRScreenStackOperation::Push || Request.Operation == EHSRScreenStackOperation::Replace)
	{
		if (Request.ScreenId.IsNone())
		{
			return EHSRScreenStackResult::InvalidScreenId;
		}
		if (ContainsScreen(Candidate, Request.ScreenId))
		{
			return EHSRScreenStackResult::DuplicateScreen;
		}
	}

	if (Request.Operation == EHSRScreenStackOperation::Push)
	{
		if (Candidate.IsEmpty() && Request.Layer != EHSRUIScreenLayer::HUD)
		{
			return EHSRScreenStackResult::RootRequired;
		}
		if (!Candidate.IsEmpty() && Request.Layer == EHSRUIScreenLayer::HUD)
		{
			return EHSRScreenStackResult::RootAlreadyExists;
		}
		Candidate.Add({Request.ScreenId, Request.Layer, Request.InputIntent, Request.FocusToken, Request.RequestToken});
		return EHSRScreenStackResult::Success;
	}

	const int32 ActiveIndex = FindActiveEntryIndex(Candidate);
	if (ActiveIndex == INDEX_NONE)
	{
		return EHSRScreenStackResult::EmptyStack;
	}

	if (Request.Operation == EHSRScreenStackOperation::Pop)
	{
		if (ActiveIndex == 0)
		{
			return EHSRScreenStackResult::RootProtected;
		}
		Candidate.RemoveAt(ActiveIndex);
		return EHSRScreenStackResult::Success;
	}

	if (Request.Operation == EHSRScreenStackOperation::Replace)
	{
		if (ActiveIndex == 0)
		{
			return EHSRScreenStackResult::RootProtected;
		}
		if (Request.Layer == EHSRUIScreenLayer::HUD)
		{
			return EHSRScreenStackResult::RootAlreadyExists;
		}
		Candidate[ActiveIndex] = {Request.ScreenId, Request.Layer, Request.InputIntent, Request.FocusToken, Request.RequestToken};
		return EHSRScreenStackResult::Success;
	}

	if (Request.Operation == EHSRScreenStackOperation::CloseToRoot)
	{
		if (Candidate.Num() == 1)
		{
			return EHSRScreenStackResult::NoOp;
		}
		const FHSRScreenStackEntry Root = Candidate[0];
		Candidate.Reset();
		Candidate.Add(Root);
		return EHSRScreenStackResult::Success;
	}

	return EHSRScreenStackResult::InvalidRequest;
}
