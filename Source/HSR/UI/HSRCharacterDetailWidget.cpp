#include "HSRCharacterDetailWidget.h"

#include "HSRCharacterDetailViewModel.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Save/HSRSaveSubsystem.h"
#include "Engine/GameInstance.h"

// NativeConstruct：控件入树时自建 ViewModel，注入四个子系统，订阅快照变化，
// 并默认选中队伍 0 号槽位。初始化失败（缺子系统或队伍空）时通知蓝图走"不可用"分支。
void UHSRCharacterDetailWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshCount = 0;
	bHasCurrentSnapshot = false;
	ViewModel = NewObject<UHSRCharacterDetailViewModel>(this);
	UGameInstance* GameInstance = GetGameInstance();
	UHSRCharacterProfileSubsystem* Profiles = GameInstance ? GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
	UHSRSaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UHSRSaveSubsystem>() : nullptr;
	UHSRPartySubsystem* Party = GameInstance ? GameInstance->GetSubsystem<UHSRPartySubsystem>() : nullptr;
	UHSREquipmentSubsystem* Equipment = GameInstance ? GameInstance->GetSubsystem<UHSREquipmentSubsystem>() : nullptr;
	if (!ViewModel || !Profiles || !Save || !Party)
	{
		UE_LOG(LogTemp, Error, TEXT("P11-005 DetailWidgetInit Result=FAIL Reason=MissingViewModelOrSubsystems"));
		return;
	}

	ViewModel->Initialize(Profiles, Save, Party, Equipment);
	DetailChangedHandle = ViewModel->OnChanged().AddUObject(this, &UHSRCharacterDetailWidget::HandleDetailChanged);
	const EHSRCharacterDetailResult Result = ViewModel->SelectPartySlot0();
	if (Result == EHSRCharacterDetailResult::Success)
	{
		UE_LOG(LogTemp, Log, TEXT("P11-005 DetailWidgetInit Result=SUCCESS SelectionResult=%d"), static_cast<int32>(Result));
	}
	else
	{
		// 首次默认选中失败（例如队伍为空）：通知蓝图进入"详情不可用"状态。
		UE_LOG(LogTemp, Warning, TEXT("P11-005 DetailWidgetInit Result=FAIL SelectionResult=%d"), static_cast<int32>(Result));
		OnDetailUnavailable(Result);
	}
}

// NativeDestruct：控件出树时解绑订阅并反初始化 ViewModel。
void UHSRCharacterDetailWidget::NativeDestruct()
{
	if (ViewModel)
	{
		if (DetailChangedHandle.IsValid())
		{
			ViewModel->OnChanged().Remove(DetailChangedHandle);
			DetailChangedHandle.Reset();
		}
		ViewModel->Uninitialize();
		ViewModel = nullptr;
	}
	bHasCurrentSnapshot = false;
	Super::NativeDestruct();
}

// GetCurrentSnapshot：输出控件缓存的最近一次详情快照；尚无快照时返回 false。
bool UHSRCharacterDetailWidget::GetCurrentSnapshot(FHSRCharacterDetailSnapshot& OutSnapshot) const
{
	if (!bHasCurrentSnapshot)
	{
		return false;
	}
	OutSnapshot = CurrentSnapshot;
	return true;
}

// HandleDetailChanged：ViewModel 广播新快照时的回调——缓存快照、统计刷新次数并推送蓝图。
// RefreshCount 是开发期计数，用于日志确认快照确实在推进（而非停留在初始状态）。
void UHSRCharacterDetailWidget::HandleDetailChanged(const FHSRCharacterDetailSnapshot& Snapshot)
{
	CurrentSnapshot = Snapshot;
	bHasCurrentSnapshot = true;
	++RefreshCount;
	UE_LOG(LogTemp, Log, TEXT("P11-005 DetailRefresh Count=%d Character=%s Revision=%lld Level=%d Experience=%d MaxLevel=%d Valid=%d"), RefreshCount, *Snapshot.CharacterId.ToString(), Snapshot.RuntimeRevision, Snapshot.Level, Snapshot.Experience, Snapshot.MaxLevel, Snapshot.bIsValid ? 1 : 0);
	OnDetailSnapshotChanged(Snapshot);
}
