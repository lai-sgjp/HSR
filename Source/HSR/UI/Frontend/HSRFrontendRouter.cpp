#include "HSRFrontendRouter.h"

bool UHSRFrontendRouter::IsValidModule(const EHSRFrontendModule Module)
{
	return HSRFrontendModule::IsAddressable(Module);
}

EHSRFrontendRouteResult UHSRFrontendRouter::Validate(const FHSRFrontendRouteRequest& Request)
{
	if (Request.RequestToken <= 0)
	{
		return EHSRFrontendRouteResult::InvalidRequest;
	}
	if (Request.Operation == EHSRFrontendRouteOperation::OpenModule)
	{
		return IsValidModule(Request.Route.Module)
			? EHSRFrontendRouteResult::Success : EHSRFrontendRouteResult::InvalidModule;
	}
	if (Request.Route.Module != EHSRFrontendModule::None || !Request.Route.PageId.IsNone())
	{
		return EHSRFrontendRouteResult::InvalidRequest;
	}
	return EHSRFrontendRouteResult::Success;
}

EHSRFrontendRouteResult UHSRFrontendRouter::Submit(const FHSRFrontendRouteRequest& Request)
{
	const EHSRFrontendRouteResult Validation = Validate(Request);
	if (Validation == EHSRFrontendRouteResult::InvalidRequest)
	{
		return Validation;
	}
	if (Request.RequestToken < Snapshot.LastProcessedRequestToken)
	{
		return EHSRFrontendRouteResult::StaleRequest;
	}
	if (Request.RequestToken == Snapshot.LastProcessedRequestToken && Request.RequestToken != 0)
	{
		return EHSRFrontendRouteResult::AlreadyProcessed;
	}
	if (Validation != EHSRFrontendRouteResult::Success)
	{
		Snapshot.LastProcessedRequestToken = Request.RequestToken;
		return Validation;
	}

	FHSRFrontendRouteSnapshot Candidate = Snapshot;
	switch (Request.Operation)
	{
	case EHSRFrontendRouteOperation::OpenModule:
		if (!Candidate.History.IsEmpty() && Candidate.History.Last() == Request.Route)
		{
			Candidate.LastProcessedRequestToken = Request.RequestToken;
			Snapshot = MoveTemp(Candidate);
			return EHSRFrontendRouteResult::NoOp;
		}
		if (Candidate.History.IsEmpty())
		{
			Candidate.History.Add({EHSRFrontendModule::PauseHub, NAME_None});
			if (Request.Route.Module != EHSRFrontendModule::PauseHub)
			{
				Candidate.History.Add(Request.Route);
			}
			break;
		}
		if (Request.Route.Module == EHSRFrontendModule::PauseHub)
		{
			Candidate.History.SetNum(1);
		}
		else if (Candidate.History.Num() == 1)
		{
			Candidate.History.Add(Request.Route);
		}
		else
		{
			Candidate.History.Last() = Request.Route;
			Candidate.History.SetNum(2);
		}
		break;
	case EHSRFrontendRouteOperation::Back:
		if (Candidate.History.IsEmpty()) return EHSRFrontendRouteResult::NothingOpen;
		Candidate.History.Pop();
		break;
	case EHSRFrontendRouteOperation::CloseToRoot:
		if (Candidate.History.IsEmpty()) return EHSRFrontendRouteResult::NothingOpen;
		Candidate.History.Reset();
		break;
	default:
		return EHSRFrontendRouteResult::InvalidRequest;
	}

	Candidate.LastProcessedRequestToken = Request.RequestToken;
	Snapshot = MoveTemp(Candidate);
	return EHSRFrontendRouteResult::Success;
}

void UHSRFrontendRouter::Reset()
{
	Snapshot = {};
}
