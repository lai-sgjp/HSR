#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AbilitySystemComponent.h"
#include "HSRAttributeViewModel.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttributeValuesUpdated, class UHSRAttributeViewModel*, ViewModel);

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
