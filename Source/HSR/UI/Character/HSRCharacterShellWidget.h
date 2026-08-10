#pragma once

#include "CoreMinimal.h"
#include "../HSRScreenWidget.h"
#include "HSRCharacterShellTypes.h"
#include "HSRCharacterShellWidget.generated.h"

class UHSRCharacterShellViewModel;

UCLASS(Blueprintable)
class HSR_API UHSRCharacterShellWidget : public UHSRScreenWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HSR|Character Shell")
	EHSRCharacterShellResult SelectCharacter(FName CharacterId);

	UFUNCTION(BlueprintCallable, Category = "HSR|Character Shell")
	EHSRCharacterShellResult SelectTab(EHSRCharacterShellTab Tab);

	UFUNCTION(BlueprintCallable, Category = "HSR|Character Shell")
	EHSRCharacterShellResult RefreshShell();

	UFUNCTION(BlueprintPure, Category = "HSR|Character Shell")
	bool GetCurrentSnapshot(FHSRCharacterShellSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintPure, Category = "HSR|Character Shell")
	int32 GetRefreshCount() const { return RefreshCount; }

	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|Character Shell")
	void OnShellSnapshotChanged(const FHSRCharacterShellSnapshot& Snapshot);

	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|Character Shell")
	void OnShellUnavailable(EHSRCharacterShellResult Result);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void HandleShellChanged(const FHSRCharacterShellSnapshot& InSnapshot);
	void UpdateDetailStats(const FHSRCharacterShellSnapshot& InSnapshot);

	UPROPERTY(Transient) TObjectPtr<UHSRCharacterShellViewModel> ViewModel;
	FDelegateHandle ShellChangedHandle;
	FHSRCharacterShellSnapshot CurrentSnapshot;
	bool bHasCurrentSnapshot = false;
	int32 RefreshCount = 0;
};
