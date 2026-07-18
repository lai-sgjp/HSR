#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSRUserWidget.generated.h"

class UHSRAttributeViewModel;
class UHSRInteractionViewModel;

UCLASS()
class HSR_API UHSRUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UHSRUserWidget(const FObjectInitializer& ObjectInitializer);

	// Attribute ViewModel (Phase 2)
	UFUNCTION(BlueprintCallable, Category = "GAS")
	void SetAttributeViewModel(UHSRAttributeViewModel* InViewModel);

	UFUNCTION(BlueprintPure, Category = "GAS")
	UHSRAttributeViewModel* GetAttributeViewModel() const { return AttributeViewModel; }

	// Interaction ViewModel (Phase 3)
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetInteractionViewModel(UHSRInteractionViewModel* InViewModel);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	UHSRInteractionViewModel* GetInteractionViewModel() const { return InteractionViewModel; }

	// Blueprint event for prompt changes
	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
	void OnInteractionPromptChanged(bool bVisible, const FText& PromptText);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// Internal UFUNCTION for ViewModel delegate binding
	UFUNCTION()
	void OnInternalPromptChanged(bool bVisible, const FText& PromptText);

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSRAttributeViewModel> AttributeViewModel;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSRInteractionViewModel> InteractionViewModel;

	int32 WidgetInstanceId;
	int32 PromptReceiveCount;
};
