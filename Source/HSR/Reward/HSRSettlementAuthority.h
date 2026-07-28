#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSRSettlementTypes.h"
#include "HSRSettlementAuthority.generated.h"

class UHSRCharacterProfileSubsystem;
class UHSRInventorySubsystem;
class UHSRRewardSubsystem;

UCLASS()
class HSR_API UHSRSettlementAuthority : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	EHSRSettlementResult PrepareSettlement(const FHSRSettlementRequest& Request,
		FHSRSettlementCandidate& OutCandidate, FHSRSettlementReceipt& OutExistingReceipt) const;
	EHSRSettlementResult SubmitSettlement(const FHSRSettlementRequest& Request, FHSRSettlementReceipt& OutReceipt);

#if WITH_DEV_AUTOMATION_TESTS
	void SetPrepareFailureForAutomation(EHSRSettlementPrepareFailurePoint Point) { PrepareFailurePoint = Point; }
	FHSRSettlementAutomationSnapshot GetAutomationSnapshot() const { return AutomationSnapshot; }
#endif

private:
	TWeakObjectPtr<UHSRInventorySubsystem> Inventory;
	TWeakObjectPtr<UHSRCharacterProfileSubsystem> Profiles;
	TWeakObjectPtr<UHSRRewardSubsystem> Reward;

#if WITH_DEV_AUTOMATION_TESTS
	EHSRSettlementPrepareFailurePoint PrepareFailurePoint = EHSRSettlementPrepareFailurePoint::None;
	FHSRSettlementAutomationSnapshot AutomationSnapshot;
#endif
};
