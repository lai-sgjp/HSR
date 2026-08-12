#include "HSRBattleTestConsumer.h"
#include "HSRBattleTransitionSubsystem.h"

// 构造函数：关闭 Tick，默认不启用测试返回。
AHSRBattleTestConsumer::AHSRBattleTestConsumer()
{
	PrimaryActorTick.bCanEverTick = false;
	bEnableTestReturn = false;
	ReturnDelay = 0.3f;
}

void AHSRBattleTestConsumer::BeginPlay()
{
	Super::BeginPlay();

	UHSRBattleTransitionSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("AHSRBattleTestConsumer::BeginPlay - FAILED: Cannot access UHSRBattleTransitionSubsystem"));
		return;
	}

	// --- 第一次消费：应成功 ---
	// 用 Consume 的返回值（ConsumedRequest），而不是重新读子系统内部。
	FHSREncounterResult FirstResult = Subsystem->ConsumePendingEncounter();
	if (FirstResult.ResultType == EHSREncounterResultType::Success)
	{
		const FHSREncounterRequest ConsumedReq = FirstResult.ConsumedRequest;
		UE_LOG(LogTemp, Log, TEXT("AHSRBattleTestConsumer::BeginPlay - First Consume SUCCESS"));
		UE_LOG(LogTemp, Log, TEXT("  RequestId       = %s"), *ConsumedReq.RequestId.ToString());
		UE_LOG(LogTemp, Log, TEXT("  EncounterId     = %s"), *ConsumedReq.EncounterId.ToString());
		UE_LOG(LogTemp, Log, TEXT("  EnemyDefId      = %s"), *ConsumedReq.EnemyDefinitionId.ToString());
		UE_LOG(LogTemp, Log, TEXT("  Initiative      = %d"), static_cast<int32>(ConsumedReq.Initiative));
		UE_LOG(LogTemp, Log, TEXT("  BattleMap       = %s"), *ConsumedReq.BattleMapPath.ToString());
		UE_LOG(LogTemp, Log, TEXT("  ExplorationMap  = %s"), *ConsumedReq.ExplorationMapPath.ToString());
		UE_LOG(LogTemp, Log, TEXT("  ReturnLoc       = %s"), *ConsumedReq.ReturnTransform.GetLocation().ToString());
		UE_LOG(LogTemp, Log, TEXT("  Subsystem State = %d (should be 3 = Consumed)"), static_cast<int32>(Subsystem->GetCurrentState()));

		// 暂存已消费的请求；若启用测试返回则定时触发一次返回。
		StoredConsumedRequest = ConsumedReq;
		if (bEnableTestReturn && !ConsumedReq.ExplorationMapPath.IsNone())
		{
			UE_LOG(LogTemp, Log, TEXT("AHSRBattleTestConsumer::BeginPlay - Scheduling test return in %.1f seconds"), ReturnDelay);
			GetWorld()->GetTimerManager().SetTimer(ReturnTimerHandle, this, &AHSRBattleTestConsumer::RequestTestReturn, ReturnDelay, false);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AHSRBattleTestConsumer::BeginPlay - First Consume FAILED (type=%d)"),
			static_cast<int32>(FirstResult.ResultType));
		UE_LOG(LogTemp, Error, TEXT("  Message: %s"), *FirstResult.Message.ToString());
	}

	// --- 第二次消费：必须失败（AlreadyConsumed） ---
	// 首次消费后内部载荷已清空；第二次消费必须失败且不带任何 ConsumedRequest 数据。
	FHSREncounterResult SecondResult = Subsystem->ConsumePendingEncounter();
	if (SecondResult.ResultType == EHSREncounterResultType::AlreadyConsumed)
	{
		UE_LOG(LogTemp, Log, TEXT("AHSRBattleTestConsumer::BeginPlay - Second Consume correctly FAILED AlreadyConsumed"));

		// 校验失败返回里没有旧载荷数据。
		const FHSREncounterRequest& EmptyReq = SecondResult.ConsumedRequest;
		UE_LOG(LogTemp, Log, TEXT("  ConsumedRequest.EncounterId (should be None): %s"), *EmptyReq.EncounterId.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AHSRBattleTestConsumer::BeginPlay - Second Consume unexpected result type=%d (expected AlreadyConsumed)"),
			static_cast<int32>(SecondResult.ResultType));
	}

	// --- 清空以恢复 Empty 状态 ---
	Subsystem->ClearPending();
	UE_LOG(LogTemp, Log, TEXT("AHSRBattleTestConsumer::BeginPlay - Final State=%d (Empty)"),
		static_cast<int32>(Subsystem->GetCurrentState()));
}

void AHSRBattleTestConsumer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(ReturnTimerHandle);
	Super::EndPlay(EndPlayReason);
}

// 测试返回：通过子系统请求一次返回（用暂存的已消费请求）。
void AHSRBattleTestConsumer::RequestTestReturn()
{
	UHSRBattleTransitionSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	// 已消费的请求在 BeginPlay 里记录过；现在通过子系统触发返回。
	UE_LOG(LogTemp, Log, TEXT("AHSRBattleTestConsumer::RequestTestReturn - TEST: Calling RequestTestReturn..."));
	FHSRExplorationReturnResult RetResult = Subsystem->RequestTestReturn(StoredConsumedRequest);
	if (RetResult.ResultType == EHSREncounterReturnResultType::Success)
	{
		UE_LOG(LogTemp, Log, TEXT("AHSRBattleTestConsumer::RequestTestReturn - Return initiated, traveling back to exploration map"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRBattleTestConsumer::RequestTestReturn - Failed type=%d"),
			static_cast<int32>(RetResult.ResultType));
	}
}
