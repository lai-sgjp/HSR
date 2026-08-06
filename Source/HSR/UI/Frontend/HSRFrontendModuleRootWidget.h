#pragma once

#include "CoreMinimal.h"
#include "../HSRScreenWidget.h"
#include "HSRFrontendRouteTypes.h"
#include "HSRFrontendModuleRootWidget.generated.h"

UCLASS(Blueprintable)
class HSR_API UHSRFrontendModuleRootWidget : public UHSRScreenWidget
{
	GENERATED_BODY()

public:
	void PresentModule(EHSRFrontendModule InModule);
	bool SetModuleContent(UWidget* InContent);
	void ClearModuleContent();

	UFUNCTION(BlueprintCallable, Category = "HSR|UI|Frontend")
	bool RequestOpenModule(EHSRFrontendModule Module);

	UFUNCTION(BlueprintCallable, Category = "HSR|UI|Frontend")
	bool RequestCloseToRoot();

	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|UI|Frontend")
	void OnModuleChanged(EHSRFrontendModule Module);

	UFUNCTION(BlueprintPure, Category = "HSR|UI|Frontend")
	EHSRFrontendModule GetPresentedModule() const { return PresentedModule; }

private:
	UPROPERTY(meta = (BindWidgetOptional), Transient)
	TObjectPtr<class UPanelWidget> ModuleContentHost;

	UPROPERTY(Transient)
	EHSRFrontendModule PresentedModule = EHSRFrontendModule::None;
};
