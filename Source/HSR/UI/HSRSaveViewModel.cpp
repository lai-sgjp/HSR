#include "HSRSaveViewModel.h"

#include "../Save/HSRSaveSubsystem.h"

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
		LoadCompletedHandle = InSave->OnLoadCompleted().AddUObject(this, &UHSRSaveViewModel::HandleLoadCompleted);
		LastResult = InSave->GetLastLoadResult();
		FrontendResult.SlotName = ActiveSlotName;
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
	if (Save.IsValid() && LoadCompletedHandle.IsValid())
	{
		Save->OnLoadCompleted().Remove(LoadCompletedHandle);
	}
	LoadCompletedHandle.Reset();
	Save.Reset();
	bHasResult = false;
	PendingOverwriteSlot.Reset();
	ActiveSlotName.Reset();
	LastResult = FHSRSaveLoadResult();
	FrontendResult = FHSRSaveFrontendResult();
	bLastOperationWasSave = false;
}

bool UHSRSaveViewModel::GetLastResult(FHSRSaveLoadResult& OutResult) const
{
	if (!bHasResult)
	{
		return false;
	}
	OutResult = LastResult;
	return true;
}

bool UHSRSaveViewModel::GetFrontendResult(FHSRSaveFrontendResult& OutResult) const
{
	if (!bHasResult)
	{
		return false;
	}
	OutResult = FrontendResult;
	return true;
}

bool UHSRSaveViewModel::GetSlotSummary(const FString& SlotName, FHSRSaveSlotSummary& OutSummary) const
{
	return Save.IsValid() && Save->GetSlotSummary(SlotName, 0, OutSummary);
}

EHSRSaveFrontendActionResult UHSRSaveViewModel::RequestSave(const FString& SlotName)
{
	if (!Save.IsValid() || SlotName.IsEmpty() || Save->HasPendingRestore() || !PendingOverwriteSlot.IsEmpty())
	{
		return EHSRSaveFrontendActionResult::InvalidArgument;
	}

	FHSRSaveSlotSummary Summary;
	if (!Save->GetSlotSummary(SlotName, 0, Summary))
	{
		RefreshResult(EHSRSaveResult::InvalidArgument, SlotName);
		return EHSRSaveFrontendActionResult::InvalidArgument;
	}

	ActiveSlotName = SlotName;
	if (Summary.bPrimaryPresent || Summary.bBackupPresent)
	{
		PendingOverwriteSlot = SlotName;
		return EHSRSaveFrontendActionResult::ConfirmationRequired;
	}

	bLastOperationWasSave = true;
	RefreshResult(Save->SaveToSlot(SlotName), SlotName);
	return EHSRSaveFrontendActionResult::Success;
}

EHSRSaveFrontendActionResult UHSRSaveViewModel::ConfirmOverwrite()
{
	if (!Save.IsValid() || PendingOverwriteSlot.IsEmpty() || Save->HasPendingRestore())
	{
		return EHSRSaveFrontendActionResult::InvalidArgument;
	}

	const FString Slot = MoveTemp(PendingOverwriteSlot);
	ActiveSlotName = Slot;
	bLastOperationWasSave = true;
	RefreshResult(Save->SaveToSlot(Slot), Slot);
	return EHSRSaveFrontendActionResult::Success;
}

void UHSRSaveViewModel::CancelOverwrite()
{
	PendingOverwriteSlot.Reset();
}

bool UHSRSaveViewModel::GetPendingOverwriteSlot(FString& OutSlotName) const
{
	if (PendingOverwriteSlot.IsEmpty())
	{
		return false;
	}
	OutSlotName = PendingOverwriteSlot;
	return true;
}

EHSRSaveResult UHSRSaveViewModel::RequestLoad(const FString& SlotName)
{
	if (!Save.IsValid() || SlotName.IsEmpty() || Save->HasPendingRestore() || !PendingOverwriteSlot.IsEmpty())
	{
		return EHSRSaveResult::InvalidArgument;
	}

	ActiveSlotName = SlotName;
	bLastOperationWasSave = false;
	const EHSRSaveResult Result = Save->LoadFromSlot(SlotName);
	RefreshResult(Result, SlotName);
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 SaveFrontend Load Slot=%s Result=%d Generation=%llu RuntimeChanged=%s Pending=%s"),
		*SlotName, static_cast<int32>(Result), LastResult.Generation,
		LastResult.bRuntimeChanged ? TEXT("true") : TEXT("false"),
		FrontendResult.bPending ? TEXT("true") : TEXT("false"));
	return Result;
}

void UHSRSaveViewModel::HandleLoadCompleted(const FHSRSaveLoadResult& Result)
{
	LastResult = Result;
	bLastOperationWasSave = false;
	FrontendResult.SlotName = ActiveSlotName;
	FrontendResult.Result = Result.Result;
	FrontendResult.Generation = static_cast<int64>(Result.Generation);
	FrontendResult.bRecoveredFromBackup = Result.bRecoveredFromBackup;
	FrontendResult.bRuntimeChanged = Result.bRuntimeChanged;
	FrontendResult.bPending = false;
	bHasResult = true;
	Changed.Broadcast();
}

void UHSRSaveViewModel::RefreshResult(const EHSRSaveResult Result, const FString& SlotName)
{
	if (!SlotName.IsEmpty())
	{
		ActiveSlotName = SlotName;
	}
	if (Save.IsValid())
	{
		LastResult = Save->GetLastLoadResult();
	}
	LastResult.Result = Result;
	FrontendResult.SlotName = ActiveSlotName;
	FrontendResult.Result = Result;
	FrontendResult.Generation = bLastOperationWasSave && Save.IsValid()
		? static_cast<int64>(Save->GetLastWriteHeader().Generation)
		: static_cast<int64>(LastResult.Generation);
	FrontendResult.bRecoveredFromBackup = LastResult.bRecoveredFromBackup;
	FrontendResult.bRuntimeChanged = LastResult.bRuntimeChanged;
	FrontendResult.bPending = Save.IsValid() && Save->HasPendingRestore();
	bHasResult = true;
}
