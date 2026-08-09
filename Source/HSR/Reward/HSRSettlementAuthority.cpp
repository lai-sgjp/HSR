#include "HSRSettlementAuthority.h"

#include "HSRRewardSubsystem.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"

void UHSRSettlementAuthority::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Inventory = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRInventorySubsystem>() : nullptr;
	Profiles = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
	Reward = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRRewardSubsystem>() : nullptr;
}

EHSRSettlementResult UHSRSettlementAuthority::PrepareSettlement(const FHSRSettlementRequest& Request,
	FHSRSettlementCandidate& OutCandidate, FHSRSettlementReceipt& OutExistingReceipt) const
{
	if (!Request.TransactionId.IsValid())
	{
		return EHSRSettlementResult::InvalidTransactionId;
	}
	if (Request.RewardDefinitionId.IsNone() || Request.PlayerCharacterId.IsNone() || Request.Experience < 0
		|| Request.ExpectedInventoryRevision < 0 || Request.ExpectedProfileRevision < 0 || Request.ExpectedRewardRevision < 0)
	{
		return EHSRSettlementResult::InvalidRequest;
	}
	UHSRInventorySubsystem* InventorySubsystem = Inventory.Get();
	UHSRCharacterProfileSubsystem* ProfileSubsystem = Profiles.Get();
	UHSRRewardSubsystem* RewardSubsystem = Reward.Get();
	if (!InventorySubsystem || !ProfileSubsystem || !RewardSubsystem)
	{
		return EHSRSettlementResult::MissingAuthority;
	}

	FHSRSettlementCandidate Candidate;
	Candidate.Request = Request;
	TArray<FHSRInventoryGrant> Grants;
	const EHSRRewardOperationResult RewardResult = RewardSubsystem->PrepareSettlementCandidate(
		Request, Candidate.Reward, Candidate.Receipt, Grants, OutExistingReceipt);
	if (RewardResult == EHSRRewardOperationResult::NoOp)
	{
		return EHSRSettlementResult::NoOp;
	}
	if (RewardResult == EHSRRewardOperationResult::ClaimConflict)
	{
		return EHSRSettlementResult::TransactionConflict;
	}
	if (RewardResult != EHSRRewardOperationResult::Success)
	{
		if (RewardResult == EHSRRewardOperationResult::RevisionConflict)
		{
			return EHSRSettlementResult::StaleRevision;
		}
		return RewardResult == EHSRRewardOperationResult::InjectedFailure
			? EHSRSettlementResult::InjectedPrepareFailure : EHSRSettlementResult::RewardRejected;
	}
#if WITH_DEV_AUTOMATION_TESTS
	if (PrepareFailurePoint == EHSRSettlementPrepareFailurePoint::AfterReward)
	{
		return EHSRSettlementResult::InjectedPrepareFailure;
	}
#endif

	const EHSRInventoryOperationResult InventoryResult = InventorySubsystem->PrepareSettlementCandidate(
		Request.TransactionId, Grants, Request.ExpectedInventoryRevision, Candidate.Inventory);
	if (InventoryResult != EHSRInventoryOperationResult::Success)
	{
		return InventoryResult == EHSRInventoryOperationResult::RevisionConflict
			? EHSRSettlementResult::StaleRevision : EHSRSettlementResult::InventoryRejected;
	}
#if WITH_DEV_AUTOMATION_TESTS
	if (PrepareFailurePoint == EHSRSettlementPrepareFailurePoint::AfterInventory)
	{
		return EHSRSettlementResult::InjectedPrepareFailure;
	}
#endif

	const EHSRCharacterProfileResult ProfileResult = ProfileSubsystem->PrepareSettlementCandidate(
		Request.TransactionId, Request.PlayerCharacterId, Request.Experience,
		Request.ExpectedProfileRevision, Candidate.Profile);
	if (ProfileResult != EHSRCharacterProfileResult::Success)
	{
		return ProfileResult == EHSRCharacterProfileResult::RevisionConflict
			? EHSRSettlementResult::StaleRevision : EHSRSettlementResult::ProfileRejected;
	}
#if WITH_DEV_AUTOMATION_TESTS
	if (PrepareFailurePoint == EHSRSettlementPrepareFailurePoint::AfterProfile)
	{
		return EHSRSettlementResult::InjectedPrepareFailure;
	}
#endif

	Candidate.Receipt.InventoryRevision = Candidate.Inventory.NextRevision;
	Candidate.Receipt.ProfileRevision = Candidate.Profile.NextRevision;
	Candidate.Receipt.RewardRevision = Candidate.Reward.NextRevision;
	FHSRSettlementReceipt* PreparedLedgerReceipt = Candidate.Reward.SettlementLedger.Find(Request.TransactionId);
	if (!PreparedLedgerReceipt)
	{
		return EHSRSettlementResult::RewardRejected;
	}
	*PreparedLedgerReceipt = Candidate.Receipt;
#if WITH_DEV_AUTOMATION_TESTS
	switch (CandidateMismatchDomain)
	{
	case EHSRSettlementCandidateMismatchDomain::Inventory: Candidate.Inventory.TransactionId.Invalidate(); break;
	case EHSRSettlementCandidateMismatchDomain::Profile: Candidate.Profile.TransactionId.Invalidate(); break;
	case EHSRSettlementCandidateMismatchDomain::Reward: Candidate.Reward.TransactionId.Invalidate(); break;
	default: break;
	}
#endif
	if (Candidate.Inventory.TransactionId != Request.TransactionId
		|| Candidate.Profile.TransactionId != Request.TransactionId
		|| Candidate.Reward.TransactionId != Request.TransactionId
		|| Candidate.Receipt.TransactionId != Request.TransactionId
		|| PreparedLedgerReceipt->TransactionId != Request.TransactionId)
	{
		return EHSRSettlementResult::CandidateMismatch;
	}
	OutCandidate = MoveTemp(Candidate);
	return EHSRSettlementResult::Success;
}

EHSRSettlementResult UHSRSettlementAuthority::SubmitSettlement(
	const FHSRSettlementRequest& Request, FHSRSettlementReceipt& OutReceipt)
{
	FHSRSettlementCandidate Candidate;
	FHSRSettlementReceipt ExistingReceipt;
	const EHSRSettlementResult Preparation = PrepareSettlement(Request, Candidate, ExistingReceipt);
	if (Preparation == EHSRSettlementResult::NoOp)
	{
		OutReceipt = ExistingReceipt;
		return Preparation;
	}
	if (Preparation != EHSRSettlementResult::Success)
	{
		return Preparation;
	}

	Inventory->InstallSettlementCandidateNoFail(MoveTemp(Candidate.Inventory));
	Profiles->InstallSettlementCandidateNoFail(MoveTemp(Candidate.Profile));
	Reward->InstallSettlementCandidateNoFail(MoveTemp(Candidate.Reward));
#if WITH_DEV_AUTOMATION_TESTS
	++AutomationSnapshot.AggregateInstallCount;
#endif

	Inventory->FinalizeSettlementRevisionNoFail(Candidate.Receipt.InventoryRevision);
	Reward->FinalizeSettlementRevisionNoFail(Candidate.Receipt.RewardRevision);

	Inventory->PublishSettlementCommit(Candidate.Receipt.InventoryRevision);
	if (Candidate.Receipt.ProfileRevision != Request.ExpectedProfileRevision)
	{
		Profiles->PublishSettlementCommit(Request.PlayerCharacterId, Candidate.Receipt.ProfileRevision);
	}
	Reward->PublishSettlementCommit(Candidate.Receipt.RewardReceipt, Candidate.Receipt.RewardRevision);
#if WITH_DEV_AUTOMATION_TESTS
	++AutomationSnapshot.PublicationCount;
#endif
	OutReceipt = Candidate.Receipt;
	return EHSRSettlementResult::Success;
}
