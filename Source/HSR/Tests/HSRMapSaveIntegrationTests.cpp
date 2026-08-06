#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Curves/CurveFloat.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/Definitions/HSRMapDefinition.h"
#include "../Data/Definitions/HSRTeleportDefinition.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Map/HSRMapSubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Save/HSRSaveSubsystem.h"

namespace HSR::P15::SaveTests
{
struct FFixture
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHSRCharacterProfileSubsystem* Profiles = NewObject<UHSRCharacterProfileSubsystem>(GameInstance);
	UHSRPartySubsystem* Party = NewObject<UHSRPartySubsystem>(GameInstance);
	UHSREquipmentSubsystem* Equipment = NewObject<UHSREquipmentSubsystem>(GameInstance);
	UHSRMapSubsystem* Maps = NewObject<UHSRMapSubsystem>(GameInstance);
	UHSRSaveSubsystem* Save = NewObject<UHSRSaveSubsystem>(GameInstance);

	FFixture()
	{
		UHSRCharacterDefinition* Character = NewObject<UHSRCharacterDefinition>();
		Character->CharacterId = TEXT("Character.A");
		Character->MaxLevel = 2;
		UCurveFloat* Curve = NewObject<UCurveFloat>(Character);
		Curve->FloatCurve.AddKey(2.0f, 100.0f);
		Character->CumulativeExperienceCurve = Curve;
		Profiles->RegisterDefinition(Character);
		Party->InitializeForDevelopmentTest(Profiles);
		Party->AddCharacter(Character->CharacterId);

		UHSRMapDefinition* MapA = NewObject<UHSRMapDefinition>();
		MapA->MapId = TEXT("Map.A");
		MapA->World = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Map_A.Map_A")));
		MapA->RegionId = TEXT("Region.A");
		MapA->DefaultArrivalId = TEXT("Arrival.A");
		UHSRMapDefinition* MapB = NewObject<UHSRMapDefinition>();
		MapB->MapId = TEXT("Map.B");
		MapB->World = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Map_B.Map_B")));
		MapB->RegionId = TEXT("Region.B");
		MapB->DefaultArrivalId = TEXT("Arrival.B");
		UHSRTeleportDefinition* Teleport = NewObject<UHSRTeleportDefinition>();
		Teleport->TeleportId = TEXT("Teleport.AB");
		Teleport->SourceMapId = TEXT("Map.A");
		Teleport->DestinationMapId = TEXT("Map.B");
		Teleport->DestinationArrivalId = TEXT("Arrival.FromA");
		Maps->RegisterMapDefinition(*MapA);
		Maps->RegisterMapDefinition(*MapB);
		Maps->RegisterTeleportDefinition(*Teleport);
		Save->InitializeForDevelopmentTest(Profiles, Party, Equipment, nullptr, nullptr, nullptr, Maps);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRMapSaveV5IntegrationTest, "HSR.Save.MapV5Integration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRMapSaveV5IntegrationTest::RunTest(const FString&)
{
	using namespace HSR::P15::SaveTests;
	FFixture F;
	F.Maps->SetCurrentLocation(TEXT("Map.B"));
	F.Maps->UnlockRegion(TEXT("Region.B"));
	F.Maps->UnlockTeleport(TEXT("Teleport.AB"));
	F.Maps->SetExplorationFlag(TEXT("Exploration.Chest.B"));

	FHSRSaveData Captured;
	TestEqual(TEXT("v5 capture succeeds"), F.Save->SaveSnapshot(Captured), EHSRSaveResult::Success);
	TestEqual(TEXT("schema is v8"), Captured.SchemaVersion, 8);
	TestEqual(TEXT("capture carries map"), Captured.Map.CurrentLocation.MapId, FName(TEXT("Map.B")));
	TestTrue(TEXT("capture carries region"), Captured.Map.UnlockedRegionIds.Contains(TEXT("Region.B")));
	TestTrue(TEXT("capture carries teleport"), Captured.Map.UnlockedTeleportIds.Contains(TEXT("Teleport.AB")));
	TestTrue(TEXT("capture carries flag"), Captured.Map.ExplorationFlags.Contains(TEXT("Exploration.Chest.B")));

	TestEqual(TEXT("move away from saved map"), F.Maps->SetCurrentLocation(TEXT("Map.A")), EHSRMapOperationResult::Success);
	const FHSRSaveData BeforeFailedTravel = F.Save->GetSnapshot();
	TestEqual(TEXT("cross-map load without a world rejects before commit"), F.Save->LoadSnapshot(Captured), EHSRSaveResult::InvalidData);
	TestEqual(TEXT("failed cross-map load preserves Save current map"), F.Save->GetSnapshot().Map.CurrentLocation.MapId,
		BeforeFailedTravel.Map.CurrentLocation.MapId);
	TestEqual(TEXT("failed cross-map load preserves runtime map"), F.Maps->GetSnapshot().CurrentLocation.MapId, FName(TEXT("Map.A")));
	TestFalse(TEXT("failed cross-map load leaves no pending travel"), F.Maps->HasPendingTravel());
	TestEqual(TEXT("return to saved map"), F.Maps->SetCurrentLocation(TEXT("Map.B")), EHSRMapOperationResult::Success);

	F.Maps->SetExplorationFlag(TEXT("Exploration.Mutated"));
	int32 MapEvents = 0;
	int32 RestoreEvents = 0;
	bool bAggregateMapChanged = false;
	F.Maps->OnMapStateChanged().AddLambda([&](const FHSRMapRuntimeSnapshot&) { ++MapEvents; });
	F.Save->OnRestoreCommitted().AddLambda([&](const FHSRRestoreCommitInfo& Info)
	{
		++RestoreEvents;
		bAggregateMapChanged = Info.bMapChanged;
	});
	TestEqual(TEXT("changed v5 load succeeds"), F.Save->LoadSnapshot(Captured), EHSRSaveResult::Success);
	TestEqual(TEXT("map notified exactly once"), MapEvents, 1);
	TestEqual(TEXT("aggregate notified exactly once"), RestoreEvents, 1);
	TestTrue(TEXT("aggregate identifies map change"), bAggregateMapChanged);
	TestFalse(TEXT("load does not create ordinary travel"), F.Maps->HasPendingTravel());
	TestEqual(TEXT("repeat load succeeds"), F.Save->LoadSnapshot(Captured), EHSRSaveResult::Success);
	TestEqual(TEXT("repeat load has no map notification"), MapEvents, 1);
	TestEqual(TEXT("repeat load has no aggregate notification"), RestoreEvents, 1);

	const FHSRSaveData StableCurrent = F.Save->GetSnapshot();
	const FHSRMapRuntimeSnapshot StableMap = F.Maps->GetSnapshot();
	FHSRSaveData Bad = Captured;
	Bad.Map.CurrentLocation.MapId = TEXT("Map.Unknown");
	TestEqual(TEXT("bad map rejects whole restore"), F.Save->LoadSnapshot(Bad), EHSRSaveResult::InvalidData);
	TestEqual(TEXT("bad map preserves Save current"), F.Save->GetSnapshot().Map.CurrentLocation.MapId,
		StableCurrent.Map.CurrentLocation.MapId);
	TestEqual(TEXT("bad map preserves runtime map"), F.Maps->GetSnapshot().CurrentLocation.MapId,
		StableMap.CurrentLocation.MapId);
	TestEqual(TEXT("bad map emits no map event"), MapEvents, 1);
	TestEqual(TEXT("bad map emits no aggregate event"), RestoreEvents, 1);
	TestFalse(TEXT("bad map does not travel"), F.Maps->HasPendingTravel());

	for (int32 LegacyVersion = 1; LegacyVersion <= 4; ++LegacyVersion)
	{
		FHSRSaveData Legacy = Captured;
		Legacy.SchemaVersion = LegacyVersion;
		Legacy.Map = FHSRMapSaveData();
		TestEqual(FString::Printf(TEXT("v%d empty map migration succeeds"), LegacyVersion),
			F.Save->LoadSnapshot(Legacy), EHSRSaveResult::Success);
		TestEqual(FString::Printf(TEXT("v%d normalizes schema"), LegacyVersion), F.Save->GetSnapshot().SchemaVersion, 8);
		TestTrue(FString::Printf(TEXT("v%d normalizes empty map"), LegacyVersion),
			F.Save->GetSnapshot().Map.CurrentLocation.MapId.IsNone());
		TestFalse(FString::Printf(TEXT("v%d migration does not travel"), LegacyVersion), F.Maps->HasPendingTravel());

		Legacy.Map = Captured.Map;
		TestEqual(FString::Printf(TEXT("v%d nonempty map rejected"), LegacyVersion),
			F.Save->LoadSnapshot(Legacy), EHSRSaveResult::InvalidData);
		TestTrue(FString::Printf(TEXT("v%d rejection preserves empty runtime"), LegacyVersion),
			F.Maps->GetSnapshot().CurrentLocation.MapId.IsNone());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRMapRestoreTravelPolicyTest, "HSR.Save.MapTravelMutualExclusion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRMapRestoreTravelPolicyTest::RunTest(const FString&)
{
	TestTrue(TEXT("idle restore allowed"), UHSRMapSubsystem::CanRestoreState(false, false));
	TestFalse(TEXT("ordinary travel blocks restore"), UHSRMapSubsystem::CanRestoreState(true, false));
	TestFalse(TEXT("battle return blocks restore"), UHSRMapSubsystem::CanRestoreState(false, true));
	TestFalse(TEXT("both pending block restore"), UHSRMapSubsystem::CanRestoreState(true, true));
	return true;
}

#endif
