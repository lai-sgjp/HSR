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
	UPROPERTY(BlueprintReadOnly) EHSRSaveResult Result = EHSRSaveResult::LoadFailed;
	UPROPERTY(BlueprintReadOnly) int64 Generation = 0;
	UPROPERTY(BlueprintReadOnly) bool bRecoveredFromBackup = false;
	UPROPERTY(BlueprintReadOnly) bool bRuntimeChanged = false;
};

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
	void RefreshResult(EHSRSaveResult Result);

	TWeakObjectPtr<UHSRSaveSubsystem> Save;
	FHSRSaveLoadResult LastResult;
	FHSRSaveFrontendResult FrontendResult;
	bool bHasResult = false;
	FString PendingOverwriteSlot;
};
