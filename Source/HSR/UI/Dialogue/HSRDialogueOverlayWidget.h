#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../../UI/HSRScreenStackTypes.h"
#include "HSRDialoguePresentationTypes.h"
#include "HSRDialogueOverlayWidget.generated.h"

class UHSRDialoguePresentationViewModel;
class UHSRUIManagerSubsystem;
class UWidget;

UCLASS(Blueprintable)
class HSR_API UHSRDialogueOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UHSRDialogueOverlayWidget(const FObjectInitializer& ObjectInitializer);

	void SetOwningUIManager(UHSRUIManagerSubsystem* InManager);
	void SetViewModel(UHSRDialoguePresentationViewModel* InViewModel);

	UFUNCTION(BlueprintPure, Category = "HSR|Dialogue")
	bool GetSnapshot(UPARAM(ref) FHSRDialoguePresentationSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintPure, Category = "HSR|Dialogue")
	int32 GetChoiceCount() const;

	UFUNCTION(BlueprintPure, Category = "HSR|Dialogue")
	bool GetChoiceAt(int32 Index, UPARAM(ref) FHSRDialoguePresentationChoice& OutChoice) const;

	UFUNCTION(BlueprintPure, Category = "HSR|Dialogue")
	UHSRDialoguePresentationViewModel* GetViewModel() const { return ViewModel; }

	UFUNCTION(BlueprintPure, Category = "HSR|Dialogue")
	FText GetSpeakerText() const;

	UFUNCTION(BlueprintPure, Category = "HSR|Dialogue")
	FText GetBodyText() const;

	UFUNCTION(BlueprintPure, Category = "HSR|Dialogue")
	bool GetChoiceDisplayText(int32 Index, FText& OutText) const;

	UFUNCTION(BlueprintPure, Category = "HSR|Dialogue")
	bool GetChoiceEnabled(int32 Index, bool& bOutEnabled) const;

	UFUNCTION(BlueprintCallable, Category = "HSR|Dialogue")
	EHSRDialoguePresentationResult SubmitChoiceByIndex(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "HSR|Dialogue")
	EHSRUIScreenResult RequestCloseDialogue();

	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|Dialogue")
	void OnDialogueSnapshotChanged(const FHSRDialoguePresentationSnapshot& Snapshot);

	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|Dialogue")
	UWidget* GetPreferredFocusWidget() const;

#if WITH_DEV_AUTOMATION_TESTS
	bool ShouldConsumeBackKeyForAutomation(const FKey& Key) const;
	bool ShouldConsumeCloseToRootKeyForAutomation(const FKey& Key) const;
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION()
	void HandleViewModelSnapshotChanged(const FHSRDialoguePresentationSnapshot& Snapshot);

	void PopulateView();
	class UButton* FindButtonByName(const FName Name) const;
	class UTextBlock* FindTextByName(const FName Name) const;

	static bool IsBackKey(const FKey& Key);
	static bool IsCloseToRootKey(const FKey& Key);

	UPROPERTY(Transient)
	TObjectPtr<UHSRDialoguePresentationViewModel> ViewModel;

	UPROPERTY(Transient)
	TWeakObjectPtr<UHSRUIManagerSubsystem> OwningUIManager;
};
