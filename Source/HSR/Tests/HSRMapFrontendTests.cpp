#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "../Data/Definitions/HSRMapDefinition.h"
#include "../Data/Definitions/HSRTeleportDefinition.h"
#include "../Map/HSRMapSubsystem.h"
#include "../UI/HSRMapViewModel.h"

namespace
{
UHSRMapDefinition* MakeMap(const TCHAR* MapId, const TCHAR* WorldPath, const TCHAR* RegionId, const TCHAR* ArrivalId)
{
	auto* Definition = NewObject<UHSRMapDefinition>();
	Definition->MapId = FName(MapId);
	Definition->World = TSoftObjectPtr<UWorld>(FSoftObjectPath(WorldPath));
	Definition->RegionId = FName(RegionId);
	Definition->DefaultArrivalId = FName(ArrivalId);
	return Definition;
}

UHSRTeleportDefinition* MakeTeleport()
{
	auto* Definition = NewObject<UHSRTeleportDefinition>();
	Definition->TeleportId = TEXT("Teleport.AB");
	Definition->SourceMapId = TEXT("Map.A");
	Definition->DestinationMapId = TEXT("Map.B");
	Definition->DestinationArrivalId = TEXT("Arrival.FromA");
	return Definition;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRMapFrontendViewModelTest, "HSR.UI.MapFrontend.ViewModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRMapFrontendViewModelTest::RunTest(const FString&)
{
	auto* GameInstance = NewObject<UGameInstance>();
	auto* Maps = NewObject<UHSRMapSubsystem>(GameInstance);
	TestEqual(TEXT("register map A"), Maps->RegisterMapDefinition(*MakeMap(TEXT("Map.A"), TEXT("/Game/Maps/Map_A.Map_A"), TEXT("Region.A"), TEXT("Arrival.A"))), EHSRMapOperationResult::Success);
	TestEqual(TEXT("register map B"), Maps->RegisterMapDefinition(*MakeMap(TEXT("Map.B"), TEXT("/Game/Maps/Map_B.Map_B"), TEXT("Region.B"), TEXT("Arrival.B"))), EHSRMapOperationResult::Success);
	TestEqual(TEXT("register teleport"), Maps->RegisterTeleportDefinition(*MakeTeleport()), EHSRMapOperationResult::Success);
	TestEqual(TEXT("set current location"), Maps->SetCurrentLocation(TEXT("Map.A")), EHSRMapOperationResult::Success);

	auto* ViewModel = NewObject<UHSRMapViewModel>();
	ViewModel->Initialize(Maps);
	FHSRMapRuntimeSnapshot Snapshot;
	TestTrue(TEXT("initial committed snapshot available"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("current location projected"), Snapshot.CurrentLocation.MapId, FName(TEXT("Map.A")));

	const FHSRMapRuntimeSnapshot BeforeLocked = Snapshot;
	TestEqual(TEXT("locked intent forwarded without travel"), ViewModel->RequestTeleport(TEXT("Teleport.AB")), EHSRMapOperationResult::Locked);
	TestTrue(TEXT("locked intent preserves projection"), ViewModel->GetSnapshot(Snapshot));
	TestTrue(TEXT("locked intent preserves committed location"), Snapshot.CurrentLocation == BeforeLocked.CurrentLocation);

	int32 Broadcasts = 0;
	ViewModel->OnChanged().AddLambda([&Broadcasts](const FHSRMapRuntimeSnapshot&) { ++Broadcasts; });
	TestEqual(TEXT("unlock region A"), Maps->UnlockRegion(TEXT("Region.A")), EHSRMapOperationResult::Success);
	TestEqual(TEXT("one committed map change refreshes once"), Broadcasts, 1);
	ViewModel->Shutdown();
	TestEqual(TEXT("post-shutdown map mutation"), Maps->UnlockRegion(TEXT("Region.B")), EHSRMapOperationResult::Success);
	TestEqual(TEXT("shutdown removes subscription"), Broadcasts, 1);
	return true;
}

#endif
