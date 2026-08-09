#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Equipment/HSREquipmentSubsystem.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRSaveFrontendSlotSummaryTest, "HSR.UI.SaveFrontend.SlotSummary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRSaveFrontendSlotSummaryTest::RunTest(const FString&)
{
	const FString Slot(TEXT("p17_save_frontend_summary"));
	const FString BackupSlot = Slot + TEXT(".__hsr_backup_v1");
	const FString StagingSlot = Slot + TEXT(".__hsr_staging_v1");
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	UGameplayStatics::DeleteGameInSlot(BackupSlot, 0);
	UGameplayStatics::DeleteGameInSlot(StagingSlot, 0);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHSRCharacterProfileSubsystem* Profiles = NewObject<UHSRCharacterProfileSubsystem>(GameInstance);
	UHSRPartySubsystem* Party = NewObject<UHSRPartySubsystem>(GameInstance);
	UHSREquipmentSubsystem* Equipment = NewObject<UHSREquipmentSubsystem>(GameInstance);
	UHSRSaveSubsystem* Save = NewObject<UHSRSaveSubsystem>(GameInstance);
	UHSRCharacterDefinition* Definition = NewObject<UHSRCharacterDefinition>();
	Definition->CharacterId = TEXT("p17.save.frontend.character");
	Definition->MaxLevel = 2;
	UCurveFloat* Curve = NewObject<UCurveFloat>(Definition);
	Curve->FloatCurve.AddKey(2.0f, 100.0f);
	Definition->CumulativeExperienceCurve = Curve;
	Profiles->RegisterDefinition(Definition);
	Party->InitializeForDevelopmentTest(Profiles);
	Save->InitializeForDevelopmentTest(Profiles, Party, Equipment);

	FHSRSaveSlotSummary Summary;
	TestTrue(TEXT("empty summary query succeeds"), Save->GetSlotSummary(Slot, 0, Summary));
	TestEqual(TEXT("empty state"), Summary.State, EHSRSaveSlotState::Empty);
	TestEqual(TEXT("empty result"), Summary.Result, EHSRSaveResult::SlotNotFound);
	TestFalse(TEXT("empty primary absent"), Summary.bPrimaryPresent);
	TestFalse(TEXT("empty backup absent"), Summary.bBackupPresent);

	Party->AddCharacter(Definition->CharacterId);
	TestEqual(TEXT("save summary fixture"), Save->SaveToSlot(Slot), EHSRSaveResult::Success);
	TestTrue(TEXT("ready summary query succeeds"), Save->GetSlotSummary(Slot, 0, Summary));
	TestEqual(TEXT("ready state"), Summary.State, EHSRSaveSlotState::Ready);
	TestEqual(TEXT("ready result"), Summary.Result, EHSRSaveResult::Success);
	TestTrue(TEXT("ready primary present"), Summary.bPrimaryPresent);
	TestFalse(TEXT("ready backup absent"), Summary.bBackupPresent);
	TestEqual(TEXT("ready generation"), Summary.Generation, static_cast<int64>(1));
	TestEqual(TEXT("ready party count"), Summary.PartyMemberCount, 1);
	TestEqual(TEXT("ready map"), Summary.MapId, NAME_None);

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	UGameplayStatics::DeleteGameInSlot(BackupSlot, 0);
	UGameplayStatics::DeleteGameInSlot(StagingSlot, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRSaveFrontendDeferredResultTest, "HSR.UI.SaveFrontend.DeferredResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRSaveFrontendDeferredResultTest::RunTest(const FString&)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHSRSaveSubsystem* Save = NewObject<UHSRSaveSubsystem>(GameInstance);
	UHSRSaveViewModel* ViewModel = NewObject<UHSRSaveViewModel>();
	ViewModel->Initialize(Save);

	FHSRSaveLoadResult DeferredFailure;
	DeferredFailure.Result = EHSRSaveResult::LoadFailed;
	DeferredFailure.bRuntimeChanged = false;
	Save->OnLoadCompleted().Broadcast(DeferredFailure);

	FHSRSaveFrontendResult FrontendResult;
	TestTrue(TEXT("deferred result is projected"), ViewModel->GetFrontendResult(FrontendResult));
	TestEqual(TEXT("deferred failure is visible"), FrontendResult.Result, EHSRSaveResult::LoadFailed);
	TestFalse(TEXT("deferred result is no longer pending"), FrontendResult.bPending);
	ViewModel->Shutdown();
	return true;
}

#endif
