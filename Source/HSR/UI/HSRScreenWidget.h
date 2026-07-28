#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSRScreenWidget.generated.h"

class UHSRUIManagerSubsystem;
class UWidget;

UCLASS(Blueprintable)
class HSR_API UHSRScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwningUIManager(UHSRUIManagerSubsystem* Manager);
	UHSRUIManagerSubsystem* GetOwningUIManager() const { return OwningUIManager.Get(); }

	UFUNCTION(BlueprintCallable, Category = "HSR|UI")
	bool RequestBack();

	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|UI")
	UWidget* GetPreferredFocusWidget() const;

#if WITH_DEV_AUTOMATION_TESTS
	bool ShouldConsumeBackKeyForAutomation(const FKey& Key) const;
#endif

protected:
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	bool ShouldConsumeBackKey(const FKey& Key) const;
	UPROPERTY(Transient)
	TWeakObjectPtr<UHSRUIManagerSubsystem> OwningUIManager;
};
