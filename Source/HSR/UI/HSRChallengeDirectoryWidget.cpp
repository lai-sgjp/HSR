#include "HSRChallengeDirectoryWidget.h"

#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "Engine/GameInstance.h"

EHSRChallengeDirectoryResult UHSRChallengeDirectoryWidget::InitializeDirectory(
	const TArray<FHSRChallengeDirectorySource>& Sources)
{
	if (!ViewModel)
	{
		ViewModel = NewObject<UHSRChallengeDirectoryViewModel>(this);
	}
	const EHSRChallengeDirectoryResult Result = ViewModel->Initialize(Sources);
	OnDirectoryChanged(ViewModel->GetSnapshot());
	return Result;
}

EHSRChallengeDirectoryResult UHSRChallengeDirectoryWidget::InitializeConfiguredDirectory()
{
	return InitializeDirectory(ChallengeSources);
}

FHSRChallengeDirectorySnapshot UHSRChallengeDirectoryWidget::GetDirectorySnapshot() const
{
	return ViewModel ? ViewModel->GetSnapshot() : FHSRChallengeDirectorySnapshot();
}

FHSREncounterResult UHSRChallengeDirectoryWidget::BuildChallengeTemplate(
	const FName EncounterId, const EHSREncounterInitiative Initiative, FHSREncounterRequest& OutTemplate) const
{
	UHSREncounterDefinition* Definition = nullptr;
	const EHSRChallengeDirectoryResult SelectionResult = ViewModel
		? ViewModel->ResolveSelection(EncounterId, Definition)
		: EHSRChallengeDirectoryResult::EmptyDirectory;
	if (SelectionResult != EHSRChallengeDirectoryResult::Success)
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidDefinition,
			FText::FromString(SelectionResult == EHSRChallengeDirectoryResult::Locked
				? TEXT("Challenge is locked.") : TEXT("Challenge is unavailable.")));
	}

	UHSRBattleTransitionSubsystem* Transition = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>() : nullptr;
	return Transition
		? Transition->BuildPreBattleEncounterTemplate(Definition, Initiative, OutTemplate)
		: FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidRequest,
			FText::FromString(TEXT("Battle transition is unavailable.")));
}
