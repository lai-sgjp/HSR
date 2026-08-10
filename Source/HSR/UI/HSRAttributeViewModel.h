#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AbilitySystemComponent.h"
#include "HSRAttributeViewModel.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttributeValuesUpdated, class UHSRAttributeViewModel*, ViewModel);

/**
 * Mirror of one owner's own attributes, held as a subobject by the actor that owns the ASC.
 *
 * This deliberately keeps a live ASC handle, which looks like it contradicts the pointer-free UI
 * rule -- it does not. That rule is about not reaching across a boundary: the battle command
 * ViewModel used to subscribe to whichever participant was currently selected, re-deriving numbers
 * the Coordinator had already published, so a second authority for the same value appeared. Here
 * the subscriber and the ASC belong to the same actor, and there is no published snapshot that
 * already carries these values. Reading them through a battle-scoped snapshot instead would not
 * work: this model also serves exploration, where no battle snapshot exists.
 *
 * If you need another participant's numbers for display, read the published participant view
 * rather than widening this class to accept a foreign ASC.
 */
UCLASS(BlueprintType)
class HSR_API UHSRAttributeViewModel : public UObject
{
	GENERATED_BODY()

public:
	void InitializeFromASC(UAbilitySystemComponent* InASC);
	void Teardown();

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	float Health;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	float MaxHealth;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	float Energy;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	float MaxEnergy;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	float Speed;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnAttributeValuesUpdated OnValuesUpdated;

	// Diagnostic counters for Phase 2 testing
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Diagnostics")
	int32 HealthChangeCount;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Diagnostics")
	int32 MaxHealthChangeCount;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Diagnostics")
	int32 EnergyChangeCount;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Diagnostics")
	int32 MaxEnergyChangeCount;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Diagnostics")
	int32 SpeedChangeCount;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Diagnostics")
	int32 TotalBroadcastCount;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Diagnostics")
	int32 SnapshotBroadcastCount;

	// Public reset for P2-002
	void ResetDiagnosticCounters();

	// Re-broadcast current values (after HUD rebuild etc.)
	UFUNCTION(BlueprintCallable, Category = "Diagnostics", meta = (DevelopmentOnly))
	void BroadcastCurrentValues();

protected:
	void PushCurrentValues();
	void OnAttributeChanged(const FOnAttributeChangeData& Data);

	TMap<FGameplayAttribute, FDelegateHandle> DelegateHandles;
	TWeakObjectPtr<UAbilitySystemComponent> ASCWeakPtr;
};
