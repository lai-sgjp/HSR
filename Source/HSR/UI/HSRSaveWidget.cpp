#include "HSRSaveWidget.h"

#include "../Save/HSRSaveSubsystem.h"
#include "Engine/GameInstance.h"

void UHSRSaveWidget::SetViewModel(UHSRSaveViewModel* InViewModel) { ViewModel = InViewModel; bOwnsViewModel = false; Refresh(); }
bool UHSRSaveWidget::GetCurrentResult(FHSRSaveFrontendResult& OutResult) const { if (!bHasResult) return false; OutResult = Current; return true; }
EHSRSaveFrontendActionResult UHSRSaveWidget::RequestSave(const FString& SlotName) { const EHSRSaveFrontendActionResult Result = ViewModel ? ViewModel->RequestSave(SlotName) : EHSRSaveFrontendActionResult::InvalidArgument; Refresh(); return Result; }
EHSRSaveFrontendActionResult UHSRSaveWidget::ConfirmOverwrite() { const EHSRSaveFrontendActionResult Result = ViewModel ? ViewModel->ConfirmOverwrite() : EHSRSaveFrontendActionResult::InvalidArgument; Refresh(); return Result; }
void UHSRSaveWidget::CancelOverwrite() { if (ViewModel) ViewModel->CancelOverwrite(); }
EHSRSaveResult UHSRSaveWidget::RequestLoad(const FString& SlotName) { const EHSRSaveResult Result = ViewModel ? ViewModel->RequestLoad(SlotName) : EHSRSaveResult::InvalidArgument; Refresh(); return Result; }
void UHSRSaveWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!ViewModel) if (UGameInstance* GameInstance = GetGameInstance()) if (UHSRSaveSubsystem* Save = GameInstance->GetSubsystem<UHSRSaveSubsystem>()) { ViewModel = NewObject<UHSRSaveViewModel>(this); ViewModel->Initialize(Save); bOwnsViewModel = true; }
	Refresh();
}
void UHSRSaveWidget::NativeDestruct() { if (bOwnsViewModel && ViewModel) ViewModel->Shutdown(); ViewModel = nullptr; bOwnsViewModel = false; Super::NativeDestruct(); }
void UHSRSaveWidget::Refresh() { FHSRSaveFrontendResult Result; if (ViewModel && ViewModel->GetFrontendResult(Result)) { Current = Result; bHasResult = true; OnSaveResultChanged(Current); } }
