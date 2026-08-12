#include "HSRSettlementAuthority.h"

#include "HSRRewardSubsystem.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"

// 结算权威子系统初始化：持有三个下游子系统的引用（背包/角色成长/奖励）。
// 结算事务要同时改动这三个子系统，因此这里在启动时统一缓存它们的指针。
void UHSRSettlementAuthority::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Inventory = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRInventorySubsystem>() : nullptr;
	Profiles = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
	Reward = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRRewardSubsystem>() : nullptr;
}

// PrepareSettlement：为一次结算事务做「预演」——把背包、角色、奖励三个子系统的
// 候选状态全部准备好，但不写入任何运行时状态。三个候选都成功才算准备成功。
// 预演阶段还负责把各子系统算出的 NextRevision 写回统一收据（Receipt），并做最终
// 的「事务 ID 一致」校验，防止候选之间串号。
EHSRSettlementResult UHSRSettlementAuthority::PrepareSettlement(const FHSRSettlementRequest& Request,
	FHSRSettlementCandidate& OutCandidate, FHSRSettlementReceipt& OutExistingReceipt) const
{
	// 入参合法性校验。
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

	// 第一步：先让奖励子系统解析定义并产出候选（含掉落/固定物品、收据与结算台账条目）。
	// 注意 PrepareSettlementCandidate 会检查 TransactionId 是否已在台账中——若已存在且
	// 请求完全一致，返回 NoOp（幂等重放），此处直接短路返回。
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

	// 第二步：背包候选。奖励子系统解析出的 Grants 在这里被真正应用成背包的候选变更。
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

	// 第三步：角色成长候选（经验发放）。
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

	// 汇总收据：把三个子系统的 NextRevision 写进统一收据，并同步回奖励子系统的
	// 结算台账里那份预演收据（这样提交阶段能拿到一致的收据）。
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
	// 自动化测试注入：故意使某个候选的 TransactionId 失效，用来验证提交前的
	// 一致性校验确实能拦截串号候选。
	switch (CandidateMismatchDomain)
	{
	case EHSRSettlementCandidateMismatchDomain::Inventory:
		Candidate.Inventory.TransactionId.Invalidate();
		break;
	case EHSRSettlementCandidateMismatchDomain::Profile:
		Candidate.Profile.TransactionId.Invalidate();
		break;
	case EHSRSettlementCandidateMismatchDomain::Reward:
		Candidate.Reward.TransactionId.Invalidate();
		break;
	default:
		break;
	}
#endif
	// 最终一致性校验：所有候选与收据必须指向同一个 TransactionId。
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

// SubmitSettlement：结算事务的完整执行。先预演（PrepareSettlement），全部成功后按
// 「安装 → 定版 → 发布」三步提交：
//   - 安装：把三个候选一次性写入各子系统（NoFail，预演已保证必然成功）；
//   - 定版：背包与奖励的版本号推进到收据里的新值；角色成长在无经验时不发布；
//   - 发布：广播变更事件，让 UI/战斗侧感知到新状态。
EHSRSettlementResult UHSRSettlementAuthority::SubmitSettlement(
	const FHSRSettlementRequest& Request, FHSRSettlementReceipt& OutReceipt)
{
	FHSRSettlementCandidate Candidate;
	FHSRSettlementReceipt ExistingReceipt;
	const EHSRSettlementResult Preparation = PrepareSettlement(Request, Candidate, ExistingReceipt);
	if (Preparation == EHSRSettlementResult::NoOp)
	{
		// 幂等重放：该事务已提交过且请求一致，直接回放既有收据。
		OutReceipt = ExistingReceipt;
		return Preparation;
	}
	if (Preparation != EHSRSettlementResult::Success)
	{
		return Preparation;
	}

	// 安装阶段：整体替换三个子系统的状态，不触发各自的广播（发布统一放到后面）。
	Inventory->InstallSettlementCandidateNoFail(MoveTemp(Candidate.Inventory));
	Profiles->InstallSettlementCandidateNoFail(MoveTemp(Candidate.Profile));
	Reward->InstallSettlementCandidateNoFail(MoveTemp(Candidate.Reward));
#if WITH_DEV_AUTOMATION_TESTS
	++AutomationSnapshot.AggregateInstallCount;
#endif

	// 定版阶段：推进版本号。注意角色版本号已在候选里设置好，无需单独定版。
	Inventory->FinalizeSettlementRevisionNoFail(Candidate.Receipt.InventoryRevision);
	Reward->FinalizeSettlementRevisionNoFail(Candidate.Receipt.RewardRevision);

	// 发布阶段：背包与奖励总是广播；角色成长只有在版本号确实变化时才广播
	// （经验为 0 的结算不产生角色变更事件）。
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
