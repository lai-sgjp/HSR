#include "HSRSkillButtonWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

// 构造完成：把按钮点击事件绑定到本地处理。
void UHSRSkillButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (BTN_Skill)
	{
		BTN_Skill->OnClicked.AddDynamic(this, &UHSRSkillButtonWidget::HandleClicked);
	}
}

// 析构：解绑按钮点击事件。
void UHSRSkillButtonWidget::NativeDestruct()
{
	if (BTN_Skill)
	{
		BTN_Skill->OnClicked.RemoveDynamic(this, &UHSRSkillButtonWidget::HandleClicked);
	}
	Super::NativeDestruct();
}

// 用技能视图数据刷新按钮：可用性、名称、描述、消耗。
void UHSRSkillButtonWidget::SetSkillView(const FHSRBattleCommandSkillView& InView, bool bInSelected)
{
	SkillView = InView;
	bSelected = bInSelected;

	if (BTN_Skill)
	{
		BTN_Skill->SetIsEnabled(SkillView.bAvailable);
	}
	if (TXT_Name)
	{
		TXT_Name->SetText(SkillView.DisplayName);
	}
	if (TXT_Description)
	{
		// 占位描述是作者遗漏，不是面向玩家的文案——留空即可。
		TXT_Description->SetText(SkillView.bDescriptionIsPlaceholder ? FText::GetEmpty() : SkillView.Description);
	}
	if (TXT_Cost)
	{
		TXT_Cost->SetText(SkillView.BuildCostText());
	}

	// 通知子类视图已更新（可做样式高亮等）。
	OnSkillViewChanged(SkillView, bSelected);
}

// 按钮点击：把技能 ID 通过委托回传给外部（战斗命令面板）。
void UHSRSkillButtonWidget::HandleClicked()
{
	OnSkillClicked.ExecuteIfBound(SkillView.SkillId);
}
