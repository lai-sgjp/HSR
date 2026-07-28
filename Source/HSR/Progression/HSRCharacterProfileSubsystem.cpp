#include "HSRCharacterProfileSubsystem.h"
#include "HSRCharacterProgressionLibrary.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/Definitions/HSRCharacterCatalog.h"
#include "HSRCharacterStatAggregator.h"
#include "../Reward/HSRSettlementTypes.h"

EHSRCharacterProfileResult UHSRCharacterProfileSubsystem::RegisterDefinition(const UHSRCharacterDefinition* Definition)
{
	TArray<const UHSRCharacterDefinition*> One{Definition}; return RegisterDefinitions(One);
}

EHSRCharacterProfileResult UHSRCharacterProfileSubsystem::RegisterLoadedCatalog(const UHSRCharacterCatalog* Catalog)
{
	if (!Catalog) return EHSRCharacterProfileResult::CatalogNotLoaded;
	TArray<const UHSRCharacterDefinition*> Loaded;
	for (const TSubclassOf<UHSRCharacterDefinition>& Entry : Catalog->Characters)
	{
		const UHSRCharacterDefinition* Definition = Entry ? Entry->GetDefaultObject<UHSRCharacterDefinition>() : nullptr;
		if (!Definition) { UE_LOG(LogTemp, Error, TEXT("P11-002 CatalogRegister FAILED Reason=DefinitionCDO")); return EHSRCharacterProfileResult::AssetLoadFailed; }
		if (Definition->CumulativeExperienceCurve.IsNull() || !Definition->CumulativeExperienceCurve.LoadSynchronous())
		{
			UE_LOG(LogTemp, Error, TEXT("P11-002 CatalogRegister FAILED Reason=ExperienceCurve CharacterId=%s"), *Definition->CharacterId.ToString());
			return EHSRCharacterProfileResult::ExperienceCurveLoadFailed;
		}
		Loaded.Add(Definition);
	}
	return RegisterDefinitions(Loaded);
}

EHSRCharacterProfileResult UHSRCharacterProfileSubsystem::RegisterDefinitions(const TArray<const UHSRCharacterDefinition*>& InDefinitions)
{
	TMap<FName, TObjectPtr<const UHSRCharacterDefinition>> CandidateDefinitions = Definitions;
	TMap<FName, FHSRCharacterProfileSnapshot> CandidateProfiles = Profiles;
	TSet<FName> BatchIds;
	for (const UHSRCharacterDefinition* Definition : InDefinitions)
	{
		if (!Definition || Definition->CharacterId.IsNone()) return EHSRCharacterProfileResult::ProgressionRejected;
		if (CandidateDefinitions.Contains(Definition->CharacterId) || BatchIds.Contains(Definition->CharacterId)) return EHSRCharacterProfileResult::DefinitionAlreadyRegistered;
		FHSRCharacterProfileSnapshot Candidate; Candidate.RuntimeState.CharacterId = Definition->CharacterId;
		if (UHSRCharacterProgressionLibrary::ValidateRuntimeState(Definition, Candidate.RuntimeState) != EHSRCharacterProgressionResult::Success) return EHSRCharacterProfileResult::ProgressionRejected;
		FHSRCharacterProgressionContext InitialContext;
		if (!UHSRCharacterStatAggregator::BuildContext(Definition, Candidate.RuntimeState, 0, InitialContext)) return EHSRCharacterProfileResult::ProgressionRejected;
		BatchIds.Add(Definition->CharacterId); CandidateDefinitions.Add(Definition->CharacterId, Definition); CandidateProfiles.Add(Definition->CharacterId, Candidate);
	}
	Definitions = MoveTemp(CandidateDefinitions); Profiles = MoveTemp(CandidateProfiles); return EHSRCharacterProfileResult::Success;
}

EHSRCharacterProfileResult UHSRCharacterProfileSubsystem::GrantExperience(FName CharacterId, int32 ExperienceToGrant)
{
	FHSRCharacterProfileSnapshot* Profile = Profiles.Find(CharacterId);
	const TObjectPtr<const UHSRCharacterDefinition>* Definition = Definitions.Find(CharacterId);
	if (!Profile || !Definition) return EHSRCharacterProfileResult::ProfileNotFound;
	FHSRCharacterRuntimeState Candidate = Profile->RuntimeState;
	if (UHSRCharacterProgressionLibrary::TryGrantExperience(Definition->Get(), ExperienceToGrant, Candidate) != EHSRCharacterProgressionResult::Success)
		return EHSRCharacterProfileResult::ProgressionRejected;
	if (ExperienceToGrant == 0) return EHSRCharacterProfileResult::Success;
	Profile->RuntimeState = MoveTemp(Candidate);
	++Profile->RuntimeRevision;
	ProfileChanged.Broadcast(CharacterId, Profile->RuntimeRevision);
	return EHSRCharacterProfileResult::Success;
}

EHSRCharacterProfileResult UHSRCharacterProfileSubsystem::SetSkillLevel(FName CharacterId, FName SkillId, int32 SkillLevel)
{
	FHSRCharacterProfileSnapshot* Profile = Profiles.Find(CharacterId);
	const TObjectPtr<const UHSRCharacterDefinition>* Definition = Definitions.Find(CharacterId);
	if (!Profile || !Definition) return EHSRCharacterProfileResult::ProfileNotFound;
	if (Profile->RuntimeState.SkillLevels.FindRef(SkillId) == SkillLevel && !SkillId.IsNone()) return EHSRCharacterProfileResult::Success;
	FHSRCharacterRuntimeState Candidate = Profile->RuntimeState;
	if (UHSRCharacterProgressionLibrary::TrySetSkillLevel(Definition->Get(), SkillId, SkillLevel, Candidate) != EHSRCharacterProgressionResult::Success)
		return EHSRCharacterProfileResult::ProgressionRejected;
	Profile->RuntimeState = MoveTemp(Candidate);
	++Profile->RuntimeRevision;
	ProfileChanged.Broadcast(CharacterId, Profile->RuntimeRevision);
	return EHSRCharacterProfileResult::Success;
}

bool UHSRCharacterProfileSubsystem::GetProfileSnapshot(FName CharacterId, FHSRCharacterProfileSnapshot& OutSnapshot) const
{
	const FHSRCharacterProfileSnapshot* Found = Profiles.Find(CharacterId);
	if (!Found) return false;
	OutSnapshot = *Found;
	return true;
}

bool UHSRCharacterProfileSubsystem::GetProgressionContext(FName CharacterId, FHSRCharacterProgressionContext& OutContext) const
{
	const FHSRCharacterProfileSnapshot* Profile = Profiles.Find(CharacterId);
	const TObjectPtr<const UHSRCharacterDefinition>* Definition = Definitions.Find(CharacterId);
	return Profile && Definition && UHSRCharacterStatAggregator::BuildContext(Definition->Get(), Profile->RuntimeState, Profile->RuntimeRevision, OutContext);
}

bool UHSRCharacterProfileSubsystem::GetDefinition(FName CharacterId, const UHSRCharacterDefinition*& OutDefinition) const
{
	const TObjectPtr<const UHSRCharacterDefinition>* Found = Definitions.Find(CharacterId); OutDefinition = Found ? Found->Get() : nullptr; return OutDefinition != nullptr;
}

void UHSRCharacterProfileSubsystem::ExportProfiles(TArray<FHSRCharacterProfileSnapshot>& OutProfiles) const
{
	Profiles.GenerateValueArray(OutProfiles);
	OutProfiles.Sort([](const auto& A,const auto& B){ return A.RuntimeState.CharacterId.LexicalLess(B.RuntimeState.CharacterId); });
}

bool UHSRCharacterProfileSubsystem::PrepareRestore(const TArray<FHSRCharacterProfileSnapshot>& Saved, TMap<FName,FHSRCharacterProfileSnapshot>& Out) const
{
	if(Saved.Num()!=Profiles.Num()) return false; TMap<FName,FHSRCharacterProfileSnapshot> Candidate;
	for(const auto& P:Saved){ const auto* D=Definitions.Find(P.RuntimeState.CharacterId); if(!D||Candidate.Contains(P.RuntimeState.CharacterId)||P.RuntimeRevision<0||UHSRCharacterProgressionLibrary::ValidateRuntimeState(D->Get(),P.RuntimeState)!=EHSRCharacterProgressionResult::Success)return false; Candidate.Add(P.RuntimeState.CharacterId,P); }
	Out=MoveTemp(Candidate); return true;
}

void UHSRCharacterProfileSubsystem::NotifyRestored(const TArray<FName>& Ids){ for(FName Id:Ids){if(const auto* P=Profiles.Find(Id))ProfileChanged.Broadcast(Id,P->RuntimeRevision);} }

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

void UHSRCharacterProfileSubsystem::InstallSettlementCandidateNoFail(FHSRProfileSettlementCandidate&& Candidate)
{
	Profiles = MoveTemp(Candidate.Profiles);
}

void UHSRCharacterProfileSubsystem::PublishSettlementCommit(FName CharacterId, int64 PreparedRevision)
{
	ProfileChanged.Broadcast(CharacterId, PreparedRevision);
}
