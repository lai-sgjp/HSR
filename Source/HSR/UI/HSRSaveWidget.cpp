#include "HSRSaveWidget.h"

#include "../Save/HSRSaveSubsystem.h"
#include "Engine/GameInstance.h"

void UHSRSaveWidget::SetViewModel(UHSRSaveViewModel* InViewModel)
{
	UnbindViewModelChanged();
	ViewModel = InViewModel;
	bOwnsViewModel = false;
	BindViewModelChanged();
	Refresh();
	RefreshSlotSummaries();
}
bool UHSRSaveWidget::GetCurrentResult(FHSRSaveFrontendResult& OutResult) const { if (!bHasResult) return false; OutResult = Current; return true; }
bool UHSRSaveWidget::GetSlotSummary(const FString& SlotName, FHSRSaveSlotSummary& OutSummary) const { return ViewModel && ViewModel->GetSlotSummary(SlotName, OutSummary); }
void UHSRSaveWidget::RefreshSlotSummaries()
{
	if (!ViewModel)
	{
		return;
	}
	static const TCHAR* SlotNames[] = { TEXT("p17_slot_01"), TEXT("p17_slot_02") };
	for (const TCHAR* SlotName : SlotNames)
	{
		FHSRSaveSlotSummary Summary;
		if (ViewModel->GetSlotSummary(SlotName, Summary))
		{
			OnSaveSlotSummaryChanged(Summary);
		}
	}
}
EHSRSaveFrontendActionResult UHSRSaveWidget::RequestSave(const FString& SlotName) { const EHSRSaveFrontendActionResult Result = ViewModel ? ViewModel->RequestSave(SlotName) : EHSRSaveFrontendActionResult::InvalidArgument; Refresh(); RefreshSlotSummaries(); return Result; }
EHSRSaveFrontendActionResult UHSRSaveWidget::ConfirmOverwrite() { const EHSRSaveFrontendActionResult Result = ViewModel ? ViewModel->ConfirmOverwrite() : EHSRSaveFrontendActionResult::InvalidArgument; Refresh(); RefreshSlotSummaries(); return Result; }
void UHSRSaveWidget::CancelOverwrite() { if (ViewModel) ViewModel->CancelOverwrite(); Refresh(); RefreshSlotSummaries(); }
EHSRSaveResult UHSRSaveWidget::RequestLoad(const FString& SlotName) { const EHSRSaveResult Result = ViewModel ? ViewModel->RequestLoad(SlotName) : EHSRSaveResult::InvalidArgument; Refresh(); RefreshSlotSummaries(); return Result; }
void UHSRSaveWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!ViewModel) if (UGameInstance* GameInstance = GetGameInstance()) if (UHSRSaveSubsystem* Save = GameInstance->GetSubsystem<UHSRSaveSubsystem>()) { ViewModel = NewObject<UHSRSaveViewModel>(this); ViewModel->Initialize(Save); bOwnsViewModel = true; }
	BindViewModelChanged();
	Refresh();
	RefreshSlotSummaries();
}
void UHSRSaveWidget::NativeDestruct() { UnbindViewModelChanged(); if (bOwnsViewModel && ViewModel) ViewModel->Shutdown(); ViewModel = nullptr; bOwnsViewModel = false; Super::NativeDestruct(); }
void UHSRSaveWidget::Refresh() { FHSRSaveFrontendResult Result; if (ViewModel && ViewModel->GetFrontendResult(Result)) { Current = Result; bHasResult = true; OnSaveResultChanged(Current); } }
void UHSRSaveWidget::HandleViewModelChanged() { Refresh(); RefreshSlotSummaries(); }
void UHSRSaveWidget::BindViewModelChanged() { if (ViewModel && !ViewModelChangedHandle.IsValid()) ViewModelChangedHandle = ViewModel->OnChanged().AddUObject(this, &UHSRSaveWidget::HandleViewModelChanged); }
void UHSRSaveWidget::UnbindViewModelChanged() { if (ViewModel && ViewModelChangedHandle.IsValid()) ViewModel->OnChanged().Remove(ViewModelChangedHandle); ViewModelChangedHandle.Reset(); }
