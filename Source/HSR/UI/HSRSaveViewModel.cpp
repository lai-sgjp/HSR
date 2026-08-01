#include "HSRSaveViewModel.h"

#include "../Save/HSRSaveSubsystem.h"
#include "Kismet/GameplayStatics.h"

void UHSRSaveViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UHSRSaveViewModel::Initialize(UHSRSaveSubsystem* InSave)
{
	Shutdown();
	Save = InSave;
	if (InSave)
	{
		LastResult = InSave->GetLastLoadResult();
		FrontendResult.Result = LastResult.Result;
		FrontendResult.Generation = static_cast<int64>(LastResult.Generation);
		FrontendResult.bRecoveredFromBackup = LastResult.bRecoveredFromBackup;
		FrontendResult.bRuntimeChanged = LastResult.bRuntimeChanged;
		FrontendResult.bPending = InSave->HasPendingRestore();
		bHasResult = true;
	}
}

void UHSRSaveViewModel::Shutdown()
{
	Save.Reset();
	bHasResult = false;
	PendingOverwriteSlot.Reset();
	LastResult = FHSRSaveLoadResult();
	FrontendResult = FHSRSaveFrontendResult();
}

bool UHSRSaveViewModel::GetLastResult(FHSRSaveLoadResult& OutResult) const
{
	if (!bHasResult) return false;
	OutResult = LastResult;
	return true;
}

bool UHSRSaveViewModel::GetFrontendResult(FHSRSaveFrontendResult& OutResult) const
{
	if (!bHasResult) return false;
	OutResult = FrontendResult;
	return true;
}

EHSRSaveFrontendActionResult UHSRSaveViewModel::RequestSave(const FString& SlotName)
{
	if (!Save.IsValid() || SlotName.IsEmpty()) return EHSRSaveFrontendActionResult::InvalidArgument;
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0)) { PendingOverwriteSlot = SlotName; return EHSRSaveFrontendActionResult::ConfirmationRequired; }
	RefreshResult(Save->SaveToSlot(SlotName));
	return EHSRSaveFrontendActionResult::Success;
}

EHSRSaveFrontendActionResult UHSRSaveViewModel::ConfirmOverwrite() { if (!Save.IsValid() || PendingOverwriteSlot.IsEmpty()) return EHSRSaveFrontendActionResult::InvalidArgument; const FString Slot = MoveTemp(PendingOverwriteSlot); RefreshResult(Save->SaveToSlot(Slot)); return EHSRSaveFrontendActionResult::Success; }
void UHSRSaveViewModel::CancelOverwrite() { PendingOverwriteSlot.Reset(); }
bool UHSRSaveViewModel::GetPendingOverwriteSlot(FString& OutSlotName) const { if (PendingOverwriteSlot.IsEmpty()) return false; OutSlotName = PendingOverwriteSlot; return true; }

EHSRSaveResult UHSRSaveViewModel::RequestLoad(const FString& SlotName)
{
	const EHSRSaveResult Result = Save.IsValid() ? Save->LoadFromSlot(SlotName) : EHSRSaveResult::InvalidArgument;
	RefreshResult(Result);
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 SaveFrontend Load Slot=%s Result=%d Generation=%llu RuntimeChanged=%s"),
		*SlotName, static_cast<int32>(Result), LastResult.Generation, LastResult.bRuntimeChanged ? TEXT("true") : TEXT("false"));
	return Result;
}

void UHSRSaveViewModel::RefreshResult(const EHSRSaveResult Result)
{
	if (Save.IsValid())
	{
		LastResult = Save->GetLastLoadResult();
	}
	LastResult.Result = Result;
	FrontendResult.Result = Result;
	FrontendResult.Generation = static_cast<int64>(LastResult.Generation);
	FrontendResult.bRecoveredFromBackup = LastResult.bRecoveredFromBackup;
	FrontendResult.bRuntimeChanged = LastResult.bRuntimeChanged;
	FrontendResult.bPending = Save.IsValid() && Save->HasPendingRestore();
	bHasResult = true;
}
