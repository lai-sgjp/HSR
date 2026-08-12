#include "HSRPreBattleCandidateWidget.h"

#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

// InitializeCandidate：用遭遇模板初始化战前编队界面。
// 绑定队伍子系统（队伍变化会影响候选默认值），并把模板交给 ViewModel 建起草稿。
void UHSRPreBattleCandidateWidget::InitializeCandidate(const FHSREncounterRequest& Template)
{
	UGameInstance* GameInstance = GetGameInstance();
	UHSRPartySubsystem* Party = GameInstance ? GameInstance->GetSubsystem<UHSRPartySubsystem>() : nullptr;
	UHSRCharacterProfileSubsystem* Profiles = GameInstance ? GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
	if (Party && PartyChangedHandle.IsValid())
	{
		Party->OnPartyChanged().Remove(PartyChangedHandle);
		PartyChangedHandle.Reset();
	}
	if (!ViewModel)
	{
		ViewModel = NewObject<UHSRPreBattleCandidateViewModel>(this);
	}
	ViewModel->Initialize(Party, Profiles, Template);
	if (Party)
	{
		PartyChangedHandle = Party->OnPartyChanged().AddUObject(this, &ThisClass::HandlePartyChanged);
	}
	RefreshSnapshot();
}

// SetCandidateSlot：把某角色放入指定槽位，随后刷新显示。
EHSRPreBattleCandidateResult UHSRPreBattleCandidateWidget::SetCandidateSlot(int32 SlotIndex, FName CharacterId)
{
	const EHSRPreBattleCandidateResult Result = ViewModel ? ViewModel->SetCandidateSlot(SlotIndex, CharacterId) : EHSRPreBattleCandidateResult::InvalidCandidate;
	RefreshSnapshot();
	return Result;
}

// SetBuff：添加出战 Buff，随后刷新显示。
EHSRPreBattleCandidateResult UHSRPreBattleCandidateWidget::SetBuff(FName BuffId)
{
	const EHSRPreBattleCandidateResult Result = ViewModel ? ViewModel->SetBuff(BuffId) : EHSRPreBattleCandidateResult::InvalidCandidate;
	RefreshSnapshot();
	return Result;
}

// ConfirmCandidate：校验当前候选并输出遭遇请求（不改 UI，由调用方决定下一步）。
EHSRPreBattleCandidateResult UHSRPreBattleCandidateWidget::ConfirmCandidate(FHSREncounterRequest& OutRequest)
{
	return ViewModel ? ViewModel->ConfirmCandidate(OutRequest) : EHSRPreBattleCandidateResult::InvalidCandidate;
}

// ConfirmAndSubmitEncounter：校验候选并直接把遭遇请求提交给战斗切换子系统。
// 这是"战前编队 -> 进入战斗"的 UI 入口：先本地校验，再交给子系统发起传送。
FHSREncounterResult UHSRPreBattleCandidateWidget::ConfirmAndSubmitEncounter(FHSREncounterRequest& OutRequest)
{
	if (!ViewModel)
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidRequest,
			FText::FromString(TEXT("Pre-battle candidate is not initialized.")));
	}

	if (ConfirmCandidate(OutRequest) != EHSRPreBattleCandidateResult::Success)
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidRequest,
			FText::FromString(TEXT("Pre-battle candidate validation failed.")));
	}

	UHSRBattleTransitionSubsystem* Transition = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>()
		: nullptr;
	return Transition
		? Transition->SubmitEncounterRequestFromUI(OutRequest)
		: FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidRequest,
			FText::FromString(TEXT("Battle transition is unavailable.")));
}

// CancelCandidate：放弃编辑并重置为队伍权威数据，随后刷新显示。
EHSRPreBattleCandidateResult UHSRPreBattleCandidateWidget::CancelCandidate()
{
	const EHSRPreBattleCandidateResult Result = ViewModel ? ViewModel->CancelCandidate() : EHSRPreBattleCandidateResult::InvalidCandidate;
	RefreshSnapshot();
	return Result;
}

// GetCandidateSnapshot：暴露当前候选快照给蓝图读取。
FHSRPreBattleCandidateSnapshot UHSRPreBattleCandidateWidget::GetCandidateSnapshot() const
{
	return ViewModel ? ViewModel->GetSnapshot() : FHSRPreBattleCandidateSnapshot();
}

// HandlePartyChanged：队伍变化时刷新显示（例如其它界面改了队伍，候选面板需同步）。
void UHSRPreBattleCandidateWidget::HandlePartyChanged(int64)
{
	RefreshSnapshot();
}

// RefreshSnapshot：拉取最新快照，更新槽位文本并推送蓝图事件。
void UHSRPreBattleCandidateWidget::RefreshSnapshot()
{
	if (ViewModel)
	{
		const FHSRPreBattleCandidateSnapshot Snapshot = ViewModel->GetSnapshot();
		UpdateSlotTextBlocks(Snapshot);
		OnCandidateSnapshotChanged(Snapshot);
	}
}

// UpdateSlotTextBlocks：把候选角色 ID 写入四个命名的文本控件。
// The panel renders up to four candidate slots.  Resolving by name keeps the C++ side
// independent of how many slot widgets the Blueprint actually places, and lets an authored
// panel show all committed members without hardcoding slot indices in the graph.
// 面板最多渲染四个候选槽位。按名字解析文本控件使 C++ 侧与蓝图实际摆放的槽位数量解耦，
// 也避免在图里硬编码槽位下标。空槽位显示 "Empty"。
void UHSRPreBattleCandidateWidget::UpdateSlotTextBlocks(const FHSRPreBattleCandidateSnapshot& Snapshot)
{
	if (!WidgetTree)
	{
		return;
	}
	const FName SlotNames[] = { TEXT("Text_Slot0_Character"), TEXT("Text_Slot1_Character"),
		TEXT("Text_Slot2_Character"), TEXT("Text_Slot3_Character") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SlotNames); ++Index)
	{
		UTextBlock* TextBlock = WidgetTree->FindWidget<UTextBlock>(SlotNames[Index]);
		if (!TextBlock)
		{
			continue;
		}
		if (Snapshot.CandidateCharacterIds.IsValidIndex(Index) && !Snapshot.CandidateCharacterIds[Index].IsNone())
		{
			TextBlock->SetText(FText::FromName(Snapshot.CandidateCharacterIds[Index]));
		}
		else
		{
			TextBlock->SetText(FText::FromString(TEXT("Empty")));
		}
	}
}

// NativeDestruct：控件出树时解除队伍监听并清空 ViewModel 引用。
void UHSRPreBattleCandidateWidget::NativeDestruct()
{
	if (ViewModel)
	{
		if (UHSRPartySubsystem* Party = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRPartySubsystem>() : nullptr)
		{
			Party->OnPartyChanged().Remove(PartyChangedHandle);
		}
	}
	PartyChangedHandle.Reset();
	ViewModel = nullptr;
	Super::NativeDestruct();
}
