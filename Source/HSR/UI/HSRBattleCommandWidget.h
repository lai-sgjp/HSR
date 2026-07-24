#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSRBattleCommandTypes.h"
#include "HSRBattleCommandWidget.generated.h"

class UHSRBattleCommandViewModel;
class UHSRBattleCoordinator;
class UButton;
class UComboBoxString;
class UTextBlock;

UCLASS(Abstract)
class HSR_API UHSRBattleCommandWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UHSRBattleCommandWidget(const FObjectInitializer& ObjectInitializer);
	void BindViewModel(UHSRBattleCommandViewModel* InViewModel, UHSRBattleCoordinator* InCoordinator);
#if WITH_EDITOR
	int32 GetBindGenerationForDevelopmentTest() const { return BindGeneration; }
	bool HasActiveViewModelBindingForDevelopmentTest() const { return StateChangedHandle.IsValid(); }
	int32 GetSubmitCountForDevelopmentTest() const { return SubmitCount; }
#endif

	UFUNCTION(BlueprintPure, Category = "Battle|Command")
	FHSRBattleCommandViewState GetCurrentViewState() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetCurrentActorText() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetEnergyText() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetSkillPointsText() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetLastResolutionText() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetWeaknessText() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetToughnessText() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetBreakText() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetDelayText() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetTurnOrderText() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Participants") FText GetParticipantsText() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Presentation") FText GetPresentationText() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Status") FText GetStatusText() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Status") FText GetStatusOperationText() const;
	UFUNCTION(BlueprintCallable, Category = "Battle|Command") bool SelectSkill(EHSRSkillCategory Category);
	UFUNCTION(BlueprintCallable, Category = "Battle|Command") bool SelectTarget(FName TargetId);
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FName GetSelectedSkillId() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FName GetSelectedTargetId() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Command") TArray<FName> GetTargetOptions() const;

	/** Submits stable IDs only. The Coordinator remains authoritative. */
	UFUNCTION(BlueprintCallable, Category = "Battle|Command")
	FHSRAbilityResolution SubmitCommand(FGuid ActionId, FName ActorParticipantId, FName SkillId, FName TargetParticipantId);

	/** Generates one action ID and submits only the C++-maintained selected stable IDs. */
	UFUNCTION(BlueprintCallable, Category = "Battle|Command") FHSRAbilityResolution SubmitSelectedSkill();
	/** Sends only the displayed request ID; GameMode owns consumption and travel. */
	UFUNCTION(BlueprintCallable, Category = "Battle|Result") bool ConfirmBattleResult();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Command", meta = (DisplayName = "Command View State Changed"))
	void OnCommandViewStateChanged(const FHSRBattleCommandViewState& State);

private:
	FGuid LastSubmittedActionId;
	FHSRAbilityResolution LastSubmittedResolution;
	UFUNCTION() void HandleBasicClicked();
	UFUNCTION() void HandleSkillClicked();
	UFUNCTION() void HandleUltimateClicked();
	UFUNCTION() void HandleHealClicked();
	UFUNCTION() void HandleExecuteClicked();
	UFUNCTION() void HandleResultConfirmClicked();

	void HandleViewStateChanged(const FHSRBattleCommandViewState& State);
	void UnbindViewModel();
	void BindDesignerEvents();
	void UnbindDesignerEvents();
	void RefreshDesignerControls(const FHSRBattleCommandViewState& State);
	void RefreshSkillControls(const FHSRBattleCommandViewState& State, EHSRSkillCategory Category, UButton* Button, UTextBlock* NameText, UTextBlock* DescriptionText, UTextBlock* CostText);
	const FHSRBattleCommandSkillView* FindSkillView(const FHSRBattleCommandViewState& State, EHSRSkillCategory Category) const;
	void FocusResultConfirm();

	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_CurrentActor;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Energy;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_SkillPoints;
	UPROPERTY(BlueprintReadOnly, Category="Battle|Command", meta=(BindWidgetOptional, AllowPrivateAccess="true")) TObjectPtr<UComboBoxString> CB_Target;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> BTN_Basic;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> BTN_Skill;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> BTN_Ultimate;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Heal;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> BTN_Execute;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_BasicName;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_BasicDescription;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_BasicCost;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_SkillName;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_SkillDescription;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_SkillCost;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_UltimateName;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_UltimateDescription;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_UltimateCost;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_HealName;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_HealDescription;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_HealCost;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Last_Resolution;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Weakness;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Toughness;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Break;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Delay;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Statuses;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_StatisOperation;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_DisabledReason;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_TurnOrder;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Participants;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Presentation;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TXT_Result;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> BTN_ResultConfirm;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> PendingOverlay;

	TWeakObjectPtr<UHSRBattleCommandViewModel> ViewModel;
	TWeakObjectPtr<UHSRBattleCoordinator> Coordinator;
	FDelegateHandle StateChangedHandle;
	int32 BindGeneration = 0;
	int32 SubmitCount = 0;
	bool bRefreshingDesignerControls = false;
};
