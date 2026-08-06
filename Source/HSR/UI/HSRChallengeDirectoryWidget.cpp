#include "HSRChallengeDirectoryWidget.h"

#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "Engine/GameInstance.h"

void UHSRChallengeDirectoryWidget::UnbindProgression()
{
	if (BoundProgression.IsValid() && ProgressionChangedHandle.IsValid())
	{
		BoundProgression->OnProgressionChanged().Remove(ProgressionChangedHandle);
	}
	BoundProgression.Reset();
	ProgressionChangedHandle.Reset();
}

void UHSRChallengeDirectoryWidget::NativeDestruct()
{
	UnbindProgression();
	Super::NativeDestruct();
}

void UHSRChallengeDirectoryWidget::HandleProgressionChanged(const FHSRChallengeProgressionSnapshot&)
{
	if (ViewModel)
	{
		ViewModel->Refresh();
		OnDirectoryChanged(ViewModel->GetSnapshot());
	}
}

EHSRChallengeDirectoryResult UHSRChallengeDirectoryWidget::InitializeDirectory(
	const TArray<FHSRChallengeDirectorySource>& Sources)
{
	UnbindProgression();
	if (!ViewModel)
	{
		ViewModel = NewObject<UHSRChallengeDirectoryViewModel>(this);
	}
	UHSRChallengeProgressionSubsystem* Progression = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UHSRChallengeProgressionSubsystem>() : nullptr;
	const EHSRChallengeDirectoryResult Result = ViewModel->Initialize(Sources, Progression);
	if (Progression)
	{
		BoundProgression = Progression;
		ProgressionChangedHandle = Progression->OnProgressionChanged().AddUObject(
			this, &UHSRChallengeDirectoryWidget::HandleProgressionChanged);
	}
	SelectedEncounterId = NAME_None;
	OnDirectoryChanged(ViewModel->GetSnapshot());
	return Result;
}

EHSRChallengeDirectoryResult UHSRChallengeDirectoryWidget::InitializeConfiguredDirectory()
{
	const EHSRChallengeDirectoryResult Result = InitializeDirectory(ChallengeSources);
	UE_LOG(LogTemp, Log, TEXT("HSR ChallengeDirectory initialized Sources=%d Entries=%d Result=%d"),
		ChallengeSources.Num(), ViewModel ? ViewModel->GetSnapshot().Entries.Num() : 0, static_cast<int32>(Result));
	return Result;
}

FHSRChallengeDirectorySnapshot UHSRChallengeDirectoryWidget::GetDirectorySnapshot() const
{
	return ViewModel ? ViewModel->GetSnapshot() : FHSRChallengeDirectorySnapshot();
}

EHSRChallengeDirectoryResult UHSRChallengeDirectoryWidget::SelectChallenge(const FName EncounterId)
{
	if (!ViewModel)
	{
		return EHSRChallengeDirectoryResult::EmptyDirectory;
	}

	UHSREncounterDefinition* Definition = nullptr;
	const EHSRChallengeDirectoryResult Result = ViewModel->ResolveSelection(EncounterId, Definition);
	if (Result == EHSRChallengeDirectoryResult::Success)
	{
		SelectedEncounterId = EncounterId;
	}
	return Result;
}

FHSREncounterResult UHSRChallengeDirectoryWidget::BuildChallengeTemplate(
	const FName EncounterId, const EHSREncounterInitiative Initiative, FHSREncounterRequest& OutTemplate)
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
	if (!Transition)
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidRequest,
			FText::FromString(TEXT("Battle transition is unavailable.")));
	}

	FHSREncounterResult Result = Transition->BuildPreBattleEncounterTemplate(Definition, Initiative, OutTemplate);
	if (Result.ResultType == EHSREncounterResultType::Success)
	{
		SelectedEncounterId = EncounterId;
	}
	return Result;
}
