#include "HSRFrontendRouter.h"

// 判断模块枚举是否为可寻址的合法模块（排除 None 等哨兵值）。
bool UHSRFrontendRouter::IsValidModule(const EHSRFrontendModule Module)
{
	return HSRFrontendModule::IsAddressable(Module);
}

// 校验一次路由请求的合法性：请求令牌必须有效；打开模块时模块必须可寻址；
// 其它操作（返回/关闭）要求模块为 None 且没有 PageId 残留，否则请求自相矛盾。
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

// 提交一次路由请求并更新路由快照。
// 令牌单调递增：过期令牌（小于已处理的）返回 StaleRequest，重复令牌返回 AlreadyProcessed。
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
		// 再次打开当前栈顶模块：视为无操作，但仍推进令牌。
		if (!Candidate.History.IsEmpty() && Candidate.History.Last() == Request.Route)
		{
			Candidate.LastProcessedRequestToken = Request.RequestToken;
			Snapshot = MoveTemp(Candidate);
			return EHSRFrontendRouteResult::NoOp;
		}
		// 空栈：先把 PauseHub 作为根压入（前端 hub 永不从栈里消失），再追加目标模块。
		if (Candidate.History.IsEmpty())
		{
			Candidate.History.Add({EHSRFrontendModule::PauseHub, NAME_None});
			if (Request.Route.Module != EHSRFrontendModule::PauseHub)
			{
				Candidate.History.Add(Request.Route);
			}
			break;
		}
		// 打开 PauseHub 相当于回到根：栈只保留 PauseHub 一项。
		if (Request.Route.Module == EHSRFrontendModule::PauseHub)
		{
			Candidate.History.SetNum(1);
		}
		else if (Candidate.History.Num() == 1)
		{
			// 只有根时：直接追加新模块。
			Candidate.History.Add(Request.Route);
		}
		else
		{
			// 已有二级模块：用新模块替换栈顶，保持“根 + 当前模块”两层结构。
			Candidate.History.Last() = Request.Route;
			Candidate.History.SetNum(2);
		}
		break;
	case EHSRFrontendRouteOperation::Back:
		// 返回：弹出栈顶模块；根模块不能弹。
		if (Candidate.History.IsEmpty())
		{
			return EHSRFrontendRouteResult::NothingOpen;
		}
		Candidate.History.Pop();
		break;
	case EHSRFrontendRouteOperation::CloseToRoot:
		// 关闭到根：清空历史（前端会显示根界面）。
		if (Candidate.History.IsEmpty())
		{
			return EHSRFrontendRouteResult::NothingOpen;
		}
		Candidate.History.Reset();
		break;
	default:
		return EHSRFrontendRouteResult::InvalidRequest;
	}

	Candidate.LastProcessedRequestToken = Request.RequestToken;
	Snapshot = MoveTemp(Candidate);
	return EHSRFrontendRouteResult::Success;
}

// 重置路由状态（清空快照）。
void UHSRFrontendRouter::Reset()
{
	Snapshot = {};
}
