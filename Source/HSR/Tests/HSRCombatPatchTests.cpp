#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "../Battle/HSRBattleCoordinator.h"
#include "../Battle/HSRBattleGameMode.h"
#include "../Battle/HSRBattleParticipant.h"
#include "../Battle/HSRTurnManager.h"
#include "../Data/HSRSkillDefinition.h"
#include "../Data/Definitions/HSRStatusDefinition.h"
#include "../GAS/HSRAbilitySystemComponent.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "../Status/HSRStatusComponent.h"
#include "../Data/Definitions/HSREnemyDefinition.h"
#include "../Enemy/HSREnemyAIController.h"
#include "../Enemy/HSREnemyCharacter.h"
#include <limits>

namespace HSRActionDistanceAutomation
{
	struct FSnapshot
	{
		float Speed = 0.0f;
		float Base = 0.0f;
		float Remaining = 0.0f;
		int32 Pending = 0;
	};

	static FHSRBattleParticipant MakeParticipant(UWorld* World, FName Id, float Speed, EHSRBattleParticipantTeam Team = EHSRBattleParticipantTeam::Player)
	{
		AActor* Actor = World ? World->SpawnActor<AActor>() : nullptr;
		UAbilitySystemComponent* ASC = Actor
			? Cast<UAbilitySystemComponent>(Actor->AddComponentByClass(UHSRAbilitySystemComponent::StaticClass(), false, FTransform::Identity, false)) : nullptr;
		if (ASC)
		{
			ASC->InitStats(UHSRCoreAttributeSet::StaticClass(), nullptr);
			ASC->InitAbilityActorInfo(Actor, Actor);
			ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 1000.0f);
			ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 1000.0f);
			ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), Speed);
		}
		FHSRBattleParticipant Participant;
		Participant.ParticipantId = Id;
		Participant.DefinitionId = FName(*FString::Printf(TEXT("Definition.%s"), *Id.ToString()));
		Participant.Team = Team;
		Participant.Actor = Actor;
		Participant.AbilitySystemComponent = ASC;
		return Participant;
	}

	static FSnapshot Read(UHSRTurnManager* Manager, FName Id)
	{
		FSnapshot Out;
		if (Manager) Manager->GetActionDistanceForAutomation(Id, Out.Speed, Out.Base, Out.Remaining, Out.Pending);
		return Out;
	}

	static bool SameSnapshot(const FSnapshot& A, const FSnapshot& B)
	{
		return FMath::IsNearlyEqual(A.Speed, B.Speed, 1.e-4f)
			&& FMath::IsNearlyEqual(A.Base, B.Base, 1.e-4f)
			&& FMath::IsNearlyEqual(A.Remaining, B.Remaining, 1.e-4f)
			&& A.Pending == B.Pending;
	}

	static FHSRActionDistanceResult Request(UHSRTurnManager* Manager, FName Target, EHSRActionDistanceAdjustmentKind Kind, float Ratio, FGuid OperationId = FGuid())
	{
		FHSRActionDistanceRequest Value;
		Value.BattleEpoch = Manager ? Manager->GetBattleEpoch() : 0;
		Value.OperationId = OperationId.IsValid() ? OperationId : FGuid::NewGuid();
		Value.TargetParticipantId = Target;
		Value.Kind = Kind;
		Value.Ratio = Ratio;
		return Manager->RequestActionDistanceAdjustment(Value);
	}

	static void BroadcastSpeed(UAbilitySystemComponent* ASC, float OldValue, float NewValue)
	{
		FOnAttributeChangeData Data;
		Data.Attribute = UHSRCoreAttributeSet::GetSpeedAttribute();
		Data.OldValue = OldValue;
		Data.NewValue = NewValue;
		ASC->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetSpeedAttribute()).Broadcast(Data);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRStatusGenericPatchTest, "HSR.Battle.Patch.StatusGeneric", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRRepeatableBreakPatchTest, "HSR.Battle.Patch.RepeatableBreak", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRActionDistancePatchTest, "HSR.Battle.Patch.ActionDistance.Baseline", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRActionDistanceCurrentPendingPatchTest, "HSR.Battle.Patch.ActionDistance.CurrentPending", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRActionDistanceLifecyclePatchTest, "HSR.Battle.Patch.ActionDistance.LifecycleOrdering", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRActionDistanceThreeParticipantPatchTest, "HSR.Battle.Patch.ActionDistance.ThreeParticipant", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRActionDistanceRequestMatrixPatchTest, "HSR.Battle.Patch.ActionDistance.RequestMatrix", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRActionDistanceNumericLifecyclePatchTest, "HSR.Battle.Patch.ActionDistance.NumericAndBinding", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRBehaviorTreeAdapterPatchTest, "HSR.Exploration.Patch.BehaviorTreeAdapter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRBehaviorTreeAdapterPatchTest::RunTest(const FString& Parameters)
{
	UHSREnemyDefinition* Definition = NewObject<UHSREnemyDefinition>();
	TestEqual(TEXT("Behavior Tree default reference is the Stage-A user asset"), Definition->BehaviorTreeAsset.ToSoftObjectPath().ToString(), FString(TEXT("/Game/AI/Enemy/BT_HSREnemy_Exploration.BT_HSREnemy_Exploration")));
	TestEqual(TEXT("Blackboard default reference is the Stage-A user asset"), Definition->BlackboardAsset.ToSoftObjectPath().ToString(), FString(TEXT("/Game/AI/Enemy/BB_HSREnemy_Exploration.BB_HSREnemy_Exploration")));
	TestTrue(TEXT("Recovery state is distinct from LostTarget and MoveFailed"), EHSREnemyExplorationState::ReturningToSpawnOrigin != EHSREnemyExplorationState::LostTarget && EHSREnemyExplorationState::ReturningToSpawnOrigin != EHSREnemyExplorationState::MoveFailed);
	AHSREnemyAIController* Controller = NewObject<AHSREnemyAIController>();
	TestFalse(TEXT("Behavior Tree adapter does not enable Actor Tick"), Controller->PrimaryActorTick.bCanEverTick);
	TestEqual(TEXT("Fresh controller begins at epoch zero before Possess"), Controller->GetBehaviorTreeEpoch(), 0);
	const FVector ExpectedSpawnOrigin(137.0f, -29.0f, 11.0f);
	const FVector CandidatePatrolLocation(291.0f, -83.0f, 11.0f);
	Controller->PublishPatrolIntentForAutomation(nullptr, ExpectedSpawnOrigin, CandidatePatrolLocation, true);
	TestEqual(TEXT("BT initialization publishes a patrol state instead of Idle"), Controller->GetCurrentState(), EHSREnemyExplorationState::MovingToPatrol);
	FVector PatrolLocation = FVector::ZeroVector;
	TestTrue(TEXT("BT initialization writes PatrolLocation"), Controller->GetPatrolLocationForAutomation(PatrolLocation));
	TestEqual(TEXT("Reachable patrol candidate is published without a movement request"), PatrolLocation, CandidatePatrolLocation);
	Controller->PublishPatrolIntentForAutomation(nullptr, ExpectedSpawnOrigin, FVector::ZeroVector, false);
	TestEqual(TEXT("Unreachable patrol fallback waits instead of issuing a movement loop"), Controller->GetCurrentState(), EHSREnemyExplorationState::PatrolWaiting);
	TestTrue(TEXT("Unreachable patrol fallback publishes SpawnOrigin"), Controller->GetPatrolLocationForAutomation(PatrolLocation));
	TestEqual(TEXT("Unreachable patrol fallback location is SpawnOrigin"), PatrolLocation, ExpectedSpawnOrigin);
	TestTrue(TEXT("Nav-ready retry arms once"), Controller->ArmNavReadyRetryForAutomation(7));
	TestFalse(TEXT("Nav-ready retry refuses a second pending arm"), Controller->ArmNavReadyRetryForAutomation(7));
	TestFalse(TEXT("Nav-ready retry rejects stale epoch consumption"), Controller->ConsumeNavReadyRetryForAutomation(8));
	TestTrue(TEXT("Nav-ready retry consumes matching epoch exactly once"), Controller->ConsumeNavReadyRetryForAutomation(7));
	TestFalse(TEXT("Nav-ready retry cannot repeat after consumption"), Controller->ConsumeNavReadyRetryForAutomation(7));
	UWorld* OriginWorld = UWorld::CreateWorld(EWorldType::GamePreview, false);
	ON_SCOPE_EXIT { if (OriginWorld) OriginWorld->DestroyWorld(false); };
	AHSREnemyCharacter* OriginEnemy = OriginWorld ? OriginWorld->SpawnActor<AHSREnemyCharacter>() : nullptr;
	if (!TestNotNull(TEXT("Origin fallback fixture spawns Enemy"), OriginEnemy))
	{
		return false;
	}
	const FVector PreBeginPlayLocation(701.0f, -113.0f, 42.0f);
	OriginEnemy->SetActorLocation(PreBeginPlayLocation);
	TestEqual(TEXT("Pre-BeginPlay origin falls back to ActorLocation"), OriginEnemy->GetSpawnOrigin(), PreBeginPlayLocation);
	OriginEnemy->CaptureSpawnOriginForAutomation();
	OriginEnemy->SetActorLocation(FVector(999.0f, 999.0f, 999.0f));
	TestEqual(TEXT("Post-BeginPlay captured origin remains stable"), OriginEnemy->GetSpawnOrigin(), PreBeginPlayLocation);
	return true;
}

bool FHSRActionDistancePatchTest::RunTest(const FString& Parameters)
{
	if (!GEngine) return false;
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine); GameInstance->AddToRoot();
	UWorld* World = nullptr; UHSRBattleCoordinator* Coordinator = nullptr;
	ON_SCOPE_EXIT { if (Coordinator) Coordinator->Reset(); if (GameInstance) { GameInstance->Shutdown(); if (World) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); } GameInstance->RemoveFromRoot(); } };
	GameInstance->InitializeStandalone(FName(*FString::Printf(TEXT("HSRActionDistance_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))));
	World = GameInstance->GetWorld();
	TSubclassOf<AHSRBattleGameMode> Class = LoadClass<AHSRBattleGameMode>(nullptr, TEXT("/Game/Blueprints/Framework/BP_HSRBattleGameMode.BP_HSRBattleGameMode_C"));
	FText Failure; Coordinator = World && Class ? AHSRBattleGameMode::CreateRepeatableBreakAutomationFixture(GameInstance, World, Class, Failure) : nullptr;
	if (!TestNotNull(TEXT("Action-distance fixture builds"), Coordinator) || !TestNotNull(TEXT("Action-distance manager exists"), Coordinator->GetTurnManager())) return false;
	UHSRTurnManager* Manager = Coordinator->GetTurnManager();
	const FName Current = Manager->GetCurrentParticipantId();
	const uint64 Epoch = Manager->GetBattleEpoch(); const uint64 Sequence = Manager->GetTurnSequence();
	const FHSRBattleParticipant* CurrentParticipant = Manager->GetOrderedParticipants().FindByPredicate([Current](const FHSRBattleParticipant& P){ return P.ParticipantId == Current; });
	const FHSRBattleParticipant* OtherParticipant = Manager->GetOrderedParticipants().FindByPredicate([Current](const FHSRBattleParticipant& P){ return P.ParticipantId != Current; });
	if (!TestNotNull(TEXT("Current participant is present"), CurrentParticipant) || !TestNotNull(TEXT("Other participant is present"), OtherParticipant)) return false;
	float OldSpeed=0, OldBase=0, OldRemaining=0; int32 OldPending=0;
	Manager->GetActionDistanceForAutomation(OtherParticipant->ParticipantId, OldSpeed, OldBase, OldRemaining, OldPending);
	OtherParticipant->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), OldSpeed * 2.0f);
	float NewSpeed=0, NewBase=0, NewRemaining=0; int32 NewPending=0;
	Manager->GetActionDistanceForAutomation(OtherParticipant->ParticipantId, NewSpeed, NewBase, NewRemaining, NewPending);
	TestTrue(TEXT("Non-current Speed Up applies inverse Base distance"), FMath::IsNearlyEqual(NewBase / OldBase, 0.5f));
	float CurrentSpeed=0, CurrentBase=0, CurrentRemaining=0; int32 CurrentPending=0;
	Manager->GetActionDistanceForAutomation(Current, CurrentSpeed, CurrentBase, CurrentRemaining, CurrentPending);
	CurrentParticipant->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), CurrentSpeed * 0.5f);
	float LockedSpeed=0, LockedBase=0, LockedRemaining=0; int32 LockedPending=0;
	Manager->GetActionDistanceForAutomation(Current, LockedSpeed, LockedBase, LockedRemaining, LockedPending);
	TestEqual(TEXT("Current Speed change never cancels locked actor"), Manager->GetCurrentParticipantId(), Current);
	TestEqual(TEXT("Current Speed change does not change current remaining before resolve"), LockedRemaining, CurrentRemaining);
	TestEqual(TEXT("Speed delegate binding count matches fixture participants"), Manager->GetSpeedDelegateBindingCountForAutomation(), 2);
	FHSRActionDistanceRequest Delay; Delay.BattleEpoch = Epoch; Delay.OperationId = FGuid::NewGuid(); Delay.TargetParticipantId = Current; Delay.Ratio = 0.3f; Delay.Kind = EHSRActionDistanceAdjustmentKind::Delay;
	const FHSRActionDistanceResult DelayResult = Manager->RequestActionDistanceAdjustment(Delay);
	TestEqual(TEXT("Current actor delay is accepted as ordered pending"), DelayResult.Result, EHSRActionDistanceAdjustmentResult::Accepted);
	TestEqual(TEXT("Accepted current pending reports real old/new counts"), DelayResult.NewPendingOperationCount, DelayResult.OldPendingOperationCount + 1);
	TestEqual(TEXT("Duplicate operation has zero mutation"), Manager->RequestActionDistanceAdjustment(Delay).Result, EHSRActionDistanceAdjustmentResult::DuplicateOperation);
	FHSRActionDistanceRequest InvalidKind = Delay; InvalidKind.OperationId = FGuid::NewGuid(); InvalidKind.Kind = static_cast<EHSRActionDistanceAdjustmentKind>(255);
	TestEqual(TEXT("Unknown adjustment kind is rejected before consuming id"), Manager->RequestActionDistanceAdjustment(InvalidKind).Result, EHSRActionDistanceAdjustmentResult::InvalidRequest);
	InvalidKind.Kind = EHSRActionDistanceAdjustmentKind::Advance;
	TestEqual(TEXT("Unknown-kind operation id remains usable"), Manager->RequestActionDistanceAdjustment(InvalidKind).Result, EHSRActionDistanceAdjustmentResult::Accepted);
	FHSRActionDistanceRequest InvalidRatio = Delay; InvalidRatio.OperationId = FGuid::NewGuid(); InvalidRatio.Ratio = FMath::Sqrt(-1.0f);
	TestEqual(TEXT("Non-finite ratio rejects without consuming id"), Manager->RequestActionDistanceAdjustment(InvalidRatio).Result, EHSRActionDistanceAdjustmentResult::InvalidRequest);
	InvalidRatio.Ratio = 0.0f;
	TestEqual(TEXT("Rejected ratio id can subsequently be accepted"), Manager->RequestActionDistanceAdjustment(InvalidRatio).Result, EHSRActionDistanceAdjustmentResult::Accepted);
	FHSRActionDistanceRequest BadTarget = Delay; BadTarget.OperationId = FGuid::NewGuid(); BadTarget.TargetParticipantId = TEXT("Unknown");
	TestEqual(TEXT("Unknown target is structured rejection"), Manager->RequestActionDistanceAdjustment(BadTarget).Result, EHSRActionDistanceAdjustmentResult::InvalidTarget);
	TestEqual(TEXT("Rejected valid-id target replay is duplicate"), Manager->RequestActionDistanceAdjustment(BadTarget).Result, EHSRActionDistanceAdjustmentResult::DuplicateOperation);
	TestEqual(TEXT("Adjustment does not advance lifecycle"), Manager->GetTurnSequence(), Sequence);
	TestTrue(TEXT("Current action resolves after pending delay"), Manager->ResolveAction(Current));
	TestEqual(TEXT("Resolve emits exactly one successor turn"), Manager->GetTurnSequence(), Sequence + 1);
	FHSRActionDistanceRequest OldEpoch = Delay; OldEpoch.OperationId = FGuid::NewGuid(); OldEpoch.BattleEpoch = Epoch - 1;
	TestEqual(TEXT("Old epoch is rejected after consuming its valid operation id"), Manager->RequestActionDistanceAdjustment(OldEpoch).Result, EHSRActionDistanceAdjustmentResult::InvalidEpoch);
	return true;
}

bool FHSRActionDistanceCurrentPendingPatchTest::RunTest(const FString& Parameters)
{
	using namespace HSRActionDistanceAutomation;
	UWorld* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
	ON_SCOPE_EXIT { if (World) World->DestroyWorld(false); };
	if (!TestNotNull(TEXT("CurrentPending world"), World)) return false;

	const auto Build = [World](const TCHAR* Prefix)
	{
		TArray<FHSRBattleParticipant> Participants;
		Participants.Add(MakeParticipant(World, FName(*FString::Printf(TEXT("%sA"), Prefix)), 100.0f));
		Participants.Add(MakeParticipant(World, FName(*FString::Printf(TEXT("%sB"), Prefix)), 50.0f, EHSRBattleParticipantTeam::Enemy));
		UHSRTurnManager* Manager = NewObject<UHSRTurnManager>();
		return TPair<UHSRTurnManager*, TArray<FHSRBattleParticipant>>(Manager, MoveTemp(Participants));
	};

	auto Frozen = Build(TEXT("Frozen"));
	TestTrue(TEXT("CurrentPending Frozen initializes"), Frozen.Key->Initialize(Frozen.Value));
	const FName FrozenCurrent = Frozen.Key->GetCurrentParticipantId();
	TestEqual(TEXT("CurrentPending initial actor is A"), FrozenCurrent, FName(TEXT("FrozenA")));
	TestEqual(TEXT("CurrentPending Advance 0.25 accepted"), Request(Frozen.Key, FrozenCurrent, EHSRActionDistanceAdjustmentKind::Advance, 0.25f).Result, EHSRActionDistanceAdjustmentResult::Accepted);
	TestEqual(TEXT("CurrentPending Delay 0.3 accepted"), Request(Frozen.Key, FrozenCurrent, EHSRActionDistanceAdjustmentKind::Delay, 0.3f).Result, EHSRActionDistanceAdjustmentResult::Accepted);
	const FSnapshot BeforeSpeed = Read(Frozen.Key, FrozenCurrent);
	Frozen.Value[0].AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 200.0f);
	const FSnapshot AfterSpeed = Read(Frozen.Key, FrozenCurrent);
	TestTrue(TEXT("CurrentPending accepted distances stay frozen while Base changes"), FMath::IsNearlyEqual(BeforeSpeed.Base, 100.0f) && FMath::IsNearlyEqual(AfterSpeed.Base, 50.0f) && FMath::IsNearlyEqual(AfterSpeed.Remaining, 0.0f) && AfterSpeed.Pending == 2);
	TestTrue(TEXT("CurrentPending resolve succeeds"), Frozen.Key->ResolveAction(FrozenCurrent));
	const FSnapshot FrozenAfter = Read(Frozen.Key, FrozenCurrent);
	TestTrue(TEXT("CurrentPending latest recharge plus frozen Advance/Delay equals 55"), FMath::IsNearlyEqual(Frozen.Key->GetLastPostRechargeDistanceForAutomation(), 55.0f) && FMath::IsNearlyEqual(FrozenAfter.Remaining, 0.0f) && FMath::IsNearlyEqual(Read(Frozen.Key, TEXT("FrozenB")).Remaining, 45.0f));
	TestEqual(TEXT("CurrentPending queue is consumed exactly once"), FrozenAfter.Pending, 0);

	auto DelayThenAdvance = Build(TEXT("DA"));
	TestTrue(TEXT("CurrentPending DelayThenAdvance initializes"), DelayThenAdvance.Key->Initialize(DelayThenAdvance.Value));
	const FName DACurrent = DelayThenAdvance.Key->GetCurrentParticipantId();
	Request(DelayThenAdvance.Key, DACurrent, EHSRActionDistanceAdjustmentKind::Delay, 0.3f);
	Request(DelayThenAdvance.Key, DACurrent, EHSRActionDistanceAdjustmentKind::Advance, 1.0f);
	DelayThenAdvance.Value[0].AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 1000.0f);
	TestTrue(TEXT("CurrentPending Delay then Advance resolves"), DelayThenAdvance.Key->ResolveAction(DACurrent));
	TestTrue(TEXT("CurrentPending Delay then Advance clamps per step to zero"), FMath::IsNearlyEqual(DelayThenAdvance.Key->GetLastPostRechargeDistanceForAutomation(), 0.0f) && FMath::IsNearlyEqual(Read(DelayThenAdvance.Key, DACurrent).Remaining, 0.0f));

	auto AdvanceThenDelay = Build(TEXT("AD"));
	TestTrue(TEXT("CurrentPending AdvanceThenDelay initializes"), AdvanceThenDelay.Key->Initialize(AdvanceThenDelay.Value));
	const FName ADCurrent = AdvanceThenDelay.Key->GetCurrentParticipantId();
	Request(AdvanceThenDelay.Key, ADCurrent, EHSRActionDistanceAdjustmentKind::Advance, 1.0f);
	Request(AdvanceThenDelay.Key, ADCurrent, EHSRActionDistanceAdjustmentKind::Delay, 0.3f);
	AdvanceThenDelay.Value[0].AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 1000.0f);
	TestTrue(TEXT("CurrentPending Advance then Delay resolves"), AdvanceThenDelay.Key->ResolveAction(ADCurrent));
	const float ADRecharge = AdvanceThenDelay.Key->GetLastPostRechargeDistanceForAutomation();
	const float ADCurrentRemaining = Read(AdvanceThenDelay.Key, ADCurrent).Remaining;
	const float ADBRemaining = Read(AdvanceThenDelay.Key, TEXT("ADB")).Remaining;
	const bool bAdvanceThenDelayExact = FMath::IsNearlyEqual(ADRecharge, 30.0f, 1.e-3f) && FMath::IsNearlyEqual(ADCurrentRemaining, 0.0f, 1.e-3f) && FMath::IsNearlyEqual(ADBRemaining, 70.0f, 1.e-3f);
	TestTrue(TEXT("CurrentPending Advance then Delay preserves ordered +30"), bAdvanceThenDelayExact);
	UE_LOG(LogTemp, Log, TEXT("ActionDistanceCase Case=CurrentPending Result=%s FrozenRecharge=55 ReverseDA=0 ReverseAD=%.6f ReverseADCurrent=%.6f ReverseADB=%.6f"), bAdvanceThenDelayExact ? TEXT("PASS") : TEXT("FAIL"), ADRecharge, ADCurrentRemaining, ADBRemaining);
	return true;
}

bool FHSRActionDistanceLifecyclePatchTest::RunTest(const FString& Parameters)
{
	using namespace HSRActionDistanceAutomation;
	UWorld* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
	ON_SCOPE_EXIT { if (World) World->DestroyWorld(false); };
	if (!TestNotNull(TEXT("LifecycleOrdering world"), World)) return false;
	FHSRBattleParticipant A = MakeParticipant(World, TEXT("LifeA"), 100.0f);
	FHSRBattleParticipant B = MakeParticipant(World, TEXT("LifeB"), 50.0f, EHSRBattleParticipantTeam::Enemy);
	UHSRTurnManager* Manager = NewObject<UHSRTurnManager>();
	int32 Started = 0;
	int32 Ended = 0;
	TArray<FString> Order;
	Manager->OnTurnStarted().AddLambda([&](const FHSRTurnLifecycleEvent& Event) { ++Started; Order.Add(FString::Printf(TEXT("Start:%s:%llu"), *Event.ParticipantId.ToString(), Event.TurnSequence)); });
	Manager->OnTurnEnded().AddLambda([&](const FHSRTurnLifecycleEvent& Event)
	{
		++Ended;
		Order.Add(FString::Printf(TEXT("End:%s:%llu"), *Event.ParticipantId.ToString(), Event.TurnSequence));
		if (Event.ParticipantId == TEXT("LifeA")) A.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 200.0f);
	});
	TestTrue(TEXT("LifecycleOrdering initializes"), Manager->Initialize({ A, B }));
	TestEqual(TEXT("LifecycleOrdering initialization emits one start"), Started, 1);
	const uint64 InitialSequence = Manager->GetTurnSequence();
	TestTrue(TEXT("LifecycleOrdering resolves locked actor"), Manager->ResolveAction(TEXT("LifeA")));
	TestEqual(TEXT("LifecycleOrdering exactly one TurnEnded"), Ended, 1);
	TestEqual(TEXT("LifecycleOrdering exactly one successor TurnStarted"), Started, 2);
	TestEqual(TEXT("LifecycleOrdering sequence increments only with TurnStarted"), Manager->GetTurnSequence(), InitialSequence + 1);
	TestEqual(TEXT("LifecycleOrdering callback retains current actor through recharge"), Manager->GetCurrentParticipantId(), FName(TEXT("LifeA")));
	const FSnapshot ASnapshot = Read(Manager, TEXT("LifeA"));
	const FSnapshot BSnapshot = Read(Manager, TEXT("LifeB"));
	TestTrue(TEXT("LifecycleOrdering TurnEnded Speed callback updates latest Base before recharge"), FMath::IsNearlyEqual(ASnapshot.Base, 50.0f) && FMath::IsNearlyEqual(Manager->GetLastPostRechargeDistanceForAutomation(), 50.0f));
	TestTrue(TEXT("LifecycleOrdering candidate selection follows latest recharge"), FMath::IsNearlyEqual(ASnapshot.Remaining, 0.0f) && FMath::IsNearlyEqual(BSnapshot.Remaining, 50.0f));
	TestTrue(TEXT("LifecycleOrdering event order is Start End Start"), Order.Num() == 3 && Order[0].StartsWith(TEXT("Start:LifeA")) && Order[1].StartsWith(TEXT("End:LifeA")) && Order[2].StartsWith(TEXT("Start:LifeA")));
	const int32 StartedBeforeRequest = Started;
	const int32 EndedBeforeRequest = Ended;
	const uint64 SequenceBeforeRequest = Manager->GetTurnSequence();
	Request(Manager, TEXT("LifeB"), EHSRActionDistanceAdjustmentKind::Advance, 0.25f);
	TestTrue(TEXT("LifecycleOrdering adjustment emits no lifecycle"), Started == StartedBeforeRequest && Ended == EndedBeforeRequest && Manager->GetTurnSequence() == SequenceBeforeRequest);
	UE_LOG(LogTemp, Log, TEXT("ActionDistanceCase Case=LifecycleOrdering Result=PASS Starts=%d Ends=%d Sequence=%llu Recharge=%.3f Current=%s"), Started, Ended, Manager->GetTurnSequence(), Manager->GetLastPostRechargeDistanceForAutomation(), *Manager->GetCurrentParticipantId().ToString());
	return true;
}

bool FHSRActionDistanceThreeParticipantPatchTest::RunTest(const FString& Parameters)
{
	using namespace HSRActionDistanceAutomation;
	UWorld* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
	ON_SCOPE_EXIT { if (World) World->DestroyWorld(false); };
	if (!TestNotNull(TEXT("ThreeParticipant world"), World)) return false;
	FHSRBattleParticipant A = MakeParticipant(World, TEXT("A"), 100.0f);
	FHSRBattleParticipant B = MakeParticipant(World, TEXT("B"), 200.0f, EHSRBattleParticipantTeam::Enemy);
	FHSRBattleParticipant C = MakeParticipant(World, TEXT("C"), 50.0f, EHSRBattleParticipantTeam::Enemy);
	UHSRTurnManager* Manager = NewObject<UHSRTurnManager>();
	TArray<FName> TurnOrder;
	Manager->OnTurnStarted().AddLambda([&](const FHSRTurnLifecycleEvent& Event) { TurnOrder.Add(Event.ParticipantId); });
	TestTrue(TEXT("ThreeParticipant A/B/C initializes"), Manager->Initialize({ A, B, C }));
	TestEqual(TEXT("ThreeParticipant fastest B acts first"), Manager->GetCurrentParticipantId(), FName(TEXT("B")));
	const FSnapshot ASnapshot = Read(Manager, TEXT("A"));
	const FSnapshot BSnapshot = Read(Manager, TEXT("B"));
	const FSnapshot CSnapshot = Read(Manager, TEXT("C"));
	TestTrue(TEXT("ThreeParticipant initial Base distances are inverse speed"), FMath::IsNearlyEqual(ASnapshot.Base, 100.0f) && FMath::IsNearlyEqual(BSnapshot.Base, 50.0f) && FMath::IsNearlyEqual(CSnapshot.Base, 200.0f));
	TestTrue(TEXT("ThreeParticipant initial Remaining distances are deterministic"), FMath::IsNearlyEqual(ASnapshot.Remaining, 50.0f) && FMath::IsNearlyEqual(BSnapshot.Remaining, 0.0f) && FMath::IsNearlyEqual(CSnapshot.Remaining, 150.0f));
	FHSRBattleParticipant SpeedA = MakeParticipant(World, TEXT("SpeedA"), 200.0f);
	FHSRBattleParticipant SpeedB = MakeParticipant(World, TEXT("SpeedB"), 100.0f, EHSRBattleParticipantTeam::Enemy);
	FHSRBattleParticipant SpeedC = MakeParticipant(World, TEXT("SpeedC"), 50.0f, EHSRBattleParticipantTeam::Enemy);
	UHSRTurnManager* SpeedManager = NewObject<UHSRTurnManager>();
	TestTrue(TEXT("ThreeParticipant SpeedUpSlow fixture initializes"), SpeedManager->Initialize({ SpeedA, SpeedB, SpeedC }));
	TestEqual(TEXT("ThreeParticipant SpeedUpSlow keeps A current"), SpeedManager->GetCurrentParticipantId(), FName(TEXT("SpeedA")));
	const FSnapshot SpeedBBefore = Read(SpeedManager, TEXT("SpeedB"));
	const FSnapshot SpeedCBefore = Read(SpeedManager, TEXT("SpeedC"));
	SpeedB.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 200.0f);
	SpeedC.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 25.0f);
	const FSnapshot SpeedBAfter = Read(SpeedManager, TEXT("SpeedB"));
	const FSnapshot SpeedCAfter = Read(SpeedManager, TEXT("SpeedC"));
	TestTrue(TEXT("ThreeParticipant B SpeedUp preserves Remaining ratio"), FMath::IsNearlyEqual(SpeedBAfter.Base / SpeedBBefore.Base, 0.5f) && FMath::IsNearlyEqual(SpeedBAfter.Remaining / SpeedBBefore.Remaining, 0.5f));
	TestTrue(TEXT("ThreeParticipant C Slow preserves Remaining ratio"), FMath::IsNearlyEqual(SpeedCAfter.Base / SpeedCBefore.Base, 2.0f) && FMath::IsNearlyEqual(SpeedCAfter.Remaining / SpeedCBefore.Remaining, 2.0f));
	TestTrue(TEXT("ThreeParticipant current A remains locked during B/C changes"), SpeedManager->GetCurrentParticipantId() == TEXT("SpeedA") && FMath::IsNearlyEqual(Read(SpeedManager, TEXT("SpeedA")).Remaining, 0.0f));
	TestTrue(TEXT("ThreeParticipant SpeedUpSlow resolves"), SpeedManager->ResolveAction(TEXT("SpeedA")));
	TestEqual(TEXT("ThreeParticipant next actor follows changed B/C distances"), SpeedManager->GetCurrentParticipantId(), FName(TEXT("SpeedB")));

	FHSRBattleParticipant TieA = MakeParticipant(World, TEXT("TieA"), 100.0f);
	FHSRBattleParticipant TieB = MakeParticipant(World, TEXT("TieB"), 10000.0f / 100.00005f, EHSRBattleParticipantTeam::Enemy);
	UHSRTurnManager* TieManager = NewObject<UHSRTurnManager>();
	TestTrue(TEXT("ThreeParticipant epsilon tie initializes"), TieManager->Initialize({ TieB, TieA }));
	TestEqual(TEXT("ThreeParticipant epsilon tie uses lexical ParticipantId"), TieManager->GetCurrentParticipantId(), FName(TEXT("TieA")));

	int32 CountA = 0, CountB = 0, CountC = 0;
	for (int32 Step = 0; Step < 18 && Manager->GetState() != EHSRTurnManagerState::Finished; ++Step)
	{
		const FName Current = Manager->GetCurrentParticipantId();
		if (Current == TEXT("A")) ++CountA; else if (Current == TEXT("B")) ++CountB; else if (Current == TEXT("C")) ++CountC;
		TestTrue(FString::Printf(TEXT("ThreeParticipant Resolve step %d"), Step), Manager->ResolveAction(Current));
	}
	TestTrue(TEXT("ThreeParticipant high speed acts more frequently than low speed"), CountB > CountA && CountA > CountC);
	TestTrue(TEXT("ThreeParticipant lifecycle order is stable and populated"), TurnOrder.Num() == 19 && TurnOrder[0] == TEXT("B"));
	UE_LOG(LogTemp, Log, TEXT("ActionDistanceCase Case=ThreeParticipant Result=PASS BaseA=%.3f BaseB=%.3f BaseC=%.3f Counts=%d/%d/%d Tie=%s"), ASnapshot.Base, BSnapshot.Base, CSnapshot.Base, CountA, CountB, CountC, *TieManager->GetCurrentParticipantId().ToString());
	return true;
}

bool FHSRActionDistanceRequestMatrixPatchTest::RunTest(const FString& Parameters)
{
	using namespace HSRActionDistanceAutomation;
	UWorld* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
	ON_SCOPE_EXIT { if (World) World->DestroyWorld(false); };
	if (!TestNotNull(TEXT("RequestMatrix world"), World)) return false;
	FHSRBattleParticipant A = MakeParticipant(World, TEXT("ReqA"), 200.0f);
	FHSRBattleParticipant B = MakeParticipant(World, TEXT("ReqB"), 100.0f, EHSRBattleParticipantTeam::Enemy);
	FHSRBattleParticipant C = MakeParticipant(World, TEXT("ReqC"), 50.0f, EHSRBattleParticipantTeam::Enemy);
	UHSRTurnManager* Manager = NewObject<UHSRTurnManager>();
	TestTrue(TEXT("RequestMatrix initializes"), Manager->Initialize({ A, B, C }));
	int32 RequestStarted = 0;
	int32 RequestEnded = 0;
	Manager->OnTurnStarted().AddLambda([&](const FHSRTurnLifecycleEvent&) { ++RequestStarted; });
	Manager->OnTurnEnded().AddLambda([&](const FHSRTurnLifecycleEvent&) { ++RequestEnded; });
	const FName Current = Manager->GetCurrentParticipantId();
	const FSnapshot BeforeB = Read(Manager, TEXT("ReqB"));
	const FGuid DuplicateId = FGuid::NewGuid();
	const FHSRActionDistanceResult Accepted = Request(Manager, TEXT("ReqB"), EHSRActionDistanceAdjustmentKind::Advance, 0.25f, DuplicateId);
	TestEqual(TEXT("RequestMatrix Advance 0.25 accepted"), Accepted.Result, EHSRActionDistanceAdjustmentResult::Accepted);
	TestTrue(TEXT("RequestMatrix accepted snapshot contains old/new values"), Accepted.OldPendingOperationCount == 0 && Accepted.NewPendingOperationCount == 0 && Accepted.OldRemaining > Accepted.NewRemaining);
	const FSnapshot AcceptedTargetBeforeReplay = Read(Manager, TEXT("ReqB"));
	const FSnapshot CrossTargetBeforeReplay = Read(Manager, TEXT("ReqC"));
	const EHSRTurnManagerState StateBeforeCrossReplay = Manager->GetState();
	const FName CurrentBeforeCrossReplay = Manager->GetCurrentParticipantId();
	const uint64 EpochBeforeCrossReplay = Manager->GetBattleEpoch();
	const uint64 SequenceBeforeCrossReplay = Manager->GetTurnSequence();
	const int32 BindingsBeforeCrossReplay = Manager->GetSpeedDelegateBindingCountForAutomation();
	const int32 StartsBeforeCrossReplay = RequestStarted;
	const int32 EndsBeforeCrossReplay = RequestEnded;
	const FHSRActionDistanceResult CrossReplay = Manager->RequestActionDistanceAdjustment(FHSRActionDistanceRequest{ Manager->GetBattleEpoch(), DuplicateId, TEXT("ReqC"), 1.0f, EHSRActionDistanceAdjustmentKind::Delay });
	TestEqual(TEXT("RequestMatrix accepted OperationId is global across target and kind"), CrossReplay.Result, EHSRActionDistanceAdjustmentResult::DuplicateOperation);
	const FSnapshot AcceptedTargetAfterReplay = Read(Manager, TEXT("ReqB"));
	const FSnapshot CrossTargetAfterReplay = Read(Manager, TEXT("ReqC"));
	const bool bCrossReplayAtomic = SameSnapshot(AcceptedTargetBeforeReplay, AcceptedTargetAfterReplay)
		&& SameSnapshot(CrossTargetBeforeReplay, CrossTargetAfterReplay)
		&& Manager->GetState() == StateBeforeCrossReplay
		&& Manager->GetCurrentParticipantId() == CurrentBeforeCrossReplay
		&& Manager->GetBattleEpoch() == EpochBeforeCrossReplay
		&& Manager->GetTurnSequence() == SequenceBeforeCrossReplay
		&& Manager->GetSpeedDelegateBindingCountForAutomation() == BindingsBeforeCrossReplay
		&& RequestStarted == StartsBeforeCrossReplay && RequestEnded == EndsBeforeCrossReplay
		&& CrossReplay.CurrentParticipantId == CurrentBeforeCrossReplay && CrossReplay.NextParticipantId == CurrentBeforeCrossReplay
		&& CrossReplay.BattleEpoch == EpochBeforeCrossReplay && CrossReplay.TurnSequence == SequenceBeforeCrossReplay;
	TestTrue(TEXT("RequestMatrix cross-target cross-kind replay has complete zero mutation"), bCrossReplayAtomic);
	UE_LOG(LogTemp, Log, TEXT("ActionDistanceCase Case=CrossTargetKindReplay Result=%s OperationId=%s FirstTarget=ReqB FirstKind=Advance ReplayTarget=ReqC ReplayKind=Delay Current=%s Epoch=%llu Sequence=%llu Bindings=%d Starts=%d Ends=%d"), bCrossReplayAtomic ? TEXT("PASS") : TEXT("FAIL"), *DuplicateId.ToString(), *CurrentBeforeCrossReplay.ToString(), EpochBeforeCrossReplay, SequenceBeforeCrossReplay, BindingsBeforeCrossReplay, RequestStarted, RequestEnded);

	const float Ratios[] = { 0.0f, 0.3f, 1.0f };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Ratios); ++Index)
	{
		const EHSRActionDistanceAdjustmentKind Kind = Index == 0 ? EHSRActionDistanceAdjustmentKind::Advance : EHSRActionDistanceAdjustmentKind::Delay;
		const FHSRActionDistanceResult Result = Request(Manager, TEXT("ReqC"), Kind, Ratios[Index]);
		TestEqual(FString::Printf(TEXT("RequestMatrix boundary ratio %.2f accepted"), Ratios[Index]), Result.Result, EHSRActionDistanceAdjustmentResult::Accepted);
	}
	const FSnapshot BeforeInvalid = Read(Manager, TEXT("ReqC"));
	const FGuid InvalidRatioId = FGuid::NewGuid();
	FHSRActionDistanceRequest InvalidRatio;
	InvalidRatio.BattleEpoch = Manager->GetBattleEpoch(); InvalidRatio.OperationId = InvalidRatioId; InvalidRatio.TargetParticipantId = TEXT("ReqC"); InvalidRatio.Kind = EHSRActionDistanceAdjustmentKind::Delay; InvalidRatio.Ratio = std::numeric_limits<float>::infinity();
	const FHSRActionDistanceResult InvalidRatioResult = Manager->RequestActionDistanceAdjustment(InvalidRatio);
	TestEqual(TEXT("RequestMatrix Inf ratio is InvalidRequest"), InvalidRatioResult.Result, EHSRActionDistanceAdjustmentResult::InvalidRequest);
	TestTrue(TEXT("RequestMatrix InvalidRequest has zero mutation"), FMath::IsNearlyEqual(Read(Manager, TEXT("ReqC")).Remaining, BeforeInvalid.Remaining));
	InvalidRatio.Ratio = 0.0f;
	TestEqual(TEXT("RequestMatrix rejected ratio OperationId remains reusable"), Manager->RequestActionDistanceAdjustment(InvalidRatio).Result, EHSRActionDistanceAdjustmentResult::Accepted);

	FHSRActionDistanceRequest BadTarget;
	BadTarget.BattleEpoch = Manager->GetBattleEpoch(); BadTarget.OperationId = FGuid::NewGuid(); BadTarget.TargetParticipantId = TEXT("ReqUnknown"); BadTarget.Kind = EHSRActionDistanceAdjustmentKind::Delay; BadTarget.Ratio = 0.3f;
	TestEqual(TEXT("RequestMatrix unknown target is InvalidTarget"), Manager->RequestActionDistanceAdjustment(BadTarget).Result, EHSRActionDistanceAdjustmentResult::InvalidTarget);
	TestEqual(TEXT("RequestMatrix unknown target id is consumed"), Manager->RequestActionDistanceAdjustment(BadTarget).Result, EHSRActionDistanceAdjustmentResult::DuplicateOperation);
	FHSRActionDistanceRequest OldEpoch = BadTarget; OldEpoch.OperationId = FGuid::NewGuid(); OldEpoch.TargetParticipantId = TEXT("ReqB"); OldEpoch.BattleEpoch = Manager->GetBattleEpoch() - 1;
	TestEqual(TEXT("RequestMatrix old epoch is InvalidEpoch"), Manager->RequestActionDistanceAdjustment(OldEpoch).Result, EHSRActionDistanceAdjustmentResult::InvalidEpoch);
	TestEqual(TEXT("RequestMatrix old epoch id is consumed"), Manager->RequestActionDistanceAdjustment(OldEpoch).Result, EHSRActionDistanceAdjustmentResult::DuplicateOperation);

	B.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 0.0f);
	const FSnapshot BeforeDead = Read(Manager, TEXT("ReqB"));
	FHSRActionDistanceRequest DeadRequest; DeadRequest.BattleEpoch = Manager->GetBattleEpoch(); DeadRequest.OperationId = FGuid::NewGuid(); DeadRequest.TargetParticipantId = TEXT("ReqB"); DeadRequest.Kind = EHSRActionDistanceAdjustmentKind::Delay; DeadRequest.Ratio = 0.3f;
	TestEqual(TEXT("RequestMatrix defeated target is DefeatedTarget"), Manager->RequestActionDistanceAdjustment(DeadRequest).Result, EHSRActionDistanceAdjustmentResult::DefeatedTarget);
	TestTrue(TEXT("RequestMatrix defeated target has zero mutation"), FMath::IsNearlyEqual(Read(Manager, TEXT("ReqB")).Remaining, BeforeDead.Remaining));

	const FSnapshot BeforeFinished = Read(Manager, Current);
	Manager->FinishBattle();
	FHSRActionDistanceRequest FinishedRequest; FinishedRequest.BattleEpoch = Manager->GetBattleEpoch(); FinishedRequest.OperationId = FGuid::NewGuid(); FinishedRequest.TargetParticipantId = Current; FinishedRequest.Kind = EHSRActionDistanceAdjustmentKind::Delay; FinishedRequest.Ratio = 1.0f;
	TestEqual(TEXT("RequestMatrix finished battle is Finished"), Manager->RequestActionDistanceAdjustment(FinishedRequest).Result, EHSRActionDistanceAdjustmentResult::Finished);
	TestTrue(TEXT("RequestMatrix Finished has zero mutation"), FMath::IsNearlyEqual(Read(Manager, Current).Remaining, BeforeFinished.Remaining));

	FHSRBattleParticipant OverflowA = MakeParticipant(World, TEXT("OverflowA"), 100.0f);
	FHSRBattleParticipant OverflowB = MakeParticipant(World, TEXT("OverflowB"), 50.0f, EHSRBattleParticipantTeam::Enemy);
	UHSRTurnManager* OverflowManager = NewObject<UHSRTurnManager>();
	TestTrue(TEXT("RequestMatrix overflow fixture initializes"), OverflowManager->Initialize({ OverflowA, OverflowB }));
	TestTrue(TEXT("RequestMatrix overflow fixture seeds finite boundary"), OverflowManager->SetActionDistanceForAutomation(TEXT("OverflowB"), 1.0f, std::numeric_limits<float>::max(), std::numeric_limits<float>::max()));
	const FSnapshot BeforeOverflow = Read(OverflowManager, TEXT("OverflowB"));
	const FHSRActionDistanceResult Overflow = Request(OverflowManager, TEXT("OverflowB"), EHSRActionDistanceAdjustmentKind::Delay, 1.0f);
	TestEqual(TEXT("RequestMatrix overflow is ArithmeticFailure"), Overflow.Result, EHSRActionDistanceAdjustmentResult::ArithmeticFailure);
	TestTrue(TEXT("RequestMatrix ArithmeticFailure is atomic"), FMath::IsNearlyEqual(Read(OverflowManager, TEXT("OverflowB")).Remaining, BeforeOverflow.Remaining) && Read(OverflowManager, TEXT("OverflowB")).Pending == BeforeOverflow.Pending);

	UE_LOG(LogTemp, Log, TEXT("ActionDistanceCase Case=RequestMatrix Result=PASS Accepted=%d Duplicate=%d InvalidRatio=%d InvalidTarget=%d InvalidEpoch=%d Defeated=%d Finished=%d ArithmeticFailure=%d Current=%s"), static_cast<int32>(Accepted.Result), static_cast<int32>(EHSRActionDistanceAdjustmentResult::DuplicateOperation), static_cast<int32>(InvalidRatioResult.Result), static_cast<int32>(EHSRActionDistanceAdjustmentResult::InvalidTarget), static_cast<int32>(EHSRActionDistanceAdjustmentResult::InvalidEpoch), static_cast<int32>(EHSRActionDistanceAdjustmentResult::DefeatedTarget), static_cast<int32>(EHSRActionDistanceAdjustmentResult::Finished), static_cast<int32>(Overflow.Result), *Current.ToString());
	return true;
}

bool FHSRActionDistanceNumericLifecyclePatchTest::RunTest(const FString& Parameters)
{
	using namespace HSRActionDistanceAutomation;
	UWorld* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
	ON_SCOPE_EXIT { if (World) World->DestroyWorld(false); };
	if (!TestNotNull(TEXT("NumericAndBinding world"), World)) return false;
	FHSRBattleParticipant Zero = MakeParticipant(World, TEXT("Zero"), 0.0f);
	FHSRBattleParticipant Negative = MakeParticipant(World, TEXT("Negative"), -10.0f, EHSRBattleParticipantTeam::Enemy);
	UHSRTurnManager* ClampManager = NewObject<UHSRTurnManager>();
	TestTrue(TEXT("NumericAndBinding zero/negative initialize"), ClampManager->Initialize({ Zero, Negative }));
	TestTrue(TEXT("NumericAndBinding finite non-positive speed clamps to one"), FMath::IsNearlyEqual(Read(ClampManager, TEXT("Zero")).Speed, 1.0f) && FMath::IsNearlyEqual(Read(ClampManager, TEXT("Negative")).Speed, 1.0f) && FMath::IsNearlyEqual(Read(ClampManager, TEXT("Zero")).Base, 10000.0f));

	FHSRBattleParticipant InitialNaN = MakeParticipant(World, TEXT("InitialNaN"), 100.0f);
	if (const UHSRCoreAttributeSet* Attributes = InitialNaN.AbilitySystemComponent->GetSet<UHSRCoreAttributeSet>())
	{
		const_cast<UHSRCoreAttributeSet*>(Attributes)->Speed.SetBaseValue(FMath::Sqrt(-1.0f));
		const_cast<UHSRCoreAttributeSet*>(Attributes)->Speed.SetCurrentValue(FMath::Sqrt(-1.0f));
	}
	UHSRTurnManager* NaNManager = NewObject<UHSRTurnManager>();
	TestFalse(TEXT("NumericAndBinding NaN initialization rolls back atomically"), NaNManager->Initialize({ InitialNaN }));
	TestEqual(TEXT("NumericAndBinding NaN init leaves no delegates"), NaNManager->GetSpeedDelegateBindingCountForAutomation(), 0);
	FHSRBattleParticipant InitialInf = MakeParticipant(World, TEXT("InitialInf"), 100.0f);
	InitialInf.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), std::numeric_limits<float>::infinity());
	UHSRTurnManager* InfManager = NewObject<UHSRTurnManager>();
	TestFalse(TEXT("NumericAndBinding Inf initialization rolls back atomically"), InfManager->Initialize({ InitialInf }));

	FHSRBattleParticipant RuntimeA = MakeParticipant(World, TEXT("RuntimeA"), 100.0f);
	FHSRBattleParticipant RuntimeB = MakeParticipant(World, TEXT("RuntimeB"), 50.0f, EHSRBattleParticipantTeam::Enemy);
	UHSRTurnManager* RuntimeManager = NewObject<UHSRTurnManager>();
	TestTrue(TEXT("NumericAndBinding runtime fixture initializes"), RuntimeManager->Initialize({ RuntimeA, RuntimeB }));
	const FSnapshot RuntimeBefore = Read(RuntimeManager, TEXT("RuntimeB"));
	const uint64 RuntimeEpoch = RuntimeManager->GetBattleEpoch();
	const uint64 RuntimeSequence = RuntimeManager->GetTurnSequence();
	BroadcastSpeed(RuntimeB.AbilitySystemComponent.Get(), RuntimeBefore.Speed, FMath::Sqrt(-1.0f));
	const FSnapshot RuntimeAfterNaN = Read(RuntimeManager, TEXT("RuntimeB"));
	TestTrue(TEXT("NumericAndBinding runtime NaN callback is rejected with zero mutation"), FMath::IsNearlyEqual(RuntimeAfterNaN.Speed, RuntimeBefore.Speed) && FMath::IsNearlyEqual(RuntimeAfterNaN.Base, RuntimeBefore.Base) && FMath::IsNearlyEqual(RuntimeAfterNaN.Remaining, RuntimeBefore.Remaining) && RuntimeAfterNaN.Pending == RuntimeBefore.Pending && RuntimeManager->GetBattleEpoch() == RuntimeEpoch && RuntimeManager->GetTurnSequence() == RuntimeSequence);
	BroadcastSpeed(RuntimeB.AbilitySystemComponent.Get(), RuntimeBefore.Speed, std::numeric_limits<float>::infinity());
	const FSnapshot RuntimeAfterInf = Read(RuntimeManager, TEXT("RuntimeB"));
	TestTrue(TEXT("NumericAndBinding runtime Inf callback is rejected with zero mutation"), FMath::IsNearlyEqual(RuntimeAfterInf.Speed, RuntimeBefore.Speed) && FMath::IsNearlyEqual(RuntimeAfterInf.Base, RuntimeBefore.Base) && FMath::IsNearlyEqual(RuntimeAfterInf.Remaining, RuntimeBefore.Remaining));
	BroadcastSpeed(RuntimeB.AbilitySystemComponent.Get(), RuntimeBefore.Speed, 0.0f);
	const FSnapshot RuntimeAfterZero = Read(RuntimeManager, TEXT("RuntimeB"));
	TestTrue(TEXT("NumericAndBinding runtime zero callback clamps to one"), FMath::IsNearlyEqual(RuntimeAfterZero.Speed, 1.0f) && FMath::IsNearlyEqual(RuntimeAfterZero.Base, 10000.0f) && FMath::IsNearlyEqual(RuntimeAfterZero.Remaining, RuntimeBefore.Remaining * RuntimeAfterZero.Base / RuntimeBefore.Base));
	BroadcastSpeed(RuntimeB.AbilitySystemComponent.Get(), 0.0f, -10.0f);
	TestTrue(TEXT("NumericAndBinding runtime negative callback clamps to one"), FMath::IsNearlyEqual(Read(RuntimeManager, TEXT("RuntimeB")).Speed, 1.0f) && FMath::IsNearlyEqual(Read(RuntimeManager, TEXT("RuntimeB")).Base, 10000.0f));
	BroadcastSpeed(RuntimeB.AbilitySystemComponent.Get(), RuntimeBefore.Speed, 200.0f);
	TestTrue(TEXT("NumericAndBinding runtime finite callback updates non-current ratio"), FMath::IsNearlyEqual(Read(RuntimeManager, TEXT("RuntimeB")).Base / RuntimeAfterZero.Base, 0.005f));

	FHSRBattleParticipant BindA = MakeParticipant(World, TEXT("BindA"), 100.0f);
	FHSRBattleParticipant BindB = MakeParticipant(World, TEXT("BindB"), 50.0f, EHSRBattleParticipantTeam::Enemy);
	UHSRTurnManager* BindManager = NewObject<UHSRTurnManager>();
	BindManager->SetSpeedDelegateBindFailureAfterForAutomation(1);
	int32 BindStarted = 0;
	int32 BindEnded = 0;
	BindManager->OnTurnStarted().AddLambda([&](const FHSRTurnLifecycleEvent&) { ++BindStarted; });
	BindManager->OnTurnEnded().AddLambda([&](const FHSRTurnLifecycleEvent&) { ++BindEnded; });
	TestFalse(TEXT("NumericAndBinding nth bind failure rejects initialization"), BindManager->Initialize({ BindA, BindB }));
	TestEqual(TEXT("NumericAndBinding nth bind failure removes every handle"), BindManager->GetSpeedDelegateBindingCountForAutomation(), 0);
	TestEqual(TEXT("NumericAndBinding nth bind failure emits no TurnStarted"), BindStarted, 0);
	TestEqual(TEXT("NumericAndBinding nth bind failure leaves waiting state"), BindManager->GetState(), EHSRTurnManagerState::Waiting);
	BindManager->SetSpeedDelegateBindFailureAfterForAutomation(INDEX_NONE);
	TestTrue(TEXT("NumericAndBinding reinitialize succeeds after failure"), BindManager->Initialize({ BindA, BindB }));
	const uint64 OldEpoch = BindManager->GetBattleEpoch();
	const FSnapshot BeforeReset = Read(BindManager, TEXT("BindB"));
	BindManager->Reset();
	TestEqual(TEXT("NumericAndBinding Reset unbinds all speed delegates"), BindManager->GetSpeedDelegateBindingCountForAutomation(), 0);
	BroadcastSpeed(BindB.AbilitySystemComponent.Get(), BeforeReset.Speed, 300.0f);
	TestEqual(TEXT("NumericAndBinding stale ASC callback after Reset has no effect"), BindManager->GetSpeedDelegateBindingCountForAutomation(), 0);
	FHSRBattleParticipant FreshA = MakeParticipant(World, TEXT("FreshA"), 100.0f);
	FHSRBattleParticipant FreshB = MakeParticipant(World, TEXT("FreshB"), 50.0f, EHSRBattleParticipantTeam::Enemy);
	TestTrue(TEXT("NumericAndBinding reinitialize binds fresh epoch"), BindManager->Initialize({ FreshA, FreshB }));
	const FSnapshot FreshABeforeOldASCBroadcast = Read(BindManager, TEXT("FreshA"));
	const FSnapshot FreshBBeforeOldASCBroadcast = Read(BindManager, TEXT("FreshB"));
	const EHSRTurnManagerState FreshStateBeforeOldASCBroadcast = BindManager->GetState();
	const FName FreshCurrentBeforeOldASCBroadcast = BindManager->GetCurrentParticipantId();
	const uint64 FreshEpochBeforeOldASCBroadcast = BindManager->GetBattleEpoch();
	const uint64 FreshSequenceBeforeOldASCBroadcast = BindManager->GetTurnSequence();
	const int32 FreshBindingsBeforeOldASCBroadcast = BindManager->GetSpeedDelegateBindingCountForAutomation();
	const int32 FreshStartsBeforeOldASCBroadcast = BindStarted;
	const int32 FreshEndsBeforeOldASCBroadcast = BindEnded;
	BroadcastSpeed(BindB.AbilitySystemComponent.Get(), BeforeReset.Speed, 300.0f);
	const bool bOldASCPostReinitializeAtomic = SameSnapshot(FreshABeforeOldASCBroadcast, Read(BindManager, TEXT("FreshA")))
		&& SameSnapshot(FreshBBeforeOldASCBroadcast, Read(BindManager, TEXT("FreshB")))
		&& BindManager->GetState() == FreshStateBeforeOldASCBroadcast
		&& BindManager->GetCurrentParticipantId() == FreshCurrentBeforeOldASCBroadcast
		&& BindManager->GetBattleEpoch() == FreshEpochBeforeOldASCBroadcast
		&& BindManager->GetTurnSequence() == FreshSequenceBeforeOldASCBroadcast
		&& BindManager->GetSpeedDelegateBindingCountForAutomation() == FreshBindingsBeforeOldASCBroadcast
		&& BindStarted == FreshStartsBeforeOldASCBroadcast && BindEnded == FreshEndsBeforeOldASCBroadcast;
	TestTrue(TEXT("NumericAndBinding old ASC broadcast after reinitialize has complete zero mutation"), bOldASCPostReinitializeAtomic);
	UE_LOG(LogTemp, Log, TEXT("ActionDistanceCase Case=OldASCPostReinitialize Result=%s OldParticipant=BindB FreshCurrent=%s FreshEpoch=%llu Sequence=%llu Bindings=%d Starts=%d Ends=%d"), bOldASCPostReinitializeAtomic ? TEXT("PASS") : TEXT("FAIL"), *FreshCurrentBeforeOldASCBroadcast.ToString(), FreshEpochBeforeOldASCBroadcast, FreshSequenceBeforeOldASCBroadcast, FreshBindingsBeforeOldASCBroadcast, BindStarted, BindEnded);
	const FSnapshot FreshBefore = Read(BindManager, TEXT("FreshB"));
	BindManager->InvokeSpeedChangedForAutomation(TEXT("FreshB"), FreshB.AbilitySystemComponent.Get(), OldEpoch, 300.0f);
	TestTrue(TEXT("NumericAndBinding stale epoch callback has zero effect"), FMath::IsNearlyEqual(Read(BindManager, TEXT("FreshB")).Base, FreshBefore.Base) && FMath::IsNearlyEqual(Read(BindManager, TEXT("FreshB")).Remaining, FreshBefore.Remaining));
	BindManager->FinishBattle();
	TestEqual(TEXT("NumericAndBinding FinishBattle unbinds all speed delegates"), BindManager->GetSpeedDelegateBindingCountForAutomation(), 0);
	const FSnapshot FinishedABeforeBroadcast = Read(BindManager, TEXT("FreshA"));
	const FSnapshot FinishedBBeforeBroadcast = Read(BindManager, TEXT("FreshB"));
	const EHSRTurnManagerState FinishedStateBeforeBroadcast = BindManager->GetState();
	const FName FinishedCurrentBeforeBroadcast = BindManager->GetCurrentParticipantId();
	const uint64 FinishedEpochBeforeBroadcast = BindManager->GetBattleEpoch();
	const uint64 FinishedSequenceBeforeBroadcast = BindManager->GetTurnSequence();
	const int32 FinishedBindingsBeforeBroadcast = BindManager->GetSpeedDelegateBindingCountForAutomation();
	const int32 FinishedStartsBeforeBroadcast = BindStarted;
	const int32 FinishedEndsBeforeBroadcast = BindEnded;
	BroadcastSpeed(FreshB.AbilitySystemComponent.Get(), FreshBefore.Speed, 400.0f);
	const bool bOldASCAfterFinishAtomic = SameSnapshot(FinishedABeforeBroadcast, Read(BindManager, TEXT("FreshA")))
		&& SameSnapshot(FinishedBBeforeBroadcast, Read(BindManager, TEXT("FreshB")))
		&& BindManager->GetState() == FinishedStateBeforeBroadcast && BindManager->GetState() == EHSRTurnManagerState::Finished
		&& BindManager->GetCurrentParticipantId() == FinishedCurrentBeforeBroadcast && FinishedCurrentBeforeBroadcast.IsNone()
		&& BindManager->GetBattleEpoch() == FinishedEpochBeforeBroadcast
		&& BindManager->GetTurnSequence() == FinishedSequenceBeforeBroadcast
		&& BindManager->GetSpeedDelegateBindingCountForAutomation() == FinishedBindingsBeforeBroadcast && FinishedBindingsBeforeBroadcast == 0
		&& BindStarted == FinishedStartsBeforeBroadcast && BindEnded == FinishedEndsBeforeBroadcast;
	TestTrue(TEXT("NumericAndBinding old ASC broadcast after Finish has complete zero mutation"), bOldASCAfterFinishAtomic);
	UE_LOG(LogTemp, Log, TEXT("ActionDistanceCase Case=OldASCAfterFinish Result=%s State=%d Current=%s Epoch=%llu Sequence=%llu Bindings=%d Starts=%d Ends=%d"), bOldASCAfterFinishAtomic ? TEXT("PASS") : TEXT("FAIL"), static_cast<int32>(BindManager->GetState()), *BindManager->GetCurrentParticipantId().ToString(), BindManager->GetBattleEpoch(), BindManager->GetTurnSequence(), BindManager->GetSpeedDelegateBindingCountForAutomation(), BindStarted, BindEnded);
	UE_LOG(LogTemp, Log, TEXT("ActionDistanceCase Case=NumericAndBinding Result=PASS InitNaN=Rollback InitInf=Rollback RuntimeNaN=ZeroMutation RuntimeInf=ZeroMutation BindFailure=Atomic ResetStale=ZeroEpochStale=Zero"));
	return true;
}

bool FHSRRepeatableBreakPatchTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Engine is available"), GEngine)) return false;
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	if (!TestNotNull(TEXT("Standalone GameInstance is created"), GameInstance)) return false;
	GameInstance->AddToRoot();
	UWorld* BattleWorld = nullptr;
	UHSRBattleCoordinator* Coordinator = nullptr;
	ON_SCOPE_EXIT
	{
		if (Coordinator) Coordinator->Reset();
		if (GameInstance)
		{
			GameInstance->Shutdown();
			if (BattleWorld)
			{
				BattleWorld->DestroyWorld(false);
				GEngine->DestroyWorldContext(BattleWorld);
			}
			GameInstance->RemoveFromRoot();
		}
	};
	GameInstance->InitializeStandalone(FName(*FString::Printf(TEXT("HSRRepeatableBreak_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))));
	BattleWorld = GameInstance->GetWorld();
	if (!TestNotNull(TEXT("InitializeStandalone creates a World"), BattleWorld)
		|| !TestEqual(TEXT("World owns the standalone GameInstance"), BattleWorld->GetGameInstance(), GameInstance)) return false;

	TSubclassOf<AHSRBattleGameMode> GameModeClass = LoadClass<AHSRBattleGameMode>(nullptr, TEXT("/Game/Blueprints/Framework/BP_HSRBattleGameMode.BP_HSRBattleGameMode_C"));
	if (!TestNotNull(TEXT("Configured battle GameMode loads"), GameModeClass.Get())) return false;
	FText Failure;
	Coordinator = AHSRBattleGameMode::CreateRepeatableBreakAutomationFixture(GameInstance, BattleWorld, GameModeClass, Failure);
	if (!TestNotNull(*FString::Printf(TEXT("Production fixture builds: %s"), *Failure.ToString()), Coordinator)) return false;

	const auto Prepare = [this, Coordinator](float Toughness, bool bWeakness, float Health = 1000.0f)
	{
		if (Coordinator->GetParticipants().Num() != 2 || !Coordinator->GetBasicAttackDefinition()) return false;
		const FHSRBattleParticipant& Source = Coordinator->GetParticipants()[0];
		const FHSRBattleParticipant& Target = Coordinator->GetParticipants()[1];
		if (!Source.AbilitySystemComponent.IsValid() || !Target.AbilitySystemComponent.IsValid() || !Coordinator->GetTurnManager()) return false;
		const UHSRSkillDefinition* Skill = Coordinator->GetBasicAttackDefinition();
		const FString Element = Skill->ElementTag.ToString();
		const FGameplayTag Weakness = Element.StartsWith(TEXT("Element."))
			? FGameplayTag::RequestGameplayTag(FName(*FString::Printf(TEXT("Weakness.%s"), *Element.RightChop(8))), false) : FGameplayTag();
		FHSRBattleParticipant& MutableTarget = const_cast<FHSRBattleParticipant&>(Target);
		MutableTarget.WeaknessTags.Reset();
		if (bWeakness && Weakness.IsValid()) MutableTarget.WeaknessTags.AddTag(Weakness);
		Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 120.0f);
		Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 80.0f);
		Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 1000.0f);
		Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), Health);
		Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxToughnessAttribute(), FMath::Max(Toughness, Skill->ToughnessDamage));
		Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetToughnessAttribute(), Toughness);
		Coordinator->SetTeamSkillPointsForDevelopmentTest(1, 3);
		return Coordinator->GetTurnManager()->Initialize(Coordinator->GetParticipants());
	};
	const auto CommandFor = [Coordinator](const FGuid& ActionId, const FGuid& BattleId = FGuid())
	{
		FHSRBattleActionCommand Command;
		Command.ActionId = ActionId;
		Command.BattleId = BattleId.IsValid() ? BattleId : Coordinator->GetCurrentRequestId();
		Command.ActorParticipantId = Coordinator->GetParticipants()[0].ParticipantId;
		Command.SkillId = Coordinator->GetBasicAttackDefinition()->SkillId;
		Command.TargetParticipantIds.Add(Coordinator->GetParticipants()[1].ParticipantId);
		return Command;
	};
	const auto Counts = [Coordinator]() { return FIntPoint(Coordinator->GetBreakStatusRequestCountForDevelopmentTest(), Coordinator->GetBreakDelayRegistrationCountForDevelopmentTest()); };
	const float BreakDamage = Coordinator->GetBasicAttackDefinition()->ToughnessDamage;
	if (!TestTrue(TEXT("Configured attack has Toughness damage"), BreakDamage > 0.0f) || !Prepare(BreakDamage, true)) return false;

	const FGuid FirstId = FGuid::NewGuid();
	float FirstBreakTargetAtTurnEnd = -1.0f;
	UHSRTurnManager* FirstManager = Coordinator->GetTurnManager();
	FirstManager->OnTurnEnded().AddLambda([&FirstBreakTargetAtTurnEnd, Coordinator](const FHSRTurnLifecycleEvent& Event)
	{
		if (Coordinator && Event.ParticipantId == Coordinator->GetParticipants()[0].ParticipantId)
		{
			FirstBreakTargetAtTurnEnd = HSRActionDistanceAutomation::Read(Coordinator->GetTurnManager(), Coordinator->GetParticipants()[1].ParticipantId).Remaining;
		}
	});
	const HSRActionDistanceAutomation::FSnapshot FirstTargetBefore = HSRActionDistanceAutomation::Read(FirstManager, Coordinator->GetParticipants()[1].ParticipantId);
	const FIntPoint BeforeFirst = Counts();
	const uint64 TurnBeforeFirst = Coordinator->GetTurnManager()->GetTurnSequence();
	const FHSRAbilityResolution First = Coordinator->RequestAction(CommandFor(FirstId));
	TestTrue(TEXT("First positive-to-zero edge triggers Break"), First.Succeeded() && First.bHasBreakResult && First.BreakResult.bTriggered);
	TestEqual(TEXT("First edge reaches zero Toughness"), First.BreakResult.ToughnessAfter, 0.0f);
	TestEqual(TEXT("First edge adds one successful Status"), Counts().X, BeforeFirst.X + 1);
	TestEqual(TEXT("First edge adds one accepted Delay"), Counts().Y, BeforeFirst.Y + 1);
	TestEqual(TEXT("First Status result succeeds"), Coordinator->GetLastBreakStatusResultForDevelopmentTest(), EHSRStatusOperationResult::Success);
	TestTrue(TEXT("First Delay is accepted"), Coordinator->WasLastBreakDelayAcceptedForDevelopmentTest());
	TestTrue(TEXT("Break Delay is exactly one Base distance"), FMath::IsNearlyEqual(FirstBreakTargetAtTurnEnd, FirstTargetBefore.Remaining + FirstTargetBefore.Base));
	TestEqual(TEXT("First action advances the turn exactly once"), Coordinator->GetTurnManager()->GetTurnSequence(), TurnBeforeFirst + 1);
	const HSRActionDistanceAutomation::FSnapshot BeforeReplayDistance = HSRActionDistanceAutomation::Read(Coordinator->GetTurnManager(), Coordinator->GetParticipants()[1].ParticipantId);
	const FIntPoint BeforeReplay = Counts();
	const uint64 TurnBeforeReplay = Coordinator->GetTurnManager()->GetTurnSequence();
	const FHSRAbilityResolution Replay = Coordinator->RequestAction(CommandFor(FirstId));
	TestTrue(TEXT("Replay returns every cached Resolution field"), FHSRAbilityResolution::StaticStruct()->CompareScriptStruct(&Replay, &First, 0));
	TestEqual(TEXT("Replay has zero Status delta"), Counts().X, BeforeReplay.X);
	TestEqual(TEXT("Replay has zero Delay delta"), Counts().Y, BeforeReplay.Y);
	TestEqual(TEXT("Replay has zero turn delta"), Coordinator->GetTurnManager()->GetTurnSequence(), TurnBeforeReplay);
	const HSRActionDistanceAutomation::FSnapshot AfterReplayDistance = HSRActionDistanceAutomation::Read(Coordinator->GetTurnManager(), Coordinator->GetParticipants()[1].ParticipantId);
	TestTrue(TEXT("Break replay has zero action-distance mutation"), FMath::IsNearlyEqual(AfterReplayDistance.Base, BeforeReplayDistance.Base) && FMath::IsNearlyEqual(AfterReplayDistance.Remaining, BeforeReplayDistance.Remaining) && AfterReplayDistance.Pending == BeforeReplayDistance.Pending);

	Coordinator->GetParticipants()[1].AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetToughnessAttribute(), BreakDamage);
	TestEqual(TEXT("Recovery has zero Status delta"), Counts().X, BeforeReplay.X);
	TestEqual(TEXT("Recovery has zero Delay delta"), Counts().Y, BeforeReplay.Y);
	// Advance the existing turn lifecycle until the one-turn Break status has
	// naturally expired. This is fixture progression, not a recovery rule.
	UHSRStatusComponent* BreakComponent = Coordinator->GetStatusComponent(Coordinator->GetParticipants()[1].ParticipantId);
	for (int32 Step = 0; BreakComponent && Step < 4
		&& BreakComponent->GetSnapshot(TEXT("Status.Debuff.Break")).InstanceCount > 0; ++Step)
	{
		Coordinator->GetTurnManager()->ResolveAction(Coordinator->GetTurnManager()->GetCurrentParticipantId());
	}
	// Action distance preserves a full +Base Delay rather than the former skip-once
	// queue slot. Reach the player-owned command boundary deterministically.
	for (int32 Step = 0; Step < 8 && Coordinator->GetTurnManager()->GetCurrentParticipantId() != Coordinator->GetParticipants()[0].ParticipantId; ++Step)
	{
		Coordinator->GetTurnManager()->ResolveAction(Coordinator->GetTurnManager()->GetCurrentParticipantId());
	}
	TestEqual(TEXT("Second edge reaches the player command boundary"), Coordinator->GetTurnManager()->GetCurrentParticipantId(), Coordinator->GetParticipants()[0].ParticipantId);
	TestTrue(TEXT("Second action runtime prepares"), Prepare(BreakDamage, true));
	const FGuid SecondId = FGuid::NewGuid();
	const uint64 TurnBeforeSecond = Coordinator->GetTurnManager()->GetTurnSequence();
	const FHSRAbilityResolution Second = Coordinator->RequestAction(CommandFor(SecondId));
	TestTrue(TEXT("Second independent edge triggers Break"), Second.bHasBreakResult && Second.BreakResult.bTriggered && SecondId != FirstId);
	TestEqual(TEXT("Two edges produce two Status successes"), Counts().X, BeforeFirst.X + 2);
	TestEqual(TEXT("Two edges produce two accepted Delays"), Counts().Y, BeforeFirst.Y + 2);
	TestEqual(TEXT("Second edge advances the turn exactly once"), Coordinator->GetTurnManager()->GetTurnSequence(), TurnBeforeSecond + 1);

	TestTrue(TEXT("Zero-to-zero runtime prepares"), Prepare(0.0f, true));
	const FIntPoint BeforeZero = Counts();
	const FHSRAbilityResolution Zero = Coordinator->RequestAction(CommandFor(FGuid::NewGuid()));
	TestTrue(TEXT("Initial zero does not trigger Break"), Zero.bHasBreakResult && !Zero.BreakResult.bTriggered);
	TestEqual(TEXT("Initial zero has zero Break side effects"), Counts(), BeforeZero);
	TestTrue(TEXT("Continued zero runtime prepares"), Prepare(0.0f, true));
	Coordinator->RequestAction(CommandFor(FGuid::NewGuid()));
	TestEqual(TEXT("Continued zero has zero Break side effects"), Counts(), BeforeZero);

	TestTrue(TEXT("Non-zero runtime prepares"), Prepare(BreakDamage * 2.0f, true));
	const FIntPoint BeforeNonZero = Counts();
	const FHSRAbilityResolution NonZero = Coordinator->RequestAction(CommandFor(FGuid::NewGuid()));
	TestTrue(TEXT("Non-zero result does not trigger Break"), NonZero.bHasBreakResult && !NonZero.BreakResult.bTriggered && NonZero.ToughnessResult.After > 0.0f);
	TestEqual(TEXT("Non-zero result has zero Break side effects"), Counts(), BeforeNonZero);
	TestTrue(TEXT("No-weakness runtime prepares"), Prepare(BreakDamage, false));
	const FIntPoint BeforeNoWeakness = Counts();
	Coordinator->RequestAction(CommandFor(FGuid::NewGuid()));
	TestEqual(TEXT("No weakness has zero Break side effects"), Counts(), BeforeNoWeakness);

	const FGuid OldBattleId = Coordinator->GetCurrentRequestId();
	// FirstId was processed in the old battle and must become valid again in
	// the fresh battle-local resolution cache.
	const FGuid ReusedId = FirstId;
	const FHSRBattleInitResult Rebuild = Coordinator->ResetAndRebuildForDevelopmentTest(BattleWorld);
	TestTrue(TEXT("Reset rebuild succeeds"), Rebuild.IsSuccess());
	TestTrue(TEXT("New battle has a new BattleId"), Coordinator->GetCurrentRequestId() != OldBattleId);
	TestTrue(TEXT("New battle runtime prepares"), Prepare(BreakDamage, true));
	const FIntPoint BeforeStale = Counts();
	const FHSRAbilityResolution Stale = Coordinator->RequestAction(CommandFor(FGuid::NewGuid(), OldBattleId));
	TestFalse(TEXT("Old BattleId is rejected"), Stale.Succeeded());
	TestEqual(TEXT("Old BattleId has zero side effects"), Counts(), BeforeStale);
	float ReusedTargetAtTurnEnd = -1.0f;
	Coordinator->GetTurnManager()->OnTurnEnded().AddLambda([&ReusedTargetAtTurnEnd, Coordinator](const FHSRTurnLifecycleEvent& Event)
	{
		if (Coordinator && Event.ParticipantId == Coordinator->GetParticipants()[0].ParticipantId)
			ReusedTargetAtTurnEnd = HSRActionDistanceAutomation::Read(Coordinator->GetTurnManager(), Coordinator->GetParticipants()[1].ParticipantId).Remaining;
	});
	const HSRActionDistanceAutomation::FSnapshot ReusedTargetBefore = HSRActionDistanceAutomation::Read(Coordinator->GetTurnManager(), Coordinator->GetParticipants()[1].ParticipantId);
	const uint64 TurnBeforeReused = Coordinator->GetTurnManager()->GetTurnSequence();
	const FHSRAbilityResolution FreshReused = Coordinator->RequestAction(CommandFor(ReusedId));
	TestTrue(TEXT("ActionId is battle-local after Reset"), FreshReused.bHasBreakResult && FreshReused.BreakResult.bTriggered);
	TestEqual(TEXT("Reused ActionId adds one Status success"), Counts().X, BeforeStale.X + 1);
	TestEqual(TEXT("Reused ActionId adds one accepted Delay"), Counts().Y, BeforeStale.Y + 1);
	TestEqual(TEXT("Reused ActionId reaches exact zero Toughness"), FreshReused.ToughnessResult.After, 0.0f);
	TestTrue(TEXT("Reset reused ActionId adds exactly one fresh Base distance"), FMath::IsNearlyEqual(ReusedTargetAtTurnEnd, ReusedTargetBefore.Remaining + ReusedTargetBefore.Base));
	TestEqual(TEXT("Reused ActionId advances the turn exactly once"), Coordinator->GetTurnManager()->GetTurnSequence(), TurnBeforeReused + 1);

	const FHSRBattleInitResult DeadAdmissionRebuild = Coordinator->ResetAndRebuildForDevelopmentTest(BattleWorld);
	TestTrue(TEXT("Already-dead admission rebuild succeeds"), DeadAdmissionRebuild.IsSuccess());
	TestTrue(TEXT("Already-dead admission runtime prepares"), Prepare(BreakDamage, true));
	const FIntPoint BeforeDeadAdmission = Counts();
	const uint64 TurnBeforeDeadAdmission = Coordinator->GetTurnManager()->GetTurnSequence();
	Coordinator->GetParticipants()[1].AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 0.0f);
	const HSRActionDistanceAutomation::FSnapshot DeadAdmissionDistanceBefore = HSRActionDistanceAutomation::Read(Coordinator->GetTurnManager(), Coordinator->GetParticipants()[1].ParticipantId);
	const FHSRAbilityResolution DeadAdmission = Coordinator->RequestAction(CommandFor(FGuid::NewGuid()));
	TestFalse(TEXT("Target dead before command admission is rejected"), DeadAdmission.Succeeded());
	TestEqual(TEXT("Already-dead admission has zero Break side effects"), Counts(), BeforeDeadAdmission);
	TestEqual(TEXT("Already-dead admission has zero turn delta"), Coordinator->GetTurnManager()->GetTurnSequence(), TurnBeforeDeadAdmission);
	const HSRActionDistanceAutomation::FSnapshot DeadAdmissionDistanceAfter = HSRActionDistanceAutomation::Read(Coordinator->GetTurnManager(), Coordinator->GetParticipants()[1].ParticipantId);
	TestTrue(TEXT("Already-dead admission has zero action-distance mutation"), FMath::IsNearlyEqual(DeadAdmissionDistanceAfter.Remaining, DeadAdmissionDistanceBefore.Remaining) && DeadAdmissionDistanceAfter.Pending == DeadAdmissionDistanceBefore.Pending);

	const FHSRBattleInitResult LethalRebuild = Coordinator->ResetAndRebuildForDevelopmentTest(BattleWorld);
	TestTrue(TEXT("Lethal matrix rebuild succeeds"), LethalRebuild.IsSuccess());
	TestTrue(TEXT("Lethal runtime prepares"), Prepare(BreakDamage, true, 1.0f));
	const HSRActionDistanceAutomation::FSnapshot LethalTargetBefore = HSRActionDistanceAutomation::Read(Coordinator->GetTurnManager(), Coordinator->GetParticipants()[1].ParticipantId);
	const FIntPoint BeforeLethal = Counts();
	const int32 DefeatBefore = Coordinator->GetDefeatCountForDevelopmentTest();
	const uint64 TurnBeforeLethal = Coordinator->GetTurnManager()->GetTurnSequence();
	const FHSRAbilityResolution Lethal = Coordinator->RequestAction(CommandFor(FGuid::NewGuid()));
	TestTrue(TEXT("Same-frame lethal publishes Break before terminal defeat"), Lethal.bHasBreakResult && Lethal.BreakResult.bTriggered
		&& Counts() == BeforeLethal + FIntPoint(1, 1) && Coordinator->GetDefeatCountForDevelopmentTest() == DefeatBefore + 1
		&& Coordinator->GetCurrentState() == EHSRBattleCoordinatorState::Finished);
	TestEqual(TEXT("Same-frame lethal has exact zero turn-advance delta"), Coordinator->GetTurnManager()->GetTurnSequence(), TurnBeforeLethal);
	const HSRActionDistanceAutomation::FSnapshot LethalTargetAfter = HSRActionDistanceAutomation::Read(Coordinator->GetTurnManager(), Coordinator->GetParticipants()[1].ParticipantId);
	TestTrue(TEXT("Same-frame admitted-alive Break adds exactly one Base distance"), FMath::IsNearlyEqual(LethalTargetAfter.Remaining, LethalTargetBefore.Remaining + LethalTargetBefore.Base));
	const FIntPoint BeforeFinished = Counts();
	const FHSRAbilityResolution Finished = Coordinator->RequestAction(CommandFor(FGuid::NewGuid()));
	TestFalse(TEXT("Finished battle rejects new action"), Finished.Succeeded());
	TestEqual(TEXT("Finished battle has zero Break side effects"), Counts(), BeforeFinished);
	return true;
}

bool FHSRStatusGenericPatchTest::RunTest(const FString& Parameters)
{
	UGameplayEffect* EffectDefault = UGameplayEffect::StaticClass()->GetDefaultObject<UGameplayEffect>();
	const EGameplayEffectDurationType OriginalDurationPolicy = EffectDefault->DurationPolicy;
	const EGameplayEffectStackingType OriginalStackingType = EffectDefault->StackingType;
	EffectDefault->DurationPolicy = EGameplayEffectDurationType::Infinite;
	EffectDefault->StackingType = EGameplayEffectStackingType::AggregateByTarget;

	const auto MakeTransientBuff = [](FName StatusId, EHSRStatusRefreshPolicy Policy, int32 MaxStacks, int32 DurationTurns)
	{
		UHSRStatusDefinition* Definition = NewObject<UHSRStatusDefinition>();
		Definition->StatusId = StatusId;
		Definition->GrantedStatusTag = FGameplayTag::RequestGameplayTag(StatusId, false);
		Definition->InfiniteGameplayEffectClass = UGameplayEffect::StaticClass();
		Definition->Classification = EHSRStatusClassification::Buff;
		Definition->EffectKind = EHSRStatusEffectKind::TagOnly;
		Definition->RefreshPolicy = Policy;
		Definition->DurationTurns = DurationTurns;
		Definition->MaxStacks = MaxStacks;
		return Definition;
	};

	UHSRStatusDefinition* Attack = MakeTransientBuff(TEXT("Status.Buff.AttackUp"), EHSRStatusRefreshPolicy::RefreshDuration, 1, 2);
	UHSRStatusDefinition* Speed = MakeTransientBuff(TEXT("Status.Buff.SpeedUp"), EHSRStatusRefreshPolicy::AddStack, 2, 2);
	UHSRStatusDefinition* Shield = MakeTransientBuff(TEXT("Status.Buff.Shield"), EHSRStatusRefreshPolicy::RefreshDuration, 1, 1);
	TestEqual(TEXT("Attack transient definition is field-valid"), Attack->Validate(), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Speed transient definition is field-valid"), Speed->Validate(), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Shield transient definition is field-valid"), Shield->Validate(), EHSRStatusOperationResult::Success);

	Speed->GrantedStatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Buff.AttackUp"), false);
	TestEqual(TEXT("Tag/id mismatch is structured"), Speed->Validate(), EHSRStatusOperationResult::InvalidDefinition);
	Speed->GrantedStatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Buff.SpeedUp"), false);
	Shield->StatusId = TEXT("Buff.Shield");
	TestEqual(TEXT("Invalid root is structured"), Shield->Validate(), EHSRStatusOperationResult::InvalidStatusId);
	Shield->StatusId = TEXT("Status.Buff.Shield");
	Shield->RefreshPolicy = EHSRStatusRefreshPolicy::AddStack;
	Shield->MaxStacks = 1;
	TestEqual(TEXT("Invalid policy is structured"), Shield->Validate(), EHSRStatusOperationResult::InvalidPolicy);
	Shield->RefreshPolicy = EHSRStatusRefreshPolicy::RefreshDuration;
	Shield->MaxStacks = 1;

	UWorld* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
	AActor* Actor = World->SpawnActor<AActor>();
	UAbilitySystemComponent* AbilitySystem = Cast<UAbilitySystemComponent>(Actor->AddComponentByClass(UHSRAbilitySystemComponent::StaticClass(), false, FTransform::Identity, false));
	const UHSRCoreAttributeSet* Attributes = Cast<UHSRCoreAttributeSet>(AbilitySystem->InitStats(UHSRCoreAttributeSet::StaticClass(), nullptr));
	TestNotNull(TEXT("Transient core attributes initialize"), Attributes);
	AbilitySystem->InitAbilityActorInfo(Actor, Actor);
	const_cast<UHSRCoreAttributeSet*>(Attributes)->MaxHealth.SetBaseValue(100.0f);
	const_cast<UHSRCoreAttributeSet*>(Attributes)->MaxHealth.SetCurrentValue(100.0f);
	const_cast<UHSRCoreAttributeSet*>(Attributes)->Health.SetBaseValue(100.0f);
	const_cast<UHSRCoreAttributeSet*>(Attributes)->Health.SetCurrentValue(100.0f);
	AbilitySystem->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 100.0f);
	AbilitySystem->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 100.0f);
	AbilitySystem->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 100.0f);
	TestTrue(TEXT("Transient participant has positive health"), AbilitySystem->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) > 0.0f);
	FHSRBattleParticipant Participant;
	Participant.ParticipantId = TEXT("StatusGenericTarget");
	Participant.DefinitionId = TEXT("StatusGenericDefinition");
	Participant.Team = EHSRBattleParticipantTeam::Player;
	Participant.InitiativeSpeed = 100.0f;
	Participant.Actor = Actor;
	Participant.AbilitySystemComponent = AbilitySystem;
	UHSRTurnManager* TurnManager = NewObject<UHSRTurnManager>();
	TestTrue(TEXT("Transient turn manager initializes"), TurnManager->Initialize({ Participant }));
	UHSRStatusComponent* Component = NewObject<UHSRStatusComponent>(Actor);
	TestTrue(TEXT("Transient status component initializes"), Component->InitializeStatusRuntime(Participant.ParticipantId, AbilitySystem));
	TestTrue(TEXT("Transient status component binds turn manager"), Component->BindTurnManager(TurnManager));

	const FGuid AttackOperation = FGuid::NewGuid();
	TestEqual(TEXT("Attack adds through generic runtime"), Component->AddOrRefreshStatus(Attack, Participant.ParticipantId, Participant.ParticipantId, AttackOperation), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Duplicate operation is ignored"), Component->AddOrRefreshStatus(Attack, Participant.ParticipantId, Participant.ParticipantId, AttackOperation), EHSRStatusOperationResult::IgnoredEvent);
	FHSRStatusPublicSnapshot AttackSnapshot;
	FHSRStatusPublicSnapshot SpeedSnapshot;
	FHSRStatusPublicSnapshot ShieldSnapshot;
	TestEqual(TEXT("Attack typed query succeeds"), Component->GetPublicSnapshot(Attack->StatusId, AttackSnapshot), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Attack begins with one stack"), AttackSnapshot.Stacks, 1);
	TestEqual(TEXT("Attack refreshes through generic runtime"), Component->AddOrRefreshStatus(Attack, Participant.ParticipantId, Participant.ParticipantId), EHSRStatusOperationResult::Refreshed);

	FHSRTurnLifecycleEvent FirstEnd;
	FirstEnd.EventType = EHSRTurnLifecycleEventType::TurnEnded;
	FirstEnd.BattleEpoch = TurnManager->GetBattleEpoch();
	FirstEnd.ParticipantId = Participant.ParticipantId;
	FirstEnd.TurnSequence = 1;
	Component->ConsumeLifecycleEventForDevelopmentTest(FirstEnd);

	FHSRTurnLifecycleEvent SecondEnd = FirstEnd;
	SecondEnd.TurnSequence = 2;
	Component->ConsumeLifecycleEventForDevelopmentTest(SecondEnd);
	TestEqual(TEXT("Attack expires after refreshed duration"), Component->GetPublicSnapshot(Attack->StatusId, AttackSnapshot), EHSRStatusOperationResult::UnknownStatus);

	TestEqual(TEXT("Speed adds through generic runtime"), Component->AddOrRefreshStatus(Speed, Participant.ParticipantId, Participant.ParticipantId), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Speed typed query succeeds"), Component->GetPublicSnapshot(Speed->StatusId, SpeedSnapshot), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Speed begins with one stack"), SpeedSnapshot.Stacks, 1);
	TestEqual(TEXT("Speed stacks through generic runtime"), Component->AddOrRefreshStatus(Speed, Participant.ParticipantId, Participant.ParticipantId), EHSRStatusOperationResult::StackAdded);
	TestEqual(TEXT("Speed stack count is public"), Component->GetPublicSnapshot(Speed->StatusId, SpeedSnapshot), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Speed exposes two stacks"), SpeedSnapshot.Stacks, 2);
	FHSRTurnLifecycleEvent SpeedFirstEnd = FirstEnd;
	SpeedFirstEnd.TurnSequence = 3;
	Component->ConsumeLifecycleEventForDevelopmentTest(SpeedFirstEnd);
	TestEqual(TEXT("Speed keeps stacked state after one turn"), Component->GetPublicSnapshot(Speed->StatusId, SpeedSnapshot), EHSRStatusOperationResult::Success);
	FHSRTurnLifecycleEvent SpeedSecondEnd = FirstEnd;
	SpeedSecondEnd.TurnSequence = 4;
	Component->ConsumeLifecycleEventForDevelopmentTest(SpeedSecondEnd);
	TestEqual(TEXT("Speed expires after stacked duration"), Component->GetPublicSnapshot(Speed->StatusId, SpeedSnapshot), EHSRStatusOperationResult::UnknownStatus);

	TestEqual(TEXT("Shield adds through generic runtime"), Component->AddOrRefreshStatus(Shield, Participant.ParticipantId, Participant.ParticipantId), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Shield typed query succeeds"), Component->GetPublicSnapshot(Shield->StatusId, ShieldSnapshot), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Shield begins with one stack"), ShieldSnapshot.Stacks, 1);
	FHSRTurnLifecycleEvent ShieldEnd = FirstEnd;
	ShieldEnd.TurnSequence = 5;
	Component->ConsumeLifecycleEventForDevelopmentTest(ShieldEnd);
	TestEqual(TEXT("Shield expires on its first target turn"), Component->GetPublicSnapshot(Shield->StatusId, ShieldSnapshot), EHSRStatusOperationResult::UnknownStatus);
	TestEqual(TEXT("Shield re-adds for clear coverage"), Component->AddOrRefreshStatus(Shield, Participant.ParticipantId, Participant.ParticipantId), EHSRStatusOperationResult::Success);
	Component->SetForceClearRemoveFailureForDevelopmentTest(true);
	TestEqual(TEXT("Controlled clear removal failure is structured"), Component->ClearStatus(), EHSRStatusOperationResult::RemoveFailed);
	TestEqual(TEXT("Failed clear retains typed ownership"), Component->GetPublicSnapshot(Shield->StatusId, ShieldSnapshot), EHSRStatusOperationResult::Success);
	TestTrue(TEXT("Failed clear retains an active handle"), Component->GetSnapshot(Shield->StatusId).bHandleActiveInAbilitySystem);
	Component->SetForceClearRemoveFailureForDevelopmentTest(false);
	TestEqual(TEXT("Clear retry succeeds"), Component->ClearStatus(), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Clear retry removes typed ownership"), Component->GetPublicSnapshot(Shield->StatusId, ShieldSnapshot), EHSRStatusOperationResult::UnknownStatus);

	FHSRStatusPublicSnapshot UnknownSnapshot;
	TestEqual(TEXT("Unknown lookup returns structured result"), Component->GetPublicSnapshot(TEXT("Status.Buff.Unknown"), UnknownSnapshot), EHSRStatusOperationResult::UnknownStatus);
	TestEqual(TEXT("Unknown lookup preserves requested id"), UnknownSnapshot.StatusId, FName(TEXT("Status.Buff.Unknown")));
	EffectDefault->DurationPolicy = OriginalDurationPolicy;
	EffectDefault->StackingType = OriginalStackingType;
	World->DestroyWorld(false);
	return true;
}

#endif
