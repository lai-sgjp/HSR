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
		TXT_Cost->SetText(BuildCostText());
	}

	OnSkillViewChanged(SkillView, bSelected);
}

// Cost text is derived from the authored numbers, never from the category. A skill that costs two
// skill points reads "SP -2" whatever category it carries, and a positive delta reads "SP +1".
FText UHSRSkillButtonWidget::BuildCostText() const
{
	TArray<FText> Parts;
	if (SkillView.SkillPointDelta < 0)
	{
		Parts.Add(FText::Format(NSLOCTEXT("HSRCommand", "EntrySkillPointSpend", "SP -{0}"), FText::AsNumber(-SkillView.SkillPointDelta)));
	}
	else if (SkillView.SkillPointDelta > 0)
	{
		Parts.Add(FText::Format(NSLOCTEXT("HSRCommand", "EntrySkillPointGain", "SP +{0}"), FText::AsNumber(SkillView.SkillPointDelta)));
	}

	if (SkillView.bEnergyCostIsKnown && SkillView.EnergyCost > 0.0f)
	{
		Parts.Add(FText::Format(NSLOCTEXT("HSRCommand", "EntryEnergyCost", "Energy -{0}"), FText::AsNumber(FMath::RoundToInt(SkillView.EnergyCost))));
	}

	return Parts.IsEmpty() ? FText::GetEmpty() : FText::Join(FText::FromString(TEXT("  ")), Parts);
}

void UHSRSkillButtonWidget::HandleClicked()
{
	OnSkillClicked.ExecuteIfBound(SkillView.SkillId);
}
