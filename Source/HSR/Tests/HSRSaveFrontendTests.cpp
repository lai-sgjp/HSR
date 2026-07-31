#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "../Save/HSRSaveSubsystem.h"
#include "../Save/HSRSaveGame.h"
#include "../UI/HSRSaveViewModel.h"
#include "../UI/HSRSaveWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRSaveFrontendIntentTest, "HSR.UI.SaveFrontend.Intent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRSaveFrontendIntentTest::RunTest(const FString&)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHSRSaveSubsystem* Save = NewObject<UHSRSaveSubsystem>(GameInstance);
	UHSRSaveViewModel* ViewModel = NewObject<UHSRSaveViewModel>();
	ViewModel->Initialize(Save);

	FHSRSaveLoadResult Result;
	TestTrue(TEXT("initial result is projected"), ViewModel->GetLastResult(Result));
	TestEqual(TEXT("missing slot is forwarded as a typed result"),
		ViewModel->RequestLoad(TEXT("p17_save_frontend_missing")), EHSRSaveResult::SlotNotFound);
	TestTrue(TEXT("load result refreshes after request"), ViewModel->GetLastResult(Result));
	UHSRSaveWidget* Widget = NewObject<UHSRSaveWidget>();
	Widget->SetViewModel(ViewModel);
	FHSRSaveFrontendResult FrontendResult;
	TestTrue(TEXT("widget projects result"), Widget->GetCurrentResult(FrontendResult));
	TestEqual(TEXT("widget receives typed failure"), FrontendResult.Result, EHSRSaveResult::SlotNotFound);
	const FString ExistingSlot(TEXT("p17_save_frontend_overwrite"));
	UGameplayStatics::DeleteGameInSlot(ExistingSlot, 0);
	TestTrue(TEXT("create existing slot"), UGameplayStatics::SaveGameToSlot(NewObject<UHSRSaveGame>(), ExistingSlot, 0));
	TestEqual(TEXT("existing slot requires confirmation"), ViewModel->RequestSave(ExistingSlot), EHSRSaveFrontendActionResult::ConfirmationRequired);
	FString PendingSlot;
	TestTrue(TEXT("confirmation records slot"), ViewModel->GetPendingOverwriteSlot(PendingSlot));
	TestEqual(TEXT("confirmation preserves slot name"), PendingSlot, ExistingSlot);
	ViewModel->CancelOverwrite();
	TestFalse(TEXT("cancel clears pending overwrite"), ViewModel->GetPendingOverwriteSlot(PendingSlot));
	UGameplayStatics::DeleteGameInSlot(ExistingSlot, 0);
	ViewModel->Shutdown();
	return true;
}

#endif
