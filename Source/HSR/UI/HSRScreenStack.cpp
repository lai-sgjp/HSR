#include "HSRScreenStack.h"

namespace
{
	// 匿名命名空间的辅助函数：仅在本编译单元内可见。
	// 作用是把 UI 层级枚举转换为整型数值，作为“层级优先级”的比较基准。
	// 数值越大表示层级越高（例如 Modal 高于 Menu 高于 HUD），
	// 屏幕栈依赖这个顺序决定当前哪个界面处于激活态。
	int32 LayerPriority(const EHSRUIScreenLayer Layer)
	{
		return static_cast<int32>(Layer);
	}
}

// SubmitRequest 是屏幕栈的唯一“写入口”。外部（UI 管理器）把一次屏幕操作
// （Push/Replace/Pop/CloseToRoot）封装成 FHSRScreenRequest 提交进来。
// 本方法负责：1) 先做令牌校验，拒绝乱序/重复/过期请求；2) 对一份候选副本执行操作；
// 3) 只有成功（或 NoOp）才把候选副本写回正式快照，保证失败不污染当前状态。
// 这套“先校验、后应用、再提交”的设计让屏幕栈的状态变更可预测、可重放。
EHSRScreenStackResult UHSRScreenStack::SubmitRequest(const FHSRScreenRequest& Request)
{
	// 请求令牌必须为正数，0/负数视为非法请求（令牌由调用方自增生成）。
	if (Request.RequestToken <= 0)
	{
		return EHSRScreenStackResult::InvalidRequest;
	}
	// 与上一次已处理令牌相同：说明是同一请求被重复提交，直接判定为已处理，不重复执行。
	if (Request.RequestToken == Snapshot.LastProcessedRequestToken)
	{
		return EHSRScreenStackResult::AlreadyProcessed;
	}
	// 令牌比上次已处理的小：说明提交顺序错乱，属于过期请求，拒绝执行。
	if (Request.RequestToken < Snapshot.LastProcessedRequestToken)
	{
		return EHSRScreenStackResult::StaleRequest;
	}
	// 令牌通过后，再做语义层面的校验（层级/输入意图/操作是否合法）。
	const EHSRScreenStackResult ValidationResult = ValidateRequest(Request);
	if (ValidationResult != EHSRScreenStackResult::Success)
	{
		return ValidationResult;
	}

	// 在正式快照的副本上模拟操作，而不是直接改快照——这样失败时可以整体丢弃副本。
	TArray<FHSRScreenStackEntry> Candidate = Snapshot.Entries;
	const EHSRScreenStackResult Result = ApplyToCandidate(Request, Candidate);
	// 只有成功或 NoOp 才允许写入；其它结果（如 RootRequired、DuplicateScreen）直接返回失败。
	if (Result != EHSRScreenStackResult::Success && Result != EHSRScreenStackResult::NoOp)
	{
		return Result;
	}

	// 应用成功：把副本移动进正式快照，并记录本次处理的令牌供下一次去重。
	Snapshot.Entries = MoveTemp(Candidate);
	Snapshot.LastProcessedRequestToken = Request.RequestToken;
	return Result;
}

// 返回当前处于激活态的屏幕条目。激活态的定义是“层级最高的那一层”，
// 同层时取后进栈者（FindActiveEntryIndex 的 >= 逻辑保证这一点）。
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

// 静态工具：在给定条目列表中找出激活条目下标。
// 遍历所有条目，记录遇到的最高层级优先级；优先值相等时用 >= 让后出现的覆盖先出现的，
// 即同层情况下“后入栈者获胜”，这正好符合屏幕栈“新界面盖在旧界面之上”的直觉。
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

// 请求的语义校验：在动手改状态之前先确认请求本身是合法的。
// 之所以拆成独立函数，是因为令牌校验（SubmitRequest 内）只关心“顺序”，
// 而这里关心“内容”，两者职责不同，便于单独测试。
EHSRScreenStackResult UHSRScreenStack::ValidateRequest(const FHSRScreenRequest& Request)
{
	// 层级只允许 HUD / Menu / Modal 三档，其它值视为非法。
	if (Request.Layer != EHSRUIScreenLayer::HUD
		&& Request.Layer != EHSRUIScreenLayer::Menu
		&& Request.Layer != EHSRUIScreenLayer::Modal)
	{
		return EHSRScreenStackResult::InvalidRequest;
	}
	// 输入意图只允许 GameOnly / GameAndUI / UIOnly 三档。
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
		// Push/Replace 必须携带 ScreenId，否则不知道该显示哪个界面。
		return Request.ScreenId.IsNone() ? EHSRScreenStackResult::InvalidScreenId : EHSRScreenStackResult::Success;
	case EHSRScreenStackOperation::Pop:
	case EHSRScreenStackOperation::CloseToRoot:
		// Pop/CloseToRoot 是“弹出”类操作，反而要求不携带 ScreenId，携带即视为非法。
		return Request.ScreenId.IsNone() ? EHSRScreenStackResult::Success : EHSRScreenStackResult::InvalidRequest;
	default:
		return EHSRScreenStackResult::InvalidRequest;
	}
}

// 静态工具：判断条目列表中是否已存在指定 ScreenId 的条目。
// 用于 Push/Replace 的去重——屏幕栈不允许同一界面被重复压入。
bool UHSRScreenStack::ContainsScreen(const TArray<FHSRScreenStackEntry>& Entries, const FName ScreenId)
{
	return Entries.ContainsByPredicate([ScreenId](const FHSRScreenStackEntry& Entry)
	{
		return Entry.ScreenId == ScreenId;
	});
}

// 把请求应用到候选列表上，返回应用结果。所有修改都发生在 Candidate 上，
// 由 SubmitRequest 决定是否最终采纳。这保证操作失败时不会污染正式快照。
EHSRScreenStackResult UHSRScreenStack::ApplyToCandidate(const FHSRScreenRequest& Request, TArray<FHSRScreenStackEntry>& Candidate) const
{
	// Push/Replace 属于“入栈”类操作：必须先有 ScreenId，且不得与栈内已有界面重名。
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

	// Push：栈底必须是 HUD（根界面）。空栈只能推 HUD，已有 HUD 则不允许再推 HUD。
	if (Request.Operation == EHSRScreenStackOperation::Push)
	{
		// 栈为空却要推非 HUD 界面：缺少根界面，拒绝。
		if (Candidate.IsEmpty() && Request.Layer != EHSRUIScreenLayer::HUD)
		{
			return EHSRScreenStackResult::RootRequired;
		}
		// 栈非空却要推 HUD：根界面只能有一个，拒绝。
		if (!Candidate.IsEmpty() && Request.Layer == EHSRUIScreenLayer::HUD)
		{
			return EHSRScreenStackResult::RootAlreadyExists;
		}
		// 追加新条目到栈顶（使用大括号初始化逐字段构造条目）。
		Candidate.Add({Request.ScreenId, Request.Layer, Request.InputIntent, Request.FocusToken, Request.RequestToken});
		return EHSRScreenStackResult::Success;
	}

	// Pop/Replace/CloseToRoot 都作用于“当前激活条目”，先找出来。
	const int32 ActiveIndex = FindActiveEntryIndex(Candidate);
	if (ActiveIndex == INDEX_NONE)
	{
		return EHSRScreenStackResult::EmptyStack;
	}

	// Pop：弹出栈顶（激活条目）。栈底根界面受保护，不允许弹出。
	if (Request.Operation == EHSRScreenStackOperation::Pop)
	{
		if (ActiveIndex == 0)
		{
			return EHSRScreenStackResult::RootProtected;
		}
		Candidate.RemoveAt(ActiveIndex);
		return EHSRScreenStackResult::Success;
	}

	// Replace：用新界面原地替换当前激活条目。根界面同样受保护，
	// 且替换目标不允许是 HUD（HUD 只能由 Push 作为根建立）。
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

	// CloseToRoot：清掉根界面之上的所有界面，回到只剩根的“主菜单”状态。
	if (Request.Operation == EHSRScreenStackOperation::CloseToRoot)
	{
		// 栈里本来就只有一个根界面时无需任何操作，返回 NoOp 表示“无变化”。
		if (Candidate.Num() == 1)
		{
			return EHSRScreenStackResult::NoOp;
		}
		// 先保存根条目，再清空列表，最后把根加回去。
		const FHSRScreenStackEntry Root = Candidate[0];
		Candidate.Reset();
		Candidate.Add(Root);
		return EHSRScreenStackResult::Success;
	}

	// 理论上走不到这里（ValidateRequest 已拦截非法操作），兜底返回非法请求。
	return EHSRScreenStackResult::InvalidRequest;
}
