#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HSRBattleCommandTypes.h"
#include "HSRBattleCommandViewModel.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRBattleCommandStateChanged, const FHSRBattleCommandViewState&);

UCLASS()
class HSR_API UHSRBattleCommandViewModel : public UObject
{
	GENERATED_BODY()

public:
	const FHSRBattleCommandViewState& GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Battle|Command")
	FHSRBattleCommandViewState GetStateCopy() const { return State; }

	/** Display-ready values. WBP must not format command state itself. */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FText CurrentActorText;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FText EnergyText;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FText SkillPointsText;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FText LastResolutionText;

	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetCurrentActorText() const { return CurrentActorText; }
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetEnergyText() const { return EnergyText; }
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetSkillPointsText() const { return SkillPointsText; }
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetLastResolutionText() const { return LastResolutionText; }

	/** Selects a configured skill and automatically chooses its first valid target. */
	UFUNCTION(BlueprintCallable, Category = "Battle|Command") bool SelectSkill(EHSRSkillCategory Category);
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FName GetSelectedSkillId() const { return SelectedSkillId; }
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FName GetSelectedTargetId() const { return SelectedTargetId; }
	UFUNCTION(BlueprintPure, Category = "Battle|Command") TArray<FName> GetTargetOptions() const;
	UFUNCTION(BlueprintCallable, Category = "Battle|Command") bool SelectTarget(FName TargetId);

	void SetState(const FHSRBattleCommandViewState& InState);
	FHSRBattleCommandStateChanged& OnChanged() { return Changed; }

private:
	UPROPERTY()
	FHSRBattleCommandViewState State;
	UPROPERTY() FName SelectedSkillId;
	UPROPERTY() FName SelectedTargetId;
	FHSRBattleCommandStateChanged Changed;

	const FHSRBattleCommandSkillView* FindSelectedSkill() const;
	void RefreshPresentationAndSelection();
};
