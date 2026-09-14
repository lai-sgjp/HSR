#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Curves/CurveFloat.h"
#include "../Exploration/HSRSceneContent.h"
#include "../Map/HSRSafePlacement.h"
#include "../Map/HSRMapArrivalPoint.h"
#include "../Map/HSRMapSubsystem.h"
#include "../Data/Definitions/HSRMapDefinition.h"
#include "../Data/Definitions/HSRTeleportDefinition.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Save/HSRSaveSubsystem.h"

namespace HSRSafePlacementPresentationTests
{
	struct FFixture
	{
		UGameInstance* GI = nullptr;
		UWorld* World = nullptr;
		APawn* Pawn = nullptr;
		UHSRMapSubsystem* Maps = nullptr;
		AHSRMapArrivalPoint* Arrival = nullptr;
		FName MapId = TEXT("Map.Placement.Automation");
		FName ArrivalId = TEXT("Arrival.Placement.Safe");

		~FFixture()
		{
			if (!GI) return;
			GI->Shutdown();
			if (World) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
			GI->RemoveFromRoot();
		}

		UBoxComponent* Box(const FVector& Location, const FVector& Extent)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			auto* Component = NewObject<UBoxComponent>(Actor);
			Actor->SetRootComponent(Component);
			Actor->AddInstanceComponent(Component);
			Component->SetBoxExtent(Extent);
			Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Component->SetCollisionObjectType(ECC_WorldStatic);
			Component->SetCollisionResponseToAllChannels(ECR_Block);
			Component->RegisterComponent();
			Actor->SetActorLocation(Location);
			return Component;
		}

		AHSRMapArrivalPoint* AddArrival(FName Id, const FVector& Location)
		{
			auto* Point = World->SpawnActor<AHSRMapArrivalPoint>();
			Point->ArrivalId = Id;
			Point->SetActorLocation(Location);
			return Point;
		}

		bool Initialize(FAutomationTestBase& Test, bool bRebuilt = true)
		{
			if (!GEngine) return false;
			GI = NewObject<UGameInstance>(GEngine);
			GI->AddToRoot();
			GI->InitializeStandalone(FName(*FString::Printf(TEXT("HSRSafePlacement_%s"),
				*FGuid::NewGuid().ToString(EGuidFormats::Digits))));
			World = GI->GetWorld();
			if (!Test.TestNotNull(TEXT("Placement world"), World)) return false;
			Maps = NewObject<UHSRMapSubsystem>(GI);
			auto* Map = NewObject<UHSRMapDefinition>(GI);
			Map->MapId = MapId;
			Map->RegionId = TEXT("Region.Placement");
			Map->DefaultArrivalId = ArrivalId;
			Map->World = TSoftObjectPtr<UWorld>(World);
			if (!Test.TestEqual(TEXT("Map definition registered"), Maps->RegisterMapDefinition(*Map), EHSRMapOperationResult::Success)) return false;
			Maps->SetCurrentLocation(MapId);
			Box(FVector(0, 0, -20), FVector(1000, 1000, 20));
			Pawn = World->SpawnActor<APawn>();
			auto* Capsule = NewObject<UCapsuleComponent>(Pawn);
			Pawn->SetRootComponent(Capsule);
			Pawn->AddInstanceComponent(Capsule);
			Capsule->InitCapsuleSize(34.f, 90.f);
			Capsule->SetCollisionProfileName(TEXT("Pawn"));
			Capsule->RegisterComponent();
			Pawn->SetActorLocation(FVector(0, 0, 93));
			auto* PC = World->SpawnActor<APlayerController>();
			// This isolated collision world deliberately does not run BeginPlay. Register the
			// controller as AController::PostInitializeComponents normally would, so restore
			// exercises the production GetFirstPlayerController path rather than PawnUnavailable.
			World->AddController(PC);
			PC->Possess(Pawn);
			if (!Test.TestTrue(TEXT("Restore controller owns fixture pawn"),
				World->GetFirstPlayerController() && World->GetFirstPlayerController()->GetPawn() == Pawn)) return false;
			FName ResolvedMap;
			if (!Test.TestTrue(TEXT("Fixture world resolves to registered map"),
				Maps->ResolveMapIdByPackage(World->GetOutermost()->GetFName(), ResolvedMap))
				|| !Test.TestEqual(TEXT("Fixture map identity"), ResolvedMap, MapId)) return false;
			Arrival = AddArrival(ArrivalId, FVector(-500, 0, 110));
			if (bRebuilt) World->SpawnActor<AHSRSceneContent>();
			return true;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRLegacyPlacementGeometryTest,
	"HSR.Map.SafePlacement.LegacyGeometryFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRLegacyPlacementGeometryTest::RunTest(const FString&)
{
	using namespace HSRSafePlacementPresentationTests;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	F.Box(FVector(400, 0, 150), FVector(100, 100, 150));
	FTransform Resolved;
	FName Fallback;
	const FVector InvalidPositions[] = {FVector(400, 0, 93), FVector(0, 0, 500),
		FVector(5000, 0, 93), FVector(990, 0, 93)};
	for (const FVector& Position : InvalidPositions)
	{
		TestTrue(TEXT("Invalid legacy position resolves safely"), HSRSafePlacement::Resolve(
			F.Pawn, FTransform(Position), Resolved, F.ArrivalId, &Fallback));
		TestEqual(TEXT("Resolution identifies safe arrival"), Fallback, F.ArrivalId);
		TestTrue(TEXT("Resolution uses safe floor beneath arrival"), Resolved.GetLocation().Equals(FVector(-500, 0, 93), .1));
	}
	TestTrue(TEXT("Open reachable floor is accepted"), HSRSafePlacement::Resolve(
		F.Pawn, FTransform(FVector(250, 250, 96)), Resolved, F.ArrivalId, &Fallback));
	TestTrue(TEXT("Valid position keeps its horizontal coordinates"), Resolved.GetLocation().Equals(FVector(250, 250, 93), .1));
	TestTrue(TEXT("Valid placement does not claim an arrival fallback"), Fallback.IsNone());
	TestTrue(TEXT("Resolver is pure; pawn does not move"), F.Pawn->GetActorLocation().Equals(FVector(0, 0, 93)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRAmbiguousPlacementTest,
	"HSR.Map.SafePlacement.DefaultArrivalIdentityAndFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRAmbiguousPlacementTest::RunTest(const FString&)
{
	using namespace HSRSafePlacementPresentationTests;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	const FTransform Invalid(FVector(0, 0, 900));
	FTransform Resolved;
	F.AddArrival(TEXT("Arrival.Other"), FVector(500, 500, 110));
	TestTrue(TEXT("Unrelated arrivals do not make configured default ambiguous"),
		HSRSafePlacement::Resolve(F.Pawn, Invalid, Resolved, F.ArrivalId));
	F.AddArrival(F.ArrivalId, FVector(500, 500, 110));
	const FTransform Sentinel(FVector(1, 2, 3));
	Resolved = Sentinel;
	TestFalse(TEXT("Duplicate default arrivals fail closed"), HSRSafePlacement::Resolve(F.Pawn, Invalid, Resolved, F.ArrivalId));
	TestTrue(TEXT("Failure does not overwrite result"), Resolved.Equals(Sentinel));
	const auto Before = F.Maps->GetSnapshot();
	FHSRMapRuntimeSnapshot Candidate = Before;
	Candidate.CurrentLocation.WorldTransform = Invalid;
	const FVector PawnBefore = F.Pawn->GetActorLocation();
	TestEqual(TEXT("Ambiguous fallback rejects restore placement"), F.Maps->ApplyRestoreLocation(Candidate), EHSRMapOperationResult::PlacementFailed);
	TestTrue(TEXT("Failed placement does not move pawn"), F.Pawn->GetActorLocation().Equals(PawnBefore));
	TestTrue(TEXT("Failed placement does not rewrite candidate"), Candidate.CurrentLocation.WorldTransform.Equals(Invalid));
	TestTrue(TEXT("Failed placement leaves runtime location unchanged"), F.Maps->GetSnapshot().CurrentLocation == Before.CurrentLocation);
	TestFalse(TEXT("Failed placement cannot create ordinary travel"), F.Maps->HasPendingTravel());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRPlacementSaveSnapshotTest,
	"HSR.Map.SafePlacement.SaveAndBattleReturnStayConsistent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRPlacementSaveSnapshotTest::RunTest(const FString&)
{
	using namespace HSRSafePlacementPresentationTests;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	auto* Profiles = NewObject<UHSRCharacterProfileSubsystem>(F.GI);
	auto* Party = NewObject<UHSRPartySubsystem>(F.GI);
	auto* Equipment = NewObject<UHSREquipmentSubsystem>(F.GI);
	auto* Save = NewObject<UHSRSaveSubsystem>(F.GI);
	auto* Character = NewObject<UHSRCharacterDefinition>(F.GI);
	Character->CharacterId = TEXT("Character.Placement");
	Character->MaxLevel = 2;
	auto* Curve = NewObject<UCurveFloat>(Character);
	Curve->FloatCurve.AddKey(2.f, 100.f);
	Character->CumulativeExperienceCurve = Curve;
	Profiles->RegisterDefinition(Character);
	Party->InitializeForDevelopmentTest(Profiles);
	Party->AddCharacter(Character->CharacterId);
	Save->InitializeForDevelopmentTest(Profiles, Party, Equipment, nullptr, nullptr, nullptr, F.Maps);
	FHSRSaveData Saved;
	if (!TestEqual(TEXT("Save fixture captures"), Save->SaveSnapshot(Saved), EHSRSaveResult::Success)) return false;
	Saved.Map.CurrentLocation.WorldTransform = FTransform(FVector(0, 0, 2000));
	if (!TestEqual(TEXT("Legacy airborne save restores"), Save->LoadSnapshot(Saved), EHSRSaveResult::Success)) return false;
	const FTransform Actual = F.Pawn->GetActorTransform();
	TestTrue(TEXT("Pawn lands at safe arrival"), Actual.GetLocation().Equals(FVector(-500, 0, 93), .1));
	TestTrue(TEXT("Runtime snapshot stores actual restored transform"), F.Maps->GetSnapshot().CurrentLocation.WorldTransform.Equals(Actual));
	TestTrue(TEXT("Public save snapshot stores actual restored transform"), Save->GetSnapshot().Map.CurrentLocation.WorldTransform.Equals(Actual));
	TestEqual(TEXT("Fallback arrival identity is retained"), Save->GetSnapshot().Map.CurrentLocation.ArrivalId, F.ArrivalId);
	TestFalse(TEXT("Same-world restoration creates no travel"), F.Maps->HasPendingTravel());
	TestEqual(TEXT("Invalid battle return also falls back safely"), F.Maps->CommitBattleReturnLocation(
		F.MapId, F.Pawn, FTransform(FVector(5000, 0, 93))), EHSRMapOperationResult::Success);
	TestTrue(TEXT("Battle return snapshot matches pawn"), F.Maps->GetSnapshot().CurrentLocation.WorldTransform.Equals(F.Pawn->GetActorTransform()));
	TestFalse(TEXT("Battle return does not create ordinary travel state"), F.Maps->HasPendingTravel());
	const int64 Generation = F.Maps->GetArrivalCommitGeneration();
	TestEqual(TEXT("Foreign map return rejected"), F.Maps->CommitBattleReturnLocation(TEXT("Map.Unknown"), F.Pawn, FTransform::Identity), EHSRMapOperationResult::UnknownMap);
	TestEqual(TEXT("Rejected return cannot publish arrival"), F.Maps->GetArrivalCommitGeneration(), Generation);
	auto* OtherMap = NewObject<UHSRMapDefinition>(F.GI);
	OtherMap->MapId = TEXT("Map.Placement.Other");
	OtherMap->RegionId = TEXT("Region.Placement.Other");
	OtherMap->DefaultArrivalId = TEXT("Arrival.Placement.Other");
	OtherMap->World = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/Map_PlacementOther.Map_PlacementOther")));
	TestEqual(TEXT("Travel destination registered"), F.Maps->RegisterMapDefinition(*OtherMap), EHSRMapOperationResult::Success);
	TestEqual(TEXT("Travel destination region unlocked"), F.Maps->UnlockRegion(OtherMap->RegionId), EHSRMapOperationResult::Success);
	auto* Teleport = NewObject<UHSRTeleportDefinition>(F.GI);
	Teleport->TeleportId = TEXT("Teleport.Placement.Other");
	Teleport->SourceMapId = F.MapId;
	Teleport->DestinationMapId = OtherMap->MapId;
	Teleport->DestinationArrivalId = OtherMap->DefaultArrivalId;
	Teleport->bInitiallyUnlocked = true;
	TestEqual(TEXT("Travel point registered"), F.Maps->RegisterTeleportDefinition(*Teleport), EHSRMapOperationResult::Success);
	if (!TestEqual(TEXT("Ordinary travel can be staged independently"), F.Maps->StageTeleportForAutomation(Teleport->TeleportId), EHSRMapOperationResult::Success)) return false;
	FHSRTeleportRequest Pending, After;
	F.Maps->GetPendingRequest(Pending);
	TestEqual(TEXT("Battle return cannot overtake ordinary travel"), F.Maps->CommitBattleReturnLocation(F.MapId, F.Pawn, Actual), EHSRMapOperationResult::AlreadyPending);
	TestTrue(TEXT("Ordinary travel survives rejected battle return"), F.Maps->GetPendingRequest(After));
	TestEqual(TEXT("Ordinary request identity is unchanged"), After.RequestId, Pending.RequestId);
	F.Maps->CancelPendingTravel(Pending.RequestId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRHistoricalPlacementContractTest,
	"HSR.Map.SafePlacement.HistoricalMapsRemainUnchanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRHistoricalPlacementContractTest::RunTest(const FString&)
{
	HSRSafePlacementPresentationTests::FFixture F;
	if (!F.Initialize(*this, false)) return false;
	const FTransform Original(FVector(0, 0, 5000));
	FTransform Resolved;
	TestTrue(TEXT("Historical map preserves old contract"), HSRSafePlacement::Resolve(F.Pawn, Original, Resolved));
	TestTrue(TEXT("Historical transform is not normalized"), Resolved.Equals(Original));
	return true;
}

#endif
