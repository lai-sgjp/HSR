#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSRChallengeDirectoryViewModel.h"
#include "../Battle/HSREncounterTypes.h"
#include "HSRChallengeDirectoryWidget.generated.h"

UCLASS(Blueprintable)
class HSR_API UHSRChallengeDirectoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Encounter definitions supplied by the owning frontend instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HSR|Challenge", meta = (ExposeOnSpawn = "true"))
	TArray<FHSRChallengeDirectorySource> ChallengeSources;

	UFUNCTION(BlueprintCallable, Category = "HSR|Challenge")
	EHSRChallengeDirectoryResult InitializeDirectory(const TArray<FHSRChallengeDirectorySource>& Sources);

	UFUNCTION(BlueprintCallable, Category = "HSR|Challenge")
	EHSRChallengeDirectoryResult InitializeConfiguredDirectory();

	UFUNCTION(BlueprintPure, Category = "HSR|Challenge")
	FHSRChallengeDirectorySnapshot GetDirectorySnapshot() const;

	/** Resolves a directory entry and changes selection only on success. */
	UFUNCTION(BlueprintCallable, Category = "HSR|Challenge")
	EHSRChallengeDirectoryResult SelectChallenge(FName EncounterId);

	UFUNCTION(BlueprintPure, Category = "HSR|Challenge")
	FName GetSelectedEncounterId() const { return SelectedEncounterId; }

	UFUNCTION(BlueprintCallable, Category = "HSR|Challenge")
	FHSREncounterResult BuildChallengeTemplate(FName EncounterId, EHSREncounterInitiative Initiative,
		UPARAM(ref) FHSREncounterRequest& OutTemplate);

	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|Challenge")
	void OnDirectoryChanged(const FHSRChallengeDirectorySnapshot& Snapshot);

private:
	UPROPERTY(Transient)
	TObjectPtr<UHSRChallengeDirectoryViewModel> ViewModel;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "HSR|Challenge", meta = (AllowPrivateAccess = "true"))
	FName SelectedEncounterId;
};
