#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSRUserWidget.generated.h"

class UHSRAttributeViewModel;

UCLASS()
class HSR_API UHSRUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UHSRUserWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "GAS")
	void SetAttributeViewModel(UHSRAttributeViewModel* InViewModel);

	UFUNCTION(BlueprintPure, Category = "GAS")
	UHSRAttributeViewModel* GetAttributeViewModel() const { return AttributeViewModel; }

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSRAttributeViewModel> AttributeViewModel;
};
