#include "HSRChallengeDirectoryWidget.h"

#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "../Map/HSRMapSubsystem.h"

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
		RefreshPresentation();
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
	RefreshPresentation();
	return Result;
}

void UHSRChallengeDirectoryWidget::RefreshPresentation()
{
	if (!WidgetTree || !ViewModel) return;
	const auto SetText=[](UUserWidget* Owner,FName Name,const FText& Value)
	{
		if (auto* Text=Cast<UTextBlock>(Owner->GetWidgetFromName(Name))) Text->SetText(Value);
	};
	SetText(this,TEXT("TXT_Title"),NSLOCTEXT("HSRChallenge","Title","区域挑战"));
	SetText(this,TEXT("TXT_Enter"),NSLOCTEXT("HSRChallenge","Prepare","进入战斗准备"));
	auto* List=WidgetTree->FindWidget<UPanelWidget>(TEXT("EntryListContent"));
	if (!List) return;
	const auto& Entries=ViewModel->GetSnapshot().Entries;
	for(int32 I=0;I<FMath::Min(List->GetChildrenCount(),Entries.Num());++I)
	{
		auto* Card=Cast<UUserWidget>(List->GetChildAt(I));if(!Card) continue;
		const auto& Entry=Entries[I];
		SetText(Card,TEXT("TXT_EncounterId"),Entry.DisplayName);
		SetText(Card,TEXT("TXT_EnemyInfo"),Entry.Description);
		FText MapTitle=NSLOCTEXT("HSRChallenge","Arena","战斗区域");
		if(auto* Maps=GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRMapSubsystem>() : nullptr)
		{
			FName MapId;if(Maps->ResolveMapIdByPackage(Entry.BattleMapPath,MapId)) MapTitle=Maps->GetMapDisplayName(MapId);
		}
		SetText(Card,TEXT("TXT_Map"),MapTitle);
		SetText(Card,TEXT("TXT_Diagnostic"),Entry.Diagnostic);
		if (UWidget* Diagnostic = Card->GetWidgetFromName(TEXT("TXT_Diagnostic")))
		{
			Diagnostic->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UPanelWidget* Cell = Diagnostic->GetParent()) Cell->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		SetText(Card,TEXT("TXT_Select"),Entry.bAvailable ? NSLOCTEXT("HSRChallenge","Select","选择") : Entry.bCompleted ? NSLOCTEXT("HSRChallenge","Done","已完成") : NSLOCTEXT("HSRChallenge","NotReady","未解锁"));
	}
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
