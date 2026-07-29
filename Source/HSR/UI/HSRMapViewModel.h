#pragma once

#include "CoreMinimal.h"
#include "../Map/HSRMapTypes.h"
#include "HSRMapViewModel.generated.h"

class UHSRMapSubsystem;

UCLASS(BlueprintType)
class HSR_API UHSRMapViewModel : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;
	void Initialize(UHSRMapSubsystem* InMaps);
	void Shutdown();

	UFUNCTION(BlueprintPure, Category = "HSR|Map")
	bool GetSnapshot(FHSRMapRuntimeSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintCallable, Category = "HSR|Map")
	EHSRMapOperationResult RequestTeleport(FName TeleportId);

	FHSRMapStateChanged& OnChanged() { return Changed; }

	UPROPERTY(BlueprintAssignable, Category = "HSR|Map")
	FHSRMapStateChangedBlueprint OnSnapshotChanged;

private:
	void HandleMapStateChanged(const FHSRMapRuntimeSnapshot& InSnapshot);

	TWeakObjectPtr<UHSRMapSubsystem> Maps;
	FDelegateHandle MapStateChangedHandle;
	FHSRMapRuntimeSnapshot Snapshot;
	bool bHasSnapshot = false;
	FHSRMapStateChanged Changed;
};
