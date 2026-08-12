#include "HSRExplorationReturnConsumer.h"
#include "HSRBattleTransitionSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

// 构造函数：关闭 Tick，初始化重试计数。
AHSRExplorationReturnConsumer::AHSRExplorationReturnConsumer()
{
	PrimaryActorTick.bCanEverTick = false;
	RetryCount = 0;
}

void AHSRExplorationReturnConsumer::BeginPlay()
{
	Super::BeginPlay();

	// 等玩家 Pawn 存在后消费返回上下文并应用传送。
	// 用一个一次性定时器触发首次尝试（失败会安排重试）。
	GetWorld()->GetTimerManager().SetTimer(ReturnTimerHandle, this, &AHSRExplorationReturnConsumer::TryConsumeAndReturn, RetryInterval, false);
}

void AHSRExplorationReturnConsumer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(ReturnTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

// 尝试消费并应用返回传送：
// - 没有挂起返回 → 无事可做；
// - 玩家 Pawn 还没生成 → 安排重试；
// - 成功提交 → 校验第二次消费必须返回 AlreadyConsumed（恰好一次不变量）。
void AHSRExplorationReturnConsumer::TryConsumeAndReturn()
{
	UHSRBattleTransitionSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	if (!Subsystem->HasReturnPending())
	{
		// 没有挂起返回，无事可做。
		UE_LOG(LogTemp, Log, TEXT("AHSRExplorationReturnConsumer::TryConsumeAndReturn - No return pending"));
		return;
	}

	// 等玩家 Pawn 可用。
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;

	// Pawn 还没生成时重试。
	if (!PlayerPawn && ScheduleRetry(TEXT("player pawn unavailable")))
	{
		return;
	}

	if (!PlayerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRExplorationReturnConsumer::TryConsumeAndReturn - FAILED: no player pawn after %d retries"), MaxRetries);
		Subsystem->ClearReturn();
		return;
	}

	// 只有 MapSubsystem 校验过加载的世界并放置好 Pawn 后才提交。
	FHSRExplorationReturnResult Result = Subsystem->CommitReturnContext(PlayerPawn);

	if (Result.ResultType == EHSREncounterReturnResultType::Success)
	{
		UE_LOG(LogTemp, Log, TEXT("AHSRExplorationReturnConsumer::TryConsumeAndReturn - SUCCESS: Teleported pawn to %s"),
			*Result.ConsumedContext.ReturnTransform.GetLocation().ToString());

		// 验证第二次消费返回上下文必须 AlreadyConsumed（恰好一次消费不变量）。
		FHSRExplorationReturnResult SecondResult = Subsystem->CommitReturnContext(PlayerPawn);
		if (SecondResult.ResultType == EHSREncounterReturnResultType::AlreadyConsumed)
		{
			UE_LOG(LogTemp, Log, TEXT("A4c: Second ConsumeReturnContext correctly returned AlreadyConsumed"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("A4c: Second ConsumeReturnContext unexpected type=%d (expected AlreadyConsumed)"),
				static_cast<int32>(SecondResult.ResultType));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRExplorationReturnConsumer::TryConsumeAndReturn - FAILED: ConsumeReturnContext returned type=%d"),
			static_cast<int32>(Result.ResultType));
		// 提交被拒：重试；重试耗尽则清空返回上下文以便重新走流程。
		if (!ScheduleRetry(TEXT("return commit rejected")))
		{
			UE_LOG(LogTemp, Warning, TEXT("AHSRExplorationReturnConsumer::TryConsumeAndReturn - retries exhausted; clearing return for retry"));
			Subsystem->ClearReturn();
		}
	}
}

// 安排一次重试：超过最大重试次数或无世界时返回 false。
bool AHSRExplorationReturnConsumer::ScheduleRetry(const TCHAR* Reason)
{
	if (RetryCount >= MaxRetries || !GetWorld())
	{
		return false;
	}
	++RetryCount;
	GetWorld()->GetTimerManager().SetTimer(ReturnTimerHandle, this,
		&AHSRExplorationReturnConsumer::TryConsumeAndReturn, RetryInterval, false);
	UE_LOG(LogTemp, Log, TEXT("AHSRExplorationReturnConsumer::TryConsumeAndReturn - waiting: %s (attempt %d/%d)"),
		Reason, RetryCount, MaxRetries);
	return true;
}
