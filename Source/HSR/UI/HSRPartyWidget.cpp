#include "HSRPartyWidget.h"

#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "Engine/GameInstance.h"

// SetViewModel：外部注入已有 ViewModel（外部拥有其生命周期）。
// 先解绑旧订阅并 Shutdown 自建 VM，再接管新 VM 并立即绑定 + 全量刷新。
void UHSRPartyWidget::SetViewModel(UHSRPartyViewModel* InViewModel)
{
	Unbind();
	if (bOwnsViewModel && ViewModel)
	{
		ViewModel->Shutdown();
	}
	ViewModel = InViewModel;
	// 外部注入的 VM 不由本控件负责 Shutdown。
	bOwnsViewModel = false;
	BindAndRefresh();
}

// GetCurrentSnapshot：输出控件缓存的最近一次前端快照；尚无快照时返回 false。
bool UHSRPartyWidget::GetCurrentSnapshot(FHSRPartyFrontendSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot)
	{
		return false;
	}
	OutSnapshot = Current;
	return true;
}

// GetSlotAt：按下标读取某个槽位的视图数据；越界时返回 false。
bool UHSRPartyWidget::GetSlotAt(int32 SlotIndex, FHSRPartySlotViewData& OutSlot) const
{
	if (!Current.Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}
	OutSlot = Current.Slots[SlotIndex];
	return true;
}

// IsSlotOccupied：槽位是否已被角色占用（供蓝图快速判断）。
bool UHSRPartyWidget::IsSlotOccupied(int32 SlotIndex) const
{
	FHSRPartySlotViewData ViewSlot;
	return GetSlotAt(SlotIndex, ViewSlot) && ViewSlot.bOccupied;
}

// GetSlotCharacterId：读取槽位内角色的 ID；空槽位/越界返回 NAME_None。
FName UHSRPartyWidget::GetSlotCharacterId(int32 SlotIndex) const
{
	FHSRPartySlotViewData ViewSlot;
	return GetSlotAt(SlotIndex, ViewSlot) ? ViewSlot.CharacterId : NAME_None;
}

// 以下操作函数都是薄转发：把用户操作转给 ViewModel，返回值原样传递。
EHSRPartyResult UHSRPartyWidget::SetCandidateSlot(int32 SlotIndex, FName CharacterId)
{
	return ViewModel ? ViewModel->SetCandidateSlot(SlotIndex, CharacterId) : EHSRPartyResult::InvalidCandidate;
}

EHSRPartyResult UHSRPartyWidget::ClearCandidateSlot(int32 SlotIndex)
{
	return ViewModel ? ViewModel->ClearCandidateSlot(SlotIndex) : EHSRPartyResult::InvalidCandidate;
}

EHSRPartyResult UHSRPartyWidget::SwapCandidateSlots(int32 FirstSlot, int32 SecondSlot)
{
	return ViewModel ? ViewModel->SwapCandidateSlots(FirstSlot, SecondSlot) : EHSRPartyResult::InvalidCandidate;
}

EHSRPartyResult UHSRPartyWidget::ConfirmCandidate()
{
	return ViewModel ? ViewModel->ConfirmCandidate() : EHSRPartyResult::InvalidCandidate;
}

EHSRPartyResult UHSRPartyWidget::CancelCandidate()
{
	return ViewModel ? ViewModel->CancelCandidate() : EHSRPartyResult::InvalidCandidate;
}

// NativeConstruct：控件入树时若无外部注入的 VM，则自建一个并绑定到两个子系统。
void UHSRPartyWidget::NativeConstruct()
{
	if (!ViewModel)
	{
		UGameInstance* GameInstance = GetGameInstance();
		UHSRPartySubsystem* Party = GameInstance ? GameInstance->GetSubsystem<UHSRPartySubsystem>() : nullptr;
		UHSRCharacterProfileSubsystem* Profiles = GameInstance ? GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
		ViewModel = NewObject<UHSRPartyViewModel>(this);
		ViewModel->Initialize(Party, Profiles);
		// 自建 VM 标记 bOwnsViewModel，析构时由本控件负责 Shutdown。
		bOwnsViewModel = true;
	}
	BindAndRefresh();
	// Initialize before Super so the Blueprint Construct event can read a valid snapshot.
	// 必须在调用 Super（触发蓝图 Construct 事件）之前先完成数据初始化，
	// 这样蓝图端在 Construct 事件里就能读到有效的队伍快照。
	Super::NativeConstruct();
}

// NativeDestruct：控件出树时解绑订阅并清理自建 VM。
void UHSRPartyWidget::NativeDestruct()
{
	Unbind();
	if (bOwnsViewModel && ViewModel)
	{
		ViewModel->Shutdown();
		ViewModel = nullptr;
		bOwnsViewModel = false;
	}
	Super::NativeDestruct();
}

// BindAndRefresh：订阅 ViewModel 的 Changed 事件，并立即拉取一次快照完成初次显示。
void UHSRPartyWidget::BindAndRefresh()
{
	if (!ViewModel || Subscription.IsValid())
	{
		return;
	}
	Subscription = ViewModel->OnChanged().AddUObject(this, &ThisClass::HandleSnapshot);
#if WITH_DEV_AUTOMATION_TESTS
	++BindCount;
#endif
	FHSRPartyFrontendSnapshot Snapshot;
	if (ViewModel->GetSnapshot(Snapshot))
	{
		HandleSnapshot(Snapshot);
	}
}

// Unbind：解除订阅并复位缓存快照，保证控件可被安全复用。
void UHSRPartyWidget::Unbind()
{
	if (ViewModel && Subscription.IsValid())
	{
		ViewModel->OnChanged().Remove(Subscription);
#if WITH_DEV_AUTOMATION_TESTS
		++UnbindCount;
#endif
	}
	Subscription.Reset();
	bHasSnapshot = false;
	Current = FHSRPartyFrontendSnapshot();
}

// HandleSnapshot：ViewModel 广播新快照时的回调——缓存快照并推送给蓝图事件。
void UHSRPartyWidget::HandleSnapshot(const FHSRPartyFrontendSnapshot& InSnapshot)
{
	Current = InSnapshot;
	bHasSnapshot = true;
	OnPartySnapshotChanged(Current);
}
