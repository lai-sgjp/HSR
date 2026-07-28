#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../Reward/HSRSettlementAuthority.h"
#include "../Reward/HSRSettlementTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRSettlementFoundationTest,
	"HSR.Settlement.Foundation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRSettlementFoundationTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->AddToRoot();
	GameInstance->InitializeStandalone(FName(*FString::Printf(TEXT("HSRSettlement_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits))));

	UHSRSettlementAuthority* Authority = GameInstance->GetSubsystem<UHSRSettlementAuthority>();
	UHSRInventorySubsystem* Inventory = GameInstance->GetSubsystem<UHSRInventorySubsystem>();
	UHSRCharacterProfileSubsystem* Profiles = GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>();
	UHSRRewardSubsystem* Reward = GameInstance->GetSubsystem<UHSRRewardSubsystem>();
	TestNotNull(TEXT("Settlement authority is available"), Authority);
	TestNotNull(TEXT("Inventory authority is available"), Inventory);
	TestNotNull(TEXT("Profile authority is available"), Profiles);
	TestNotNull(TEXT("Reward authority is available"), Reward);

	FHSRSettlementRequest InvalidRequest;
	FHSRSettlementReceipt Receipt;
	TestEqual(TEXT("Invalid transaction is rejected"),
		Authority->SubmitSettlement(InvalidRequest, Receipt),
		EHSRSettlementResult::InvalidTransactionId);

	const FHSRSettlementAutomationSnapshot Snapshot = Authority->GetAutomationSnapshot();
	TestEqual(TEXT("Rejected request installs nothing"), Snapshot.AggregateInstallCount, 0);
	TestEqual(TEXT("Rejected request publishes nothing"), Snapshot.PublicationCount, 0);

	UWorld* World = GameInstance->GetWorld();
	GameInstance->Shutdown();
	if (World)
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	GameInstance->RemoveFromRoot();
	return true;
}

#endif
