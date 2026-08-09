#pragma once

#include "CoreMinimal.h"
#include "HSRScreenWidget.h"
#include "../Map/HSRMapTypes.h"
#include "HSRMapWidget.generated.h"

class UHSRMapViewModel;

UCLASS(Blueprintable)
class HSR_API UHSRMapWidget : public UHSRScreenWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HSR|Map")
	void SetViewModel(UHSRMapViewModel* InViewModel);

	UFUNCTION(BlueprintPure, Category = "HSR|Map")
	bool GetCurrentSnapshot(FHSRMapRuntimeSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintCallable, Category = "HSR|Map")
	EHSRMapOperationResult RequestTeleport(FName TeleportId);

	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|Map")
	void OnMapSnapshotChanged(const FHSRMapRuntimeSnapshot& Snapshot);

#if WITH_DEV_AUTOMATION_TESTS
	void AttachForAutomation() { BindAndRefresh(); }
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BindAndRefresh();
	void Unbind();
	void HandleSnapshot(const FHSRMapRuntimeSnapshot& InSnapshot);

	UPROPERTY(Transient)
	TObjectPtr<UHSRMapViewModel> ViewModel;
	FDelegateHandle Subscription;
	FHSRMapRuntimeSnapshot Current;
	bool bHasSnapshot = false;
	bool bOwnsViewModel = false;
};
