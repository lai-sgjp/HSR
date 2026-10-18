#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "UObject/StrongObjectPtr.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../UI/Character/Preview/HSRCharacterPreviewSession.h"
#include "../UI/Character/Preview/HSRCharacterPreviewStage.h"

namespace HSR::CharacterPreviewTests
{
	struct FFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
		TStrongObjectPtr<UGameInstance> Instance{NewObject<UGameInstance>()};
		TStrongObjectPtr<UHSRCharacterProfileSubsystem> Profiles{NewObject<UHSRCharacterProfileSubsystem>(Instance.Get())};
		TStrongObjectPtr<UHSRCharacterPreviewSession> Session{NewObject<UHSRCharacterPreviewSession>()};
		FFixture() { Session->Initialize(World, Profiles.Get()); }
		~FFixture() { Session->Close(); if (World) World->DestroyWorld(false); }
		UHSRCharacterDefinition* Register(FAutomationTestBase& Test, FName Id, USkeletalMesh* Mesh = nullptr)
		{
			auto* Definition = NewObject<UHSRCharacterDefinition>(Instance.Get());
			Definition->CharacterId = Id;
			Definition->MaxLevel = 1;
			Definition->CumulativeExperienceCurve = NewObject<UCurveFloat>(Definition);
			Definition->CharacterMesh = Mesh;
			if (!Test.TestEqual(TEXT("Preview fixture registers through real profile authority"),
				Profiles->RegisterDefinition(Definition), EHSRCharacterProfileResult::Success)) return nullptr;
			return Definition;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRCharacterPreviewIsolationTest,
	"HSR.UI.CharacterPreview.StageIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRCharacterPreviewIsolationTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
	if (!TestNotNull(TEXT("Isolated test world"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* Stage = World->SpawnActor<AHSRCharacterPreviewStage>();
	if (!TestNotNull(TEXT("Presentation stage spawns without a gameplay Pawn"), Stage)) return false;
	auto* Mesh = Stage->GetPreviewMesh();
	auto* Capture = Stage->GetCaptureComponent();
	TestNull(TEXT("Stage carries no ability system"), Stage->FindComponentByClass<UAbilitySystemComponent>());
	TestFalse(TEXT("Stage does not replicate"), Stage->GetIsReplicated());
	TestFalse(TEXT("Stage cannot run gameplay tick"), Stage->PrimaryActorTick.bCanEverTick);
	TestFalse(TEXT("Stage actor collision disabled"), Stage->GetActorEnableCollision());
	TestEqual(TEXT("Mesh cannot collide with world"), Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Mesh cannot generate gameplay overlaps"), Mesh->GetGenerateOverlapEvents());
	TestFalse(TEXT("Mesh has no physics simulation"), Mesh->IsSimulatingPhysics());
	TestTrue(TEXT("Main camera cannot see stage mesh regardless of location"), Mesh->bVisibleInSceneCaptureOnly != 0);
	TestFalse(TEXT("Capture does not render automatically every frame"), Capture->bCaptureEveryFrame != 0);
	TestFalse(TEXT("Camera movement does not add a duplicate automatic capture"), Capture->bCaptureOnMovement != 0);
	TestEqual(TEXT("Capture excludes arbitrary world primitives"), Capture->PrimitiveRenderMode,
		ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList);
	Stage->Configure(nullptr, nullptr, 0.f, FVector::ZeroVector, 1.f);
	TestEqual(TEXT("ShowOnly contains exactly the display mesh"), Capture->ShowOnlyComponents.Num(), 1);
	TestTrue(TEXT("ShowOnly display mesh identity"), Capture->ShowOnlyComponents.Contains(Mesh));
	TestFalse(TEXT("Missing animation remains a safe static presentation"), Stage->HasAnimation());
	Stage->Stop();
	Stage->Stop();
	TestNull(TEXT("Idempotent stop detaches rendering destination"), Capture->TextureTarget.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRCharacterPreviewFramingTest,
	"HSR.UI.CharacterPreview.BoundsFramingAndControls", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRCharacterPreviewFramingTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
	if (!TestNotNull(TEXT("Framing world"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* Stage = World->SpawnActor<AHSRCharacterPreviewStage>();
	if (!TestNotNull(TEXT("Framing stage"), Stage)) return false;
	Stage->Configure(nullptr, nullptr, 37.f, FVector(0.f, 15.f, 30.f), 1.f);
	for (float Aspect : {0.25f, 0.6666667f, 1.f, 1.7777778f, 4.f})
	{
		Stage->SetAspectRatio(Aspect);
		const float Radius = Stage->GetBoundsRadius();
		const float Distance = Stage->GetCameraDistance();
		const float HalfHorizontal = FMath::DegreesToRadians(Stage->GetCaptureComponent()->FOVAngle * 0.5f);
		const float HalfVertical = FMath::Atan(FMath::Tan(HalfHorizontal) / Aspect);
		// Independent geometric criterion: a sphere's angular radius must fit both frustum axes.
		const float AngularRadius = FMath::Asin(Radius / Distance);
		TestTrue(TEXT("Default view contains the entire conservative bounds sphere"),
			FMath::IsFinite(Distance) && Distance > Radius && AngularRadius < FMath::Min(HalfHorizontal, HalfVertical));
		for (int32 Index = 0; Index < 50; ++Index) Stage->Zoom(100.f);
		const float ClosestAngle = FMath::Asin(Radius / Stage->GetCameraDistance());
		TestTrue(TEXT("Closest zoom never clips bounds or penetrates model"),
			Stage->GetCameraDistance() > Radius && ClosestAngle < FMath::Min(HalfHorizontal, HalfVertical));
		for (int32 Index = 0; Index < 50; ++Index) Stage->Zoom(-100.f);
		TestTrue(TEXT("Farthest zoom remains bounded"), Stage->GetCameraDistance() <= Stage->GetFitDistance() * 1.61f);
		Stage->Rotate(120.f);
		TestFalse(TEXT("Pixel drag changes presentation yaw"), FMath::IsNearlyEqual(Stage->GetPreviewYaw(), 37.f));
		Stage->ResetView();
		TestTrue(TEXT("Reset restores configured yaw"), FMath::IsNearlyEqual(Stage->GetPreviewYaw(), 37.f));
		TestEqual(TEXT("Reset restores automatic fitted distance"), Stage->GetCameraDistance(), Stage->GetFitDistance());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRCharacterPreviewErrorLifecycleTest,
	"HSR.UI.CharacterPreview.ErrorAndRepeatedLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRCharacterPreviewErrorLifecycleTest::RunTest(const FString&)
{
	using namespace HSR::CharacterPreviewTests;
	FFixture Fixture;
	if (!TestNotNull(TEXT("Lifecycle world"), Fixture.World)) return false;
	auto* Definition = Fixture.Register(*this, TEXT("Automation.Preview.NoMesh"));
	if (!Definition) return false;
	FHSRCharacterProfileSnapshot Before;
	Fixture.Profiles->GetProfileSnapshot(Definition->CharacterId, Before);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		Fixture.Session->Initialize(Fixture.World, Fixture.Profiles.Get());
		TestEqual(TEXT("Reinitialized session begins closed"), Fixture.Session->GetState(), EHSRCharacterPreviewState::Closed);
		Fixture.Session->RequestCharacter(TEXT("Automation.Preview.Unknown"));
		TestEqual(TEXT("Unknown ID completes with an explicit error"), Fixture.Session->GetState(), EHSRCharacterPreviewState::Error);
		TestFalse(TEXT("Unknown ID has useful failure text"), Fixture.Session->GetErrorText().IsEmpty());
		Fixture.Session->RequestCharacter(Definition->CharacterId);
		TestEqual(TEXT("Missing mesh never remains loading"), Fixture.Session->GetState(), EHSRCharacterPreviewState::Error);
		TestNull(TEXT("Failed request allocates no stage"), Fixture.Session->GetStage());
		const int64 Generation = Fixture.Session->GetRequestGeneration();
		Fixture.Session->Close();
		Fixture.Session->Close();
		TestTrue(TEXT("Close invalidates all prior requests"), Fixture.Session->GetRequestGeneration() > Generation);
		TestEqual(TEXT("Repeated close remains closed"), Fixture.Session->GetState(), EHSRCharacterPreviewState::Closed);
		TestNull(TEXT("Closed session releases render target"), Fixture.Session->GetRenderTarget());
		TestEqual(TEXT("Error and closed sessions produce no capture"), Fixture.Session->GetCaptureCount(), 0);
	}
	FHSRCharacterProfileSnapshot After;
	TestTrue(TEXT("Profile remains available after preview errors"), Fixture.Profiles->GetProfileSnapshot(Definition->CharacterId, After));
	TestEqual(TEXT("Preview never advances authoritative revision"), After.RuntimeRevision, Before.RuntimeRevision);
	TestEqual(TEXT("Preview never changes level"), After.RuntimeState.Level, Before.RuntimeState.Level);
	TestEqual(TEXT("Preview never changes experience"), After.RuntimeState.Experience, Before.RuntimeState.Experience);
	Fixture.Session->Initialize(nullptr, Fixture.Profiles.Get());
	Fixture.Session->RequestCharacter(Definition->CharacterId);
	TestEqual(TEXT("Missing world fails safely"), Fixture.Session->GetState(), EHSRCharacterPreviewState::Error);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRCharacterPreviewAsyncCancellationTest,
	"HSR.UI.CharacterPreview.RealAsyncCloseInvalidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRCharacterPreviewAsyncCancellationTest::RunTest(const FString&)
{
	using namespace HSR::CharacterPreviewTests;
	auto Fixture = MakeShared<FFixture>();
	if (!TestNotNull(TEXT("Async fixture world"), Fixture->World)) return false;
	// Use a genuine project asset, not a fabricated handle or a test-only completion callback.
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr,
		TEXT("/Game/Characters/Player/SKM_Kachujin_G_Rosales.SKM_Kachujin_G_Rosales"));
	if (!TestNotNull(TEXT("Existing project mesh for actual StreamableManager request"), Mesh)) return false;
	auto* Definition = Fixture->Register(*this, TEXT("Automation.Preview.Cancelled"), Mesh);
	if (!Definition) return false;
	Fixture->Session->RequestCharacter(Definition->CharacterId);
	TestEqual(TEXT("Actual request enters Loading"), Fixture->Session->GetState(), EHSRCharacterPreviewState::Loading);
	const int64 Generation = Fixture->Session->GetRequestGeneration();
	Fixture->Session->RequestCharacter(Definition->CharacterId);
	TestEqual(TEXT("Duplicate selection does not restart the pending request"), Fixture->Session->GetRequestGeneration(), Generation);
	Fixture->Session->RequestCharacter(TEXT("Automation.Preview.NewerInvalid"));
	TestEqual(TEXT("Newer invalid request supersedes pending valid request"), Fixture->Session->GetState(), EHSRCharacterPreviewState::Error);
	Fixture->Session->Close();
	const double Started = FPlatformTime::Seconds();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Fixture, Started]()
	{
		// The normal engine loop advances async delegates/core ticker during this wait.
		if (FPlatformTime::Seconds() - Started < 0.5) return false;
		TestEqual(TEXT("Cancelled async completion cannot reopen session"), Fixture->Session->GetState(), EHSRCharacterPreviewState::Closed);
		TestNull(TEXT("Cancelled completion cannot install a stage"), Fixture->Session->GetStage());
		TestNull(TEXT("Cancelled completion cannot allocate a render target"), Fixture->Session->GetRenderTarget());
		int32 Count = 0;
		for (TActorIterator<AHSRCharacterPreviewStage> It(Fixture->World); It; ++It) ++Count;
		TestEqual(TEXT("No orphan actor from stale completion"), Count, 0);
		TestEqual(TEXT("Closed pending request generates no capture"), Fixture->Session->GetCaptureCount(), 0);
		return true;
	}));
	return true;
}

#endif
