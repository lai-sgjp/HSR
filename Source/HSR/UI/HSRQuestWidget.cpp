#include "HSRQuestWidget.h"

#include "../Quest/HSRQuestSubsystem.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

// 设置/替换 ViewModel。若本 Widget 之前拥有一个自建 VM（bOwnsViewModel），
// 替换前先关闭它，避免泄漏。然后绑定新 VM 并刷新显示。
void UHSRQuestWidget::SetViewModel(UHSRQuestViewModel* InViewModel)
{
	Unbind();
	// 拥有权转移：旧 VM 若由本 Widget 创建，需要负责关闭。
	if (bOwnsViewModel && ViewModel)
	{
		ViewModel->Shutdown();
	}
	ViewModel = InViewModel;
	bOwnsViewModel = false;
	BindAndRefresh();
}

// 取当前快照：未就绪返回 false，就绪时写入出参。
bool UHSRQuestWidget::GetCurrentSnapshot(FHSRQuestFrontendSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot)
	{
		return false;
	}
	OutSnapshot = Current;
	return true;
}

// 构造完成：若外部未注入 VM，则本 Widget 自建一个并绑定任务子系统。
// 这是“无宿主自举”路径——界面可独立于外部管理器运行。
void UHSRQuestWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!ViewModel)
	{
		UGameInstance* GameInstance = GetGameInstance();
		UHSRQuestSubsystem* Quest = GameInstance ? GameInstance->GetSubsystem<UHSRQuestSubsystem>() : nullptr;
		ViewModel = NewObject<UHSRQuestViewModel>(this);
		ViewModel->Initialize(Quest);
		// 标记拥有权：本 Widget 负责关闭这个自建 VM。
		bOwnsViewModel = true;
	}
	BindAndRefresh();
}

// 销毁：解绑订阅；若 VM 由本 Widget 拥有，则一并关闭并置空。
void UHSRQuestWidget::NativeDestruct()
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

// 建立订阅并立即刷新：订阅 VM 的 OnChanged，随后拉取当前快照同步显示。
// 已有订阅时不再重复绑定（Subscription 有效即直接返回）。
void UHSRQuestWidget::BindAndRefresh()
{
	if (!ViewModel || Subscription.IsValid())
	{
		return;
	}
	Subscription = ViewModel->OnChanged().AddUObject(this, &ThisClass::HandleSnapshot);
#if WITH_DEV_AUTOMATION_TESTS
	++BindCount;
#endif
	// 用当前快照初始化一次，避免界面等待下一次变化。
	FHSRQuestFrontendSnapshot Snapshot;
	if (ViewModel->GetSnapshot(Snapshot))
	{
		HandleSnapshot(Snapshot);
	}
}

// 解绑：移除订阅、复位订阅句柄并清空本地缓存快照。
void UHSRQuestWidget::Unbind()
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
	Current = FHSRQuestFrontendSnapshot();
}

// VM 快照回调：缓存快照并通知蓝图事件刷新显示。
void UHSRQuestWidget::HandleSnapshot(const FHSRQuestFrontendSnapshot& InSnapshot)
{
	Current = InSnapshot;
	bHasSnapshot = true;
	OnQuestSnapshotChanged(Current);
	RefreshQuestList();
}

FText UHSRQuestWidget::FormatQuestCard(const FHSRQuestViewData& Quest)
{
	const FText Title = Quest.DisplayName.IsEmpty()
		? NSLOCTEXT("HSRQuest", "UnnamedQuest", "未命名任务") : Quest.DisplayName;
	FText State = NSLOCTEXT("HSRQuest", "InProgress", "进行中");
	if (Quest.bRewardClaimed) State = NSLOCTEXT("HSRQuest", "RewardClaimed", "已完成 · 奖励已领取");
	else if (Quest.State == EHSRQuestState::Completed) State = NSLOCTEXT("HSRQuest", "Completed", "已完成");
	else if (Quest.State == EHSRQuestState::NotStarted) State = NSLOCTEXT("HSRQuest", "NotStarted", "未开始");
	TArray<FText> Lines;
	Lines.Add(FText::Format(NSLOCTEXT("HSRQuest", "CardHeading", "{0}  ·  {1}"), Title, State));
	for (const FHSRQuestObjectiveViewData& Objective : Quest.Objectives)
	{
		const FText Description = Objective.Description.IsEmpty()
			? NSLOCTEXT("HSRQuest", "UnnamedObjective", "完成调查") : Objective.Description;
		Lines.Add(FText::Format(NSLOCTEXT("HSRQuest", "ObjectiveLine", "{0}  {1}  {2}/{3}"),
			Objective.bCompleted ? FText::FromString(TEXT("✓")) : FText::FromString(TEXT("◇")),
			Description, FText::AsNumber(Objective.CurrentCount), FText::AsNumber(Objective.RequiredCount)));
	}
	return FText::Join(FText::FromString(TEXT("\n")), Lines);
}

void UHSRQuestWidget::RefreshQuestList()
{
	if (!WidgetTree) return;
	UPanelWidget* List = Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("QuestList")));
	if (!List) return;
	List->ClearChildren();
	for (const FHSRQuestViewData& Quest : Current.Quests)
	{
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
		Card->SetPadding(FMargin(24.f, 20.f));
		Card->SetBrushColor(FLinearColor(.025f, .045f, .075f, .94f));
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
		Text->SetText(FormatQuestCard(Quest));
		Text->SetAutoWrapText(true);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = 22;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(FLinearColor(.9f, .93f, 1.f)));
		Card->SetContent(Text);
		List->AddChild(Card);
	}
	const bool bReady = Current.Status == EHSRQuestFrontendStatus::Ready;
	if (UWidget* Scroll = WidgetTree->FindWidget(TEXT("QuestScrollBox")))
		Scroll->SetVisibility(bReady ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (UWidget* Empty = WidgetTree->FindWidget(TEXT("EmptyState")))
		Empty->SetVisibility(Current.Status == EHSRQuestFrontendStatus::Empty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (UWidget* Unavailable = WidgetTree->FindWidget(TEXT("UnavailableState")))
		Unavailable->SetVisibility(Current.Status == EHSRQuestFrontendStatus::Unavailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}
