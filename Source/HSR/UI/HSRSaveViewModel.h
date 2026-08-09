#pragma once

#include "CoreMinimal.h"
#include "../Save/HSRSaveTypes.h"
#include "HSRSaveViewModel.generated.h"

class UHSRSaveSubsystem;

UENUM(BlueprintType)
enum class EHSRSaveFrontendActionResult : uint8 { Success, ConfirmationRequired, InvalidArgument };

USTRUCT(BlueprintType)
struct HSR_API FHSRSaveFrontendResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString SlotName;
	UPROPERTY(BlueprintReadOnly) EHSRSaveResult Result = EHSRSaveResult::LoadFailed;
	UPROPERTY(BlueprintReadOnly) int64 Generation = 0;
	UPROPERTY(BlueprintReadOnly) bool bRecoveredFromBackup = false;
	UPROPERTY(BlueprintReadOnly) bool bRuntimeChanged = false;
	UPROPERTY(BlueprintReadOnly) bool bPending = false;
};

DECLARE_MULTICAST_DELEGATE(FHSRSaveViewModelChanged);

UCLASS(BlueprintType)
class HSR_API UHSRSaveViewModel : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;
	void Initialize(UHSRSaveSubsystem* InSave);
	void Shutdown();

	bool GetLastResult(FHSRSaveLoadResult& OutResult) const;
	UFUNCTION(BlueprintPure, Category = "HSR|Save")
	bool GetFrontendResult(FHSRSaveFrontendResult& OutResult) const;
	UFUNCTION(BlueprintPure, Category = "HSR|Save")
	bool GetSlotSummary(const FString& SlotName, FHSRSaveSlotSummary& OutSummary) const;
	FHSRSaveViewModelChanged& OnChanged() { return Changed; }

	UFUNCTION(BlueprintCallable, Category = "HSR|Save")
	EHSRSaveFrontendActionResult RequestSave(const FString& SlotName);
	UFUNCTION(BlueprintCallable, Category = "HSR|Save")
	EHSRSaveFrontendActionResult ConfirmOverwrite();
	UFUNCTION(BlueprintCallable, Category = "HSR|Save")
	void CancelOverwrite();
	UFUNCTION(BlueprintPure, Category = "HSR|Save")
	bool GetPendingOverwriteSlot(FString& OutSlotName) const;

	UFUNCTION(BlueprintCallable, Category = "HSR|Save")
	EHSRSaveResult RequestLoad(const FString& SlotName);

private:
	void HandleLoadCompleted(const FHSRSaveLoadResult& Result);
	void RefreshResult(EHSRSaveResult Result, const FString& SlotName = FString());

	TWeakObjectPtr<UHSRSaveSubsystem> Save;
	FDelegateHandle LoadCompletedHandle;
	FHSRSaveLoadResult LastResult;
	FHSRSaveFrontendResult FrontendResult;
	FHSRSaveViewModelChanged Changed;
	bool bHasResult = false;
	FString PendingOverwriteSlot;
	FString ActiveSlotName;
	bool bLastOperationWasSave = false;
};
