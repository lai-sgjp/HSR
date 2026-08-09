#pragma once

#include "CoreMinimal.h"
#include "../../Dialogue/HSRDialogueTypes.h"
#include "HSRDialoguePresentationTypes.h"
#include "HSRDialoguePresentationViewModel.generated.h"

class UHSRDialogueSubsystem;

UCLASS(BlueprintType)
class HSR_API UHSRDialoguePresentationViewModel : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	void Initialize(UHSRDialogueSubsystem* InDialogue);
	void Shutdown();

	UFUNCTION(BlueprintPure, Category = "HSR|Dialogue")
	bool GetSnapshot(UPARAM(ref) FHSRDialoguePresentationSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintPure, Category = "HSR|Dialogue")
	EHSRDialoguePresentationResult GetLastResult() const { return LastResult; }

	UFUNCTION(BlueprintPure, Category = "HSR|Dialogue")
	EHSRQuestOperationResult GetLastAuthorityResult() const { return LastAuthorityResult; }

	UFUNCTION(BlueprintPure, Category = "HSR|Dialogue")
	FHSRDialogueChoiceResult GetLastChoiceResult() const { return LastChoiceResult; }

	UFUNCTION(BlueprintCallable, Category = "HSR|Dialogue")
	EHSRDialoguePresentationResult BeginDialogue(const FHSRDialoguePresentationRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "HSR|Dialogue")
	EHSRDialoguePresentationResult SubmitChoice(const FHSRDialoguePresentationChoiceRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "HSR|Dialogue")
	EHSRDialoguePresentationResult ExitDialogue(const FGuid& QueryId);

	FHSRDialoguePresentationChanged& OnChanged() { return Changed; }

	UPROPERTY(BlueprintAssignable, Category = "HSR|Dialogue")
	FHSRDialoguePresentationBlueprintChanged OnSnapshotChanged;

private:
	static EHSRDialoguePresentationResult MapNodeResult(EHSRQuestOperationResult AuthorityResult);
	static EHSRDialoguePresentationResult MapBranchResult(EHSRDialogueChoiceOperationResult BranchResult);
	static EHSRQuestOperationResult MapBranchAuthorityResult(const FHSRDialogueChoiceResult& Result);
	static FHSRDialoguePresentationSnapshot BuildSnapshot(const FGuid& QueryId,
		FName DialogueId, const FHSRDialogueNodeDefinition& Node,
		EHSRQuestOperationResult AuthorityResult);

	bool IsActiveRequest(const FHSRDialoguePresentationRequest& Request) const;
	bool IsActiveRequest(const FHSRDialoguePresentationChoiceRequest& Request) const;
	EHSRDialoguePresentationResult Reject(EHSRDialoguePresentationResult Result,
		EHSRQuestOperationResult AuthorityResult);
	void Publish(FHSRDialoguePresentationSnapshot&& Next,
		EHSRDialoguePresentationResult Result, EHSRQuestOperationResult AuthorityResult);

	TWeakObjectPtr<UHSRDialogueSubsystem> Dialogue;
	FHSRDialoguePresentationSnapshot Snapshot;
	EHSRDialoguePresentationResult LastResult = EHSRDialoguePresentationResult::Unavailable;
	EHSRQuestOperationResult LastAuthorityResult = EHSRQuestOperationResult::NoOp;
	FHSRDialogueChoiceResult LastChoiceResult;
	FHSRDialoguePresentationChanged Changed;
};
