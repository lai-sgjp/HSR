#include "HSREnemyAIController.h"
#include "HSREnemyCharacter.h"
#include "../Data/Definitions/HSREnemyDefinition.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../Character/HSRExplorationCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "GameFramework/Character.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"

namespace HSREnemyBlackboardKeys
{
	// 所有敌人在黑板上读写的一致键名，集中定义避免字符串拼写不一致。
	static const FName TargetActor(TEXT("TargetActor"));
	static const FName SpawnOrigin(TEXT("SpawnOrigin"));
	static const FName PatrolLocation(TEXT("PatrolLocation"));
	static const FName AIState(TEXT("AIState"));
	static const FName TreeEpoch(TEXT("TreeEpoch"));
	static const FName EncounterRequestId(TEXT("EncounterRequestId"));
}

AHSREnemyAIController::AHSREnemyAIController()
{
	// AI 行为由行为树驱动，不需要控制器自身 Tick。
	PrimaryActorTick.bCanEverTick = false;

	// 感知组件：敌人通过视觉感知发现玩家。
	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));

	// 视觉配置：视线半径 1000，丢失视野半径放宽到 1500（避免在边界来回闪烁），
	// 视野角 90 度；对敌/中立/友好均检测。SetMaxAge(5) 表示 5 秒内未见则判定目标丢失。
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 1000.0f;
	SightConfig->LoseSightRadius = 1500.0f;
	SightConfig->PeripheralVisionAngleDegrees = 90.0f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->SetMaxAge(5.0f);

	PerceptionComponent->ConfigureSense(*SightConfig);
	PerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());

	CurrentState = EHSREnemyExplorationState::Idle;
}

void AHSREnemyAIController::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::BeginPlay - %s"), *GetName());

	// Stage A 刻意不再启动旧的巡逻计时器：行为树填充后，Stage B 的
	// 原生 Move To / Wait 节点是唯一移动驱动。这里只做"导航就绪后补发一次巡逻意图"，
	// 避免开局导航未就绪导致首次巡逻目标丢失。
	ScheduleNavReadyPatrolIntent();
}

void AHSREnemyAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 清理顺序：先停行为树，再清状态，最后移除感知委托，保证不再有任何回调残留。
	StopBehaviorTreeRuntime();
	ClearState();

	if (PerceptionComponent)
	{
		PerceptionComponent->OnTargetPerceptionUpdated.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AHSREnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::OnPossess - Controller=%s Pawn=%s"), *GetName(), InPawn ? *InPawn->GetName() : TEXT("null"));

	// P4-002: 重新 Possess 时强制重新绑定感知委托，保证整条观察链只有一份绑定（防止重复触发）。
	UE_LOG(LogTemp, Log, TEXT("P4-002: %s - OnPossess, fresh delegate binding (single observation chain)"), *GetName());

	// Possess 到 Pawn 时绑定感知委托：先 RemoveAll 再 AddDynamic，确保幂等。
	if (PerceptionComponent)
	{
		PerceptionComponent->OnTargetPerceptionUpdated.RemoveAll(this);
		PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AHSREnemyAIController::OnTargetPerceptionUpdated);
	}
	// 用敌人定义中的感知参数覆盖默认配置（视线半径等来自数据资产）。
	ApplyDefinitionPerceptionConfig();

	StartBehaviorTreeRuntime();
	ScheduleNavReadyPatrolIntent();
}

void AHSREnemyAIController::OnUnPossess()
{
	// P4-002: UnPossess -> 清空计时器/状态、移除委托，保证零残留回调。
	UE_LOG(LogTemp, Log, TEXT("P4-002: %s - OnUnPossess, clearing state and delegates"), *GetName());
	StopBehaviorTreeRuntime();
	ClearState();

	if (PerceptionComponent)
	{
		PerceptionComponent->OnTargetPerceptionUpdated.RemoveAll(this);
	}

	Super::OnUnPossess();
}

void AHSREnemyAIController::SetState(EHSREnemyExplorationState NewState)
{
	// 状态机写入统一入口：相同状态直接忽略，避免无意义的黑板写入与日志刷屏。
	if (CurrentState == NewState)
	{
		return;
	}

	EHSREnemyExplorationState OldState = CurrentState;
	CurrentState = NewState;
	// 把状态同步到黑板，行为树据此切换分支（AIState 键）。
	WriteBlackboardRuntimeState();

	UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::SetState - %s: %d -> %d"),
		*GetName(), static_cast<int32>(OldState), static_cast<int32>(NewState));
}

void AHSREnemyAIController::ClearState()
{
	// 停止移动：终止正在进行的 MoveTo 请求，防止路径跟随回调继续干扰后续逻辑。
	if (GetPathFollowingComponent())
	{
		GetPathFollowingComponent()->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished);
	}

	// 清空目标引用与黑板目标，重置巡逻发布状态。
	CurrentTarget.Reset();
	SetBlackboardTarget(nullptr);
	bHasPublishedPatrolLocation = false;
	PublishedPatrolLocation = FVector::ZeroVector;
	CurrentState = EHSREnemyExplorationState::Idle;
	WriteBlackboardRuntimeState();
}

void AHSREnemyAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	// 在 EncounterPending（已提交遭遇，等待传送）或 Idle（已清理）状态下不处理移动结果，
	// 避免行为树分支切换产生的 MoveTo 终止事件干扰状态机。
	if (CurrentState == EHSREnemyExplorationState::EncounterPending ||
		CurrentState == EHSREnemyExplorationState::Idle)
	{
		return;
	}

	if (!Result.IsSuccess())
	{
		// 移动失败或被中止：仅在确实需要处理的分支状态下做恢复，否则视为"分支切换导致的中止"忽略。
		if (ShouldHandleMoveFailureOrAbort())
		{
			HandleMoveFailedOrAborted();
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("P17-PATCH-02 MoveAbortIgnored Controller=%s State=%d Reason=BranchSwitch"), *GetName(), static_cast<int32>(CurrentState));
		}
	}
	else if (CurrentState == EHSREnemyExplorationState::MovingToPatrol)
	{
		// 巡逻移动到点：到达后发布下一个巡逻意图（以 SpawnOrigin 为圆心随机选取可达点）。
		AHSREnemyCharacter* Enemy = Cast<AHSREnemyCharacter>(GetPawn());
		UHSREnemyDefinition* Definition = Enemy ? Enemy->EnemyDefinition : nullptr;
		if (Enemy && Definition)
		{
			PublishNextPatrolIntent(Enemy->GetSpawnOrigin(), Definition->PatrolRadius);
		}
	}
	else if (CurrentState == EHSREnemyExplorationState::Chasing)
	{
		// 追击完成（通常意味着追到了玩家附近）：若目标已失效则安全清理。
		AActor* Target = CurrentTarget.Get();
		if (!Target || !IsValid(Target))
		{
			// P4-002: 追击途中目标被销毁/过期，通过弱引用安全清理。
			HandleChaseTargetLost();
		}
	}
	else if (CurrentState == EHSREnemyExplorationState::ReturningToSpawnOrigin)
	{
		// 返回出生点完成：恢复巡逻节奏。
		AHSREnemyCharacter* Enemy = Cast<AHSREnemyCharacter>(GetPawn());
		UHSREnemyDefinition* Definition = Enemy ? Enemy->EnemyDefinition : nullptr;
		if (Enemy && Definition)
		{
			ResumePatrolAfterReturn(Enemy->GetSpawnOrigin(), Definition->PatrolRadius);
		}
	}
}

void AHSREnemyAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// 只关心探索角色（玩家）；其他 Actor 的感知事件一律忽略。
	AHSRExplorationCharacter* PlayerChar = Cast<AHSRExplorationCharacter>(Actor);
	if (!PlayerChar)
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		// 感知成功：开始追击该目标。
		BeginChasingTarget(Actor);
	}
	else
	{
		// 目标丢失：只有当我们正在追击的正是这个目标、且处于追击/警戒状态时才响应。
		if (CurrentTarget.Get() == Actor &&
			(CurrentState == EHSREnemyExplorationState::Chasing || CurrentState == EHSREnemyExplorationState::Alert))
		{
			UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::OnTargetPerceptionUpdated - %s lost sight of %s"), *GetName(), *Actor->GetName());
			// P4-002: 追击途中目标被销毁/过期，通过弱引用安全清理。
			HandleChaseTargetLost();
		}
	}
}

void AHSREnemyAIController::HandleChaseTargetLost()
{
	// 统一的目标丢失处理：清目标 -> 回出生点恢复（LostTarget 状态）。
	CurrentTarget.Reset();
	SetBlackboardTarget(nullptr);
	BeginSpawnOriginRecovery(EHSREnemyExplorationState::LostTarget);
}

void AHSREnemyAIController::ApplyDefinitionPerceptionConfig()
{
	// 从当前 Possess 的敌人身上取定义，把感知参数应用上去。
	const AHSREnemyCharacter* Enemy = Cast<AHSREnemyCharacter>(GetPawn());
	const UHSREnemyDefinition* Definition = Enemy ? Enemy->EnemyDefinition : nullptr;
	ApplyPerceptionConfig(Definition);
}

void AHSREnemyAIController::ApplyPerceptionConfig(const UHSREnemyDefinition* Definition)
{
	if (!SightConfig)
	{
		return;
	}
	// 定义缺省时回退到默认值；视线半径下限截到 0，LoseSightRadius 不小于 SightRadius。
	SightConfig->SightRadius = FMath::Max(0.0f, Definition ? Definition->SightRadius : 1000.0f);
	SightConfig->LoseSightRadius = FMath::Max(SightConfig->SightRadius, Definition ? Definition->LoseSightRadius : 1500.0f);
	if (PerceptionComponent)
	{
		// 配置变更后必须请求更新，否则新半径不会生效。
		PerceptionComponent->RequestStimuliListenerUpdate();
	}
}

#if WITH_DEV_AUTOMATION_TESTS
// ---- 自动化测试专用访问器：仅在自动化测试构建中存在，不参与正常游戏逻辑 ----
void AHSREnemyAIController::ApplyDefinitionPerceptionConfigForAutomation(const UHSREnemyDefinition* InDefinition)
{
	ApplyPerceptionConfig(InDefinition);
}

float AHSREnemyAIController::GetSightRadiusForAutomation() const
{
	return SightConfig ? SightConfig->SightRadius : -1.0f;
}

float AHSREnemyAIController::GetLoseSightRadiusForAutomation() const
{
	return SightConfig ? SightConfig->LoseSightRadius : -1.0f;
}
#endif

void AHSREnemyAIController::BeginChasingTarget(AActor* Actor)
{
	// 感知只负责把"警戒/追击"意图交给行为树原生 Move To 节点；
	// 遭遇战准入（Encounter）仍只由角色 Overlap 接触独占，感知不会提交遭遇。
	if (CurrentState == EHSREnemyExplorationState::EncounterPending)
	{
		return;
	}

	if (CurrentState == EHSREnemyExplorationState::Chasing && CurrentTarget.Get() == Actor)
	{
		UE_LOG(LogTemp, Log, TEXT("P4-002: repeat perception of same target, blocked (no storm)"));
		return;
	}

	// 状态推进：警戒 -> 记目标 -> 停步 -> 追击。
	SetState(EHSREnemyExplorationState::Alert);
	CurrentTarget = Actor;
	SetBlackboardTarget(Actor);
	StopMovement();
	SetState(EHSREnemyExplorationState::Chasing);

	UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::BeginChasingTarget - %s sensed %s, chasing without encounter submission"), *GetName(), *GetNameSafe(Actor));
}

void AHSREnemyAIController::HandleMoveFailedOrAborted()
{
	BeginSpawnOriginRecovery(EHSREnemyExplorationState::MoveFailed);
}

bool AHSREnemyAIController::ShouldHandleMoveFailureOrAbort() const
{
	// 只有巡逻移动与返回出生点这两类"常规移动"才需要处理失败/中止；
	// 追击中中止应由感知丢失逻辑处理。
	return CurrentState == EHSREnemyExplorationState::MovingToPatrol || CurrentState == EHSREnemyExplorationState::ReturningToSpawnOrigin;
}

void AHSREnemyAIController::BeginSpawnOriginRecovery(EHSREnemyExplorationState RecoveryState)
{
	AHSREnemyCharacter* Enemy = Cast<AHSREnemyCharacter>(GetPawn());
	if (!Enemy)
	{
		return;
	}
	PublishSpawnOriginRecoveryIntent(Enemy->GetSpawnOrigin(), RecoveryState);
}


void AHSREnemyAIController::PublishSpawnOriginRecoveryIntent(const FVector& InSpawnOrigin, EHSREnemyExplorationState RecoveryState)
{
#if WITH_DEV_AUTOMATION_TESTS
	LastRecoveryStateForAutomation = RecoveryState;
#endif
	// 先把恢复状态写进状态机（便于日志与黑板状态一致），再把出生点写入黑板让行为树执行返回。
	SetState(RecoveryState);
	if (RuntimeBlackboard)
	{
		RuntimeBlackboard->SetValueAsVector(HSREnemyBlackboardKeys::PatrolLocation, InSpawnOrigin);
	}
	SetState(EHSREnemyExplorationState::ReturningToSpawnOrigin);
	if (RuntimeBlackboard)
	{
		RuntimeBlackboard->SetValueAsVector(HSREnemyBlackboardKeys::SpawnOrigin, InSpawnOrigin);
		RuntimeBlackboard->SetValueAsVector(HSREnemyBlackboardKeys::PatrolLocation, InSpawnOrigin);
	}
	// Stage B 的原生 Move To 节点会消费 PatrolLocation=SpawnOrigin 回到出生点。
}

void AHSREnemyAIController::ResumePatrolAfterReturn(const FVector& InSpawnOrigin, float PatrolRadius)
{
	// 计算距出生点距离仅用于日志排查；随后恢复普通巡逻。
	const float ReturnDistance = FVector::Dist(GetPawn() ? GetPawn()->GetActorLocation() : InSpawnOrigin, InSpawnOrigin);
	UE_LOG(LogTemp, Log, TEXT("P17-PATCH-02 ReturnComplete Controller=%s Location=%s SpawnOrigin=%s Distance=%.2f RequestId=%s"), *GetName(), GetPawn() ? *GetPawn()->GetActorLocation().ToString() : TEXT("None"), *InSpawnOrigin.ToString(), ReturnDistance, *ActiveEncounterRequestId.ToString());
	PublishNextPatrolIntent(InSpawnOrigin, PatrolRadius);
}

void AHSREnemyAIController::TryRequestEncounterFromCharacter()
{
#if WITH_DEV_AUTOMATION_TESTS
	++EncounterSubmissionAttemptsForAutomation;
#endif
	// 遭遇战准入由角色 Overlap 接触独占。除 EncounterPending（避免对同一场战斗重复提交）外，
	// 任意探索状态都接受接触。
	if (CurrentState == EHSREnemyExplorationState::EncounterPending)
	{
		UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::TryRequestEncounter - %s FAILED (state=%d, EncounterPending)"),
			*GetName(), static_cast<int32>(CurrentState));
		return;
	}
	if (ActiveEncounterRequestId.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("P17-PATCH-02 Encounter duplicate rejected: %s RequestId=%s"), *GetName(), *ActiveEncounterRequestId.ToString());
		return;
	}

	AHSREnemyCharacter* EnemyChar = Cast<AHSREnemyCharacter>(GetPawn());
	if (!EnemyChar)
	{
		return;
	}

	UHSREnemyDefinition* Def = EnemyChar->EnemyDefinition;
	if (!Def)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSREnemyAIController::TryRequestEncounter - %s FAILED (EnemyDefinition null)"), *GetName());
		return;
	}

	UHSREncounterDefinition* EncounterDef = Def->EncounterDefinition;
	if (!EncounterDef)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSREnemyAIController::TryRequestEncounter - %s FAILED (EncounterDefinition null, EnemyId=%s)"),
			*GetName(), *Def->EnemyDefinitionId.ToString());
		return;
	}

	UHSRBattleTransitionSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSREnemyAIController::TryRequestEncounter - %s FAILED (no Subsystem)"), *GetName());
		return;
	}

	if (Subsystem->HasPending())
	{
		UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::TryRequestEncounter - %s: subsystem already pending, skipping"), *GetName());
		return;
	}

	FHSREncounterResult EncResult = Subsystem->RequestEncounter(EncounterDef, EHSREncounterInitiative::Enemy);
	if (EncResult.ResultType == EHSREncounterResultType::Success)
	{
		// 提交成功：记录 RequestId、进入 EncounterPending 状态并停止移动，等待传送。
		ActiveEncounterRequestId = EncResult.RequestId;
		SetState(EHSREnemyExplorationState::EncounterPending);
		StopMovement();

		UE_LOG(LogTemp, Log, TEXT("AHSREnemyAIController::TryRequestEncounter - %s SUCCESS (RequestId=%s, EnemyId=%s)"),
			*GetName(), *EncResult.RequestId.ToString(), *Def->EnemyDefinitionId.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSREnemyAIController::TryRequestEncounter - %s FAILED (type=%d, msg=%s)"),
			*GetName(), static_cast<int32>(EncResult.ResultType), *EncResult.Message.ToString());
		// 保持追击状态：Encounter Overlap 可能再次触发，届时可重试。
	}
}

bool AHSREnemyAIController::StartBehaviorTreeRuntime()
{
	AHSREnemyCharacter* Enemy = Cast<AHSREnemyCharacter>(GetPawn());
	UHSREnemyDefinition* Definition = Enemy ? Enemy->EnemyDefinition : nullptr;
	UBehaviorTree* Tree = Definition ? Definition->BehaviorTreeAsset.LoadSynchronous() : nullptr;
	UBlackboardData* BlackboardData = Definition ? Definition->BlackboardAsset.LoadSynchronous() : nullptr;
	UBlackboardComponent* BlackboardComponent = nullptr;
	// 校验 BT/BB 引用完整、且行为树声明的黑板与定义的黑板一致，再初始化黑板组件。
	if (!Tree || !BlackboardData || Tree->BlackboardAsset != BlackboardData || !UseBlackboard(BlackboardData, BlackboardComponent) || !BlackboardComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("P17-PATCH-02 AI init failed: %s has invalid BT/BB references"), *GetName());
		return false;
	}

	// 记录运行时黑板；递增行为树纪元（每次重启都刷新，用于让旧的重试计时器失效）。
	RuntimeBlackboard = BlackboardComponent;
	++BehaviorTreeEpoch;
	ActiveEncounterRequestId.Invalidate();
	PublishNextPatrolIntent(Enemy->GetSpawnOrigin(), Definition->PatrolRadius);
	if (!RunBehaviorTree(Tree))
	{
		// 运行失败：清理黑板运行态，恢复 Idle。
		ClearBlackboardRuntimeState();
		RuntimeBlackboard = nullptr;
		CurrentState = EHSREnemyExplorationState::Idle;
		UE_LOG(LogTemp, Error, TEXT("P17-PATCH-02 AI init failed: %s could not run the Behavior Tree"), *GetName());
		return false;
	}
	return true;
}

void AHSREnemyAIController::StopBehaviorTreeRuntime()
{
	// 判断当前是否持有运行时所有权（黑板、重试计时器、激活的遭遇请求）。
	const bool bHadRuntimeOwnership = RuntimeBlackboard != nullptr || bNavReadyRetryScheduled || ActiveEncounterRequestId.IsValid();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NavReadyRetryTimerHandle);
	}
	bNavReadyRetryScheduled = false;
	NavReadyRetryEpoch = INDEX_NONE;
	if (BrainComponent)
	{
		BrainComponent->StopLogic(TEXT("P17-PATCH-02 lifecycle teardown"));
	}
	ClearBlackboardRuntimeState();
	// ClearState/UnPossess/EndPlay 会紧随此调用。先解绑黑板再返回，
	// 确保后续任何清理写入都不会往刚清空的黑板里重新写入数据。
	RuntimeBlackboard = nullptr;
	ActiveEncounterRequestId.Invalidate();
	if (bHadRuntimeOwnership)
	{
		++BehaviorTreeEpoch;
	}
}

void AHSREnemyAIController::WriteBlackboardRuntimeState()
{
	if (!RuntimeBlackboard)
	{
		return;
	}

	// 把运行时状态完整同步到黑板：出生点、AI 状态、行为树纪元、激活的遭遇请求与当前目标。
	const AHSREnemyCharacter* Enemy = Cast<AHSREnemyCharacter>(GetPawn());
	RuntimeBlackboard->SetValueAsVector(HSREnemyBlackboardKeys::SpawnOrigin, Enemy ? Enemy->GetSpawnOrigin() : FVector::ZeroVector);
	RuntimeBlackboard->SetValueAsEnum(HSREnemyBlackboardKeys::AIState, static_cast<uint8>(CurrentState));
	RuntimeBlackboard->SetValueAsInt(HSREnemyBlackboardKeys::TreeEpoch, BehaviorTreeEpoch);
	RuntimeBlackboard->SetValueAsName(HSREnemyBlackboardKeys::EncounterRequestId, ActiveEncounterRequestId.IsValid() ? FName(*ActiveEncounterRequestId.ToString()) : NAME_None);
	SetBlackboardTarget(CurrentTarget.Get());
}

void AHSREnemyAIController::PublishNextPatrolIntent(const FVector& InSpawnOrigin, float PatrolRadius)
{
	// 先取导航系统与默认导航数据；随后把出生点投影到导航网格上，
	// 再在半径内随机找一个可达点作为下一个巡逻目的地。
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ANavigationData* NavData = NavSystem ? NavSystem->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
	FNavLocation ProjectedCenter;
	const FVector ProjectionExtent(100.0f, 100.0f, 300.0f);
	const bool bProjected = NavData
		&& NavSystem->ProjectPointToNavigation(InSpawnOrigin, ProjectedCenter, ProjectionExtent, NavData);
	FNavLocation Candidate;
	const bool bHasReachableCandidate = bProjected
		&& NavSystem->GetRandomReachablePointInRadius(ProjectedCenter.Location, FMath::Max(0.0f, PatrolRadius), Candidate);
	UE_LOG(LogTemp, Log, TEXT("P17-PATCH-02 PatrolIntent Controller=%s Center=%s Radius=%.2f NavSystem=%s NavData=%s Result=%s Candidate=%s"),
		*GetName(), *InSpawnOrigin.ToString(), PatrolRadius, *GetNameSafe(NavSystem), *GetNameSafe(NavData),
		bHasReachableCandidate ? TEXT("Reachable") : TEXT("Fallback"), *Candidate.Location.ToString());
	UE_LOG(LogTemp, Log, TEXT("P17-PATCH-02 PatrolProjection Controller=%s Input=%s Extent=%s Projected=%s Result=%s Random=%s"),
		*GetName(), *InSpawnOrigin.ToString(), *ProjectionExtent.ToString(), *ProjectedCenter.Location.ToString(),
		bProjected ? TEXT("Success") : TEXT("Failed"), bHasReachableCandidate ? TEXT("Success") : TEXT("SkippedOrFailed"));
	if (!bHasReachableCandidate)
	{
		// 无法取到可达点时记录回退原因，便于排查是投影失败还是随机取点失败。
		UE_LOG(LogTemp, Warning, TEXT("P17-PATCH-02 patrol intent fallback: %s Reason=%s"), *GetName(),
			bProjected ? TEXT("RandomReachableFailed") : TEXT("ProjectPointFailed"));
	}
	// 结果三态：可达 -> 发布真实候选点；不可达但投影成功 -> 随机取点失败；投影失败 -> 两者皆失败。
	PublishPatrolIntent(InSpawnOrigin, Candidate.Location, bHasReachableCandidate
		? EHSRPatrolIntentResult::Reachable
		: (bProjected ? EHSRPatrolIntentResult::RandomReachableFailed : EHSRPatrolIntentResult::ProjectPointFailed));
}

void AHSREnemyAIController::ScheduleNavReadyPatrolIntent()
{
	// 防止重复调度；且黑板尚未就绪或没有 World 时不调度。
	if (bNavReadyRetryScheduled || !RuntimeBlackboard || !GetWorld())
	{
		return;
	}

	AHSREnemyCharacter* Enemy = Cast<AHSREnemyCharacter>(GetPawn());
	UHSREnemyDefinition* Definition = Enemy ? Enemy->EnemyDefinition : nullptr;
	if (!Enemy || !Definition)
	{
		return;
	}

	// 记录调度时的纪元：计时器触发时若纪元已变化，说明行为树已重启，这次重试应作废。
	const int32 ScheduledEpoch = BehaviorTreeEpoch;
	bNavReadyRetryScheduled = true;
	NavReadyRetryEpoch = ScheduledEpoch;
	UE_LOG(LogTemp, Log, TEXT("P17-PATCH-02 NavReadyRetry Scheduled Controller=%s Epoch=%d Center=%s Radius=%.2f"),
		*GetName(), ScheduledEpoch, *Enemy->GetSpawnOrigin().ToString(), Definition->PatrolRadius);
	// 延迟 0.2 秒后补发巡逻意图，给导航系统时间完成初始化。
	GetWorld()->GetTimerManager().SetTimer(NavReadyRetryTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this, ScheduledEpoch]() { RunNavReadyPatrolIntent(ScheduledEpoch); }), 0.2f, false);
}

void AHSREnemyAIController::RunNavReadyPatrolIntent(int32 ScheduledEpoch)
{
	// 重试只在仍被"调度中"且纪元匹配时有效：否则可能是一次过期回调。
	if (!bNavReadyRetryScheduled || NavReadyRetryEpoch != ScheduledEpoch)
	{
		return;
	}

	bNavReadyRetryScheduled = false;
	NavReadyRetryEpoch = INDEX_NONE;
	if (BehaviorTreeEpoch != ScheduledEpoch)
	{
		UE_LOG(LogTemp, Log, TEXT("P17-PATCH-02 NavReadyRetry Stale Controller=%s ScheduledEpoch=%d CurrentEpoch=%d"), *GetName(), ScheduledEpoch, BehaviorTreeEpoch);
		return;
	}

	// 纪元一致：正式补发一次巡逻意图。
	AHSREnemyCharacter* Enemy = Cast<AHSREnemyCharacter>(GetPawn());
	UHSREnemyDefinition* Definition = Enemy ? Enemy->EnemyDefinition : nullptr;
	if (Enemy && Definition)
	{
		PublishNextPatrolIntent(Enemy->GetSpawnOrigin(), Definition->PatrolRadius);
	}
}

void AHSREnemyAIController::PublishPatrolIntent(const FVector& InSpawnOrigin, const FVector& InCandidate, EHSRPatrolIntentResult Result)
{
	// 有可达候选点时去候选点，否则回退到出生点；同时更新已发布状态，供测试/调试读取。
	const bool bHasReachableCandidate = Result == EHSRPatrolIntentResult::Reachable;
	const FVector PublishedLocation = bHasReachableCandidate ? InCandidate : InSpawnOrigin;
	bHasPublishedPatrolLocation = true;
	PublishedPatrolLocation = PublishedLocation;
	SetState(bHasReachableCandidate ? EHSREnemyExplorationState::MovingToPatrol : EHSREnemyExplorationState::PatrolWaiting);

	if (!RuntimeBlackboard)
	{
		return;
	}

	RuntimeBlackboard->SetValueAsVector(HSREnemyBlackboardKeys::SpawnOrigin, InSpawnOrigin);
	RuntimeBlackboard->SetValueAsVector(HSREnemyBlackboardKeys::PatrolLocation, PublishedLocation);
}

void AHSREnemyAIController::ClearBlackboardRuntimeState()
{
	if (!RuntimeBlackboard)
	{
		return;
	}

	// 清空全部运行时键，确保下次启动行为树时不会读到上一次的残留值。
	RuntimeBlackboard->ClearValue(HSREnemyBlackboardKeys::TargetActor);
	RuntimeBlackboard->ClearValue(HSREnemyBlackboardKeys::PatrolLocation);
	RuntimeBlackboard->ClearValue(HSREnemyBlackboardKeys::EncounterRequestId);
	RuntimeBlackboard->ClearValue(HSREnemyBlackboardKeys::TreeEpoch);
	RuntimeBlackboard->ClearValue(HSREnemyBlackboardKeys::AIState);
	RuntimeBlackboard->ClearValue(HSREnemyBlackboardKeys::SpawnOrigin);
}

void AHSREnemyAIController::SetBlackboardTarget(AActor* Target)
{
	if (RuntimeBlackboard)
	{
		RuntimeBlackboard->SetValueAsObject(HSREnemyBlackboardKeys::TargetActor, Target);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
// ---- 以下全部为自动化测试专用注入点：把运行时状态直接喂给被测逻辑，便于断言 ----
bool AHSREnemyAIController::GetPatrolLocationForAutomation(FVector& OutPatrolLocation) const
{
	if (!RuntimeBlackboard)
	{
		OutPatrolLocation = PublishedPatrolLocation;
		return bHasPublishedPatrolLocation;
	}

	OutPatrolLocation = RuntimeBlackboard->GetValueAsVector(HSREnemyBlackboardKeys::PatrolLocation);
	return true;
}

void AHSREnemyAIController::PublishPatrolIntentForAutomation(UBlackboardComponent* InBlackboard, const FVector& InSpawnOrigin, const FVector& InCandidate, EHSRPatrolIntentResult Result)
{
	RuntimeBlackboard = InBlackboard;
	PublishPatrolIntent(InSpawnOrigin, InCandidate, Result);
}

bool AHSREnemyAIController::ArmNavReadyRetryForAutomation(int32 InEpoch)
{
	if (bNavReadyRetryScheduled)
	{
		return false;
	}
	bNavReadyRetryScheduled = true;
	NavReadyRetryEpoch = InEpoch;
	return true;
}

bool AHSREnemyAIController::ConsumeNavReadyRetryForAutomation(int32 InEpoch)
{
	if (!bNavReadyRetryScheduled || NavReadyRetryEpoch != InEpoch)
	{
		return false;
	}
	bNavReadyRetryScheduled = false;
	NavReadyRetryEpoch = INDEX_NONE;
	return true;
}

void AHSREnemyAIController::ApplySuccessfulPerceptionForAutomation(AActor* Target)
{
	BeginChasingTarget(Target);
}

void AHSREnemyAIController::BindRuntimeBlackboardForAutomation(UBlackboardComponent* InBlackboard)
{
	RuntimeBlackboard = InBlackboard;
	++BehaviorTreeEpoch;
}

void AHSREnemyAIController::StopBehaviorTreeRuntimeForAutomation()
{
	StopBehaviorTreeRuntime();
}

void AHSREnemyAIController::ClearStateForAutomation()
{
	ClearState();
}

void AHSREnemyAIController::SetActiveEncounterRequestForAutomation(const FGuid& InRequestId)
{
	ActiveEncounterRequestId = InRequestId;
}

void AHSREnemyAIController::PublishMoveFailureRecoveryForAutomation(UBlackboardComponent* InBlackboard, const FVector& InSpawnOrigin)
{
	RuntimeBlackboard = InBlackboard;
	PublishSpawnOriginRecoveryIntent(InSpawnOrigin, EHSREnemyExplorationState::MoveFailed);
}

void AHSREnemyAIController::PublishLostTargetRecoveryForAutomation(UBlackboardComponent* InBlackboard, const FVector& InSpawnOrigin)
{
	RuntimeBlackboard = InBlackboard;
	CurrentTarget.Reset();
	SetBlackboardTarget(nullptr);
	PublishSpawnOriginRecoveryIntent(InSpawnOrigin, EHSREnemyExplorationState::LostTarget);
}

void AHSREnemyAIController::CompleteReturnToPatrolForAutomation(UBlackboardComponent* InBlackboard, const FVector& InSpawnOrigin, const FVector& InCandidate, EHSRPatrolIntentResult Result)
{
	RuntimeBlackboard = InBlackboard;
	SetState(EHSREnemyExplorationState::ReturningToSpawnOrigin);
	PublishPatrolIntent(InSpawnOrigin, InCandidate, Result);
}

bool AHSREnemyAIController::HandleMoveFailureForAutomation(UBlackboardComponent* InBlackboard, const FVector& InSpawnOrigin)
{
	RuntimeBlackboard = InBlackboard;
	if (!ShouldHandleMoveFailureOrAbort())
	{
		return false;
	}
	PublishSpawnOriginRecoveryIntent(InSpawnOrigin, EHSREnemyExplorationState::MoveFailed);
	return true;
}
#endif
