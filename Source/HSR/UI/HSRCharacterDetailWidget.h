#pragma once

#include "CoreMinimal.h"
#include "HSRScreenWidget.h"
#include "HSRCharacterDetailTypes.h"
#include "HSRCharacterDetailWidget.generated.h"

class UHSRCharacterDetailViewModel;

/**
 * Native owner for WBP_CharacterDetail_P11. It only consumes the ViewModel's
 * pure-value snapshots; Blueprint presentation must not query battle actors or ASC state.
 */
UCLASS(Blueprintable)
class HSR_API UHSRCharacterDetailWidget : public UHSRScreenWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="HSR|Character Detail")
	bool GetCurrentSnapshot(FHSRCharacterDetailSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintPure, Category="HSR|Character Detail")
	int32 GetRefreshCount() const { return RefreshCount; }

	/** Implement in the WBP to update text, bars, and portraits from a pure-value snapshot. */
	UFUNCTION(BlueprintImplementableEvent, Category="HSR|Character Detail")
	void OnDetailSnapshotChanged(const FHSRCharacterDetailSnapshot& Snapshot);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void HandleDetailChanged(const FHSRCharacterDetailSnapshot& Snapshot);

	UPROPERTY(Transient) TObjectPtr<UHSRCharacterDetailViewModel> ViewModel;
	FDelegateHandle DetailChangedHandle;
	FHSRCharacterDetailSnapshot CurrentSnapshot;
	bool bHasCurrentSnapshot = false;
	int32 RefreshCount = 0;
};
