#include "HSRChallengeDirectoryWidget.h"

#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "Engine/GameInstance.h"

// UnbindProgression：解除对挑战进度子系统的监听。
// 与 Bind 对称，用于重新初始化或控件销毁前清理，防止进度变化时回调已销毁的控件。
void UHSRChallengeDirectoryWidget::UnbindProgression()
{
	if (BoundProgression.IsValid() && ProgressionChangedHandle.IsValid())
	{
		BoundProgression->OnProgressionChanged().Remove(ProgressionChangedHandle);
	}
	BoundProgression.Reset();
	ProgressionChangedHandle.Reset();
}

// NativeDestruct：控件出树时先解绑进度监听，再执行默认析构逻辑。
void UHSRChallengeDirectoryWidget::NativeDestruct()
{
	UnbindProgression();
	Super::NativeDestruct();
}

// HandleProgressionChanged：进度子系统广播变化时的回调（进度影响挑战的锁定/完成状态）。
// 这里触发 ViewModel 重建快照，并立即把最新目录推给蓝图事件。
void UHSRChallengeDirectoryWidget::HandleProgressionChanged(const FHSRChallengeProgressionSnapshot&)
{
	if (ViewModel)
	{
		ViewModel->Refresh();
		OnDirectoryChanged(ViewModel->GetSnapshot());
	}
}

// InitializeDirectory：用给定的来源数组初始化目录。
// 若无自建 ViewModel 则先创建；随后把来源与进度子系统注入 VM，并订阅进度变化；
// 最后清空选中项并推送一次初始目录快照。
EHSRChallengeDirectoryResult UHSRChallengeDirectoryWidget::InitializeDirectory(
	const TArray<FHSRChallengeDirectorySource>& Sources)
{
	UnbindProgression();
	if (!ViewModel)
	{
		ViewModel = NewObject<UHSRChallengeDirectoryViewModel>(this);
	}
	// 进度子系统是游戏实例级子系统；取不到时 ViewModel 将把所有挑战视为"锁定/不可用"。
	UHSRChallengeProgressionSubsystem* Progression = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UHSRChallengeProgressionSubsystem>()
		: nullptr;
	const EHSRChallengeDirectoryResult Result = ViewModel->Initialize(Sources, Progression);
	if (Progression)
	{
		BoundProgression = Progression;
		ProgressionChangedHandle = Progression->OnProgressionChanged().AddUObject(
			this, &UHSRChallengeDirectoryWidget::HandleProgressionChanged);
	}
	// 重置选中项，避免重新初始化后仍选中旧目录中已不存在的挑战。
	SelectedEncounterId = NAME_None;
	OnDirectoryChanged(ViewModel->GetSnapshot());
	return Result;
}

// InitializeConfiguredDirectory：用蓝图配置好的 ChallengeSources 初始化，并输出诊断日志。
EHSRChallengeDirectoryResult UHSRChallengeDirectoryWidget::InitializeConfiguredDirectory()
{
	const EHSRChallengeDirectoryResult Result = InitializeDirectory(ChallengeSources);
	UE_LOG(LogTemp, Log, TEXT("HSR ChallengeDirectory initialized Sources=%d Entries=%d Result=%d"),
		ChallengeSources.Num(), ViewModel ? ViewModel->GetSnapshot().Entries.Num() : 0, static_cast<int32>(Result));
	return Result;
}

// GetDirectorySnapshot：向外部（蓝图）暴露当前目录快照；无 VM 时返回空快照。
FHSRChallengeDirectorySnapshot UHSRChallengeDirectoryWidget::GetDirectorySnapshot() const
{
	return ViewModel ? ViewModel->GetSnapshot() : FHSRChallengeDirectorySnapshot();
}

// SelectChallenge：选中一个挑战（仅记录选中 ID，不校验；校验发生在真正构建模板时）。
EHSRChallengeDirectoryResult UHSRChallengeDirectoryWidget::SelectChallenge(const FName EncounterId)
{
	if (!ViewModel)
	{
		return EHSRChallengeDirectoryResult::EmptyDirectory;
	}

	UHSREncounterDefinition* Definition = nullptr;
	const EHSRChallengeDirectoryResult Result = ViewModel->ResolveSelection(EncounterId, Definition);
	if (Result == EHSRChallengeDirectoryResult::Success)
	{
		SelectedEncounterId = EncounterId;
	}
	return Result;
}

// BuildChallengeTemplate：为选中挑战构建遭遇请求模板。
// 先经 ViewModel 校验（锁定/未完成/不可用会被拒绝），再委托战斗切换子系统生成模板，
// 生成成功后才更新 SelectedEncounterId。
FHSREncounterResult UHSRChallengeDirectoryWidget::BuildChallengeTemplate(
	const FName EncounterId, const EHSREncounterInitiative Initiative, FHSREncounterRequest& OutTemplate)
{
	UHSREncounterDefinition* Definition = nullptr;
	const EHSRChallengeDirectoryResult SelectionResult = ViewModel
		? ViewModel->ResolveSelection(EncounterId, Definition)
		: EHSRChallengeDirectoryResult::EmptyDirectory;
	if (SelectionResult != EHSRChallengeDirectoryResult::Success)
	{
		// 校验未通过：按"锁定/不可用"给出对应的失败信息。
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidDefinition,
			FText::FromString(SelectionResult == EHSRChallengeDirectoryResult::Locked
				? TEXT("Challenge is locked.") : TEXT("Challenge is unavailable.")));
	}
	UHSRBattleTransitionSubsystem* Transition = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>()
		: nullptr;
	if (!Transition)
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidRequest,
			FText::FromString(TEXT("Battle transition is unavailable.")));
	}

	FHSREncounterResult Result = Transition->BuildPreBattleEncounterTemplate(Definition, Initiative, OutTemplate);
	if (Result.ResultType == EHSREncounterResultType::Success)
	{
		SelectedEncounterId = EncounterId;
	}
	return Result;
}
