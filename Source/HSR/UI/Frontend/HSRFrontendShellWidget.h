#pragma once

#include "CoreMinimal.h"
#include "../HSRScreenWidget.h"
#include "HSRFrontendRouteTypes.h"
#include "HSRFrontendShellWidget.generated.h"

UCLASS(Blueprintable)
class HSR_API UHSRFrontendShellWidget : public UHSRScreenWidget
{
	GENERATED_BODY()

public:
	void PresentRoute(const FHSRFrontendRouteSnapshot& Snapshot);

	UFUNCTION(BlueprintCallable, Category = "HSR|UI|Frontend")
	bool RequestOpenModule(EHSRFrontendModule Module);

	UFUNCTION(BlueprintCallable, Category = "HSR|UI|Frontend")
	bool RequestCloseToRoot();

	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|UI|Frontend")
	void OnRouteChanged(const FHSRFrontendRouteSnapshot& Snapshot);
};
