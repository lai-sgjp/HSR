#include "HSRSaveViewModel.h"

#include "../Save/HSRSaveSubsystem.h"

// BeginDestroy：ViewModel 被销毁前必须解绑对存档子系统的监听，
// 否则异步加载完成回调可能指向已销毁对象。Shutdown 与 Super 的顺序不能颠倒。
void UHSRSaveViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

// Initialize：把本 ViewModel 绑定到指定的存档子系统，并用子系统当前状态立刻初始化
// "最后一次操作结果"的前端展示数据。
//
// 数据来源：UHSRSaveSubsystem（游戏实例级存档子系统）。
// 转换方式：把子系统的 EHSRSaveResult / 写盘头部信息整理成 UI 可显示的
//           FHSRSaveFrontendResult（纯值 DTO，不含 UObject 引用）。
// 广播时机：本函数不广播；后续异步加载完成（HandleLoadCompleted）或显式操作
//           （RefreshResult）时才广播 Changed 通知 UI 刷新。
void UHSRSaveViewModel::Initialize(UHSRSaveSubsystem* InSave)
{
	// 先解绑旧监听，保证本函数可重复调用而不产生悬挂回调。
	Shutdown();
	Save = InSave;
	if (InSave)
	{
		// 订阅"异步加载完成"事件，使 ViewModel 能响应运行时的加载结果变化。
		LoadCompletedHandle = InSave->OnLoadCompleted().AddUObject(this, &UHSRSaveViewModel::HandleLoadCompleted);
		// 用子系统已记录的最后一次加载结果填充本地原始结果缓存。
		LastResult = InSave->GetLastLoadResult();
		// 以下把原始结果映射为前端展示字段：槽名沿用当前激活槽位，
		// 结果/代次/恢复标记等直接从 LastResult 拷贝。
		FrontendResult.SlotName = ActiveSlotName;
		FrontendResult.Result = LastResult.Result;
		FrontendResult.Generation = static_cast<int64>(LastResult.Generation);
		FrontendResult.bRecoveredFromBackup = LastResult.bRecoveredFromBackup;
		FrontendResult.bRuntimeChanged = LastResult.bRuntimeChanged;
		// 若系统内还有"待恢复"操作（例如备份恢复），需要让 UI 知道当前处于恢复过渡态。
		FrontendResult.bPending = InSave->HasPendingRestore();
		bHasResult = true;
	}
}

// Shutdown：解除与存档子系统的一切绑定并复位本地状态。
// 与 Initialize 成对使用，用于生命周期结束或重新初始化之前，避免脏状态残留。
void UHSRSaveViewModel::Shutdown()
{
	if (Save.IsValid() && LoadCompletedHandle.IsValid())
	{
		Save->OnLoadCompleted().Remove(LoadCompletedHandle);
	}
	LoadCompletedHandle.Reset();
	Save.Reset();
	bHasResult = false;
	PendingOverwriteSlot.Reset();
	ActiveSlotName.Reset();
	LastResult = FHSRSaveLoadResult();
	FrontendResult = FHSRSaveFrontendResult();
	bLastOperationWasSave = false;
}

// GetLastResult：输出原始加载结果（含完整字段）。尚无任何结果时返回 false。
bool UHSRSaveViewModel::GetLastResult(FHSRSaveLoadResult& OutResult) const
{
	if (!bHasResult)
	{
		return false;
	}
	OutResult = LastResult;
	return true;
}

// GetFrontendResult：输出面向 UI 的前端结果（槽名、代次、恢复标记等展示字段）。
bool UHSRSaveViewModel::GetFrontendResult(FHSRSaveFrontendResult& OutResult) const
{
	if (!bHasResult)
	{
		return false;
	}
	OutResult = FrontendResult;
	return true;
}

// GetSlotSummary：委托给存档子系统查询指定槽位的摘要（主档/备份是否在场）。
// 未初始化（Save 为空）时直接短路返回 false，不触碰子系统。
bool UHSRSaveViewModel::GetSlotSummary(const FString& SlotName, FHSRSaveSlotSummary& OutSummary) const
{
	return Save.IsValid() && Save->GetSlotSummary(SlotName, 0, OutSummary);
}

// RequestSave：发起一次存档请求。
// 若目标槽位已有存档（主档或备份）则进入"待确认覆盖"状态（ConfirmationRequired），
// 由 UI 决定后续调用 ConfirmOverwrite / CancelOverwrite；空槽位则直接写盘。
EHSRSaveFrontendActionResult UHSRSaveViewModel::RequestSave(const FString& SlotName)
{
	// 前置校验：子系统未初始化、槽名为空、有进行中的恢复、或已有未决覆盖请求时均拒绝。
	if (!Save.IsValid() || SlotName.IsEmpty() || Save->HasPendingRestore() || !PendingOverwriteSlot.IsEmpty())
	{
		return EHSRSaveFrontendActionResult::InvalidArgument;
	}

	// 先查槽位摘要，确认该槽位当前是否存在存档内容。
	FHSRSaveSlotSummary Summary;
	if (!Save->GetSlotSummary(SlotName, 0, Summary))
	{
		RefreshResult(EHSRSaveResult::InvalidArgument, SlotName);
		return EHSRSaveFrontendActionResult::InvalidArgument;
	}

	ActiveSlotName = SlotName;
	if (Summary.bPrimaryPresent || Summary.bBackupPresent)
	{
		// 槽位已有内容：不立即写盘，先把槽名记入待确认状态，等用户明确确认。
		PendingOverwriteSlot = SlotName;
		return EHSRSaveFrontendActionResult::ConfirmationRequired;
	}

	// 空槽位：直接写盘，并把"本次是保存操作"标记置位（影响前端 Generation 的取值来源）。
	bLastOperationWasSave = true;
	RefreshResult(Save->SaveToSlot(SlotName), SlotName);
	return EHSRSaveFrontendActionResult::Success;
}

// ConfirmOverwrite：用户确认覆盖已有存档后真正执行写盘。
// MoveTemp 取出待覆盖槽名并同时清空待确认状态，防止同一请求被重复确认。
EHSRSaveFrontendActionResult UHSRSaveViewModel::ConfirmOverwrite()
{
	if (!Save.IsValid() || PendingOverwriteSlot.IsEmpty() || Save->HasPendingRestore())
	{
		return EHSRSaveFrontendActionResult::InvalidArgument;
	}

	const FString Slot = MoveTemp(PendingOverwriteSlot);
	ActiveSlotName = Slot;
	bLastOperationWasSave = true;
	RefreshResult(Save->SaveToSlot(Slot), Slot);
	return EHSRSaveFrontendActionResult::Success;
}

// CancelOverwrite：放弃覆盖操作，仅清空待确认槽位，不改动任何存档数据。
void UHSRSaveViewModel::CancelOverwrite()
{
	PendingOverwriteSlot.Reset();
}

// GetPendingOverwriteSlot：读取当前待确认覆盖的槽位名；无待确认项时返回 false。
bool UHSRSaveViewModel::GetPendingOverwriteSlot(FString& OutSlotName) const
{
	if (PendingOverwriteSlot.IsEmpty())
	{
		return false;
	}
	OutSlotName = PendingOverwriteSlot;
	return true;
}

// RequestLoad：发起加载请求，并把结果同步整理进前端状态。
// 加载是"非保存"操作，故 bLastOperationWasSave 置 false（前端 Generation 将取自 LastResult）。
EHSRSaveResult UHSRSaveViewModel::RequestLoad(const FString& SlotName)
{
	if (!Save.IsValid() || SlotName.IsEmpty() || Save->HasPendingRestore() || !PendingOverwriteSlot.IsEmpty())
	{
		return EHSRSaveResult::InvalidArgument;
	}

	ActiveSlotName = SlotName;
	bLastOperationWasSave = false;
	const EHSRSaveResult Result = Save->LoadFromSlot(SlotName);
	RefreshResult(Result, SlotName);
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 SaveFrontend Load Slot=%s Result=%d Generation=%llu RuntimeChanged=%s Pending=%s"),
		*SlotName, static_cast<int32>(Result), LastResult.Generation,
		LastResult.bRuntimeChanged ? TEXT("true") : TEXT("false"),
		FrontendResult.bPending ? TEXT("true") : TEXT("false"));
	return Result;
}

// HandleLoadCompleted：存档子系统异步加载完成时的回调。
// 这是本 ViewModel 对外广播 Changed 的主要入口——UI 依赖该事件刷新读取结果面板。
// 与 RequestLoad 的同步路径不同，这里的数据来自异步回调，因此额外执行 Changed.Broadcast。
void UHSRSaveViewModel::HandleLoadCompleted(const FHSRSaveLoadResult& Result)
{
	LastResult = Result;
	bLastOperationWasSave = false;
	FrontendResult.SlotName = ActiveSlotName;
	FrontendResult.Result = Result.Result;
	FrontendResult.Generation = static_cast<int64>(Result.Generation);
	FrontendResult.bRecoveredFromBackup = Result.bRecoveredFromBackup;
	FrontendResult.bRuntimeChanged = Result.bRuntimeChanged;
	// 加载完成后"待恢复"标记必然清除。
	FrontendResult.bPending = false;
	bHasResult = true;
	Changed.Broadcast();
}

// RefreshResult：把某次操作（保存/加载）的原始结果整理成前端可显示结果。
// 负责统一修正 ActiveSlotName、Generation 与"待恢复"标记：
//   - 保存操作：Generation 取自最后一次写盘的头部（写盘代次才是最新代次）；
//   - 加载/恢复操作：Generation 沿用 LastResult 记录里的代次。
void UHSRSaveViewModel::RefreshResult(const EHSRSaveResult Result, const FString& SlotName)
{
	if (!SlotName.IsEmpty())
	{
		ActiveSlotName = SlotName;
	}
	if (Save.IsValid())
	{
		LastResult = Save->GetLastLoadResult();
	}
	LastResult.Result = Result;
	FrontendResult.SlotName = ActiveSlotName;
	FrontendResult.Result = Result;
	FrontendResult.Generation = bLastOperationWasSave && Save.IsValid()
		? static_cast<int64>(Save->GetLastWriteHeader().Generation)
		: static_cast<int64>(LastResult.Generation);
	FrontendResult.bRecoveredFromBackup = LastResult.bRecoveredFromBackup;
	FrontendResult.bRuntimeChanged = LastResult.bRuntimeChanged;
	FrontendResult.bPending = Save.IsValid() && Save->HasPendingRestore();
	bHasResult = true;
}
