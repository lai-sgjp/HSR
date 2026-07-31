#pragma once

#include "CoreMinimal.h"
#include "HSRScreenWidget.h"
#include "HSRSaveViewModel.h"
#include "HSRSaveWidget.generated.h"

UCLASS(Blueprintable)
class HSR_API UHSRSaveWidget : public UHSRScreenWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HSR|Save") void SetViewModel(UHSRSaveViewModel* InViewModel);
	UFUNCTION(BlueprintPure, Category = "HSR|Save") bool GetCurrentResult(FHSRSaveFrontendResult& OutResult) const;
	UFUNCTION(BlueprintCallable, Category = "HSR|Save") EHSRSaveFrontendActionResult RequestSave(const FString& SlotName);
	UFUNCTION(BlueprintCallable, Category = "HSR|Save") EHSRSaveFrontendActionResult ConfirmOverwrite();
	UFUNCTION(BlueprintCallable, Category = "HSR|Save") void CancelOverwrite();
	UFUNCTION(BlueprintCallable, Category = "HSR|Save") EHSRSaveResult RequestLoad(const FString& SlotName);
	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|Save") void OnSaveResultChanged(const FHSRSaveFrontendResult& Result);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void Refresh();
	UPROPERTY(Transient) TObjectPtr<UHSRSaveViewModel> ViewModel;
	FHSRSaveFrontendResult Current;
	bool bHasResult = false;
	bool bOwnsViewModel = false;
};
