#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "../Data/Definitions/HSRMapDefinition.h"
#include "../Data/Definitions/HSRTeleportDefinition.h"
#include "../Map/HSRMapSubsystem.h"
#include "../UI/HSRMapViewModel.h"
#include "../UI/HSRMapWidget.h"

namespace
{
UHSRMapDefinition* MakeFrontendMap(const TCHAR* MapId, const TCHAR* WorldPath, const TCHAR* RegionId, const TCHAR* ArrivalId, const TCHAR* DisplayName = TEXT(""))
{
	auto* Definition = NewObject<UHSRMapDefinition>();
	Definition->MapId = FName(MapId);
	Definition->World = TSoftObjectPtr<UWorld>(FSoftObjectPath(WorldPath));
	Definition->RegionId = FName(RegionId);
	Definition->DefaultArrivalId = FName(ArrivalId);
	Definition->DisplayName = FText::FromString(DisplayName);
	return Definition;
}

UHSRTeleportDefinition* MakeFrontendTeleport()
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
	TestEqual(TEXT("register map A"), Maps->RegisterMapDefinition(*MakeFrontendMap(TEXT("Map.A"), TEXT("/Game/Maps/Map_A.Map_A"), TEXT("Region.A"), TEXT("Arrival.A"), TEXT("观景车厢"))), EHSRMapOperationResult::Success);
	TestEqual(TEXT("register map B"), Maps->RegisterMapDefinition(*MakeFrontendMap(TEXT("Map.B"), TEXT("/Game/Maps/Map_B.Map_B"), TEXT("Region.B"), TEXT("Arrival.B"), TEXT("新艾丽都六分街地铁站"))), EHSRMapOperationResult::Success);
	TestEqual(TEXT("register teleport"), Maps->RegisterTeleportDefinition(*MakeFrontendTeleport()), EHSRMapOperationResult::Success);
	TestEqual(TEXT("set current location"), Maps->SetCurrentLocation(TEXT("Map.A")), EHSRMapOperationResult::Success);

	TestEqual(TEXT("map display name projected by subsystem"), Maps->GetMapDisplayName(TEXT("Map.A")), FText::FromString(TEXT("观景车厢")));
	TestEqual(TEXT("unknown map falls back to id"), Maps->GetMapDisplayName(TEXT("Map.Unknown")), FText::FromName(TEXT("Map.Unknown")));

	auto* ViewModel = NewObject<UHSRMapViewModel>();
	ViewModel->Initialize(Maps);
	FHSRMapRuntimeSnapshot Snapshot;
	TestTrue(TEXT("initial committed snapshot available"), ViewModel->GetSnapshot(Snapshot));
	TestEqual(TEXT("current location projected"), Snapshot.CurrentLocation.MapId, FName(TEXT("Map.A")));
	TestEqual(TEXT("map display name projected by viewmodel"), ViewModel->GetMapDisplayName(TEXT("Map.B")), FText::FromString(TEXT("新艾丽都六分街地铁站")));

	const FHSRMapRuntimeSnapshot BeforeLocked = Snapshot;
	TestEqual(TEXT("locked intent forwarded without travel"), ViewModel->RequestTeleport(TEXT("Teleport.AB")), EHSRMapOperationResult::Locked);
	TestTrue(TEXT("locked intent preserves projection"), ViewModel->GetSnapshot(Snapshot));
	TestTrue(TEXT("locked intent preserves committed location"), Snapshot.CurrentLocation == BeforeLocked.CurrentLocation);

	int32 Broadcasts = 0;
	ViewModel->OnChanged().AddLambda([&Broadcasts](const FHSRMapRuntimeSnapshot&) { ++Broadcasts; });
	TestEqual(TEXT("unlock region A"), Maps->UnlockRegion(TEXT("Region.A")), EHSRMapOperationResult::Success);
	TestEqual(TEXT("one committed map change refreshes once"), Broadcasts, 1);
	auto* Widget = NewObject<UHSRMapWidget>();
	Widget->SetViewModel(ViewModel);
	TestTrue(TEXT("widget receives committed snapshot"), Widget->GetCurrentSnapshot(Snapshot));
	TestEqual(TEXT("widget projects latest map revision"), Snapshot.Revision, Maps->GetSnapshot().Revision);
	TestEqual(TEXT("widget forwards locked intent"), Widget->RequestTeleport(TEXT("Teleport.AB")), EHSRMapOperationResult::Locked);
	TestEqual(TEXT("map display name projected by widget"), Widget->GetMapDisplayName(TEXT("Map.A")), FText::FromString(TEXT("观景车厢")));
	ViewModel->Shutdown();
	TestEqual(TEXT("post-shutdown map mutation"), Maps->UnlockRegion(TEXT("Region.B")), EHSRMapOperationResult::Success);
	TestEqual(TEXT("shutdown removes subscription"), Broadcasts, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRMapAvailableTeleportsTest, "HSR.UI.MapFrontend.AvailableTeleports",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRMapAvailableTeleportsTest::RunTest(const FString&)
{
	auto* GameInstance = NewObject<UGameInstance>();
	auto* Maps = NewObject<UHSRMapSubsystem>(GameInstance);
	TestEqual(TEXT("register map A"), Maps->RegisterMapDefinition(*MakeFrontendMap(TEXT("Map.A"), TEXT("/Game/Maps/Map_A.Map_A"), TEXT("Region.A"), TEXT("Arrival.A"), TEXT("观景车厢"))), EHSRMapOperationResult::Success);
	TestEqual(TEXT("register map B"), Maps->RegisterMapDefinition(*MakeFrontendMap(TEXT("Map.B"), TEXT("/Game/Maps/Map_B.Map_B"), TEXT("Region.B"), TEXT("Arrival.B"), TEXT("新艾丽都六分街地铁站"))), EHSRMapOperationResult::Success);
	TestEqual(TEXT("register teleport AB"), Maps->RegisterTeleportDefinition(*MakeFrontendTeleport()), EHSRMapOperationResult::Success);
	TestEqual(TEXT("set current location A"), Maps->SetCurrentLocation(TEXT("Map.A")), EHSRMapOperationResult::Success);

	// Teleport registered but locked: listed, unusable, destination name resolved.
	TArray<FHSRTeleportProjection> Projections;
	Maps->GetAvailableTeleports(Projections);
	TestEqual(TEXT("locked teleport is still listed"), Projections.Num(), 1);
	if (Projections.Num() == 1)
	{
		TestEqual(TEXT("projection teleport id"), Projections[0].TeleportId, FName(TEXT("Teleport.AB")));
		TestEqual(TEXT("projection source map"), Projections[0].SourceMapId, FName(TEXT("Map.A")));
		TestEqual(TEXT("projection destination map"), Projections[0].DestinationMapId, FName(TEXT("Map.B")));
		TestEqual(TEXT("projection destination display name"), Projections[0].DestinationDisplayName, FText::FromString(TEXT("新艾丽都六分街地铁站")));
		TestFalse(TEXT("locked teleport unusable"), Projections[0].bUsable);
	}

	// Unlock teleport + destination region, still on source map: usable.
	TestEqual(TEXT("unlock teleport AB"), Maps->UnlockTeleport(TEXT("Teleport.AB")), EHSRMapOperationResult::Success);
	TestEqual(TEXT("unlock region B"), Maps->UnlockRegion(TEXT("Region.B")), EHSRMapOperationResult::Success);
	Maps->GetAvailableTeleports(Projections);
	TestEqual(TEXT("unlocked teleport still listed"), Projections.Num(), 1);
	if (Projections.Num() == 1)
	{
		TestTrue(TEXT("unlocked teleport usable from source"), Projections[0].bUsable);
	}

	// From map B the AB teleport is out of reach: still listed, unusable.
	TestEqual(TEXT("set current location B"), Maps->SetCurrentLocation(TEXT("Map.B")), EHSRMapOperationResult::Success);
	Maps->GetAvailableTeleports(Projections);
	TestEqual(TEXT("teleport survives location change"), Projections.Num(), 1);
	if (Projections.Num() == 1)
	{
		TestFalse(TEXT("teleport unusable from wrong source map"), Projections[0].bUsable);
	}

	// ViewModel / Widget forward the projection too.
	auto* ViewModel = NewObject<UHSRMapViewModel>();
	ViewModel->Initialize(Maps);
	ViewModel->GetAvailableTeleports(Projections);
	TestEqual(TEXT("viewmodel forwards projection"), Projections.Num(), 1);
	auto* Widget = NewObject<UHSRMapWidget>();
	Widget->SetViewModel(ViewModel);
	Widget->GetAvailableTeleports(Projections);
	TestEqual(TEXT("widget forwards projection"), Projections.Num(), 1);

	// Reachable-teleport accessors: currently on Map.B, AB teleport is out of reach.
	TestEqual(TEXT("no reachable teleport from wrong source"), Widget->GetReachableTeleportCount(), 0);
	FHSRTeleportProjection Missing;
	TestFalse(TEXT("indexed reachable teleport out of range"), Widget->GetReachableTeleport(0, Missing));

	// Back on Map.A with teleport + region unlocked, the AB teleport is reachable.
	Maps->SetCurrentLocation(TEXT("Map.A"));
	TestEqual(TEXT("one reachable teleport from source map"), Widget->GetReachableTeleportCount(), 1);
	FHSRTeleportProjection Reachable;
	TestTrue(TEXT("indexed reachable teleport found"), Widget->GetReachableTeleport(0, Reachable));
	TestEqual(TEXT("reachable teleport id"), Reachable.TeleportId, FName(TEXT("Teleport.AB")));
	TestEqual(TEXT("reachable teleport destination name"), Reachable.DestinationDisplayName,
		FText::FromString(TEXT("新艾丽都六分街地铁站")));
	TestFalse(TEXT("out-of-range reachable teleport rejected"), Widget->GetReachableTeleport(1, Reachable));

	ViewModel->Shutdown();
	return true;
}

#endif
