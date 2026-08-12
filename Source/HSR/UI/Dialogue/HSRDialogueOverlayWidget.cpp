#include "HSRDialogueOverlayWidget.h"

#include "HSRDialoguePresentationViewModel.h"
#include "../../UI/HSRUIManagerSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

// 构造函数：仅把控件设为可聚焦。
// 可聚焦是键盘输入（方向键选选项、Esc 退出）能够到达本控件的前提。
UHSRDialogueOverlayWidget::UHSRDialogueOverlayWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

// SetOwningUIManager：注入所属 UI 管理器。
// 关闭对话框（Esc/手柄退出键）时需要通过它来真正关闭覆盖层。
void UHSRDialogueOverlayWidget::SetOwningUIManager(UHSRUIManagerSubsystem* InManager)
{
	OwningUIManager = InManager;
}

// SetViewModel：绑定对话展示层的 ViewModel。
// 换绑时先解绑旧 ViewModel 的动态委托，再绑定新的；
// 若新 ViewModel 已有有效快照则立即刷新一次界面，保证绑定即呈现。
void UHSRDialogueOverlayWidget::SetViewModel(UHSRDialoguePresentationViewModel* InViewModel)
{
	// 同一个对象重复设置时直接忽略，避免无谓的解绑/重绑。
	if (ViewModel == InViewModel)
	{
		return;
	}

	// 解绑旧 ViewModel，防止其广播继续驱动本控件。
	if (ViewModel)
	{
		ViewModel->OnSnapshotChanged.RemoveDynamic(this, &ThisClass::HandleViewModelSnapshotChanged);
	}

	ViewModel = InViewModel;
	if (ViewModel)
	{
		// 用 AddUniqueDynamic 保证同一个回调只绑定一次。
		ViewModel->OnSnapshotChanged.AddUniqueDynamic(this, &ThisClass::HandleViewModelSnapshotChanged);
		// 绑定后立即拉一次快照：若 ViewModel 已有数据，马上把界面画出来。
		FHSRDialoguePresentationSnapshot Snapshot;
		if (ViewModel->GetSnapshot(Snapshot))
		{
			HandleViewModelSnapshotChanged(Snapshot);
		}
	}
}

// GetSnapshot：从 ViewModel 导出当前展示快照。
// 对外统一以“无快照”为默认值，避免调用方拿到未初始化的数据。
bool UHSRDialogueOverlayWidget::GetSnapshot(FHSRDialoguePresentationSnapshot& OutSnapshot) const
{
	OutSnapshot = FHSRDialoguePresentationSnapshot();
	return ViewModel && ViewModel->GetSnapshot(OutSnapshot);
}

// GetChoiceCount：返回当前可展示的选项数量；无有效快照时按 0 处理。
int32 UHSRDialogueOverlayWidget::GetChoiceCount() const
{
	FHSRDialoguePresentationSnapshot Snapshot;
	return GetSnapshot(Snapshot) ? Snapshot.Choices.Num() : 0;
}

// GetChoiceAt：按索引取出某个选项数据给调用方（文本/可用性等）。
// 无快照或索引越界时返回 false，并保持输出为默认值。
bool UHSRDialogueOverlayWidget::GetChoiceAt(const int32 Index,
	FHSRDialoguePresentationChoice& OutChoice) const
{
	OutChoice = FHSRDialoguePresentationChoice();
	FHSRDialoguePresentationSnapshot Snapshot;
	if (!GetSnapshot(Snapshot) || !Snapshot.Choices.IsValidIndex(Index))
	{
		return false;
	}
	OutChoice = Snapshot.Choices[Index];
	return true;
}

// GetSpeakerText：返回说话人名称；无有效快照时返回空文本。
FText UHSRDialogueOverlayWidget::GetSpeakerText() const
{
	FHSRDialoguePresentationSnapshot Snapshot;
	return GetSnapshot(Snapshot) ? Snapshot.SpeakerText : FText::GetEmpty();
}

// GetBodyText：返回对话正文；无有效快照时返回空文本。
FText UHSRDialogueOverlayWidget::GetBodyText() const
{
	FHSRDialoguePresentationSnapshot Snapshot;
	return GetSnapshot(Snapshot) ? Snapshot.BodyText : FText::GetEmpty();
}

// GetChoiceDisplayText：取出某选项的显示文本。
bool UHSRDialogueOverlayWidget::GetChoiceDisplayText(const int32 Index, FText& OutText) const
{
	FHSRDialoguePresentationChoice Choice;
	if (!GetChoiceAt(Index, Choice))
	{
		OutText = FText::GetEmpty();
		return false;
	}
	OutText = Choice.DisplayText;
	return true;
}

// GetChoiceEnabled：取出某选项是否可点（用于按钮置灰）。
bool UHSRDialogueOverlayWidget::GetChoiceEnabled(const int32 Index, bool& bOutEnabled) const
{
	FHSRDialoguePresentationChoice Choice;
	if (!GetChoiceAt(Index, Choice))
	{
		bOutEnabled = false;
		return false;
	}
	bOutEnabled = Choice.bEnabled;
	return true;
}

// SubmitChoiceByIndex：把“点击第几个选项按钮”翻译成“提交对应选项请求”。
// 数据流：从当前快照取出选项数据 -> 组装请求 -> 交给 ViewModel 执行；
// 若执行结果是成功且会话已关闭，则顺带请求 UI 管理器关闭覆盖层。
EHSRDialoguePresentationResult UHSRDialogueOverlayWidget::SubmitChoiceByIndex(const int32 Index)
{
	FHSRDialoguePresentationSnapshot Snapshot;
	FHSRDialoguePresentationChoice Choice;
	if (!GetSnapshot(Snapshot) || !Snapshot.Choices.IsValidIndex(Index)
		|| !GetChoiceAt(Index, Choice))
	{
		// 快照不可用或选项索引越界时按无效选项处理。
		return EHSRDialoguePresentationResult::InvalidChoice;
	}

	// 用当前快照的会话三元组 + 选项 ID 组装提交请求。
	FHSRDialoguePresentationChoiceRequest Request;
	Request.QueryId = Snapshot.QueryId;
	Request.DialogueId = Snapshot.DialogueId;
	Request.NodeId = Snapshot.NodeId;
	Request.ChoiceId = Choice.ChoiceId;
	const EHSRDialoguePresentationResult Result = ViewModel->SubmitChoice(Request);

	// 选择成功后若对话已自然结束，则自动收起覆盖层。
	FHSRDialoguePresentationSnapshot After;
	if (Result == EHSRDialoguePresentationResult::Success
		&& ViewModel->GetSnapshot(After)
		&& After.Status == EHSRDialoguePresentationStatus::Closed
		&& OwningUIManager.IsValid())
	{
		OwningUIManager->CloseDialogueOverlay();
	}
	return Result;
}

// RequestCloseDialogue：请求 UI 管理器关闭对话覆盖层。
// 返回是否成功；管理器未接入时返回 InvalidHost。
EHSRUIScreenResult UHSRDialogueOverlayWidget::RequestCloseDialogue()
{
	return OwningUIManager.IsValid()
		? OwningUIManager->CloseDialogueOverlay()
		: EHSRUIScreenResult::InvalidHost;
}

// NativeConstruct：控件进入可视树时保持可聚焦状态。
// 对话界面必须能接收键盘事件（选择选项 / 退出），故显式强调一次。
void UHSRDialogueOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
}

// NativeDestruct：控件销毁时解除 ViewModel 订阅并清空引用。
// 防止销毁后 ViewModel 广播仍调用本控件回调（动态委托会在对象销毁时被移除，但仍显式解绑一次）。
void UHSRDialogueOverlayWidget::NativeDestruct()
{
	if (ViewModel)
	{
		ViewModel->OnSnapshotChanged.RemoveDynamic(this, &ThisClass::HandleViewModelSnapshotChanged);
	}
	ViewModel = nullptr;
	OwningUIManager.Reset();
	Super::NativeDestruct();
}

// NativeOnKeyDown：键盘/手柄按键处理。
// 按下“返回”或“关闭到根”键时关闭对话覆盖层，并消费该输入；
// 否则交给父类继续默认处理。
FReply UHSRDialogueOverlayWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if ((IsBackKey(Key) || IsCloseToRootKey(Key)) && OwningUIManager.IsValid())
	{
		OwningUIManager->CloseDialogueOverlay();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// HandleViewModelSnapshotChanged：ViewModel 每次广播新快照时触发。
// 一方面把快照转达给外部订阅者（OnDialogueSnapshotChanged），
// 另一方面刷新本地控件内容（说话人/正文/选项）。
void UHSRDialogueOverlayWidget::HandleViewModelSnapshotChanged(
	const FHSRDialoguePresentationSnapshot& Snapshot)
{
	OnDialogueSnapshotChanged(Snapshot);
	PopulateView();
}

// PopulateView：依据当前快照刷新界面显示。
// 固定槽位式布局：最多支持 4 个选项按钮；有效的选项显示文本并按可用性置灰，
// 超出当前选项数的按钮折叠隐藏。
void UHSRDialogueOverlayWidget::PopulateView()
{
	UWidgetTree* Tree = WidgetTree;
	if (!Tree)
	{
		return;
	}

	// 说话人与正文直接取自快照。
	if (UTextBlock* Speaker = FindTextByName(TEXT("TXT_Speaker")))
	{
		Speaker->SetText(GetSpeakerText());
	}
	if (UTextBlock* Body = FindTextByName(TEXT("TXT_Body")))
	{
		Body->SetText(GetBodyText());
	}

	const int32 Count = GetChoiceCount();
	for (int32 SlotIndex = 0; SlotIndex < 4; ++SlotIndex)
	{
		UButton* Choice = FindButtonByName(*FString::Printf(TEXT("ChoiceButton_%d"), SlotIndex));
		if (!Choice)
		{
			continue;
		}
		UTextBlock* Label = FindTextByName(*FString::Printf(TEXT("TXT_Choice_%d"), SlotIndex));
		if (SlotIndex < Count)
		{
			// 有效选项：显示按钮、按可用性置灰，并写入选项文本。
			Choice->SetVisibility(ESlateVisibility::Visible);
			bool bEnabled = true;
			GetChoiceEnabled(SlotIndex, bEnabled);
			Choice->SetIsEnabled(bEnabled);
			FText DisplayText;
			if (GetChoiceDisplayText(SlotIndex, DisplayText) && Label)
			{
				Label->SetText(DisplayText);
			}
		}
		else
		{
			// 空槽位：折叠隐藏，保持布局稳定。
			Choice->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

// FindButtonByName：按名字在控件树中查找按钮。
UButton* UHSRDialogueOverlayWidget::FindButtonByName(const FName Name) const
{
	return WidgetTree ? WidgetTree->FindWidget<UButton>(Name) : nullptr;
}

// FindTextByName：按名字在控件树中查找文本块。
UTextBlock* UHSRDialogueOverlayWidget::FindTextByName(const FName Name) const
{
	return WidgetTree ? WidgetTree->FindWidget<UTextBlock>(Name) : nullptr;
}

// IsBackKey：判断按键是否属于“返回”类（Esc 或手柄右侧特殊键）。
// 这类键统一触发关闭对话。
bool UHSRDialogueOverlayWidget::IsBackKey(const FKey& Key)
{
	return Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right;
}

// IsCloseToRootKey：判断按键是否属于“关闭到根”类（X 键）。
// 与返回的区别在于语义：直接一路关闭到根界面。
bool UHSRDialogueOverlayWidget::IsCloseToRootKey(const FKey& Key)
{
	return Key == EKeys::X;
}

#if WITH_DEV_AUTOMATION_TESTS
// 以下两个仅存在于自动化测试构建中的辅助方法：
// 把“按键是否应被消费”暴露给测试，方便在无真实输入事件时验证按键判定逻辑。
bool UHSRDialogueOverlayWidget::ShouldConsumeBackKeyForAutomation(const FKey& Key) const
{
	return IsBackKey(Key);
}

bool UHSRDialogueOverlayWidget::ShouldConsumeCloseToRootKeyForAutomation(const FKey& Key) const
{
	return IsCloseToRootKey(Key);
}
#endif
