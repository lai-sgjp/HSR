#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HSRBattleCommandTypes.h"
#include "HSRBattleCommandViewModel.generated.h"

class UHSRBattleCoordinator;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;

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
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FText WeaknessText;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FText ToughnessText;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FText BreakText;
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Command") FText DelayText;

	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetCurrentActorText() const { return CurrentActorText; }
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetEnergyText() const { return EnergyText; }
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetSkillPointsText() const { return SkillPointsText; }
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetLastResolutionText() const { return LastResolutionText; }
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetWeaknessText() const { return WeaknessText; }
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetToughnessText() const { return ToughnessText; }
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetBreakText() const { return BreakText; }
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FText GetDelayText() const { return DelayText; }

	/** Selects a configured skill and automatically chooses its first valid target. */
	UFUNCTION(BlueprintCallable, Category = "Battle|Command") bool SelectSkill(EHSRSkillCategory Category);
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FName GetSelectedSkillId() const { return SelectedSkillId; }
	UFUNCTION(BlueprintPure, Category = "Battle|Command") FName GetSelectedTargetId() const { return SelectedTargetId; }
	UFUNCTION(BlueprintPure, Category = "Battle|Command") TArray<FName> GetTargetOptions() const;
	UFUNCTION(BlueprintCallable, Category = "Battle|Command") bool SelectTarget(FName TargetId);

	void SetState(const FHSRBattleCommandViewState& InState);
	void BindCoordinator(UHSRBattleCoordinator* InCoordinator);
	void UnbindCoordinator();
	FHSRBattleCommandStateChanged& OnChanged() { return Changed; }

private:
	UPROPERTY()
	FHSRBattleCommandViewState State;
	UPROPERTY() FName SelectedSkillId;
	UPROPERTY() FName SelectedTargetId;
	FHSRBattleCommandStateChanged Changed;
	TWeakObjectPtr<UHSRBattleCoordinator> Coordinator;
	TWeakObjectPtr<UAbilitySystemComponent> ObservedTargetASC;
	FDelegateHandle ToughnessChangedHandle;
	FDelegateHandle MaxToughnessChangedHandle;

	const FHSRBattleCommandSkillView* FindSelectedSkill() const;
	void RefreshPresentationAndSelection();
	void RefreshReadOnlyBattlePresentation();
	void RebindTargetAttributes();
	void HandleObservedToughnessChanged(const FOnAttributeChangeData& ChangeData);
};
