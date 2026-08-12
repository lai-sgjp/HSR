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

	UFUNCTION(BlueprintPure, Category = "HSR|Map")
	FText GetMapDisplayName(FName MapId) const;

	UFUNCTION(BlueprintPure, Category = "HSR|Map")
	void GetAvailableTeleports(TArray<FHSRTeleportProjection>& OutTeleports) const;

	UFUNCTION(BlueprintPure, Category = "HSR|Map")
	int32 GetReachableTeleportCount() const;

	UFUNCTION(BlueprintPure, Category = "HSR|Map")
	bool GetReachableTeleport(int32 Index, FHSRTeleportProjection& OutTeleport) const;

	/** Refreshes named teleport buttons (TXT_TeleportAB/BTN_TeleportAB, TXT_TeleportBA/BTN_TeleportBA)
	 *  from the two reachable teleports. Call from OnMapSnapshotChanged after the widget tree is built. */
	UFUNCTION(BlueprintCallable, Category = "HSR|Map")
	void RefreshReachableTeleportPanel();

	/** Requests the reachable teleport at the given index (0 or 1). */
	UFUNCTION(BlueprintCallable, Category = "HSR|Map")
	EHSRMapOperationResult RequestReachableTeleport(int32 Index);

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
