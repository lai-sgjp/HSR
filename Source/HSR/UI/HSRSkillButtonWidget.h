#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSRBattleCommandTypes.h"
#include "HSRSkillButtonWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FHSRSkillButtonClicked, FName /*SkillId*/);

/**
 * One entry in a data-driven skill list. The owning widget spawns one per FHSRBattleCommandSkillView
 * in the view state, so adding a skill is a DataAsset edit -- no new BindWidget member, no new click
 * handler, and no new EHSRSkillCategory value.
 *
 * Blueprint subclasses may either bind the optional widgets below by name or override
 * OnSkillViewChanged to drive their own visuals.
 */
UCLASS(Abstract)
class HSR_API UHSRSkillButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Push one skill view into this entry. Safe to call every refresh. */
	void SetSkillView(const FHSRBattleCommandSkillView& InView, bool bInSelected);

	FName GetSkillId() const { return SkillView.SkillId; }

	/** Fired with this entry's SkillId; the owner routes it to SelectSkillById. */
	FHSRSkillButtonClicked OnSkillClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Blueprint hook for custom visuals. Called after the optional bound widgets are updated. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Command")
	void OnSkillViewChanged(const FHSRBattleCommandSkillView& InView, bool bInSelected);

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command")
	FHSRBattleCommandSkillView SkillView;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command")
	bool bSelected = false;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> BTN_Skill;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Name;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Description;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Cost;

private:
	/** Cost line built from the authored numbers, with no per-category special cases. */
	FText BuildCostText() const;

	UFUNCTION()
	void HandleClicked();
};
