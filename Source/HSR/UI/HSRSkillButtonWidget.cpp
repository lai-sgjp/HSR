#include "HSRSkillButtonWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UHSRSkillButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (BTN_Skill)
	{
		BTN_Skill->OnClicked.AddDynamic(this, &UHSRSkillButtonWidget::HandleClicked);
	}
}

void UHSRSkillButtonWidget::NativeDestruct()
{
	if (BTN_Skill)
	{
		BTN_Skill->OnClicked.RemoveDynamic(this, &UHSRSkillButtonWidget::HandleClicked);
	}
	Super::NativeDestruct();
}

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
		// Placeholder descriptions are authoring debt, not player-facing copy.
		TXT_Description->SetText(SkillView.bDescriptionIsPlaceholder ? FText::GetEmpty() : SkillView.Description);
	}
	if (TXT_Cost)
	{
		TXT_Cost->SetText(SkillView.BuildCostText());
	}

	OnSkillViewChanged(SkillView, bSelected);
}

void UHSRSkillButtonWidget::HandleClicked()
{
	OnSkillClicked.ExecuteIfBound(SkillView.SkillId);
}
