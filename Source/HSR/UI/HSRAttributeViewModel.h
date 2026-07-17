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

protected:
	void PushCurrentValues();
	void OnAttributeChanged(const FOnAttributeChangeData& Data);

	TMap<FGameplayAttribute, FDelegateHandle> DelegateHandles;
	TWeakObjectPtr<UAbilitySystemComponent> ASCWeakPtr;
};
