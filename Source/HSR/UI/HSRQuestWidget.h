#pragma once

#include "CoreMinimal.h"
#include "HSRScreenWidget.h"
#include "HSRQuestViewModel.h"
#include "HSRQuestWidget.generated.h"

class UHSRQuestViewModel;

UCLASS(Blueprintable)
class HSR_API UHSRQuestWidget : public UHSRScreenWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HSR|Quest")
	void SetViewModel(UHSRQuestViewModel* InViewModel);
	UFUNCTION(BlueprintPure, Category = "HSR|Quest")
	bool GetCurrentSnapshot(FHSRQuestFrontendSnapshot& OutSnapshot) const;
	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|Quest")
	void OnQuestSnapshotChanged(const FHSRQuestFrontendSnapshot& Snapshot);

#if WITH_DEV_AUTOMATION_TESTS
	void AttachForAutomation() { BindAndRefresh(); }
	int32 GetBindCountForAutomation() const { return BindCount; }
	int32 GetUnbindCountForAutomation() const { return UnbindCount; }
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BindAndRefresh();
	void Unbind();
	void HandleSnapshot(const FHSRQuestFrontendSnapshot& InSnapshot);

	UPROPERTY(Transient) TObjectPtr<UHSRQuestViewModel> ViewModel;
	FDelegateHandle Subscription;
	FHSRQuestFrontendSnapshot Current;
	bool bHasSnapshot = false;
	bool bOwnsViewModel = false;
#if WITH_DEV_AUTOMATION_TESTS
	int32 BindCount = 0;
	int32 UnbindCount = 0;
#endif
};
