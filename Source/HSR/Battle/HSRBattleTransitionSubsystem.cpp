#include "HSRBattleTransitionSubsystem.h"
#include "../Data/Definitions/HSREncounterDefinition.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Engine/Engine.h"
#include "../Data/Definitions/HSRDropTableDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../Map/HSRMapSubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "HSRStageBuffAuthority.h"
#include "../Data/Definitions/HSRStageBuffDefinition.h"

// 子系统初始化：清空状态，创建关卡 Buff 权威，订阅旅行失败事件。
void UHSRBattleTransitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CurrentState = EHSREncounterState::Empty;
	bReturnPending = false;
	bReturnConsumed = false;
	TravelKind = EHSRTravelKind::None;
	TravelRequestId = FGuid();
	TravelSourceMap = NAME_None;
	TravelCompletedEncounterId = NAME_None;
	// 关卡 Buff 定义由独立权威对象托管（按遭遇 ID 校验/查找）。
	StageBuffAuthority = NewObject<UHSRStageBuffAuthority>(this);

	if (GEngine)
	{
		// 旅行失败时能收到回调，用于清理挂起的遭遇/返回事务。
		GEngine->OnTravelFailure().AddUObject(this, &UHSRBattleTransitionSubsystem::HandleTravelFailure);
	}

	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::Initialize - State=Empty"));
}

// 子系统卸载：清除超时计时器与旅行失败订阅。
void UHSRBattleTransitionSubsystem::Deinitialize()
{
	ClearTravelTimeout();
	if (GEngine)
	{
		GEngine->OnTravelFailure().RemoveAll(this);
	}

	Super::Deinitialize();
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::Deinitialize"));
}

// 请求一次遭遇：由世界内调用（如敌人接触），以当前玩家的 Pawn 为交互者。
FHSREncounterResult UHSRBattleTransitionSubsystem::RequestEncounter(UHSREncounterDefinition* Definition, EHSREncounterInitiative Initiative)
{
	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	return RequestEncounterInternal(Definition, Initiative, PlayerController ? PlayerController->GetPawn() : nullptr);
}

// UI 提交遭遇请求的入口。
FHSREncounterResult UHSRBattleTransitionSubsystem::SubmitEncounterRequestFromUI(const FHSREncounterRequest& Request)
{
	return SubmitEncounterRequest(Request, GetWorld());
}

// 校验一组关卡 Buff ID 对给定遭遇是否合法（空集合恒为真）。
bool UHSRBattleTransitionSubsystem::ValidateStageBuffIds(FName EncounterId, const TArray<FName>& BuffIds) const
{
	return BuffIds.IsEmpty() || (StageBuffAuthority && StageBuffAuthority->ValidateBuffIds(EncounterId, BuffIds));
}

// 校验玩家是否付得起这些关卡 Buff 的资源成本（汇总所需数量与库存比对）。
bool UHSRBattleTransitionSubsystem::CanAffordStageBuffs(FName EncounterId, const TArray<FName>& BuffIds) const
{
	if (BuffIds.IsEmpty())
	{
		return true;
	}
	UHSRInventorySubsystem* Inventory = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UHSRInventorySubsystem>() : nullptr;
	if (!Inventory)
	{
		return false;
	}

	// 先汇总每个资源所需总量（并做加法溢出保护）。
	TMap<FName, int32> Required;
	for (const FName BuffId : BuffIds)
	{
		const UHSRStageBuffDefinition* Definition = FindStageBuffDefinition(EncounterId, BuffId);
		if (!Definition)
		{
			return false;
		}
		if (Definition->ResourceCost > 0)
		{
			int32& Total = Required.FindOrAdd(Definition->ResourceItemId);
			if (Definition->ResourceCost > MAX_int32 - Total)
			{
				return false;
			}
			Total += Definition->ResourceCost;
		}
	}

	// 对照库存快照逐项核对。
	FHSRInventorySnapshot Snapshot;
	Inventory->GetSnapshot(Snapshot);
	for (const TPair<FName, int32>& Entry : Required)
	{
		if (Snapshot.GetStackQuantity(Entry.Key) < Entry.Value)
		{
			return false;
		}
	}
	return true;
}

// 按遭遇 + Buff ID 查定义（转交关卡 Buff 权威）。
const UHSRStageBuffDefinition* UHSRBattleTransitionSubsystem::FindStageBuffDefinition(
	FName EncounterId, FName BuffId) const
{
	return StageBuffAuthority ? StageBuffAuthority->FindBuff(EncounterId, BuffId) : nullptr;
}

// 从“战前准入输入”构造遭遇请求：校验模板字段与候选队伍后填入队伍与 Buff。
EHSREncounterResultType UHSRBattleTransitionSubsystem::BuildEncounterRequest(
	const FHSRPreBattleAdmissionInput& Input, FHSREncounterRequest& OutRequest)
{
	if (Input.Template.EncounterId.IsNone() || Input.Template.EnemyDefinitionId.IsNone()
		|| Input.Template.BattleMapPath.IsNone() || Input.CandidateParty.IsEmpty()
		|| Input.CandidateParty[0].IsNone())
	{
		return EHSREncounterResultType::InvalidRequest;
	}
	// 队伍成员去重：同一角色不能占多个槽位。
	TSet<FName> Seen;
	for (const FName CharacterId : Input.CandidateParty)
	{
		if (CharacterId.IsNone() || Seen.Contains(CharacterId))
		{
			return EHSREncounterResultType::InvalidRequest;
		}
		Seen.Add(CharacterId);
	}
	OutRequest = Input.Template;
	OutRequest.PlayerCharacterId = Input.CandidateParty[0];
	OutRequest.PlayerPartyIds = Input.CandidateParty;
	OutRequest.BuffIds = Input.BuffIds;
	return EHSREncounterResultType::Success;
}

FHSREncounterResult UHSRBattleTransitionSubsystem::BuildPreBattleEncounterTemplate(
	UHSREncounterDefinition* Definition, EHSREncounterInitiative Initiative,
	FHSREncounterRequest& OutTemplate)
{
	OutTemplate = FHSREncounterRequest();
	if (!Definition)
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidDefinition,
			FText::FromString(TEXT("EncounterDefinition is null.")));
	}
	if (Definition->EncounterId.IsNone() || Definition->EnemyDefinitionId.IsNone())
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidRequest,
			FText::FromString(TEXT("EncounterId or EnemyDefinitionId is not set.")));
	}
	if (Definition->BattleMap.IsNull())
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidMap,
			FText::FromString(TEXT("BattleMap is not set.")));
	}
	const FString BattleMapPackage = Definition->BattleMap.GetLongPackageName();
	if (!FPackageName::DoesPackageExist(BattleMapPackage))
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidMap,
			FText::FromString(TEXT("BattleMap package does not exist on disk.")));
	}

	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!World || !IsValid(PlayerPawn))
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::NoPlayerSelection,
			FText::FromString(TEXT("Cannot resolve the current player Pawn.")));
	}
	if (!StageBuffAuthority || !StageBuffAuthority->RegisterEncounterBuffs(
		Definition->EncounterId, Definition->StageBuffDefinitions))
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidDefinition,
			FText::FromString(TEXT("Stage Buff definitions are invalid.")));
	}

	FPlatformMisc::CreateGuid(OutTemplate.RequestId);
	OutTemplate.EncounterId = Definition->EncounterId;
	OutTemplate.EnemyDefinitionId = Definition->EnemyDefinitionId;
	OutTemplate.Initiative = Initiative;
	OutTemplate.BattleMapPath = FName(*BattleMapPackage);
	OutTemplate.ReturnTransform = PlayerPawn->GetActorTransform();
	OutTemplate.ExplorationMapPath = FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetPathName()));
	if (Definition->VictoryRewardDefinition)
	{
		OutTemplate.RewardDefinitionId = Definition->VictoryRewardDefinition->RewardDefinitionId;
		OutTemplate.RewardSeed = Definition->RewardSeed;
		OutTemplate.VictoryExperience = FMath::Max(0, Definition->VictoryExperience);
	}
	return FHSREncounterResult::MakeSuccess(OutTemplate.RequestId);
}

FHSREncounterResult UHSRBattleTransitionSubsystem::RequestEncounterForInteractor(
	UHSREncounterDefinition* Definition, EHSREncounterInitiative Initiative, AActor* Interactor)

{
	return RequestEncounterInternal(Definition, Initiative, Interactor);
}

// 遭遇请求核心：从遭遇定义 + 交互者构造完整请求并提交。
// 校验状态、定义字段、当前会话内是否已解决过、队伍选择、奖励包完整性等。
FHSREncounterResult UHSRBattleTransitionSubsystem::RequestEncounterInternal(
	UHSREncounterDefinition* Definition, EHSREncounterInitiative Initiative, AActor* Interactor)
{
	// 已有挂起/旅行中的事务时拒绝。
	if (CurrentState == EHSREncounterState::Pending || CurrentState == EHSREncounterState::Traveling)
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::AlreadyPending,
			FText::FromString(TEXT("A battle transition is already pending or traveling.")));
	}

	if (!Definition)
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidDefinition,
			FText::FromString(TEXT("EncounterDefinition is null.")));
	}
	if (Definition->EncounterId.IsNone())
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidRequest,
			FText::FromString(TEXT("EncounterId is not set.")));
	}
	// 同一会话内已解决过的遭遇不能再次触发。
	if (ResolvedEncounterIds.Contains(Definition->EncounterId))
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::AlreadyConsumed,
			FText::FromString(TEXT("This encounter was already resolved in the current game session.")));
	}
	if (Definition->EnemyDefinitionId.IsNone())
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidRequest,
			FText::FromString(TEXT("EnemyDefinitionId is not set.")));
	}

	// 从队伍子系统取当前队伍：槽 0 必须已提交玩家选择。
	UHSRPartySubsystem* Party = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRPartySubsystem>() : nullptr;
	FHSRPartySnapshot PartySnapshot;
	if (!Party || !Party->GetSnapshot(PartySnapshot) || PartySnapshot.Slots.IsEmpty()
		|| PartySnapshot.Slots[0].IsEmpty())
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::NoPlayerSelection,
			FText::FromString(TEXT("Party slot 0 has no committed player selection.")));
	}
	const FName PlayerCharacterId = PartySnapshot.Slots[0].CharacterId;
	// 队伍名单在此“致密化”：空槽位在队伍网格中合法，但对战斗无意义，
	// 战斗只看到已提交的成员，领队在前。
	TArray<FName> PlayerPartyIds;
	for (const FHSRPartySlot& Slot : PartySnapshot.Slots)
	{
		if (!Slot.IsEmpty())
		{
			PlayerPartyIds.Add(Slot.CharacterId);
		}
	}

	if (Definition->BattleMap.IsNull())
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidMap,
			FText::FromString(TEXT("BattleMap is not set.")));
	}
	// 战斗地图包必须存在于磁盘。
	const FString MapPackageName = Definition->BattleMap.GetLongPackageName();
	if (!FPackageName::DoesPackageExist(MapPackageName))
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidMap,
			FText::FromString(TEXT("BattleMap package does not exist on disk.")));
	}

	UWorld* World = GetWorld();
	APawn* InteractorPawn = Cast<APawn>(Interactor);
	if (!World || !IsValid(InteractorPawn) || InteractorPawn->GetWorld() != World)
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::NoPlayerSelection,
			FText::FromString(TEXT("Cannot resolve the committed player Pawn.")));
	}
	// 记录返回传送点（玩家遭遇时的位置）。
	const FTransform ReturnTransform = InteractorPawn->GetActorTransform();

	// 若定义带胜利奖励：校验并注册奖励包（物品/掉率表/奖励定义）。
	UHSRRewardSubsystem* Reward = nullptr;
	if (Definition->VictoryRewardDefinition)
	{
		Reward = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRRewardSubsystem>() : nullptr;
		if (!Reward || !Definition->RewardDropTable)
		{
			return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidDefinition,
				FText::FromString(TEXT("Encounter reward bundle is incomplete.")));
		}
		const EHSRRewardOperationResult Validation = Reward->CanRegisterBundle(Definition->RewardItemDefinitions,
			*Definition->RewardDropTable, *Definition->VictoryRewardDefinition);
		if (Validation != EHSRRewardOperationResult::Success && Validation != EHSRRewardOperationResult::NoOp)
		{
			return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidDefinition,
				FText::FromString(TEXT("Encounter reward bundle validation failed.")));
		}
	}

	if (Definition->VictoryRewardDefinition)
	{
		const EHSRRewardOperationResult Registration = Reward->RegisterBundle(Definition->RewardItemDefinitions,
			*Definition->RewardDropTable, *Definition->VictoryRewardDefinition);
		if (Registration != EHSRRewardOperationResult::Success && Registration != EHSRRewardOperationResult::NoOp)
		{
			return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidDefinition,
				FText::FromString(TEXT("Encounter reward bundle registration failed.")));
		}
	}

	// 组装完整请求并提交。
	FGuid NewRequestId;
	FPlatformMisc::CreateGuid(NewRequestId);
	FHSREncounterRequest NewRequest;
	NewRequest.RequestId = NewRequestId;
	NewRequest.PlayerCharacterId = PlayerCharacterId;
	NewRequest.PlayerPartyIds = PlayerPartyIds;
	UE_LOG(LogTemp, Log, TEXT("HSRBattleTransition RequestEncounter PartyMembers=%d Leader=%s"), PlayerPartyIds.Num(), *PlayerCharacterId.ToString());
	NewRequest.EncounterId = Definition->EncounterId;
	NewRequest.EnemyDefinitionId = Definition->EnemyDefinitionId;
	NewRequest.Initiative = Initiative;
	NewRequest.BattleMapPath = FName(*Definition->BattleMap.GetLongPackageName());
	NewRequest.ReturnTransform = ReturnTransform;

	// 探索地图路径取当前世界（去掉 PIE 前缀）。
	NewRequest.ExplorationMapPath = FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetPathName()));
	if (Definition->VictoryRewardDefinition)
	{
		NewRequest.RewardDefinitionId = Definition->VictoryRewardDefinition->RewardDefinitionId;
		NewRequest.RewardSeed = Definition->RewardSeed;
		NewRequest.VictoryExperience = FMath::Max(0, Definition->VictoryExperience);
	}

	return SubmitEncounterRequest(NewRequest, World);
}

// 提交遭遇请求：校验后存入挂起状态，标记旅行目标并真正 OpenLevel 到战斗地图。
FHSREncounterResult UHSRBattleTransitionSubsystem::SubmitEncounterRequest(
	const FHSREncounterRequest& Request, UWorld* World)
{
	if (!World || !Request.RequestId.IsValid() || Request.BattleMapPath.IsNone())
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidRequest);
	}
	// 关卡 Buff 必须先通过合法性 + 可支付性校验。
	if (!ValidateStageBuffIds(Request.EncounterId, Request.BuffIds))
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidRequest,
			FText::FromString(TEXT("Stage Buff selection is invalid or unavailable.")));
	}
	if (!CanAffordStageBuffs(Request.EncounterId, Request.BuffIds))
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::InvalidRequest,
			FText::FromString(TEXT("Stage Buff resource is insufficient.")));
	}
	if (CurrentState == EHSREncounterState::Pending || CurrentState == EHSREncounterState::Traveling)
	{
		return FHSREncounterResult::MakeFailure(EHSREncounterResultType::AlreadyPending);
	}
	// 暂存请求，进入 Pending 再立刻转 Traveling。
	PendingRequest = Request;
	CurrentState = EHSREncounterState::Pending;
#if WITH_DEV_AUTOMATION_TESTS
	++AdmissionMutationCountForAutomation;
#endif
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::SubmitEncounterRequest - RequestId=%s EncounterId=%s"),
		*Request.RequestId.ToString(), *Request.EncounterId.ToString());
	CurrentState = EHSREncounterState::Traveling;
	TravelKind = EHSRTravelKind::Encounter;
	TravelRequestId = Request.RequestId;
	TravelSourceMap = Request.ExplorationMapPath;
	TravelTargetMap = Request.BattleMapPath;
	StartTravelTimeout();
#if WITH_DEV_AUTOMATION_TESTS
	++TravelInitiationCountForAutomation;
	if (!bSuppressTravelForAutomation)
#endif
	{
		// 真正打开战斗地图。
		UGameplayStatics::OpenLevel(World, Request.BattleMapPath, true);
	}
	return FHSREncounterResult::MakeSuccess(Request.RequestId);
}

#if WITH_DEV_AUTOMATION_TESTS
FHSRTransitionAutomationSnapshot UHSRBattleTransitionSubsystem::GetAutomationSnapshot(FName EncounterId) const
{
	FHSRTransitionAutomationSnapshot Snapshot;
	Snapshot.State = CurrentState;
	Snapshot.PendingRequest = PendingRequest;
	Snapshot.TravelKind = TravelKind;
	Snapshot.TravelRequestId = TravelRequestId;
	Snapshot.bResolvedMembership = ResolvedEncounterIds.Contains(EncounterId);
	Snapshot.AdmissionMutationCount = AdmissionMutationCountForAutomation;
	Snapshot.TravelInitiationCount = TravelInitiationCountForAutomation;
	return Snapshot;
}

void UHSRBattleTransitionSubsystem::SeedPendingEncounterForAutomation(const FHSREncounterRequest& InRequest)
{
	ResetEncounterAutomationFixture();
	PendingRequest = InRequest;
	CurrentState = EHSREncounterState::Pending;
	TravelKind = EHSRTravelKind::Encounter;
	TravelRequestId = InRequest.RequestId;
}

void UHSRBattleTransitionSubsystem::SeedResolvedEncounterForAutomation(FName EncounterId)
{
	ResetEncounterAutomationFixture();
	ResolvedEncounterIds.Add(EncounterId);
}

void UHSRBattleTransitionSubsystem::ResetEncounterAutomationFixture()
{
	ClearTravelTimeout();
	CurrentState = EHSREncounterState::Empty;
	PendingRequest = FHSREncounterRequest();
	TravelKind = EHSRTravelKind::None;
	TravelRequestId.Invalidate();
	TravelTargetMap = NAME_None;
	TravelSourceMap = NAME_None;
	TravelCompletedEncounterId = NAME_None;
	ResolvedEncounterIds.Reset();
	AdmissionMutationCountForAutomation = 0;
	TravelInitiationCountForAutomation = 0;
}
#endif

// 消费挂起的遭遇请求（战斗世界加载后由 BattleGameMode 调用，恰好一次）。
// 消费不变量：消费后内部载荷立即清空，外部只能从返回值读取完整 DTO。
FHSREncounterResult UHSRBattleTransitionSubsystem::ConsumePendingEncounter()
{
	if (CurrentState == EHSREncounterState::Empty)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::ConsumePendingEncounter - FAILED NothingPending"));
		return FHSREncounterResult::MakeFailure(
			EHSREncounterResultType::NothingPending,
			FText::FromString(TEXT("No pending encounter to consume.")));
	}

	if (CurrentState == EHSREncounterState::Consumed)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::ConsumePendingEncounter - FAILED AlreadyConsumed (request=%s)"),
			*PendingRequest.RequestId.ToString());
		return FHSREncounterResult::MakeFailure(
			EHSREncounterResultType::AlreadyConsumed,
			FText::FromString(TEXT("This encounter has already been consumed.")));
	}

	if (CurrentState == EHSREncounterState::Pending)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::ConsumePendingEncounter - FAILED NothingPending (still Pending, not yet Traveling)"));
		return FHSREncounterResult::MakeFailure(
			EHSREncounterResultType::NothingPending,
			FText::FromString(TEXT("Travel has not completed yet.")));
	}

	// 先完整拷贝 DTO，再清空内部载荷。
	FHSREncounterRequest Consumed = PendingRequest;
	FGuid ConsumedId = Consumed.RequestId;

	// 立即清空内部载荷（消费不变量：载荷不再可读）。
	PendingRequest = FHSREncounterRequest();
	CurrentState = EHSREncounterState::Consumed;
	TravelKind = EHSRTravelKind::None;
	TravelRequestId = FGuid();
	TravelTargetMap = NAME_None;
	TravelSourceMap = NAME_None;
	ClearTravelTimeout();

	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::ConsumePendingEncounter - SUCCESS RequestId=%s EncounterId=%s EnemyDefId=%s"),
		*Consumed.RequestId.ToString(), *Consumed.EncounterId.ToString(), *Consumed.EnemyDefinitionId.ToString());
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::ConsumePendingEncounter - Initiative=%d BattleMapPath=%s ExplorationMap=%s ReturnLoc=%s"),
		static_cast<int32>(Consumed.Initiative), *Consumed.BattleMapPath.ToString(),
		*Consumed.ExplorationMapPath.ToString(),
		*Consumed.ReturnTransform.GetLocation().ToString());

	// 把完整 DTO 放进返回值，消费方不需要再读子系统内部。
	FHSREncounterResult Result = FHSREncounterResult::MakeSuccess(ConsumedId);
	Result.ConsumedRequest = Consumed;
	return Result;
}

void UHSRBattleTransitionSubsystem::ClearPending()
{
	if (CurrentState != EHSREncounterState::Empty)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::ClearPending - Cleared state=%d request=%s"),
			static_cast<int32>(CurrentState), *PendingRequest.RequestId.ToString());
	}

	CurrentState = EHSREncounterState::Empty;
	PendingRequest = FHSREncounterRequest();
	TravelKind = EHSRTravelKind::None;
	TravelRequestId = FGuid();
	TravelTargetMap = NAME_None;
	TravelSourceMap = NAME_None;
	ClearTravelTimeout();
}

bool UHSRBattleTransitionSubsystem::HasPending() const
{
	return CurrentState == EHSREncounterState::Pending || CurrentState == EHSREncounterState::Traveling;
}

void UHSRBattleTransitionSubsystem::ClearReturn()
{
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::ClearReturn - Clearing Return context"));
	if (!TravelCompletedEncounterId.IsNone())
	{
		ResolvedEncounterIds.Remove(TravelCompletedEncounterId);
	}
	PendingReturnContext = FHSRExplorationReturnContext();
	bReturnPending = false;
	bReturnConsumed = false;
	TravelKind = EHSRTravelKind::None;
	TravelRequestId = FGuid();
	TravelTargetMap = NAME_None;
	TravelSourceMap = NAME_None;
	ClearTravelTimeout();
}

// 旅行失败回调：只有失败属于我们追踪的事务时才清理状态（可重试）。
void UHSRBattleTransitionSubsystem::HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - type=%d Error=%s World=%s TargetMap=%s RequestId=%s"),
		static_cast<int32>(FailureType), *ErrorString, InWorld ? *InWorld->GetName() : TEXT("null"),
		*TravelTargetMap.ToString(), *TravelRequestId.ToString());

	// 1. 无活跃事务时直接忽略。
	if (TravelKind == EHSRTravelKind::None)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - IGNORED (no active transaction, type=%d)"), static_cast<int32>(FailureType));
		return;
	}
	if (!InWorld)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - IGNORED (null World cannot be correlated)"));
		return;
	}

	// 2. 尝试把失败世界包路径与已记录的 TravelTargetMap 关联。
	FString FailureWorldPath;
	if (InWorld && InWorld->GetOutermost())
	{
		FailureWorldPath = UWorld::RemovePIEPrefix(InWorld->GetOutermost()->GetPathName());
	}
	bool bMatchesOurTransaction = DoesTravelFailureMatch(
		FailureWorldPath, TravelSourceMap.ToString(), TravelTargetMap.ToString());

	UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - MatchCheck: WorldPath=%s TargetMap=%s RequestId=%s bMatch=%d"),
		*FailureWorldPath, *TravelTargetMap.ToString(), *TravelRequestId.ToString(), bMatchesOurTransaction ? 1 : 0);

	// 3. 只有失败属于我们的追踪事务才清状态。
	if (!bMatchesOurTransaction)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - IGNORED (world/map mismatch). FailureWorld=%s ExpectedMap=%s"),
			*FailureWorldPath, *TravelTargetMap.ToString());
		return;
	}

	// 4. 清理匹配的事务。
	if (TravelKind == EHSRTravelKind::Encounter)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - Clearing Encounter state (RequestId=%s, now clean, retry available)"),
			*PendingRequest.RequestId.ToString());
		ClearPending();
	}
	else if (TravelKind == EHSRTravelKind::Return)
	{
		if (!TravelCompletedEncounterId.IsNone())
		{
			ResolvedEncounterIds.Remove(TravelCompletedEncounterId);
		}
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - Clearing Return context (RequestId=%s, now clean, retry available)"),
			*TravelRequestId.ToString());
		ClearReturn();
	}

	// 5. 清空旅行追踪（与 ClearPending/ClearReturn 冗余，但显式写更安全）。
	TravelKind = EHSRTravelKind::None;
	TravelRequestId = FGuid();
	TravelTargetMap = NAME_None;
	TravelSourceMap = NAME_None;
	TravelCompletedEncounterId = NAME_None;
	ClearTravelTimeout();
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::HandleTravelFailure - State clean. New requests can proceed."));
	return;
}


FHSRExplorationReturnResult UHSRBattleTransitionSubsystem::RequestTestReturn(const FHSREncounterRequest& FromConsumedRequest)
{
	FHSRBattleReturnContext BattleReturnContext;
	BattleReturnContext.RequestId = FromConsumedRequest.RequestId;
	BattleReturnContext.ExplorationMapPath = FromConsumedRequest.ExplorationMapPath;
	BattleReturnContext.ReturnTransform = FromConsumedRequest.ReturnTransform;
	FHSRBattleResult TestResult;
	TestResult.RequestId = BattleReturnContext.RequestId;
	TestResult.ReturnContext = BattleReturnContext;
	return RequestBattleReturn(TestResult);
}

// 请求返回探索地图：先做返回预检，再登记返回上下文、标记旅行目标并 OpenLevel。
FHSRExplorationReturnResult UHSRBattleTransitionSubsystem::RequestBattleReturn(const FHSRBattleResult& BattleResult)
{
	const FHSRExplorationReturnResult Validation = ValidateBattleReturn(BattleResult);
	if (Validation.ResultType != EHSREncounterReturnResultType::Success)
	{
		return Validation;
	}

	const FHSRBattleReturnContext& BattleReturnContext = BattleResult.ReturnContext;
	FHSRExplorationReturnContext ReturnCtx;
	ReturnCtx.RequestId = BattleReturnContext.RequestId;
	ReturnCtx.ExplorationMapPath = BattleReturnContext.ExplorationMapPath;
	// 通过地图子系统把包路径解析为已注册的地图 ID（返回校验依赖它）。
	UHSRMapSubsystem* MapSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRMapSubsystem>() : nullptr;
	if (!MapSubsystem || !MapSubsystem->ResolveMapIdByPackage(BattleReturnContext.ExplorationMapPath, ReturnCtx.ExplorationMapId))
	{
		return FHSRExplorationReturnResult::MakeFailure(EHSREncounterReturnResultType::InvalidReturnContext,
			FText::FromString(TEXT("Exploration map path is not registered with MapSubsystem.")));
	}
	ReturnCtx.ReturnTransform = BattleReturnContext.ReturnTransform;

	PendingReturnContext = ReturnCtx;
	bReturnPending = true;
	bReturnConsumed = false;

	TravelKind = EHSRTravelKind::Return;
	TravelRequestId = ReturnCtx.RequestId;
	TravelTargetMap = ReturnCtx.ExplorationMapPath;
	if (UWorld* World = GetWorld())
	{
		TravelSourceMap = FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetPathName()));
	}
	StartTravelTimeout();
	// 胜利才会把遭遇标记为已解决（失败可重试该遭遇）。
	TravelCompletedEncounterId = ShouldResolveEncounter(BattleResult.Outcome) ? BattleResult.EncounterId : NAME_None;
	if (!TravelCompletedEncounterId.IsNone())
	{
		ResolvedEncounterIds.Add(TravelCompletedEncounterId);
	}

	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::RequestTestReturn - SUCCESS RequestId=%s ExplorationMap=%s (kind=Return)"),
		*ReturnCtx.RequestId.ToString(), *ReturnCtx.ExplorationMapPath.ToString());

	UGameplayStatics::OpenLevel(GetWorld(), ReturnCtx.ExplorationMapPath, true);
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::RequestTestReturn - Traveling back to %s"), *ReturnCtx.ExplorationMapPath.ToString());

	return FHSRExplorationReturnResult::MakeSuccess();
}

// 是否把该战斗结果视为“已解决遭遇”（只有玩家胜利才消耗遭遇）。
bool UHSRBattleTransitionSubsystem::ShouldResolveEncounter(const EHSRBattleOutcome Outcome)
{
	return Outcome == EHSRBattleOutcome::PlayerVictory;
}

// 判断一次旅行失败是否属于我们追踪的源/目标地图之一（去 PIE 前缀后比较）。
bool UHSRBattleTransitionSubsystem::DoesTravelFailureMatch(const FString& FailureWorldPackage,
	const FString& SourcePackage, const FString& TargetPackage)
{
	if (FailureWorldPackage.IsEmpty())
	{
		return false;
	}
	const FString NormalizedFailure = UWorld::RemovePIEPrefix(FailureWorldPackage);
	return (!SourcePackage.IsEmpty() && NormalizedFailure == UWorld::RemovePIEPrefix(SourcePackage))
		|| (!TargetPackage.IsEmpty() && NormalizedFailure == UWorld::RemovePIEPrefix(TargetPackage));
}

// 启动旅行超时计时器（5 秒后触发 HandleTravelTimeout）。
void UHSRBattleTransitionSubsystem::StartTravelTimeout()
{
	ClearTravelTimeout();
	TravelTimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UHSRBattleTransitionSubsystem::HandleTravelTimeout), 5.0f);
}

// 清除旅行超时计时器。
void UHSRBattleTransitionSubsystem::ClearTravelTimeout()
{
	if (TravelTimeoutHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TravelTimeoutHandle);
		TravelTimeoutHandle.Reset();
	}
}

// 旅行超时：若仍在挂起则清空对应事务，允许重试。
bool UHSRBattleTransitionSubsystem::HandleTravelTimeout(float)
{
	TravelTimeoutHandle.Reset();
	if (TravelKind == EHSRTravelKind::Encounter)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Battle encounter travel timed out RequestId=%s; cleared for retry"), *TravelRequestId.ToString());
		ClearPending();
	}
	else if (TravelKind == EHSRTravelKind::Return)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Battle return travel timed out RequestId=%s; cleared for retry"), *TravelRequestId.ToString());
		ClearReturn();
	}
	return false;
}

FHSRExplorationReturnResult UHSRBattleTransitionSubsystem::ValidateBattleReturn(const FHSRBattleResult& BattleResult) const
{
	const FHSRBattleReturnContext& BattleReturnContext = BattleResult.ReturnContext;
	// Validate BEFORE writing (must not pollute Pending)
	if (bReturnPending)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestTestReturn - FAILED AlreadyPending"));
		return FHSRExplorationReturnResult::MakeFailure(
			EHSREncounterReturnResultType::AlreadyPending,
			FText::FromString(TEXT("A return is already pending.")));
	}

	if (BattleReturnContext.ExplorationMapPath.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestTestReturn - FAILED InvalidReturnContext (no path)"));
		return FHSRExplorationReturnResult::MakeFailure(
			EHSREncounterReturnResultType::InvalidReturnContext,
			FText::FromString(TEXT("No exploration map path in return context.")));
	}

	// Also reject invalid RequestId
	if (!BattleReturnContext.RequestId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestTestReturn - FAILED InvalidReturnContext (invalid RequestId)"));
		return FHSRExplorationReturnResult::MakeFailure(
			EHSREncounterReturnResultType::InvalidReturnContext,
			FText::FromString(TEXT("Invalid RequestId in return context.")));
	}

	if (!FPackageName::DoesPackageExist(BattleReturnContext.ExplorationMapPath.ToString()))
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestBattleReturn - FAILED invalid map=%s"), *BattleReturnContext.ExplorationMapPath.ToString());
		return FHSRExplorationReturnResult::MakeFailure(EHSREncounterReturnResultType::InvalidReturnContext, FText::FromString(TEXT("Exploration map package does not exist.")));
	}
	UHSRMapSubsystem* MapSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRMapSubsystem>() : nullptr;
	FName ResolvedMapId;
	if (!MapSubsystem || MapSubsystem->HasPendingTravel()
		|| !MapSubsystem->ResolveMapIdByPackage(BattleReturnContext.ExplorationMapPath, ResolvedMapId))
	{
		return FHSRExplorationReturnResult::MakeFailure(EHSREncounterReturnResultType::InvalidReturnContext,
			FText::FromString(TEXT("MapSubsystem cannot authorize the exploration return.")));
	}

	// Check World availability BEFORE writing Pending
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::RequestTestReturn - FAILED: no World, rejecting"));
		return FHSRExplorationReturnResult::MakeFailure(
			EHSREncounterReturnResultType::InvalidReturnContext,
			FText::FromString(TEXT("Cannot resolve World for travel.")));
	}

	return FHSRExplorationReturnResult::MakeSuccess();
}

FHSRExplorationReturnResult UHSRBattleTransitionSubsystem::CommitReturnContext(APawn* PlayerPawn)
{
	if (!bReturnPending)
	{
		return FHSRExplorationReturnResult::MakeFailure(bReturnConsumed
			? EHSREncounterReturnResultType::AlreadyConsumed : EHSREncounterReturnResultType::NothingPending);
	}
	UHSRMapSubsystem* Maps = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRMapSubsystem>() : nullptr;
	if (!Maps || PendingReturnContext.ExplorationMapId.IsNone())
	{
		return FHSRExplorationReturnResult::MakeFailure(EHSREncounterReturnResultType::InvalidReturnContext);
	}
	const EHSRMapOperationResult PlacementResult = Maps->CommitBattleReturnLocation(
		PendingReturnContext.ExplorationMapId, PlayerPawn, PendingReturnContext.ReturnTransform);
	if (PlacementResult != EHSRMapOperationResult::Success)
	{
		return FHSRExplorationReturnResult::MakeFailure(EHSREncounterReturnResultType::InvalidReturnContext,
			FText::FromString(TEXT("Battle return placement has not committed.")));
	}

	FHSRExplorationReturnResult Result = FHSRExplorationReturnResult::MakeSuccess();
	Result.ConsumedContext = PendingReturnContext;
	UE_LOG(LogTemp, Log, TEXT("HSR Battle return committed RequestId=%s MapId=%s Location=%s"),
		*PendingReturnContext.RequestId.ToString(), *PendingReturnContext.ExplorationMapId.ToString(),
		*PendingReturnContext.ReturnTransform.GetLocation().ToString());
	PendingReturnContext = FHSRExplorationReturnContext();
	bReturnPending = false;
	bReturnConsumed = true;
	TravelKind = EHSRTravelKind::None;
	TravelRequestId = FGuid();
	TravelTargetMap = NAME_None;
	TravelSourceMap = NAME_None;
	TravelCompletedEncounterId = NAME_None;
	ClearTravelTimeout();
	return Result;
}

FHSRExplorationReturnResult UHSRBattleTransitionSubsystem::ConsumeReturnContext()
{
	if (!bReturnPending)
	{
		if (bReturnConsumed)
		{
			UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::ConsumeReturnContext - FAILED AlreadyConsumed (request=%s)"),
				*PendingReturnContext.RequestId.ToString());
			return FHSRExplorationReturnResult::MakeFailure(
				EHSREncounterReturnResultType::AlreadyConsumed,
				FText::FromString(TEXT("Return context has already been consumed.")));
		}
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleTransitionSubsystem::ConsumeReturnContext - FAILED NothingPending"));
		return FHSRExplorationReturnResult::MakeFailure(
			EHSREncounterReturnResultType::NothingPending,
			FText::FromString(TEXT("No pending return context.")));
	}

	// Capture the full DTO before clearing internal payload
	FHSRExplorationReturnContext Consumed = PendingReturnContext;
	PendingReturnContext = FHSRExplorationReturnContext();
	bReturnPending = false;
	bReturnConsumed = true;
	TravelKind = EHSRTravelKind::None;
	TravelRequestId = FGuid();
	TravelTargetMap = NAME_None;
	TravelSourceMap = NAME_None;
	TravelCompletedEncounterId = NAME_None;
	ClearTravelTimeout();

	UE_LOG(LogTemp, Log, TEXT("UHSRBattleTransitionSubsystem::ConsumeReturnContext - SUCCESS RequestId=%s ReturnLoc=%s"),
		*Consumed.RequestId.ToString(), *Consumed.ReturnTransform.GetLocation().ToString());

	// Return the full consumed context in the Result so consumer does NOT re-read from Subsystem
	FHSRExplorationReturnResult Result = FHSRExplorationReturnResult::MakeSuccess();
	Result.ConsumedContext = Consumed;
	return Result;
}


