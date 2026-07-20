#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSRBattleCommandTypes.h"
#include "HSRBattleCommandWidget.generated.h"

class UHSRBattleCommandViewModel;
class UHSRBattleCoordinator;

UCLASS(Abstract)
class HSR_API UHSRBattleCommandWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindViewModel(UHSRBattleCommandViewModel* InViewModel, UHSRBattleCoordinator* InCoordinator);

	UFUNCTION(BlueprintPure, Category = "Battle|Command")
	FHSRBattleCommandViewState GetCurrentViewState() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetCurrentActorText() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetEnergyText() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetSkillPointsText() const;
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetLastResolutionText() const;
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

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Command", meta = (DisplayName = "Command View State Changed"))
	void OnCommandViewStateChanged(const FHSRBattleCommandViewState& State);

private:
	void HandleViewStateChanged(const FHSRBattleCommandViewState& State);
	void UnbindViewModel();

	TWeakObjectPtr<UHSRBattleCommandViewModel> ViewModel;
	TWeakObjectPtr<UHSRBattleCoordinator> Coordinator;
	FDelegateHandle StateChangedHandle;
	int32 BindGeneration = 0;
	int32 SubmitCount = 0;
};
