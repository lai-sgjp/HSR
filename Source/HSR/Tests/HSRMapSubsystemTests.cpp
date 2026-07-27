#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include <limits>
#include "Engine/GameInstance.h"
#include "../Data/Definitions/HSRMapDefinition.h"
#include "../Data/Definitions/HSRTeleportDefinition.h"
#include "../Map/HSRMapSubsystem.h"

namespace HSR::P15::Tests
{
	static UHSRMapDefinition* MakeMap(const TCHAR* MapId, const TCHAR* WorldPath, const TCHAR* RegionId, const TCHAR* ArrivalId)
	{
		UHSRMapDefinition* Definition = NewObject<UHSRMapDefinition>();
		Definition->MapId = FName(MapId);
		Definition->World = TSoftObjectPtr<UWorld>(FSoftObjectPath(WorldPath));
		Definition->RegionId = FName(RegionId);
		Definition->DefaultArrivalId = FName(ArrivalId);
		return Definition;
	}

	static UHSRTeleportDefinition* MakeTeleport(const TCHAR* Id, const TCHAR* Source, const TCHAR* Destination,
		const TCHAR* Arrival, const bool bInitiallyUnlocked = false)
	{
		UHSRTeleportDefinition* Definition = NewObject<UHSRTeleportDefinition>();
		Definition->TeleportId = FName(Id);
		Definition->SourceMapId = FName(Source);
		Definition->DestinationMapId = FName(Destination);
		Definition->DestinationArrivalId = FName(Arrival);
		Definition->bInitiallyUnlocked = bInitiallyUnlocked;
		return Definition;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRMapDefinitionRegistrationTest, "HSR.Map.Definitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRMapDefinitionRegistrationTest::RunTest(const FString&)
{
	using namespace HSR::P15::Tests;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHSRMapSubsystem* Maps = NewObject<UHSRMapSubsystem>(GameInstance);
	UHSRMapDefinition* A = MakeMap(TEXT("Map.A"), TEXT("/Game/Maps/Map_A.Map_A"), TEXT("Region.A"), TEXT("Arrival.A"));
	UHSRMapDefinition* B = MakeMap(TEXT("Map.B"), TEXT("/Game/Maps/Map_B.Map_B"), TEXT("Region.B"), TEXT("Arrival.B"));
	TestEqual(TEXT("register A"), Maps->RegisterMapDefinition(*A), EHSRMapOperationResult::Success);
	TestEqual(TEXT("same map is no-op"), Maps->RegisterMapDefinition(*A), EHSRMapOperationResult::NoOp);
	TestEqual(TEXT("register B"), Maps->RegisterMapDefinition(*B), EHSRMapOperationResult::Success);

	UHSRMapDefinition* Conflict = MakeMap(TEXT("Map.A"), TEXT("/Game/Maps/Other.Other"), TEXT("Region.A"), TEXT("Arrival.A"));
	TestEqual(TEXT("conflicting map rejected"), Maps->RegisterMapDefinition(*Conflict), EHSRMapOperationResult::DuplicateId);
	UHSRMapDefinition* Invalid = MakeMap(TEXT(""), TEXT("/Game/Maps/Invalid.Invalid"), TEXT("Region.Invalid"), TEXT("Arrival.Invalid"));
	TestEqual(TEXT("empty id rejected"), Maps->RegisterMapDefinition(*Invalid), EHSRMapOperationResult::InvalidDefinition);
	Invalid = MakeMap(TEXT("Map.Invalid"), TEXT(""), TEXT("Region.Invalid"), TEXT("Arrival.Invalid"));
	TestEqual(TEXT("null world rejected"), Maps->RegisterMapDefinition(*Invalid), EHSRMapOperationResult::InvalidDefinition);
	Invalid = MakeMap(TEXT("Map.Invalid"), TEXT("/Game/Maps/Invalid.Invalid"), TEXT(""), TEXT("Arrival.Invalid"));
	TestEqual(TEXT("empty region rejected"), Maps->RegisterMapDefinition(*Invalid), EHSRMapOperationResult::InvalidDefinition);
	Invalid = MakeMap(TEXT("Map.Invalid"), TEXT("/Game/Maps/Invalid.Invalid"), TEXT("Region.Invalid"), TEXT(""));
	TestEqual(TEXT("empty default arrival rejected"), Maps->RegisterMapDefinition(*Invalid), EHSRMapOperationResult::InvalidDefinition);

	UHSRTeleportDefinition* ToB = MakeTeleport(TEXT("Teleport.AB"), TEXT("Map.A"), TEXT("Map.B"), TEXT("Arrival.FromA"));
	TestEqual(TEXT("register teleport"), Maps->RegisterTeleportDefinition(*ToB), EHSRMapOperationResult::Success);
	TestEqual(TEXT("same teleport is no-op"), Maps->RegisterTeleportDefinition(*ToB), EHSRMapOperationResult::NoOp);
	UHSRTeleportDefinition* TeleportConflict = MakeTeleport(TEXT("Teleport.AB"), TEXT("Map.B"), TEXT("Map.A"), TEXT("Arrival.Other"));
	TestEqual(TEXT("conflicting teleport rejected"), Maps->RegisterTeleportDefinition(*TeleportConflict), EHSRMapOperationResult::DuplicateId);
	UHSRTeleportDefinition* Unknown = MakeTeleport(TEXT("Teleport.Unknown"), TEXT("Map.A"), TEXT("Map.Missing"), TEXT("Arrival.X"));
	TestEqual(TEXT("unknown destination rejected"), Maps->RegisterTeleportDefinition(*Unknown), EHSRMapOperationResult::UnknownMap);
	Unknown = MakeTeleport(TEXT("Teleport.UnknownSource"), TEXT("Map.Missing"), TEXT("Map.B"), TEXT("Arrival.X"));
	TestEqual(TEXT("unknown source rejected"), Maps->RegisterTeleportDefinition(*Unknown), EHSRMapOperationResult::UnknownMap);
	TestEqual(TEXT("empty teleport id rejected"), Maps->RegisterTeleportDefinition(*MakeTeleport(TEXT(""), TEXT("Map.A"), TEXT("Map.B"), TEXT("Arrival.X"))), EHSRMapOperationResult::InvalidDefinition);
	TestEqual(TEXT("empty source rejected"), Maps->RegisterTeleportDefinition(*MakeTeleport(TEXT("Teleport.EmptySource"), TEXT(""), TEXT("Map.B"), TEXT("Arrival.X"))), EHSRMapOperationResult::InvalidDefinition);
	TestEqual(TEXT("empty destination rejected"), Maps->RegisterTeleportDefinition(*MakeTeleport(TEXT("Teleport.EmptyDestination"), TEXT("Map.A"), TEXT(""), TEXT("Arrival.X"))), EHSRMapOperationResult::InvalidDefinition);
	TestEqual(TEXT("empty arrival rejected"), Maps->RegisterTeleportDefinition(*MakeTeleport(TEXT("Teleport.EmptyArrival"), TEXT("Map.A"), TEXT("Map.B"), TEXT(""))), EHSRMapOperationResult::InvalidDefinition);
	TestEqual(TEXT("same-map teleport rejected"), Maps->RegisterTeleportDefinition(*MakeTeleport(TEXT("Teleport.Same"), TEXT("Map.A"), TEXT("Map.A"), TEXT("Arrival.X"))), EHSRMapOperationResult::InvalidDefinition);
	UHSRTeleportDefinition* InitiallyUnlocked = MakeTeleport(TEXT("Teleport.BA"), TEXT("Map.B"), TEXT("Map.A"), TEXT("Arrival.FromB"), true);
	TestEqual(TEXT("initially unlocked teleport registers"), Maps->RegisterTeleportDefinition(*InitiallyUnlocked), EHSRMapOperationResult::Success);
	TestTrue(TEXT("initially unlocked state applied"), Maps->IsTeleportUnlocked(TEXT("Teleport.BA")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRMapStateAndRequestTest, "HSR.Map.StateAndRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRMapStateAndRequestTest::RunTest(const FString&)
{
	using namespace HSR::P15::Tests;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHSRMapSubsystem* Maps = NewObject<UHSRMapSubsystem>(GameInstance);
	Maps->RegisterMapDefinition(*MakeMap(TEXT("Map.A"), TEXT("/Game/Maps/Map_A.Map_A"), TEXT("Region.A"), TEXT("Arrival.A")));
	Maps->RegisterMapDefinition(*MakeMap(TEXT("Map.B"), TEXT("/Game/Maps/Map_B.Map_B"), TEXT("Region.B"), TEXT("Arrival.B")));
	Maps->RegisterTeleportDefinition(*MakeTeleport(TEXT("Teleport.AB"), TEXT("Map.A"), TEXT("Map.B"), TEXT("Arrival.FromA")));

	int32 Broadcasts = 0;
	Maps->OnMapStateChanged().AddLambda([&Broadcasts](const FHSRMapRuntimeSnapshot&) { ++Broadcasts; });
	TestEqual(TEXT("set current map"), Maps->SetCurrentLocation(TEXT("Map.A")), EHSRMapOperationResult::Success);
	TestEqual(TEXT("default arrival selected"), Maps->GetSnapshot().CurrentLocation.ArrivalId, FName(TEXT("Arrival.A")));
	const int64 BeforeFailures = Maps->GetSnapshot().Revision;
	FHSRTeleportRequest Request;
	TestEqual(TEXT("locked request rejected"), Maps->BuildTeleportRequest(TEXT("Teleport.AB"), Request), EHSRMapOperationResult::Locked);
	TestEqual(TEXT("unknown unlock rejected"), Maps->UnlockTeleport(TEXT("Teleport.Missing")), EHSRMapOperationResult::UnknownTeleport);
	TestEqual(TEXT("failures do not revise"), Maps->GetSnapshot().Revision, BeforeFailures);

	TestEqual(TEXT("unlock source region"), Maps->UnlockRegion(TEXT("Region.A")), EHSRMapOperationResult::Success);
	TestEqual(TEXT("unlock teleport"), Maps->UnlockTeleport(TEXT("Teleport.AB")), EHSRMapOperationResult::Success);
	TestEqual(TEXT("repeat unlock no-op"), Maps->UnlockTeleport(TEXT("Teleport.AB")), EHSRMapOperationResult::NoOp);
	const int64 BeforeLockedRegion = Maps->GetSnapshot().Revision;
	const int32 BroadcastsBeforeLockedRegion = Broadcasts;
	TestEqual(TEXT("locked destination region rejects request"), Maps->BuildTeleportRequest(TEXT("Teleport.AB"), Request), EHSRMapOperationResult::Locked);
	TestEqual(TEXT("locked region failure does not revise"), Maps->GetSnapshot().Revision, BeforeLockedRegion);
	TestEqual(TEXT("locked region failure does not broadcast"), Broadcasts, BroadcastsBeforeLockedRegion);
	TestEqual(TEXT("unlock destination region"), Maps->UnlockRegion(TEXT("Region.B")), EHSRMapOperationResult::Success);
	TestEqual(TEXT("build request"), Maps->BuildTeleportRequest(TEXT("Teleport.AB"), Request), EHSRMapOperationResult::Success);
	TestTrue(TEXT("request id valid"), Request.RequestId.IsValid());
	TestEqual(TEXT("request source"), Request.Source.MapId, FName(TEXT("Map.A")));
	TestEqual(TEXT("request destination"), Request.Destination.MapId, FName(TEXT("Map.B")));
	TestEqual(TEXT("request arrival"), Request.Destination.ArrivalId, FName(TEXT("Arrival.FromA")));

	TestEqual(TEXT("move to B"), Maps->SetCurrentLocation(TEXT("Map.B")), EHSRMapOperationResult::Success);
	TestEqual(TEXT("wrong source rejected"), Maps->BuildTeleportRequest(TEXT("Teleport.AB"), Request), EHSRMapOperationResult::InvalidSource);
	TestEqual(TEXT("one broadcast per commit"), Broadcasts, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRMapTravelTransactionTest, "HSR.Map.TravelTransaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRMapTravelTransactionTest::RunTest(const FString&)
{
	using namespace HSR::P15::Tests;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHSRMapSubsystem* Maps = NewObject<UHSRMapSubsystem>(GameInstance);
	Maps->RegisterMapDefinition(*MakeMap(TEXT("Map.A"), TEXT("/Game/Maps/Map_A.Map_A"), TEXT("Region.A"), TEXT("Arrival.A")));
	Maps->RegisterMapDefinition(*MakeMap(TEXT("Map.B"), TEXT("/Game/Maps/Map_B.Map_B"), TEXT("Region.B"), TEXT("Arrival.B")));
	Maps->RegisterTeleportDefinition(*MakeTeleport(TEXT("Teleport.AB"), TEXT("Map.A"), TEXT("Map.B"), TEXT("Arrival.FromA"), true));
	Maps->SetCurrentLocation(TEXT("Map.A"));
	Maps->UnlockRegion(TEXT("Region.B"));

	const FHSRMapLocation CommittedBefore = Maps->GetSnapshot().CurrentLocation;
	const int64 RevisionBefore = Maps->GetSnapshot().Revision;
	TestEqual(TEXT("stage pending"), Maps->StageTeleportForAutomation(TEXT("Teleport.AB")), EHSRMapOperationResult::Success);
	FHSRTeleportRequest Pending;
	TestTrue(TEXT("pending readable"), Maps->GetPendingRequest(Pending));
	TestTrue(TEXT("pending id valid"), Pending.RequestId.IsValid());
	TestTrue(TEXT("pending does not commit location"), Maps->GetSnapshot().CurrentLocation == CommittedBefore);
	TestEqual(TEXT("pending does not revise snapshot"), Maps->GetSnapshot().Revision, RevisionBefore);
	TestEqual(TEXT("second stage rejected"), Maps->StageTeleportForAutomation(TEXT("Teleport.AB")), EHSRMapOperationResult::AlreadyPending);
	TestEqual(TEXT("wrong destination rejected before pawn"),
		Maps->CommitPendingArrival(TEXT("Map.A"), TEXT("Arrival.FromA"), nullptr, FTransform::Identity),
		EHSRMapOperationResult::WrongDestination);
	TestEqual(TEXT("wrong loaded world rejected"), Maps->ValidatePendingArrivalContext(TEXT("Map.B"), TEXT("Arrival.FromA"),
		TEXT("/Game/Maps/Map_A"), 1), EHSRMapOperationResult::InvalidWorld);
	TestEqual(TEXT("missing arrival rejected"), Maps->ValidatePendingArrivalContext(TEXT("Map.B"), TEXT("Arrival.FromA"),
		TEXT("/Game/Maps/Map_B"), 0), EHSRMapOperationResult::ArrivalNotFound);
	TestEqual(TEXT("duplicate arrival rejected"), Maps->ValidatePendingArrivalContext(TEXT("Map.B"), TEXT("Arrival.FromA"),
		TEXT("/Game/Maps/Map_B"), 2), EHSRMapOperationResult::ArrivalAmbiguous);
	TestEqual(TEXT("PIE-prefixed correct world accepted"), Maps->ValidatePendingArrivalContext(TEXT("Map.B"), TEXT("Arrival.FromA"),
		TEXT("/Game/Maps/UEDPIE_0_Map_B"), 1), EHSRMapOperationResult::Success);
	TestEqual(TEXT("runtime commit without world rejected"),
		Maps->CommitPendingArrival(TEXT("Map.B"), TEXT("Arrival.FromA"), nullptr, FTransform::Identity),
		EHSRMapOperationResult::InvalidWorld);
	TestTrue(TEXT("failed arrival preserves pending"), Maps->GetPendingRequest(Pending));
	TestEqual(TEXT("wrong request cannot cancel"), Maps->CancelPendingTravel(FGuid::NewGuid()), EHSRMapOperationResult::RequestMismatch);
	TestTrue(TEXT("mismatched cancel preserves pending"), Maps->GetPendingRequest(Pending));
	TestEqual(TEXT("matching request cancels"), Maps->CancelPendingTravel(Pending.RequestId), EHSRMapOperationResult::Success);
	TestFalse(TEXT("cancel clears pending"), Maps->GetPendingRequest(Pending));
	TestEqual(TEXT("repeat cancel is nothing pending"), Maps->CancelPendingTravel(Pending.RequestId), EHSRMapOperationResult::NothingPending);
	TestEqual(TEXT("stage retry"), Maps->StageTeleportForAutomation(TEXT("Teleport.AB")), EHSRMapOperationResult::Success);
	UPackage* UnrelatedPackage = CreatePackage(TEXT("/Game/Maps/UnrelatedTravelWorld"));
	UWorld* UnrelatedWorld = NewObject<UWorld>(UnrelatedPackage);
	Maps->HandleTravelFailure(UnrelatedWorld, ETravelFailure::LoadMapFailure, TEXT("Unrelated automation failure"));
	TestTrue(TEXT("unrelated travel failure preserves pending"), Maps->GetPendingRequest(Pending));
	Maps->HandleTravelFailure(nullptr, ETravelFailure::LoadMapFailure, TEXT("Injected automation failure"));
	TestFalse(TEXT("travel failure clears pending"), Maps->GetPendingRequest(Pending));
	TestTrue(TEXT("failure leaves committed location"), Maps->GetSnapshot().CurrentLocation == CommittedBefore);
	TestEqual(TEXT("failure leaves revision"), Maps->GetSnapshot().Revision, RevisionBefore);
	TestEqual(TEXT("retry after failure available"), Maps->StageTeleportForAutomation(TEXT("Teleport.AB")), EHSRMapOperationResult::Success);

	UHSRMapSubsystem* PackageMaps = NewObject<UHSRMapSubsystem>(GameInstance);
	PackageMaps->RegisterMapDefinition(*MakeMap(TEXT("Map.RealA"), TEXT("/Game/Maps/Map_Exploration_P15_A.Map_Exploration_P15_A"),
		TEXT("Region.RealA"), TEXT("Arrival.RealA")));
	PackageMaps->RegisterMapDefinition(*MakeMap(TEXT("Map.Missing"), TEXT("/Game/Maps/DefinitelyMissingP15.DefinitelyMissingP15"),
		TEXT("Region.Missing"), TEXT("Arrival.Missing")));
	TestTrue(TEXT("existing registered map package found"), PackageMaps->DoesRegisteredMapPackageExistForAutomation(TEXT("Map.RealA")));
	TestFalse(TEXT("missing registered map package rejected"), PackageMaps->DoesRegisteredMapPackageExistForAutomation(TEXT("Map.Missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRMapArrivalCommitNotificationTest, "HSR.Map.ArrivalCommitNotification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRMapArrivalCommitNotificationTest::RunTest(const FString&)
{
	using namespace HSR::P15::Tests;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHSRMapSubsystem* Maps = NewObject<UHSRMapSubsystem>(GameInstance);
	Maps->RegisterMapDefinition(*MakeMap(TEXT("Map.A"), TEXT("/Game/Maps/Map_A.Map_A"), TEXT("Region.A"), TEXT("Arrival.A")));
	TArray<FHSRMapArrivalCommitInfo> Events;
	Maps->OnArrivalCommitted().AddLambda([&Events](const FHSRMapArrivalCommitInfo& Info) { Events.Add(Info); });

	TestEqual(TEXT("set location is not arrival"), Maps->SetCurrentLocation(TEXT("Map.A")), EHSRMapOperationResult::Success);
	TestEqual(TEXT("unlock region is not arrival"), Maps->UnlockRegion(TEXT("Region.A")), EHSRMapOperationResult::Success);
	TestEqual(TEXT("flag is not arrival"), Maps->SetExplorationFlag(TEXT("Exploration.Arrival.Test")), EHSRMapOperationResult::Success);
	TestEqual(TEXT("unrelated state emits no arrival"), Events.Num(), 0);

	Maps->PublishArrivalCommittedForAutomation(TEXT("Map.B"), TEXT("Arrival.FromA"), EHSRMapArrivalCommitKind::OrdinaryTravel);
	Maps->PublishArrivalCommittedForAutomation(TEXT("Map.B"), NAME_None, EHSRMapArrivalCommitKind::BattleReturn);
	TestEqual(TEXT("two shared publisher calls emit twice"), Events.Num(), 2);
	TestEqual(TEXT("ordinary generation"), Events[0].CommitGeneration, static_cast<int64>(1));
	TestEqual(TEXT("ordinary map"), Events[0].MapId, FName(TEXT("Map.B")));
	TestEqual(TEXT("ordinary arrival"), Events[0].ArrivalId, FName(TEXT("Arrival.FromA")));
	TestEqual(TEXT("ordinary kind"), Events[0].Kind, EHSRMapArrivalCommitKind::OrdinaryTravel);
	TestEqual(TEXT("battle generation"), Events[1].CommitGeneration, static_cast<int64>(2));
	TestEqual(TEXT("battle arrival none"), Events[1].ArrivalId, NAME_None);
	TestEqual(TEXT("battle kind"), Events[1].Kind, EHSRMapArrivalCommitKind::BattleReturn);
	TestEqual(TEXT("generation getter tracks publisher"), Maps->GetArrivalCommitGeneration(), static_cast<int64>(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRMapSaveProjectionTest, "HSR.Map.SaveV5Projection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRMapSaveProjectionTest::RunTest(const FString&)
{
	using namespace HSR::P15::Tests;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UHSRMapSubsystem* Maps = NewObject<UHSRMapSubsystem>(GameInstance);
	Maps->RegisterMapDefinition(*MakeMap(TEXT("Map.A"), TEXT("/Game/Maps/Map_A.Map_A"), TEXT("Region.A"), TEXT("Arrival.A")));
	Maps->RegisterMapDefinition(*MakeMap(TEXT("Map.B"), TEXT("/Game/Maps/Map_B.Map_B"), TEXT("Region.B"), TEXT("Arrival.B")));
	Maps->RegisterTeleportDefinition(*MakeTeleport(TEXT("Teleport.AB"), TEXT("Map.A"), TEXT("Map.B"), TEXT("Arrival.FromA"), false));
	Maps->SetCurrentLocation(TEXT("Map.B"));
	Maps->UnlockRegion(TEXT("Region.B"));
	Maps->UnlockTeleport(TEXT("Teleport.AB"));
	Maps->SetExplorationFlag(TEXT("Exploration.Chest.B"));

	FHSRMapSaveData Saved;
	Maps->ExportSaveData(Saved);
	FHSRMapRuntimeSnapshot Candidate;
	TestTrue(TEXT("valid map save prepares"), Maps->PrepareRestore(Saved, Candidate));
	TestFalse(TEXT("same candidate is no-op"), Maps->IsRestoreDifferent(Candidate));

	FHSRMapSaveData Duplicate = Saved;
	Duplicate.UnlockedTeleportIds.Add(TEXT("Teleport.AB"));
	TestFalse(TEXT("duplicate teleport id rejected"), Maps->PrepareRestore(Duplicate, Candidate));
	Duplicate = Saved;
	Duplicate.UnlockedRegionIds.Add(TEXT("Region.B"));
	TestFalse(TEXT("duplicate region id rejected"), Maps->PrepareRestore(Duplicate, Candidate));
	Duplicate = Saved;
	Duplicate.ExplorationFlags.Add(TEXT("Exploration.Chest.B"));
	TestFalse(TEXT("duplicate flag rejected"), Maps->PrepareRestore(Duplicate, Candidate));
	FHSRMapSaveData Unknown = Saved;
	Unknown.CurrentLocation.MapId = TEXT("Map.Unknown");
	TestFalse(TEXT("unknown map rejected"), Maps->PrepareRestore(Unknown, Candidate));
	Unknown = Saved;
	Unknown.UnlockedRegionIds.Add(TEXT("Region.Unknown"));
	TestFalse(TEXT("unknown region rejected"), Maps->PrepareRestore(Unknown, Candidate));
	Unknown = Saved;
	Unknown.UnlockedTeleportIds.Add(TEXT("Teleport.Unknown"));
	TestFalse(TEXT("unknown teleport rejected"), Maps->PrepareRestore(Unknown, Candidate));
	FHSRMapSaveData NoneId = Saved;
	NoneId.ExplorationFlags.Add(NAME_None);
	TestFalse(TEXT("none flag rejected"), Maps->PrepareRestore(NoneId, Candidate));
	FHSRMapSaveData NegativeRevision = Saved;
	NegativeRevision.Revision = -1;
	TestFalse(TEXT("negative revision rejected"), Maps->PrepareRestore(NegativeRevision, Candidate));
	FHSRMapSaveData EmptyWithResidue;
	EmptyWithResidue.CurrentLocation.ArrivalId = TEXT("Arrival.Residue");
	TestFalse(TEXT("empty map with arrival rejected"), Maps->PrepareRestore(EmptyWithResidue, Candidate));
	FHSRMapSaveData BadTransform = Saved;
	BadTransform.CurrentLocation.WorldTransform.SetLocation(FVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0));
	TestFalse(TEXT("non-finite transform rejected"), Maps->PrepareRestore(BadTransform, Candidate));
	TestTrue(TEXT("failed candidates preserve runtime"), Maps->GetSnapshot().CurrentLocation.MapId == Saved.CurrentLocation.MapId);

	FHSRMapSaveData Different = Saved;
	Different.ExplorationFlags.Add(TEXT("Exploration.Story.B"));
	Different.Revision++;
	TestTrue(TEXT("different valid candidate prepares"), Maps->PrepareRestore(Different, Candidate));
	TestTrue(TEXT("different candidate detected"), Maps->IsRestoreDifferent(Candidate));
	int32 Broadcasts = 0;
	Maps->OnMapStateChanged().AddLambda([&Broadcasts](const FHSRMapRuntimeSnapshot&) { ++Broadcasts; });
	Maps->CommitRestore(MoveTemp(Candidate), true);
	TestEqual(TEXT("restore notifies once"), Broadcasts, 1);
	FHSRMapRuntimeSnapshot Repeat;
	TestTrue(TEXT("repeat candidate prepares"), Maps->PrepareRestore(Different, Repeat));
	TestFalse(TEXT("repeat load detected as no-op"), Maps->IsRestoreDifferent(Repeat));
	return true;
}

#endif
