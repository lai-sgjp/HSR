#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HSRFrontendRouteTypes.h"
#include "HSRFrontendRouter.generated.h"

UCLASS()
class HSR_API UHSRFrontendRouter : public UObject
{
	GENERATED_BODY()

public:
	EHSRFrontendRouteResult Submit(const FHSRFrontendRouteRequest& Request);
	const FHSRFrontendRouteSnapshot& GetSnapshot() const { return Snapshot; }
	void RestoreSnapshotForTransaction(const FHSRFrontendRouteSnapshot& InSnapshot) { Snapshot = InSnapshot; }
	void Reset();

private:
	static bool IsValidModule(EHSRFrontendModule Module);
	static EHSRFrontendRouteResult Validate(const FHSRFrontendRouteRequest& Request);

	UPROPERTY(Transient)
	FHSRFrontendRouteSnapshot Snapshot;
};
