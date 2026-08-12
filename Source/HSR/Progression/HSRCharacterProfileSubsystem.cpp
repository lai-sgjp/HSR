#include "HSRCharacterProfileSubsystem.h"
#include "HSRCharacterProgressionLibrary.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/Definitions/HSRCharacterCatalog.h"
#include "HSRCharacterStatAggregator.h"
#include "../Reward/HSRSettlementTypes.h"

// 注册单个角色定义（委托给批量注册）。
EHSRCharacterProfileResult UHSRCharacterProfileSubsystem::RegisterDefinition(const UHSRCharacterDefinition* Definition)
{
	TArray<const UHSRCharacterDefinition*> One{Definition};
	return RegisterDefinitions(One);
}

// RegisterLoadedCatalog：注册一个角色目录（DataAsset），目录里每个条目都必须
// 能解析出 CDO、且其经验曲线能同步加载。任一环节失败都会中止整个目录注册——
// 宁可全部失败，也不让半截目录进入运行时。
EHSRCharacterProfileResult UHSRCharacterProfileSubsystem::RegisterLoadedCatalog(const UHSRCharacterCatalog* Catalog)
{
	if (!Catalog)
	{
		return EHSRCharacterProfileResult::CatalogNotLoaded;
	}

	TArray<const UHSRCharacterDefinition*> Loaded;
	for (const TSubclassOf<UHSRCharacterDefinition>& Entry : Catalog->Characters)
	{
		const UHSRCharacterDefinition* Definition = Entry ? Entry->GetDefaultObject<UHSRCharacterDefinition>() : nullptr;
		if (!Definition)
		{
			UE_LOG(LogTemp, Error, TEXT("P11-002 CatalogRegister FAILED Reason=DefinitionCDO"));
			return EHSRCharacterProfileResult::AssetLoadFailed;
		}
		if (Definition->CumulativeExperienceCurve.IsNull() || !Definition->CumulativeExperienceCurve.LoadSynchronous())
		{
			UE_LOG(LogTemp, Error, TEXT("P11-002 CatalogRegister FAILED Reason=ExperienceCurve CharacterId=%s"),
				*Definition->CharacterId.ToString());
			return EHSRCharacterProfileResult::ExperienceCurveLoadFailed;
		}
		Loaded.Add(Definition);
	}
	return RegisterDefinitions(Loaded);
}

// RegisterDefinitions：批量注册角色定义，并为每个角色生成初始快照（0 经验、1 级）。
// 采用「候选副本 + 整体替换」：先在副本上做全部校验与累积，全部成功后才一次性提交到
// Definitions / Profiles。这样批次中途失败不会留下部分注册的脏状态。
EHSRCharacterProfileResult UHSRCharacterProfileSubsystem::RegisterDefinitions(const TArray<const UHSRCharacterDefinition*>& InDefinitions)
{
	TMap<FName, TObjectPtr<const UHSRCharacterDefinition>> CandidateDefinitions = Definitions;
	TMap<FName, FHSRCharacterProfileSnapshot> CandidateProfiles = Profiles;
	TSet<FName> BatchIds;

	for (const UHSRCharacterDefinition* Definition : InDefinitions)
	{
		if (!Definition || Definition->CharacterId.IsNone())
		{
			return EHSRCharacterProfileResult::ProgressionRejected;
		}
		if (CandidateDefinitions.Contains(Definition->CharacterId) || BatchIds.Contains(Definition->CharacterId))
		{
			return EHSRCharacterProfileResult::DefinitionAlreadyRegistered;
		}

		// 初始状态：CharacterId 已知，其余字段为默认值（等级 1、经验 0）。
		FHSRCharacterProfileSnapshot Candidate;
		Candidate.RuntimeState.CharacterId = Definition->CharacterId;
		if (UHSRCharacterProgressionLibrary::ValidateRuntimeState(Definition, Candidate.RuntimeState) != EHSRCharacterProgressionResult::Success)
		{
			return EHSRCharacterProfileResult::ProgressionRejected;
		}

		// 校验初始状态的派生属性能否正常构建（曲线/数值合法）。
		FHSRCharacterProgressionContext InitialContext;
		if (!UHSRCharacterStatAggregator::BuildContext(Definition, Candidate.RuntimeState, 0, InitialContext))
		{
			return EHSRCharacterProfileResult::ProgressionRejected;
		}

		BatchIds.Add(Definition->CharacterId);
		CandidateDefinitions.Add(Definition->CharacterId, Definition);
		CandidateProfiles.Add(Definition->CharacterId, Candidate);
	}

	Definitions = MoveTemp(CandidateDefinitions);
	Profiles = MoveTemp(CandidateProfiles);
	return EHSRCharacterProfileResult::Success;
}

// GrantExperience：给角色加经验。经验为 0 时直接成功（no-op），不加版本号、不发广播。
// 真正改变状态时才递增 RuntimeRevision 并广播 ProfileChanged，保证 UI/战斗能感知刷新。
EHSRCharacterProfileResult UHSRCharacterProfileSubsystem::GrantExperience(FName CharacterId, int32 ExperienceToGrant)
{
	FHSRCharacterProfileSnapshot* Profile = Profiles.Find(CharacterId);
	const TObjectPtr<const UHSRCharacterDefinition>* Definition = Definitions.Find(CharacterId);
	if (!Profile || !Definition)
	{
		return EHSRCharacterProfileResult::ProfileNotFound;
	}

	FHSRCharacterRuntimeState Candidate = Profile->RuntimeState;
	if (UHSRCharacterProgressionLibrary::TryGrantExperience(Definition->Get(), ExperienceToGrant, Candidate) != EHSRCharacterProgressionResult::Success)
	{
		return EHSRCharacterProfileResult::ProgressionRejected;
	}
	if (ExperienceToGrant == 0)
	{
		return EHSRCharacterProfileResult::Success;
	}

	Profile->RuntimeState = MoveTemp(Candidate);
	++Profile->RuntimeRevision;
	ProfileChanged.Broadcast(CharacterId, Profile->RuntimeRevision);
	return EHSRCharacterProfileResult::Success;
}

// SetSkillLevel：设置角色某技能的等级。目标等级与当前一致且技能 ID 合法时视为 no-op；
// 否则走 TrySetSkillLevel 校验后提交，同样递增版本号并广播。
EHSRCharacterProfileResult UHSRCharacterProfileSubsystem::SetSkillLevel(FName CharacterId, FName SkillId, int32 SkillLevel)
{
	FHSRCharacterProfileSnapshot* Profile = Profiles.Find(CharacterId);
	const TObjectPtr<const UHSRCharacterDefinition>* Definition = Definitions.Find(CharacterId);
	if (!Profile || !Definition)
	{
		return EHSRCharacterProfileResult::ProfileNotFound;
	}
	if (Profile->RuntimeState.SkillLevels.FindRef(SkillId) == SkillLevel && !SkillId.IsNone())
	{
		return EHSRCharacterProfileResult::Success;
	}

	FHSRCharacterRuntimeState Candidate = Profile->RuntimeState;
	if (UHSRCharacterProgressionLibrary::TrySetSkillLevel(Definition->Get(), SkillId, SkillLevel, Candidate) != EHSRCharacterProgressionResult::Success)
	{
		return EHSRCharacterProfileResult::ProgressionRejected;
	}

	Profile->RuntimeState = MoveTemp(Candidate);
	++Profile->RuntimeRevision;
	ProfileChanged.Broadcast(CharacterId, Profile->RuntimeRevision);
	return EHSRCharacterProfileResult::Success;
}

// GetProfileSnapshot：取角色的只读快照（返回副本，防止外部改动内部状态）。
bool UHSRCharacterProfileSubsystem::GetProfileSnapshot(FName CharacterId, FHSRCharacterProfileSnapshot& OutSnapshot) const
{
	const FHSRCharacterProfileSnapshot* Found = Profiles.Find(CharacterId);
	if (!Found)
	{
		return false;
	}
	OutSnapshot = *Found;
	return true;
}

// GetAllProfileSnapshots：导出全部角色快照，按 CharacterId 字典序排序，保证输出稳定。
bool UHSRCharacterProfileSubsystem::GetAllProfileSnapshots(TArray<FHSRCharacterProfileSnapshot>& OutSnapshots) const
{
	Profiles.GenerateValueArray(OutSnapshots);
	OutSnapshots.Sort([](const FHSRCharacterProfileSnapshot& A, const FHSRCharacterProfileSnapshot& B)
	{
		return A.RuntimeState.CharacterId.LexicalLess(B.RuntimeState.CharacterId);
	});
	return true;
}

// GetProgressionContext：构建角色当前成长上下文（派生属性），供存档/战斗读取。
bool UHSRCharacterProfileSubsystem::GetProgressionContext(FName CharacterId, FHSRCharacterProgressionContext& OutContext) const
{
	const FHSRCharacterProfileSnapshot* Profile = Profiles.Find(CharacterId);
	const TObjectPtr<const UHSRCharacterDefinition>* Definition = Definitions.Find(CharacterId);
	return Profile && Definition
		&& UHSRCharacterStatAggregator::BuildContext(Definition->Get(), Profile->RuntimeState, Profile->RuntimeRevision, OutContext);
}

// GetDefinition：取出角色定义指针（写回 OutDefinition 并返回是否成功）。
bool UHSRCharacterProfileSubsystem::GetDefinition(FName CharacterId, const UHSRCharacterDefinition*& OutDefinition) const
{
	const TObjectPtr<const UHSRCharacterDefinition>* Found = Definitions.Find(CharacterId);
	OutDefinition = Found ? Found->Get() : nullptr;
	return OutDefinition != nullptr;
}

// ExportProfiles：导出全部角色快照供存档快照使用，按 CharacterId 字典序排序
// （与 GetAllProfileSnapshots 相同的稳定排序，保证存档二进制编码的一致性）。
void UHSRCharacterProfileSubsystem::ExportProfiles(TArray<FHSRCharacterProfileSnapshot>& OutProfiles) const
{
	Profiles.GenerateValueArray(OutProfiles);
	OutProfiles.Sort([](const auto& A, const auto& B)
	{
		return A.RuntimeState.CharacterId.LexicalLess(B.RuntimeState.CharacterId);
	});
}

// PrepareRestore：为一次存档恢复做「干跑」——校验存档里的角色快照集合与当前运行时
// 注册的角色集合数量一致、每个角色定义存在、版本号合法、运行时状态合法，并且没有
// 重复角色。全部通过才生成候选恢复表，供 LoadSnapshot 阶段真正提交。
bool UHSRCharacterProfileSubsystem::PrepareRestore(const TArray<FHSRCharacterProfileSnapshot>& Saved, TMap<FName, FHSRCharacterProfileSnapshot>& Out) const
{
	if (Saved.Num() != Profiles.Num())
	{
		return false;
	}

	TMap<FName, FHSRCharacterProfileSnapshot> Candidate;
	for (const auto& P : Saved)
	{
		const auto* D = Definitions.Find(P.RuntimeState.CharacterId);
		if (!D || Candidate.Contains(P.RuntimeState.CharacterId) || P.RuntimeRevision < 0
			|| UHSRCharacterProgressionLibrary::ValidateRuntimeState(D->Get(), P.RuntimeState) != EHSRCharacterProgressionResult::Success)
		{
			return false;
		}
		Candidate.Add(P.RuntimeState.CharacterId, P);
	}
	Out = MoveTemp(Candidate);
	return true;
}

// NotifyRestored：恢复完成后，对每个变更过的角色重新广播 ProfileChanged，
// 让 UI/战斗侧刷新为存档里的最新版本号。
void UHSRCharacterProfileSubsystem::NotifyRestored(const TArray<FName>& Ids)
{
	for (FName Id : Ids)
	{
		if (const auto* P = Profiles.Find(Id))
		{
			ProfileChanged.Broadcast(Id, P->RuntimeRevision);
		}
	}
}

// PrepareSettlementCandidate：为结算（Settlement）准备角色经验变更的候选状态。
// 要求调用方传入期望的运行时版本号，与实际版本不符则拒绝（并发安全）。
// 候选里保存整份 Profiles 快照，经验大于 0 时才递增版本号（与 GrantExperience 对齐）。
EHSRCharacterProfileResult UHSRCharacterProfileSubsystem::PrepareSettlementCandidate(const FGuid& TransactionId,
	FName CharacterId, int32 Experience, int64 ExpectedRevision, FHSRProfileSettlementCandidate& OutCandidate) const
{
	const FHSRCharacterProfileSnapshot* Profile = Profiles.Find(CharacterId);
	const TObjectPtr<const UHSRCharacterDefinition>* Definition = Definitions.Find(CharacterId);
	if (!Profile || !Definition)
	{
		return EHSRCharacterProfileResult::ProfileNotFound;
	}
	if (Profile->RuntimeRevision != ExpectedRevision)
	{
		return EHSRCharacterProfileResult::RevisionConflict;
	}

	FHSRCharacterProfileSnapshot Updated = *Profile;
	if (UHSRCharacterProgressionLibrary::TryGrantExperience(Definition->Get(), Experience,
		Updated.RuntimeState) != EHSRCharacterProgressionResult::Success)
	{
		return EHSRCharacterProfileResult::ProgressionRejected;
	}

	FHSRProfileSettlementCandidate Candidate;
	Candidate.TransactionId = TransactionId;
	Candidate.CharacterId = CharacterId;
	Candidate.Profiles = Profiles;
	Updated.RuntimeRevision = Profile->RuntimeRevision + (Experience > 0 ? 1 : 0);
	Candidate.Profiles.Add(CharacterId, Updated);
	Candidate.NextRevision = Updated.RuntimeRevision;
	OutCandidate = MoveTemp(Candidate);
	return EHSRCharacterProfileResult::Success;
}

// InstallSettlementCandidateNoFail：结算提交阶段，把候选的整份 Profiles 直接安装。
void UHSRCharacterProfileSubsystem::InstallSettlementCandidateNoFail(FHSRProfileSettlementCandidate&& Candidate)
{
	Profiles = MoveTemp(Candidate.Profiles);
}

// PublishSettlementCommit：结算提交后广播角色变更（版本号即结算后的新版本）。
void UHSRCharacterProfileSubsystem::PublishSettlementCommit(FName CharacterId, int64 PreparedRevision)
{
	ProfileChanged.Broadcast(CharacterId, PreparedRevision);
}
