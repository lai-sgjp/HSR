#include "HSRBattleCoordinator.h"
#include "../Reward/HSRRewardTypes.h"
#include "../Data/Definitions/HSREnemyDefinition.h"
#include "../Data/Definitions/HSREnemyCatalog.h"
#include "HSRTurnManager.h"
#include "HSREncounterTypes.h"
#include "HSRTargetingPolicy.h"
#include "../GAS/Ability/HSRGameplayAbilityBase.h"
#include "../GAS/HSRAbilitySystemComponent.h"
#include "../Character/HSRCharacterBase.h"
#include "../Progression/HSRProgressionGameplayTags.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "GameplayEffectTypes.h"
#include "../Data/HSRSkillDefinition.h"
#include "../GAS/Damage/HSRDamageEffectContext.h"
#include "../GAS/Damage/HSRDamageRuleDefinition.h"
#include "GameplayEffect.h"
#include "../Status/HSRStatusComponent.h"
#include "../Data/Definitions/HSRStatusDefinition.h"
#include "../Equipment/HSREquipmentStatAggregator.h"
#include "../Equipment/HSRRelicSetResolver.h"
#include "HSRBattleTransitionSubsystem.h"
#include "../Data/Definitions/HSRStageBuffDefinition.h"
#include "../Inventory/HSRInventorySubsystem.h"

namespace
{
	/**
	 * The four SetByCaller magnitudes UHSRDamageExecutionCalculation reads. Resolved once per
	 * damage request so the three damage paths cannot drift in which tags they look up or write.
	 */
	struct FHSRDamageSetByCallerTags
	{
		FGameplayTag AbilityMultiplier;
		FGameplayTag DefenseCoefficient;
		FGameplayTag MinDamage;
		FGameplayTag CritRoll;

		FHSRDamageSetByCallerTags()
			: AbilityMultiplier(FGameplayTag::RequestGameplayTag(FName(TEXT("Damage.Data.AbilityMultiplier")), false))
			, DefenseCoefficient(FGameplayTag::RequestGameplayTag(FName(TEXT("Damage.Data.DefenseCoefficient")), false))
			, MinDamage(FGameplayTag::RequestGameplayTag(FName(TEXT("Damage.Data.MinDamage")), false))
			, CritRoll(FGameplayTag::RequestGameplayTag(FName(TEXT("Damage.Data.CritRoll")), false))
		{
		}

		bool IsValid() const
		{
			return AbilityMultiplier.IsValid() && DefenseCoefficient.IsValid()
				&& MinDamage.IsValid() && CritRoll.IsValid();
		}

		void ApplyTo(FGameplayEffectSpec& Spec, float InAbilityMultiplier, float InDefenseCoefficient,
			float InMinDamage, float InCritRoll) const
		{
			Spec.SetSetByCallerMagnitude(AbilityMultiplier, InAbilityMultiplier);
			Spec.SetSetByCallerMagnitude(DefenseCoefficient, InDefenseCoefficient);
			Spec.SetSetByCallerMagnitude(MinDamage, InMinDamage);
			Spec.SetSetByCallerMagnitude(CritRoll, InCritRoll);
		}
	};
}

// 提交一次遭遇请求：把请求原子地捕获进本协调器，并从 Idle 进入 Consuming 状态。
// 这是战斗世界加载后的第一步，由 AHSRBattleGameMode::BeginPlay 调用。
bool UHSRBattleCoordinator::SubmitBattleRequest(const FHSREncounterRequest& InRequest)
{
	if (CurrentState != EHSRBattleCoordinatorState::Idle)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::SubmitBattleRequest - REJECTED state=%d RequestId=%s (expected Idle)"),
			static_cast<int32>(CurrentState), *InRequest.RequestId.ToString());
		return false;
	}

	if (!InRequest.RequestId.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::SubmitBattleRequest - REJECTED invalid RequestId"));
		return false;
	}

	if (InRequest.EncounterId.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::SubmitBattleRequest - REJECTED EncounterId=None RequestId=%s"),
			*InRequest.RequestId.ToString());
		return false;
	}

	if (InRequest.EnemyDefinitionId.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::SubmitBattleRequest - REJECTED EnemyDefinitionId=None RequestId=%s"),
			*InRequest.RequestId.ToString());
		return false;
	}

	if (InRequest.BattleMapPath.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::SubmitBattleRequest - REJECTED BattleMapPath=None RequestId=%s"),
			*InRequest.RequestId.ToString());
		return false;
	}

	// 若携带了关卡 Buff 选择，先校验这些 BuffId 对当前遭遇合法（由过渡子系统把关）。
	if (!InRequest.BuffIds.IsEmpty())
	{
		UHSRBattleTransitionSubsystem* Transition = GetWorld() && GetWorld()->GetGameInstance()
			? GetWorld()->GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>() : nullptr;
		if (!Transition || !Transition->ValidateStageBuffIds(InRequest.EncounterId, InRequest.BuffIds))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UHSRBattleCoordinator::SubmitBattleRequest - REJECTED invalid Stage Buff selection RequestId=%s"),
				*InRequest.RequestId.ToString());
			return false;
		}
	}

	// 从请求构造纯值返回上下文（回探索地图用）。
	FHSRBattleReturnContext RetCtx;
	RetCtx.RequestId = InRequest.RequestId;
	RetCtx.ExplorationMapPath = InRequest.ExplorationMapPath;
	RetCtx.ReturnTransform = InRequest.ReturnTransform;

	// 原子捕获整个请求到当前状态。
	CurrentRequestId = InRequest.RequestId;
	CurrentEncounterId = InRequest.EncounterId;
	CurrentEnemyDefinitionId = InRequest.EnemyDefinitionId;
	CurrentStageBuffIds = InRequest.BuffIds;
	AppliedStageBuffHandles.Reset();
	AppliedStageBuffCosts.Reset();
	CurrentRewardDefinitionId = InRequest.RewardDefinitionId;
	CurrentRewardSeed = InRequest.RewardSeed;
	ReturnContext = RetCtx;
	CurrentState = EHSRBattleCoordinatorState::Consuming;
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	LastSubmittedRequestForDevelopment = InRequest;
#endif

	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::SubmitBattleRequest - SUCCESS RequestId=%s EncounterId=%s EnemyDefId=%s BattleMap=%s ExplorationMap=%s"),
		*InRequest.RequestId.ToString(), *InRequest.EncounterId.ToString(),
		*InRequest.EnemyDefinitionId.ToString(), *InRequest.BattleMapPath.ToString(),
		*InRequest.ExplorationMapPath.ToString());

	return true;
}

// Deliberately not FindFirstOfTeam: this needs the first player participant that actually has an
// ASC, so a leader without one must fall through to the next slot rather than fail the lookup.
const FHSRBattleParticipant* UHSRBattleCoordinator::FindStageBuffPlayerParticipant() const
{
	return Participants.FindByPredicate([](const FHSRBattleParticipant& Participant)
	{
		return Participant.Team == EHSRBattleParticipantTeam::Player
			&& Participant.AbilitySystemComponent.IsValid();
	});
}

// 把本次遭遇请求携带的关卡 Buff 施加到玩家侧领队身上，并扣除对应资源（如消耗品）。
// 任一 Buff 失败都会整体失败（由调用方回滚），避免半套 Buff 生效。
bool UHSRBattleCoordinator::ApplyStageBuffs(UWorld* BattleWorld)
{
	if (CurrentStageBuffIds.IsEmpty())
	{
		return true;
	}
	if (!BattleWorld || !BattleWorld->GetGameInstance())
	{
		UE_LOG(LogTemp, Error, TEXT("P17-009D Stage Buff application failed: missing BattleWorld or GameInstance"));
		return false;
	}

	const FHSRBattleParticipant* Player = FindStageBuffPlayerParticipant();
	if (!Player)
	{
		UE_LOG(LogTemp, Error,
			TEXT("P17-009D Stage Buff application failed: no player-team participant with valid ASC CharacterId=%s"),
			*PlayerCharacterId.ToString());
		return false;
	}

	UHSRBattleTransitionSubsystem* Transition =
		BattleWorld->GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>();
	UHSRInventorySubsystem* Inventory =
		BattleWorld->GetGameInstance()->GetSubsystem<UHSRInventorySubsystem>();
	if (!Transition || !Inventory || !Transition->ValidateStageBuffIds(CurrentEncounterId, CurrentStageBuffIds))
	{
		UE_LOG(LogTemp, Error, TEXT("P17-009D Stage Buff application failed: invalid Transition, Inventory, or Buff IDs"));
		return false;
	}

	for (const FName BuffId : CurrentStageBuffIds)
	{
		// 同一 Buff 不允许重复施加。
		if (AppliedStageBuffHandles.Contains(BuffId))
		{
			UE_LOG(LogTemp, Error, TEXT("P17-009D Stage Buff application failed: duplicate runtime BuffId=%s"), *BuffId.ToString());
			return false;
		}
		const UHSRStageBuffDefinition* Definition =
			Transition->FindStageBuffDefinition(CurrentEncounterId, BuffId);
		// 定义必须有效：GE 非空、成本非负、有成本就必须有资源 ID。
		if (!Definition || !Definition->GameplayEffectClass
			|| Definition->ResourceCost < 0
			|| (Definition->ResourceCost > 0 && Definition->ResourceItemId.IsNone()))
		{
			UE_LOG(LogTemp, Error, TEXT("P17-009D Stage Buff application failed: invalid definition BuffId=%s"), *BuffId.ToString());
			return false;
		}

		// 有资源成本时，二次确认库存仍够（预检与施加之间可能被其他逻辑改动）。
		if (Definition->ResourceCost > 0)
		{
			FHSRInventorySnapshot Snapshot;
			Inventory->GetSnapshot(Snapshot);
			if (Snapshot.GetStackQuantity(Definition->ResourceItemId) < Definition->ResourceCost)
			{
				UE_LOG(LogTemp, Error, TEXT("P17-009D Stage Buff application failed: resource changed after preflight BuffId=%s"), *BuffId.ToString());
				return false;
			}
		}

		// 生成 GE spec 并施加到玩家 ASC。
		FGameplayEffectSpecHandle Spec = Player->AbilitySystemComponent->MakeOutgoingSpec(
			Definition->GameplayEffectClass, 1.0f, Player->AbilitySystemComponent->MakeEffectContext());
		if (!Spec.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("P17-009D Stage Buff application failed: invalid GE spec BuffId=%s"), *BuffId.ToString());
			return false;
		}
		const FActiveGameplayEffectHandle Handle =
			Player->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		// 施加后必须确实处于激活状态，否则视为失败。
		if (!Handle.WasSuccessfullyApplied() || !Player->AbilitySystemComponent->GetActiveGameplayEffect(Handle))
		{
			UE_LOG(LogTemp, Error, TEXT("P17-009D Stage Buff application failed: GE was not active BuffId=%s"), *BuffId.ToString());
			return false;
		}
		AppliedStageBuffHandles.Add(BuffId, Handle);

		// 记录资源成本，并在施加成功后扣除库存。
		if (Definition->ResourceCost > 0)
		{
			if (Inventory->RemoveStack(Definition->ResourceItemId, Definition->ResourceCost)
				!= EHSRInventoryOperationResult::Success)
			{
				// 扣资源失败：把刚加的 GE 移除，保持无副作用。
				Player->AbilitySystemComponent->RemoveActiveGameplayEffect(Handle);
				AppliedStageBuffHandles.Remove(BuffId);
				UE_LOG(LogTemp, Error, TEXT("P17-009D Stage Buff application failed: resource debit failed BuffId=%s"), *BuffId.ToString());
				return false;
			}
			AppliedStageBuffCosts.Add(BuffId, Definition->ResourceCost);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("P17-009D Stage Buffs applied RequestId=%s Count=%d Debits=%d"),
		*CurrentRequestId.ToString(), AppliedStageBuffHandles.Num(), AppliedStageBuffCosts.Num());
	return true;
}

UAbilitySystemComponent* UHSRBattleCoordinator::FindLeaderAbilitySystemComponent(EHSRBattleParticipantTeam Team) const
{
	// 领队是队伍名单里第一个成员，也是该队第一个生成出的参与者，所以首个匹配即返回。
	for (const FHSRBattleParticipant& Participant : Participants)
	{
		if (Participant.Team == Team)
		{
			return Participant.AbilitySystemComponent.Get();
		}
	}

	return nullptr;
}

// 回滚所有已施加的关卡 Buff：默认移除 GE，可选地退还已扣资源。
void UHSRBattleCoordinator::RollbackStageBuffs(bool bRefundResources)
{
	// 注意：这里曾用 ParticipantId 与 PlayerCharacterId 比对——两个不同的 ID 空间
	//（"Player" 对作者角色 ID），从不相等，导致回滚静默无效。关卡 Buff 落在领队身上，
	// 所以按名单位置解析领队，而不是按角色 ID。
	UAbilitySystemComponent* PlayerASC = FindLeaderAbilitySystemComponent(EHSRBattleParticipantTeam::Player);

	if (PlayerASC)
	{
		for (const TPair<FName, FActiveGameplayEffectHandle>& Entry : AppliedStageBuffHandles)
		{
			if (Entry.Value.IsValid())
			{
				PlayerASC->RemoveActiveGameplayEffect(Entry.Value);
			}
		}
	}

	if (bRefundResources)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UHSRInventorySubsystem* Inventory = GI->GetSubsystem<UHSRInventorySubsystem>())
				{
					for (const TPair<FName, int32>& Entry : AppliedStageBuffCosts)
					{
						const UHSRBattleTransitionSubsystem* Transition = GI->GetSubsystem<UHSRBattleTransitionSubsystem>();
						const UHSRStageBuffDefinition* Definition = Transition
							? Transition->FindStageBuffDefinition(CurrentEncounterId, Entry.Key) : nullptr;
						if (Definition)
						{
							Inventory->AddStack(Definition->ResourceItemId, Entry.Value);
						}
					}
				}
			}
		}
	}

	AppliedStageBuffHandles.Reset();
	AppliedStageBuffCosts.Reset();
}

// 在战斗世界里构建并生成所有参与者，完成初始化后进入 Spawned 状态。
// 这是请求提交后的第二步，任何失败都会回滚已生成的参与者。
FHSRBattleInitResult UHSRBattleCoordinator::BuildParticipants(UWorld* BattleWorld)
{
	if (CurrentState != EHSRBattleCoordinatorState::Consuming)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::BuildParticipants - REJECTED state=%d RequestId=%s (expected Consuming)"),
			static_cast<int32>(CurrentState), *CurrentRequestId.ToString());
		return FHSRBattleInitResult::MakeFailure(
			EHSRBattleInitFailureType::DefinitionNotFound,
			FText::FromString(TEXT("Coordinator is not in Consuming state."))
		);
	}

	if (!BattleWorld)
	{
		UE_LOG(LogTemp, Error,
			TEXT("UHSRBattleCoordinator::BuildParticipants - FAILED BattleWorld=null RequestId=%s EncounterId=%s"),
			*CurrentRequestId.ToString(), *CurrentEncounterId.ToString());
		CurrentState = EHSRBattleCoordinatorState::Failed;
		return FHSRBattleInitResult::MakeFailure(
			EHSRBattleInitFailureType::SpawnFailed,
			FText::FromString(TEXT("Battle World is null."))
		);
	}
	// 装备与圣遗物套装加成用的 GE 走默认路径加载（若尚未由 GameMode 配置）。
	if (!EquipmentGameplayEffect)
	{
		EquipmentGameplayEffect = LoadClass<UGameplayEffect>(nullptr, TEXT("/Game/GameplayEffects/GE_Equipment_P12.GE_Equipment_P12_C"));
	}
	if (!RelicSetGameplayEffect)
	{
		RelicSetGameplayEffect = LoadClass<UGameplayEffect>(nullptr, TEXT("/Game/GameplayEffects/GE_RelicSet_P12_A.GE_RelicSet_P12_A_C"));
	}

	// 先构建并校验参与者定义。
	FHSRBattleInitResult DefResult = BuildAndValidateParticipantDefinitions();
	if (!DefResult.IsSuccess())
	{
		CurrentState = EHSRBattleCoordinatorState::Failed;
		return DefResult;
	}

	Participants.Empty();
	// 构建失败时的统一回滚：撤 Buff、解绑委托、清成长 GE、销毁已生成参与者。
	const auto RollbackBuild = [this]()
	{
		RollbackStageBuffs(true);
		ClearRuntimeDelegates();
		ClearProgressionGameplayEffects();
		for (FHSRBattleParticipant& Existing : Participants)
		{
			if (Existing.Actor.IsValid())
			{
				Existing.Actor->Destroy();
			}
		}
		Participants.Empty();
		TurnManager = nullptr;
	};
	// 解析本次遭遇的敌人定义：先查作者目录（按 EnemyDefinitionId 主键），
	// 只有目录没有匹配时才回退到注入的单敌人定义（旧式单敌人流程）。
	UHSREnemyDefinition* ResolvedEnemyDefinition = nullptr;
	if (EnemyCatalog)
	{
		for (const TObjectPtr<UHSREnemyDefinition>& CatalogEntry : EnemyCatalog->Enemies)
		{
			if (CatalogEntry && CatalogEntry->EnemyDefinitionId == CurrentEnemyDefinitionId)
			{
				ResolvedEnemyDefinition = CatalogEntry;
				break;
			}
		}
	}
	if (!ResolvedEnemyDefinition && EnemyDefinition && EnemyDefinition->EnemyDefinitionId == CurrentEnemyDefinitionId)
	{
		ResolvedEnemyDefinition = EnemyDefinition;
	}
	if (!ResolvedEnemyDefinition)
	{
		CurrentState = EHSRBattleCoordinatorState::Failed;
		return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::DefinitionNotFound, FText::FromString(TEXT("Configured EnemyDefinition does not match encounter EnemyDefinitionId.")), CurrentEnemyDefinitionId);
	}
	EnemyDefinition = ResolvedEnemyDefinition;

	// 逐个定义生成参与者并初始化。
	for (const auto& Def : ParticipantDefinitions)
	{
		AActor* SpawnedActor = SpawnParticipantActor(BattleWorld, Def);
		if (!SpawnedActor)
		{
			UE_LOG(LogTemp, Error,
				TEXT("UHSRBattleCoordinator::BuildParticipants - FAILED spawn ParticipantId=%s DefId=%s RequestId=%s"),
				*Def.ParticipantId.ToString(), *Def.DefinitionId.ToString(), *CurrentRequestId.ToString());
			// 清理已生成的部分。
			RollbackBuild();
			ParticipantDefinitions.Empty();
			CurrentState = EHSRBattleCoordinatorState::Failed;
			return FHSRBattleInitResult::MakeFailure(
				EHSRBattleInitFailureType::SpawnFailed,
				FText::FromString(TEXT("Failed to spawn participant actor.")),
				Def.DefinitionId);
		}

		if (!InitParticipantASC(SpawnedActor))
		{
			UE_LOG(LogTemp, Error,
				TEXT("UHSRBattleCoordinator::BuildParticipants - FAILED InitASC ParticipantId=%s DefId=%s RequestId=%s"),
				*Def.ParticipantId.ToString(), *Def.DefinitionId.ToString(), *CurrentRequestId.ToString());
			SpawnedActor->Destroy();
			RollbackBuild();
			ParticipantDefinitions.Empty();
			CurrentState = EHSRBattleCoordinatorState::Failed;
			return FHSRBattleInitResult::MakeFailure(
				EHSRBattleInitFailureType::InitFailed,
				FText::FromString(TEXT("Failed to initialize ASC on participant.")),
				Def.DefinitionId);
		}

		// 组装参与者 DTO。
		FHSRBattleParticipant Participant;
		Participant.ParticipantId = Def.ParticipantId;
		Participant.DefinitionId = Def.DefinitionId;
		Participant.DisplayName = Def.DisplayName;
		Participant.Portrait = Def.Portrait;
		Participant.Team = Def.Team;
		Participant.Actor = SpawnedActor;
		Participant.AbilitySystemComponent = SpawnedActor->FindComponentByClass<UAbilitySystemComponent>();
		if (!ApplyParticipantInitializationGameplayEffect(Participant))
		{
			UE_LOG(LogTemp, Error, TEXT("P8-005 InitGE Participant=%s Result=FAILED"), *Participant.ParticipantId.ToString());
			SpawnedActor->Destroy();
			RollbackBuild();
			ParticipantDefinitions.Empty();
			CurrentState = EHSRBattleCoordinatorState::Failed;
			return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::InitFailed, FText::FromString(TEXT("Participant initialization GameplayEffect failed.")), Def.DefinitionId);
		}
		if (Def.Team == EHSRBattleParticipantTeam::Enemy)
		{
			// 敌人到这里时定义资产已解析：补全展示字段与弱点、韧性。
			// 只有当 roster 里没写这些字段时才回退到共享敌人定义（更具体的来源优先）。
			if (Participant.DisplayName.IsEmpty())
			{
				Participant.DisplayName = EnemyDefinition->DisplayName;
			}
			if (Participant.Portrait.IsNull())
			{
				Participant.Portrait = EnemyDefinition->Portrait;
			}
			Participant.WeaknessTags = EnemyDefinition->WeaknessTags;
			Participant.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxToughnessAttribute(), EnemyDefinition->InitialMaxToughness);
			Participant.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetToughnessAttribute(), EnemyDefinition->InitialToughness);
			UE_LOG(LogTemp, Log, TEXT("P8-005 EnemyToughness Participant=%s Toughness=%.2f MaxToughness=%.2f"), *Participant.ParticipantId.ToString(), Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetToughnessAttribute()), Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxToughnessAttribute()));
		}
		if (!GrantSkillLoadout(Participant))
		{
			UE_LOG(LogTemp, Error, TEXT("UHSRBattleCoordinator::BuildParticipants - FAILED to grant skill loadout ParticipantId=%s"), *Participant.ParticipantId.ToString());
			SpawnedActor->Destroy();
			RollbackBuild();
			ParticipantDefinitions.Empty();
			CurrentState = EHSRBattleCoordinatorState::Failed;
			return FHSRBattleInitResult::MakeFailure(
				EHSRBattleInitFailureType::InitFailed,
				FText::FromString(TEXT("Failed to grant skill loadout.")),
				Def.DefinitionId);
		}
		Participants.Add(Participant);
		// 订阅血量变化，用于检测死亡。
		BindHealthObserver(Participant);

		UE_LOG(LogTemp, Log,
			TEXT("UHSRBattleCoordinator::BuildParticipants - Spawned ParticipantId=%s DefId=%s Team=%d Actor=%s ASC=%s"),
			*Def.ParticipantId.ToString(), *Def.DefinitionId.ToString(),
			static_cast<int32>(Def.Team),
			*SpawnedActor->GetName(),
			Participant.AbilitySystemComponent.IsValid() ? *Participant.AbilitySystemComponent->GetName() : TEXT("null"));
	}

	if (!ApplyStageBuffs(BattleWorld))
	{
		UE_LOG(LogTemp, Error, TEXT("P17-009D Stage Buff application failed RequestId=%s"), *CurrentRequestId.ToString());
		RollbackBuild();
		ParticipantDefinitions.Empty();
		CurrentState = EHSRBattleCoordinatorState::Failed;
		return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::InitFailed,
			FText::FromString(TEXT("Stage Buff application or resource transaction failed.")));
	}

	// 初始化回合管理器与状态组件。
	TurnManager = NewObject<UHSRTurnManager>(this);
	if (!TurnManager || !TurnManager->Initialize(Participants))
	{
		UE_LOG(LogTemp, Error, TEXT("UHSRBattleCoordinator::BuildParticipants - FAILED to initialize TurnManager RequestId=%s"), *CurrentRequestId.ToString());
		RollbackBuild();
		CurrentState = EHSRBattleCoordinatorState::Failed;
		return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::InitFailed, FText::FromString(TEXT("Failed to initialize TurnManager.")));
	}
	if (!InitializeStatusComponents())
	{
		ClearStatusComponents();
		RollbackBuild();
		CurrentState = EHSRBattleCoordinatorState::Failed;
		return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::InitFailed, FText::FromString(TEXT("Failed to initialize StatusComponents.")));
	}

	// 原子切换到 Spawned，绑定装备移动投影与敌人回合监听。
	CurrentState = EHSRBattleCoordinatorState::Spawned;
	if (UGameInstance* GI = BattleWorld->GetGameInstance())
	{
		if (UHSREquipmentSubsystem* Equipment = GI->GetSubsystem<UHSREquipmentSubsystem>())
		{
			Equipment->SetRestoreProjection(FHSREquipmentRestoreProjection::CreateUObject(this, &ThisClass::ProjectEquipmentRestore));
			BindEquipmentMovementProjection(*Equipment);
		}
	}
	BindEnemyTurnManager(TurnManager);
	DevelopmentDamageRandomStream.Initialize(DevelopmentDamageSeed);
	DevelopmentDamageConsumeCount = 0;
	DevelopmentDamageResults.Empty();
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	ClearDamageTestInjection();
	LastDevelopmentFormalExecutionResult = FHSRFormalDamageExecutionResult();
#endif
	PublishCommandViewState();
	// 在 Coordinator 进入 Spawned 前就记录初始广播，统一并入后续回合的同一个队列。
	RecordCurrentEnemyTurnIfNeeded();
	DrainPendingEnemyTurns();

	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::BuildParticipants - SUCCESS RequestId=%s Participants=%d Definitions=%d"),
		*CurrentRequestId.ToString(), Participants.Num(), ParticipantDefinitions.Num());

	return FHSRBattleInitResult::MakeSuccess();
}

// 便捷入口：构造一条基础攻击指令并提交（供旧调用方使用）。
bool UHSRBattleCoordinator::RequestBasicAttack(FName AttackerParticipantId, FName TargetParticipantId)
{
	FHSRBattleActionCommand Command;
	Command.ActionId = FGuid::NewGuid();
	Command.BattleId = CurrentRequestId;
	Command.ActorParticipantId = AttackerParticipantId;
	Command.SkillId = FName(TEXT("BasicAttack"));
	Command.TargetParticipantIds.Add(TargetParticipantId);
	return RequestAction(Command).Succeeded();
}

// 公开指令入口：维护调度深度计数，交给核心解析，并在最外层解析结束后排空敌人回合队列。
FHSRAbilityResolution UHSRBattleCoordinator::RequestAction(const FHSRBattleActionCommand& Command)
{
	++RequestActionDispatchDepth;
	#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	++PublicRequestActionDepth;
	MaxPublicRequestActionDepth = FMath::Max(MaxPublicRequestActionDepth, PublicRequestActionDepth);
	++CoreExecutionDepth;
	MaxCoreExecutionDepth = FMath::Max(MaxCoreExecutionDepth, CoreExecutionDepth);
	#endif
	const FHSRAbilityResolution Resolution = RequestActionCore(Command);
	#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	check(CoreExecutionDepth > 0);
	--CoreExecutionDepth;
	check(PublicRequestActionDepth > 0);
	--PublicRequestActionDepth;
	#endif
	check(RequestActionDispatchDepth > 0);
	--RequestActionDispatchDepth;
	// 仅当回到最外层调用时才排空敌人回合，避免递归调度互相嵌套。
	if (RequestActionDispatchDepth == 0)
	{
		DrainPendingEnemyTurns();
	}
	return Resolution;
}

// 指令解析核心：幂等缓存 → 状态/回合/技能/目标校验 → 形式化伤害准备 → 施加 → 韧性/破韧 → 结算。
FHSRAbilityResolution UHSRBattleCoordinator::RequestActionCore(const FHSRBattleActionCommand& Command)
{
	FHSRAbilityResolution Resolution;
	Resolution.ActionId = Command.ActionId;
	Resolution.ActorParticipantId = Command.ActorParticipantId;
	Resolution.SkillId = Command.SkillId;
	// 收尾：把成功动作的伤害/韧性/破韧/治疗转成展示事件，缓存解析结果并广播视图状态。
	const auto Finalize = [this, &Command](FHSRAbilityResolution InResolution)
	{
		if (InResolution.Succeeded() && Command.ActionId.IsValid())
		{
			if (InResolution.bHasDamageResult)
			{
				FHSRBattlePresentationEvent& Event = PresentationEvents.AddDefaulted_GetRef();
				Event.EventId = FGuid::NewGuid();
				Event.ActionId = Command.ActionId;
				Event.SourceParticipantId = Command.ActorParticipantId;
				Event.TargetParticipantId = Command.TargetParticipantIds.Num() > 0 ? Command.TargetParticipantIds[0] : NAME_None;
				Event.EventType = EHSRPresentationEventType::Damage;
				Event.Value = InResolution.DamageResult.Breakdown.AppliedDamage;
				Event.bCritical = InResolution.DamageResult.Breakdown.bCritical;
				UE_LOG(LogTemp, Log, TEXT("P10-003 PresentationEvent Type=Damage EventId=%s ActionId=%s Source=%s Target=%s Value=%.2f Critical=%d"), *Event.EventId.ToString(), *Event.ActionId.ToString(), *Event.SourceParticipantId.ToString(), *Event.TargetParticipantId.ToString(), Event.Value, Event.bCritical ? 1 : 0);
			}
			if (InResolution.bHasToughnessResult)
			{
				FHSRBattlePresentationEvent& Event = PresentationEvents.AddDefaulted_GetRef();
				Event.EventId = FGuid::NewGuid();
				Event.ActionId = Command.ActionId;
				Event.SourceParticipantId = Command.ActorParticipantId;
				Event.TargetParticipantId = Command.TargetParticipantIds.Num() > 0 ? Command.TargetParticipantIds[0] : NAME_None;
				Event.EventType = EHSRPresentationEventType::Toughness;
				Event.Value = InResolution.ToughnessResult.Damage;
				UE_LOG(LogTemp, Log, TEXT("P10-003 PresentationEvent Type=Toughness EventId=%s ActionId=%s Source=%s Target=%s Value=%.2f"), *Event.EventId.ToString(), *Event.ActionId.ToString(), *Event.SourceParticipantId.ToString(), *Event.TargetParticipantId.ToString(), Event.Value);
			}
			if (InResolution.bHasBreakResult && InResolution.BreakResult.bTriggered)
			{
				FHSRBattlePresentationEvent& Event = PresentationEvents.AddDefaulted_GetRef();
				Event.EventId = FGuid::NewGuid();
				Event.ActionId = Command.ActionId;
				Event.SourceParticipantId = Command.ActorParticipantId;
				Event.TargetParticipantId = InResolution.BreakResult.TargetParticipantId;
				Event.EventType = EHSRPresentationEventType::Break;
				Event.Value = InResolution.BreakResult.ToughnessBefore - InResolution.BreakResult.ToughnessAfter;
				Event.bBreak = true;
				UE_LOG(LogTemp, Log, TEXT("P10-003 PresentationEvent Type=Break EventId=%s ActionId=%s Source=%s Target=%s Value=%.2f"), *Event.EventId.ToString(), *Event.ActionId.ToString(), *Event.SourceParticipantId.ToString(), *Event.TargetParticipantId.ToString(), Event.Value);
			}
			if (InResolution.bHasHealResult)
			{
				FHSRBattlePresentationEvent& Event = PresentationEvents.AddDefaulted_GetRef();
				Event.EventId = FGuid::NewGuid();
				Event.ActionId = Command.ActionId;
				Event.SourceParticipantId = Command.ActorParticipantId;
				Event.TargetParticipantId = Command.TargetParticipantIds.Num() > 0 ? Command.TargetParticipantIds[0] : NAME_None;
				Event.EventType = EHSRPresentationEventType::Heal;
				Event.Value = InResolution.HealAmount;
				UE_LOG(LogTemp, Log, TEXT("P10-003 PresentationEvent Type=Heal EventId=%s ActionId=%s Source=%s Target=%s Value=%.2f"), *Event.EventId.ToString(), *Event.ActionId.ToString(), *Event.SourceParticipantId.ToString(), *Event.TargetParticipantId.ToString(), Event.Value);
			}
			// 展示事件只保留最近 32 条。
			if (PresentationEvents.Num() > 32)
			{
				PresentationEvents.RemoveAt(0, PresentationEvents.Num() - 32, EAllowShrinking::No);
			}
		}
		if (Command.ActionId.IsValid())
		{
			ProcessedActionResolutions.Add(Command.ActionId, InResolution);
		}
		LastActionResolution = InResolution;
		PublishCommandViewState();
		return InResolution;
	};
	const auto Reject = [&Resolution, &Finalize](EHSRAbilityFailureReason Reason)
	{
		Resolution.Status = EHSRAbilityResolutionStatus::Rejected;
		Resolution.FailureReason = Reason;
		return Finalize(Resolution);
	};
	if (Command.ActionId.IsValid())
	{
		if (const FHSRAbilityResolution* ExistingResolution = ProcessedActionResolutions.Find(Command.ActionId))
		{
			if (ExistingResolution->bHasToughnessResult)
			{
				const FHSRToughnessResult& CachedToughness = ExistingResolution->ToughnessResult;
				const FString ReplayTarget = Command.TargetParticipantIds.IsEmpty() ? TEXT("<omitted>") : Command.TargetParticipantIds[0].ToString();
				const UHSRSkillDefinition* CachedSkillDefinition = FindSkillDefinition(Command.SkillId);
				const FString ReplayElement = CachedSkillDefinition ? CachedSkillDefinition->ElementTag.ToString() : TEXT("<unavailable>");
				const FGameplayTag ReplayExpectedWeaknessTag = CachedSkillDefinition
					? FHSRToughnessConfiguration::GetWeaknessTagFor(CachedSkillDefinition->ElementTag)
					: FGameplayTag();
				const FString ReplayExpectedWeakness = ReplayExpectedWeaknessTag.IsValid()
					? ReplayExpectedWeaknessTag.ToString()
					: TEXT("<invalid>");
				UE_LOG(LogTemp, Log, TEXT("P8-002 Toughness Replay ActionId=%s Actor=%s Target=%s Element=%s ExpectedWeakness=%s Matched=%d Before=%.2f Damage=%.2f After=%.2f ReachedZero=%d FailureReason=%d"),
					*Command.ActionId.ToString(), *ExistingResolution->ActorParticipantId.ToString(), *ReplayTarget, *ReplayElement, *ReplayExpectedWeakness, CachedToughness.bMatched ? 1 : 0,
					CachedToughness.Before, CachedToughness.Damage, CachedToughness.After, CachedToughness.bReachedZero ? 1 : 0, static_cast<int32>(CachedToughness.FailureReason));
			}
			if (ExistingResolution->bHasBreakResult)
			{
				const FHSRBreakResult& CachedBreak = ExistingResolution->BreakResult;
				UE_LOG(LogTemp, Log, TEXT("P8-003 Break ActionId=%s Target=%s Before=%.2f After=%.2f Triggered=%d Replay=1 FailureReason=%d"),
					*CachedBreak.ActionId.ToString(), *CachedBreak.TargetParticipantId.ToString(), CachedBreak.ToughnessBefore,
					CachedBreak.ToughnessAfter, CachedBreak.bTriggered ? 1 : 0, static_cast<int32>(CachedBreak.FailureReason));
			}
			return *ExistingResolution;
		}
	}
	if (CurrentState != EHSRBattleCoordinatorState::Spawned || !TurnManager || Command.BattleId != CurrentRequestId)
	{
		return Reject(EHSRAbilityFailureReason::InvalidBattle);
	}
	if (!Command.ActionId.IsValid())
	{
		return Reject(EHSRAbilityFailureReason::DuplicateAction);
	}
	if (TurnManager->GetCurrentParticipantId() != Command.ActorParticipantId)
	{
		return Reject(EHSRAbilityFailureReason::NotCurrentActor);
	}
	const UHSRSkillDefinition* ResolvedSkillDefinition = FindSkillDefinition(Command.SkillId);
	if (!ResolvedSkillDefinition || !ResolvedSkillDefinition->IsValidDefinition())
	{
		return Reject(EHSRAbilityFailureReason::DefinitionMissing);
	}

	const FHSRBattleParticipant* Attacker = FindParticipant(Command.ActorParticipantId);
	if (!Attacker || !Attacker->IsAlive()
		|| !FHSRTargetingPolicy::ValidateTargetIds(*ResolvedSkillDefinition, *Attacker, Participants, Command.TargetParticipantIds))
	{
		return Reject(EHSRAbilityFailureReason::InvalidTarget);
	}
	FHSRBattleParticipant* Target = FindParticipant(Command.TargetParticipantIds[0]);
	if (!Target || !Target->AbilitySystemComponent.IsValid())
	{
		return Reject(EHSRAbilityFailureReason::InvalidTarget);
	}
	// 记录目标在“录取”时是否存活（用于破韧延迟 + 延迟死亡判定）。
	const bool bTargetAliveAtAdmission = Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) > 0.0f;
	// 治疗技能对已满血目标直接拒绝。
	if (ResolvedSkillDefinition->RestoresHealth()
		&& Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) >= Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxHealthAttribute()))
	{
		return Reject(EHSRAbilityFailureReason::AlreadyAtFullHealth);
	}

	// 这是唯一会改变状态的 P6-001 技能的同步预检：
	// ResolveAction 只会在当前参与者缺失/失效或变化时拒绝。当前 ID 与 TurnManager 自己的
	// 弱参与者副本都在 GE 激活前被检查；此检查与紧随其后的 ResolveAction 之间
	// 没有任何 Tick、委托或异步工作。
	const FHSRBattleParticipant* TurnParticipant = TurnManager->GetOrderedParticipants().FindByPredicate(
		[&Command](const FHSRBattleParticipant& Participant)
		{
			return Participant.ParticipantId == Command.ActorParticipantId;
		});
	if (!TurnParticipant || !TurnParticipant->IsValid())
	{
		return Reject(EHSRAbilityFailureReason::NotCurrentActor);
	}

	FGameplayAbilitySpec* AbilitySpec = Attacker->AbilitySystemComponent->FindAbilitySpecFromClass(ResolvedSkillDefinition->AbilityClass);
	UHSRGameplayAbilityBase* Ability = AbilitySpec ? Cast<UHSRGameplayAbilityBase>(AbilitySpec->GetPrimaryInstance()) : nullptr;
	if (!Ability || !Ability->SetPendingTarget(Target->AbilitySystemComponent.Get()))
	{
		return Reject(EHSRAbilityFailureReason::AbilityUnavailable);
	}
	// Damage-dealing skills use the formal prepared-damage seam; a purely restorative skill has
	// nothing to prepare and skips it.
	if (ResolvedSkillDefinition->UsesPreparedDamage())
	{
		const EHSRAbilityFailureReason FormalPreActivationFailure = Ability->GetPreActivationFailureReason(AbilitySpec->Handle, Attacker->AbilitySystemComponent->AbilityActorInfo.Get());
		if (FormalPreActivationFailure != EHSRAbilityFailureReason::None)
		{
			UE_LOG(LogTemp, Warning, TEXT("P7-003 Formal Stage=PreActivation Result=REJECT ActionId=%s Skill=%s Reason=%d RNG=%d SP=%d"), *Command.ActionId.ToString(), *Command.SkillId.ToString(), static_cast<int32>(FormalPreActivationFailure), DevelopmentDamageConsumeCount, TeamResourceState.CurrentSkillPoints);
			Ability->ClearPendingTarget();
			return Reject(FormalPreActivationFailure);
		}
		const UHSRDamageRuleDefinition* Rule = ResolvedSkillDefinition->DamageRule.LoadSynchronous();
		const TSubclassOf<UGameplayEffect> DamageEffect = ResolvedSkillDefinition->EffectGameplayEffectClass.LoadSynchronous();
		const FHSRDamageSetByCallerTags DamageTags;
		if (!Rule || !Rule->IsValidRuleDefinition() || !DamageEffect || !ResolvedSkillDefinition->DamageType.IsValid()
			|| !FMath::IsFinite(ResolvedSkillDefinition->AbilityMultiplier) || ResolvedSkillDefinition->AbilityMultiplier <= 0.0f
			|| !DamageTags.IsValid())
		{
			Ability->ClearPendingTarget();
			return Reject(EHSRAbilityFailureReason::DefinitionMissing);
		}
		FRandomStream PreviewStream = DevelopmentDamageRandomStream;
		FHSRFormalDamageRequest Request;
		Request.BattleId = CurrentRequestId; Request.ActionId = Command.ActionId; Request.SkillId = Command.SkillId;
		Request.SourceParticipantId = Attacker->ParticipantId; Request.TargetParticipantId = Target->ParticipantId;
		Request.DamageType = ResolvedSkillDefinition->DamageType; Request.AbilityMultiplier = ResolvedSkillDefinition->AbilityMultiplier;
		Request.DefenseCoefficient = Rule->DefenseCoefficient; Request.MinDamage = Rule->MinDamage; Request.CritRoll = PreviewStream.GetFraction();
		FGameplayEffectContextHandle ContextHandle = Attacker->AbilitySystemComponent->MakeEffectContext();
		FHSRDamageEffectContext* DamageContext = ContextHandle.IsValid() ? static_cast<FHSRDamageEffectContext*>(ContextHandle.Get()) : nullptr;
		if (!DamageContext || DamageContext->GetScriptStruct() != FHSRDamageEffectContext::StaticStruct()) { Ability->ClearPendingTarget(); return Reject(EHSRAbilityFailureReason::EffectFailed); }
		ContextHandle.AddSourceObject(Attacker->Actor.Get());
		DamageContext->DamageContext.ActionId = Request.ActionId; DamageContext->DamageContext.DamageType = Request.DamageType;
		DamageContext->DamageContext.AbilityMultiplier = Request.AbilityMultiplier; DamageContext->DamageContext.CritRoll = Request.CritRoll;
		DamageContext->DefenseCoefficient = Request.DefenseCoefficient; DamageContext->MinDamage = Request.MinDamage;
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
		DamageContext->TestInjection = DamageTestInjectionActionId == Command.ActionId ? NextDamageTestInjection : EHSRDamageTestInjection::None;
		UE_LOG(LogTemp, Log, TEXT("P7-004 InjectionBind ActionId=%s BoundActionId=%s Requested=%d Applied=%d"), *Command.ActionId.ToString(), *DamageTestInjectionActionId.ToString(), static_cast<int32>(NextDamageTestInjection), static_cast<int32>(DamageContext->TestInjection));
		if (DamageTestInjectionActionId == Command.ActionId) { ClearDamageTestInjection(); }
#endif
		FGameplayEffectSpecHandle DamageSpec = Attacker->AbilitySystemComponent->MakeOutgoingSpec(DamageEffect, 1.0f, ContextHandle);
		if (!DamageSpec.IsValid()) { UE_LOG(LogTemp, Warning, TEXT("P7-003 Formal Stage=MakeSpec Result=FAIL ActionId=%s Skill=%s"), *Command.ActionId.ToString(), *Command.SkillId.ToString()); Ability->ClearPendingTarget(); return Reject(EHSRAbilityFailureReason::EffectFailed); }
		if (FHSRDamageEffectContext* SpecContext = static_cast<FHSRDamageEffectContext*>(DamageSpec.Data->GetContext().Get()))
		{
			SpecContext->TestInjection = DamageContext->TestInjection;
			SpecContext->DamageContext.ActionId = Request.ActionId;
			UE_LOG(LogTemp, Log, TEXT("P7-004 SpecContext ActionId=%s Injection=%d"), *SpecContext->DamageContext.ActionId.ToString(), static_cast<int32>(SpecContext->TestInjection));
		}
		DamageTags.ApplyTo(*DamageSpec.Data, Request.AbilityMultiplier, Request.DefenseCoefficient,
			Request.MinDamage, Request.CritRoll);
		FHSRFormalDamagePrepareResult PrepareResult;
		if (!Ability->PrepareFormalDamage(Request, DamageSpec, Target->AbilitySystemComponent.Get(), PrepareResult)) { UE_LOG(LogTemp, Warning, TEXT("P7-003 Formal Stage=Prepare Result=FAIL ActionId=%s Skill=%s DamageResult=%d"), *Command.ActionId.ToString(), *Command.SkillId.ToString(), static_cast<int32>(PrepareResult.Result)); Ability->ClearPendingTarget(); return Reject(EHSRAbilityFailureReason::EffectFailed); }
		const bool bUsesPlayerSkillPoints = Attacker->Team == EHSRBattleParticipantTeam::Player;
		const int32 SkillPointDelta = bUsesPlayerSkillPoints ? ResolvedSkillDefinition->GetSkillPointDelta() : 0;
		if (!ReserveSkillPoints(Command.ActionId, SkillPointDelta))
		{
			Ability->ClearPreparedFormalDamage();
			Ability->ClearPendingTarget();
			return Reject(EHSRAbilityFailureReason::InsufficientSkillPoint);
		}
		Ability->SetActionContext(Command.ActionId, Command.SkillId);
		bFormalDamageTransactionOpen = true;
		PendingDefeatedParticipantId = NAME_None;
		const bool bActivated = Attacker->AbilitySystemComponent->TryActivateAbility(AbilitySpec->Handle);
		const FHSRFormalDamageExecutionResult ExecutionResult = Ability->GetLastFormalDamageExecutionResult();
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
		LastDevelopmentFormalExecutionResult = ExecutionResult;
#endif
		bFormalDamageTransactionOpen = false;
		Ability->ClearPreparedFormalDamage(); Ability->ClearPendingTarget();
		if (!bActivated || !Ability->DidLastActivationSucceed() || !ExecutionResult.bSucceeded)
		{
			Resolution.bHasDamageResult = true;
			Resolution.DamageResult = ExecutionResult.DamageResult;
			UE_LOG(LogTemp, Warning, TEXT("P7-003 Formal Stage=Activate Result=FAIL ActionId=%s Skill=%s TryActivate=%d AbilitySuccess=%d ExecutionSuccess=%d DamageResult=%d CostCommitted=%d Refund=%d EnergyBefore=%.2f EnergyAfter=%.2f"), *Command.ActionId.ToString(), *Command.SkillId.ToString(), bActivated ? 1 : 0, Ability->DidLastActivationSucceed() ? 1 : 0, ExecutionResult.bSucceeded ? 1 : 0, static_cast<int32>(ExecutionResult.DamageResult.Result), ExecutionResult.bCostCommitted ? 1 : 0, ExecutionResult.bRefundApplied ? 1 : 0, ExecutionResult.EnergyBefore, ExecutionResult.EnergyAfter);
			PendingDefeatedParticipantId = NAME_None;
			RollbackSkillPoints(Command.ActionId);
			return Reject(EHSRAbilityFailureReason::EffectFailed);
		}
		DevelopmentDamageRandomStream = PreviewStream;
		++DevelopmentDamageConsumeCount;
		CommitActionEnergyGain(Command.ActionId, *ResolvedSkillDefinition, *Attacker->AbilitySystemComponent);
		CommitSkillPoints(Command.ActionId);
		ApplyAuthoredSkillStatuses(*ResolvedSkillDefinition, Command.ActorParticipantId, Command.TargetParticipantIds);
		Resolution.Status = EHSRAbilityResolutionStatus::Succeeded;
		Resolution.FailureReason = EHSRAbilityFailureReason::None;
		Resolution.bHasDamageResult = true;
		Resolution.DamageResult = ExecutionResult.DamageResult;
		// Toughness is an independent post-HP result.  Its failure deliberately
		// never rewrites the already-committed formal damage transaction.
		Resolution.bHasToughnessResult = true;
		FHSRToughnessResult& ToughnessResult = Resolution.ToughnessResult;
		ToughnessResult.Before = Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetToughnessAttribute());
		ToughnessResult.After = ToughnessResult.Before;
		const FString ElementName = ResolvedSkillDefinition->ElementTag.ToString();
		const FString ElementPrefix(TEXT("Element."));
		if (!ResolvedSkillDefinition->ElementTag.IsValid() || !ElementName.StartsWith(ElementPrefix))
		{
			ToughnessResult.FailureReason = EHSRToughnessFailureReason::MissingElement;
		}
		else if (Target->WeaknessTags.IsEmpty())
		{
			ToughnessResult.FailureReason = EHSRToughnessFailureReason::EmptyWeaknesses;
		}
		else
		{
			const FGameplayTag MatchingWeakness = FHSRToughnessConfiguration::GetWeaknessTagFor(ResolvedSkillDefinition->ElementTag);
			if (!MatchingWeakness.IsValid() || !Target->WeaknessTags.HasTagExact(MatchingWeakness))
			{
				ToughnessResult.FailureReason = EHSRToughnessFailureReason::NoWeaknessMatch;
			}
			else if (!FMath::IsFinite(ResolvedSkillDefinition->ToughnessDamage) || ResolvedSkillDefinition->ToughnessDamage <= 0.0f)
			{
				ToughnessResult.FailureReason = EHSRToughnessFailureReason::InvalidDamage;
			}
			else
			{
				ToughnessResult.bMatched = true;
				ToughnessResult.Damage = ResolvedSkillDefinition->ToughnessDamage;
				const TSubclassOf<UGameplayEffect> ToughnessEffect = ResolvedSkillDefinition->ToughnessDamageGameplayEffectClass.LoadSynchronous();
				const FGameplayTag ToughnessDamageTag = FGameplayTag::RequestGameplayTag(TEXT("Damage.Data.ToughnessDamage"), false);
				if (!ToughnessEffect || !ToughnessDamageTag.IsValid())
				{
					ToughnessResult.FailureReason = EHSRToughnessFailureReason::MissingGameplayEffect;
				}
				else
				{
					FGameplayEffectSpecHandle ToughnessSpec = Attacker->AbilitySystemComponent->MakeOutgoingSpec(ToughnessEffect, 1.0f, Attacker->AbilitySystemComponent->MakeEffectContext());
					if (!ToughnessSpec.IsValid())
					{
						ToughnessResult.FailureReason = EHSRToughnessFailureReason::EffectApplicationFailed;
					}
					else
					{
						ToughnessSpec.Data->SetSetByCallerMagnitude(ToughnessDamageTag, ToughnessResult.Damage);
						// Instant GameplayEffects may apply immediately without retaining an
						// active-effect handle.  Audit the AttributeSet write instead of
						// interpreting that invalid handle as an application failure.
						Target->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*ToughnessSpec.Data.Get());
						ToughnessResult.After = Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetToughnessAttribute());
						const float MaxToughness = Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxToughnessAttribute());
						const float ExpectedAfter = FMath::Clamp(ToughnessResult.Before - ToughnessResult.Damage, 0.0f, MaxToughness);
						if (FMath::IsNearlyEqual(ToughnessResult.After, ExpectedAfter))
						{
							ToughnessResult.bReachedZero = ToughnessResult.Before > 0.0f && ToughnessResult.After <= 0.0f;
						}
						else
						{
							ToughnessResult.FailureReason = EHSRToughnessFailureReason::EffectApplicationFailed;
						}
					}
				}
			}
		}
		const FGameplayTag ExpectedWeaknessTag = FHSRToughnessConfiguration::GetWeaknessTagFor(ResolvedSkillDefinition->ElementTag);
		const FString ExpectedWeakness = ExpectedWeaknessTag.IsValid() ? ExpectedWeaknessTag.ToString() : TEXT("<invalid>");
		UE_LOG(LogTemp, Log, TEXT("P8-002 Toughness ActionId=%s Actor=%s Target=%s Element=%s ExpectedWeakness=%s Matched=%d Before=%.2f Damage=%.2f After=%.2f ReachedZero=%d FailureReason=%d"),
			*Command.ActionId.ToString(), *Command.ActorParticipantId.ToString(), *Target->ParticipantId.ToString(), *ElementName,
			*ExpectedWeakness,
			ToughnessResult.bMatched ? 1 : 0, ToughnessResult.Before, ToughnessResult.Damage, ToughnessResult.After,
			ToughnessResult.bReachedZero ? 1 : 0, static_cast<int32>(ToughnessResult.FailureReason));
		// A Break is owned by this ActionId transaction: only the observed
		// positive-to-zero Toughness edge may publish it.  RequestAction caches
		// the completed Resolution, so an ActionId replay never reaches here.
		Resolution.bHasBreakResult = true;
		FHSRBreakResult& BreakResult = Resolution.BreakResult;
		BreakResult.ActionId = Command.ActionId;
		BreakResult.TargetParticipantId = Target->ParticipantId;
		BreakResult.ToughnessBefore = ToughnessResult.Before;
		BreakResult.ToughnessAfter = ToughnessResult.After;
		if (bBattleResultProduced || CurrentState != EHSRBattleCoordinatorState::Spawned)
		{
			BreakResult.FailureReason = EHSRBreakFailureReason::BattleFinished;
		}
		else if (!Target->IsValid())
		{
			BreakResult.FailureReason = EHSRBreakFailureReason::InvalidTarget;
		}
		else if (!ToughnessResult.bReachedZero || ToughnessResult.Before <= 0.0f || !FMath::IsNearlyZero(ToughnessResult.After))
		{
			BreakResult.FailureReason = EHSRBreakFailureReason::ToughnessNotDepleted;
		}
		else
		{
			BreakResult.bTriggered = true;
		}
		UE_LOG(LogTemp, Log, TEXT("P8-003 Break ActionId=%s Target=%s Before=%.2f After=%.2f Triggered=%d Replay=0 FailureReason=%d"),
			*BreakResult.ActionId.ToString(), *BreakResult.TargetParticipantId.ToString(), BreakResult.ToughnessBefore,
			BreakResult.ToughnessAfter, BreakResult.bTriggered ? 1 : 0, static_cast<int32>(BreakResult.FailureReason));
		if (BreakResult.bTriggered)
		{
			const bool bAllowPendingDeferredDefeat = bTargetAliveAtAdmission
				&& PendingDefeatedParticipantId == Target->ParticipantId;
			const EHSRStatusOperationResult BreakStatusResult = RequestBreakStatus(
				Command.ActorParticipantId, BreakResult.TargetParticipantId, BreakResult.ActionId, bAllowPendingDeferredDefeat);
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
			LastBreakStatusResultForTest = BreakStatusResult;
			if (BreakStatusResult == EHSRStatusOperationResult::Success)
			{
				++BreakStatusRequestCountForTest;
			}
#endif
			UE_LOG(LogTemp, Log, TEXT("P9-003 BreakStatus ActionId=%s Target=%s Result=%d"), *BreakResult.ActionId.ToString(), *BreakResult.TargetParticipantId.ToString(), static_cast<int32>(BreakStatusResult));
			FHSRTurnDelayRequest DelayRequest;
			DelayRequest.ActionId = BreakResult.ActionId;
			DelayRequest.TargetParticipantId = BreakResult.TargetParticipantId;
			LastBreakDelayActionId = DelayRequest.ActionId;
			bLastBreakDelayRegistered = bAllowPendingDeferredDefeat
				? TurnManager->ConsumeAdmittedBreakDelay(DelayRequest)
				: TurnManager->ConsumeBreakDelay(DelayRequest);
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
			bLastBreakDelayAcceptedForTest = bLastBreakDelayRegistered;
			if (bLastBreakDelayRegistered)
			{
				++BreakDelayRegistrationCountForTest;
			}
#endif
		}
		Finalize(Resolution);
		if (!PendingDefeatedParticipantId.IsNone()) { const FName Defeated = PendingDefeatedParticipantId; PendingDefeatedParticipantId = NAME_None; ResolveDefeat(Defeated); }
		else if (!bBattleResultProduced)
		{
			if (!TurnManager->ResolveAction(Command.ActorParticipantId))
			{
				UE_LOG(LogTemp, Error, TEXT("UHSRBattleCoordinator::RequestAction - turn resolve failed after formal commit ActionId=%s"), *Command.ActionId.ToString());
			}
			else
			{
				// Finalize publishes the authoritative action result while the old
				// actor still owns the turn. Publish once more after the synchronous
				// turn transition so an event-driven UI cannot remain on that stale actor.
				PublishCommandViewState();
			}
		}
		return Resolution;
	}
	const EHSRAbilityFailureReason PreActivationFailure = Ability->GetPreActivationFailureReason(AbilitySpec->Handle, Attacker->AbilitySystemComponent->AbilityActorInfo.Get());
	if (PreActivationFailure != EHSRAbilityFailureReason::None)
	{
		Ability->ClearPendingTarget();
		return Reject(PreActivationFailure);
	}
	const bool bUsesPlayerSkillPoints = Attacker->Team == EHSRBattleParticipantTeam::Player;
	const int32 SkillPointDelta = bUsesPlayerSkillPoints ? ResolvedSkillDefinition->GetSkillPointDelta() : 0;
	if (!ReserveSkillPoints(Command.ActionId, SkillPointDelta))
	{
		return Reject(EHSRAbilityFailureReason::InsufficientSkillPoint);
	}
	const float HealHealthBefore = ResolvedSkillDefinition->RestoresHealth()
		? Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) : 0.0f;
	Ability->SetActionContext(Command.ActionId, Command.SkillId);
	if (!Attacker->AbilitySystemComponent->TryActivateAbility(AbilitySpec->Handle) || !Ability->DidLastActivationSucceed())
	{
		Ability->ClearPendingTarget();
		RollbackSkillPoints(Command.ActionId);
		return Reject(Ability->GetLastFailureReason());
	}

	if (!bBattleResultProduced && !TurnManager->ResolveAction(Command.ActorParticipantId))
	{
		ensureMsgf(false, TEXT("UHSRBattleCoordinator::RequestAction post-GE ResolveAction failure violates the synchronous preflight invariant. ActionId=%s"), *Command.ActionId.ToString());
		UE_LOG(LogTemp, Error, TEXT("UHSRBattleCoordinator::RequestAction - FAILED post-GE turn resolve ActionId=%s; GE side effect already occurred and this branch is contractually unreachable after preflight"), *Command.ActionId.ToString());
		RollbackSkillPoints(Command.ActionId);
		return Reject(EHSRAbilityFailureReason::EffectFailed);
	}
	CommitSkillPoints(Command.ActionId);
	Resolution.Status = EHSRAbilityResolutionStatus::Succeeded;
	Resolution.FailureReason = EHSRAbilityFailureReason::None;
	if (ResolvedSkillDefinition->RestoresHealth())
	{
		Resolution.bHasHealResult = true;
		Resolution.HealAmount = FMath::Max(0.0f, Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) - HealHealthBefore);
	}
	Finalize(Resolution);
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::RequestAction - SUCCESS ActionId=%s Skill=%s Actor=%s Target=%s"), *Command.ActionId.ToString(), *Command.SkillId.ToString(), *Command.ActorParticipantId.ToString(), *Command.TargetParticipantIds[0].ToString());
	return Resolution;
}

void UHSRBattleCoordinator::CommitActionEnergyGain(const FGuid& ActionId, const UHSRSkillDefinition& ActionSkillDefinition, UAbilitySystemComponent& SourceASC)
{
	// Authored EnergyGain is the only gate: a category that historically gained no energy
	// simply leaves it at 0, so this stays behaviour-compatible while letting any skill
	// generate energy from its DataAsset alone.
	if (ActionSkillDefinition.EnergyGain <= 0.0f)
	{
		return;
	}

	const float EnergyBefore = SourceASC.GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	SourceASC.ApplyModToAttribute(UHSRCoreAttributeSet::GetEnergyAttribute(), EGameplayModOp::Additive, ActionSkillDefinition.EnergyGain);
	const float EnergyAfter = SourceASC.GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::CommitActionEnergyGain - ActionId=%s Skill=%s Requested=%.2f Energy=%.2f->%.2f"),
		*ActionId.ToString(), *ActionSkillDefinition.SkillId.ToString(), ActionSkillDefinition.EnergyGain, EnergyBefore, EnergyAfter);
}

FString UHSRBattleCoordinator::MakeEnemyTurnKey(const UHSRTurnManager* Manager, uint64 BattleEpoch, uint64 TurnSequence, FName ParticipantId) const
{
	return FString::Printf(TEXT("%p|%llu|%llu|%s"), Manager, BattleEpoch, TurnSequence, *ParticipantId.ToString());
}

void UHSRBattleCoordinator::ClearEnemyTurnAutomation()
{
	if (BoundEnemyTurnManager.IsValid() && EnemyTurnStartedHandle.IsValid())
	{
		BoundEnemyTurnManager->OnTurnStarted().Remove(EnemyTurnStartedHandle);
	}
	EnemyTurnStartedHandle.Reset();
	BoundEnemyTurnManager.Reset();
	PendingEnemyTurnKey.Reset();
	ConsumedEnemyTurnKeys.Empty();
	// A terminal result may be produced inside RequestActionCore.  Do not
	// clobber the outer dispatch depth while that caller still owns its unwind.
	if (RequestActionDispatchDepth == 0)
	{
		bDrainingEnemyTurns = false;
	}
}

void UHSRBattleCoordinator::BindEnemyTurnManager(UHSRTurnManager* InManager)
{
	ClearEnemyTurnAutomation();
	if (!InManager)
	{
		return;
	}
	BoundEnemyTurnManager = InManager;
	const TWeakObjectPtr<UHSRTurnManager> WeakManager(InManager);
	EnemyTurnStartedHandle = InManager->OnTurnStarted().AddWeakLambda(this, [this, WeakManager](const FHSRTurnLifecycleEvent& Event)
	{
		RecordEnemyTurnIfCurrent(WeakManager.Get(), Event);
	});
}

#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
bool UHSRBattleCoordinator::BeginEnemyTurnAutomationAuditForDevelopmentTest(UHSRTurnManager* IsolatedManager)
{
	if (!IsolatedManager || bEnemyTurnAutomationAuditActive || RequestActionDispatchDepth != 0 || bDrainingEnemyTurns)
	{
		return false;
	}
	SavedEnemyTurnManagerForAudit = BoundEnemyTurnManager;
	SavedPendingEnemyTurnKeyForAudit = PendingEnemyTurnKey;
	SavedConsumedEnemyTurnKeysForAudit = ConsumedEnemyTurnKeys;
	bEnemyTurnAutomationAuditActive = true;
	BindEnemyTurnManager(IsolatedManager);
	return true;
}

void UHSRBattleCoordinator::EndEnemyTurnAutomationAuditForDevelopmentTest()
{
	if (!bEnemyTurnAutomationAuditActive)
	{
		return;
	}
	const TWeakObjectPtr<UHSRTurnManager> RestoreManager = SavedEnemyTurnManagerForAudit;
	const TOptional<FString> RestorePending = SavedPendingEnemyTurnKeyForAudit;
	const TSet<FString> RestoreConsumed = SavedConsumedEnemyTurnKeysForAudit;
	SavedEnemyTurnManagerForAudit.Reset();
	SavedPendingEnemyTurnKeyForAudit.Reset();
	SavedConsumedEnemyTurnKeysForAudit.Empty();
	bEnemyTurnAutomationAuditActive = false;
	BindEnemyTurnManager(RestoreManager.Get());
	PendingEnemyTurnKey = RestorePending;
	ConsumedEnemyTurnKeys = RestoreConsumed;
}

void UHSRBattleCoordinator::InjectEnemyTurnStartedForDevelopmentTest(UHSRTurnManager* SourceManager, const FHSRTurnLifecycleEvent& Event)
{
	if (bEnemyTurnAutomationAuditActive)
	{
		RecordEnemyTurnIfCurrent(SourceManager, Event);
	}
}
#endif

void UHSRBattleCoordinator::RecordEnemyTurnIfCurrent(UHSRTurnManager* SourceManager, const FHSRTurnLifecycleEvent& Event)
{
	if (SourceManager != BoundEnemyTurnManager.Get() || CurrentState != EHSRBattleCoordinatorState::Spawned
		|| Event.EventType != EHSRTurnLifecycleEventType::TurnStarted || Event.BattleEpoch == 0 || Event.TurnSequence == 0)
	{
		return;
	}
	const FHSRBattleParticipant* Participant = FindParticipant(Event.ParticipantId);
	if (!Participant || Participant->Team != EHSRBattleParticipantTeam::Enemy || !Participant->IsAlive()
		|| SourceManager->GetCurrentParticipantId() != Event.ParticipantId
		|| SourceManager->GetBattleEpoch() != Event.BattleEpoch || SourceManager->GetTurnSequence() != Event.TurnSequence)
	{
		return;
	}
	const FString Key = MakeEnemyTurnKey(SourceManager, Event.BattleEpoch, Event.TurnSequence, Event.ParticipantId);
	if (!ConsumedEnemyTurnKeys.Contains(Key) && (!PendingEnemyTurnKey.IsSet() || PendingEnemyTurnKey.GetValue() != Key))
	{
		PendingEnemyTurnKey = Key;
	#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
		++EnemyTurnQueueCount;
	#endif
		UE_LOG(LogTemp, Log, TEXT("P10-001A EnemyTurn Queue Key=%s"), *Key);
	}
}

void UHSRBattleCoordinator::RecordCurrentEnemyTurnIfNeeded()
{
	UHSRTurnManager* Manager = BoundEnemyTurnManager.Get();
	if (!Manager || Manager != TurnManager || CurrentState != EHSRBattleCoordinatorState::Spawned)
	{
		return;
	}
	FHSRTurnLifecycleEvent Event;
	Event.EventType = EHSRTurnLifecycleEventType::TurnStarted;
	Event.BattleEpoch = Manager->GetBattleEpoch();
	Event.TurnSequence = Manager->GetTurnSequence();
	Event.ParticipantId = Manager->GetCurrentParticipantId();
	RecordEnemyTurnIfCurrent(Manager, Event);
}

void UHSRBattleCoordinator::DrainPendingEnemyTurns()
{
	if (bDrainingEnemyTurns || RequestActionDispatchDepth != 0)
	{
		return;
	}
	bDrainingEnemyTurns = true;
	while (PendingEnemyTurnKey.IsSet() && CurrentState == EHSRBattleCoordinatorState::Spawned && BoundEnemyTurnManager.Get() == TurnManager)
	{
		UHSRTurnManager* Manager = BoundEnemyTurnManager.Get();
		const FName EnemyId = Manager ? Manager->GetCurrentParticipantId() : NAME_None;
		const FString Key = Manager ? MakeEnemyTurnKey(Manager, Manager->GetBattleEpoch(), Manager->GetTurnSequence(), EnemyId) : FString();
		const FString QueuedKey = PendingEnemyTurnKey.GetValue();
		PendingEnemyTurnKey.Reset();
		if (Key != QueuedKey || ConsumedEnemyTurnKeys.Contains(QueuedKey))
		{
			continue;
		}
		ConsumedEnemyTurnKeys.Add(QueuedKey);
			// The enemy acts with the first attack in its own loadout, so changing an enemy's
			// authored skill set changes its AI behaviour with no code change.
			const UHSRSkillDefinition* EnemyAttack = nullptr;
			{
				const FHSRBattleParticipant* Actor = FindParticipant(EnemyId);
				if (Actor)
				{
					for (const TObjectPtr<UHSRSkillDefinition>& Candidate : GetSkillLoadoutFor(Actor->ParticipantId))
					{
						if (Candidate && Candidate->Category == EHSRSkillCategory::BasicAttack && Candidate->IsValidForCategory())
						{
							EnemyAttack = Candidate;
							break;
						}
					}
				}
			}
		const FHSRBattleParticipant* Enemy = FindParticipant(EnemyId);
		if (!Enemy || Enemy->Team != EHSRBattleParticipantTeam::Enemy || !Enemy->IsAlive() || !EnemyAttack)
		{
			UE_LOG(LogTemp, Warning, TEXT("P10-001A EnemyTurn ConsumedWithoutAction Key=%s"), *QueuedKey);
			continue;
		}
		TArray<FName> Targets = FHSRTargetingPolicy::BuildCandidateTargetIds(*EnemyAttack, *Enemy, Participants);
		Targets.Sort([](const FName& Left, const FName& Right) { return Left.LexicalLess(Right); });
		if (Targets.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("P10-001A EnemyTurn NoTarget Key=%s"), *QueuedKey);
			continue;
		}
		FHSRBattleActionCommand Command;
		Command.ActionId = FGuid::NewGuid();
		Command.BattleId = CurrentRequestId;
		Command.ActorParticipantId = EnemyId;
		Command.SkillId = EnemyAttack->SkillId;
		Command.TargetParticipantIds.Add(Targets[0]);
		UE_LOG(LogTemp, Log, TEXT("P10-001A EnemyTurn Dispatch Key=%s ActionId=%s"), *QueuedKey, *Command.ActionId.ToString());
	#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
		++EnemyTurnDispatchCount;
	#endif
		// This is the explicit non-public dispatch scope.  A TurnStarted raised
		// by the core can only queue; the loop observes it after this scope ends.
		++RequestActionDispatchDepth;
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
		++CoreExecutionDepth;
		MaxCoreExecutionDepth = FMath::Max(MaxCoreExecutionDepth, CoreExecutionDepth);
#endif
		const FHSRAbilityResolution EnemyResolution = RequestActionCore(Command);
	#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
		if (!EnemyResolution.Succeeded()) ++EnemyTurnRejectedCount;
	#endif
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
		check(CoreExecutionDepth > 0);
		--CoreExecutionDepth;
#endif
		check(RequestActionDispatchDepth > 0);
		--RequestActionDispatchDepth;
	}
	bDrainingEnemyTurns = false;
}

FHSRBattleCommandViewState UHSRBattleCoordinator::GetCommandViewState() const
{
	FHSRBattleCommandViewState State;
	State.BattleId = CurrentRequestId;
	State.SkillPoints = TeamResourceState.CurrentSkillPoints;
	State.MaxSkillPoints = TeamResourceState.MaxSkillPoints;
	if (TurnManager && CurrentState == EHSRBattleCoordinatorState::Spawned)
	{
		State.CurrentActorId = TurnManager->GetCurrentParticipantId();
		const TArray<FHSRBattleParticipant>& OrderedParticipants = TurnManager->GetOrderedParticipants();
		const int32 CurrentIndex = OrderedParticipants.IndexOfByPredicate([&State](const FHSRBattleParticipant& Participant)
		{
			return Participant.ParticipantId == State.CurrentActorId;
		});
		if (CurrentIndex != INDEX_NONE)
		{
			for (int32 Offset = 0; Offset < OrderedParticipants.Num(); ++Offset)
			{
				const FName ParticipantId = OrderedParticipants[(CurrentIndex + Offset) % OrderedParticipants.Num()].ParticipantId;
				const FHSRBattleParticipant* Participant = FindParticipant(ParticipantId);
				if (Participant && Participant->IsAlive())
				{
					State.TurnOrderParticipantIds.Add(ParticipantId);
				}
			}
		}
		State.TurnForecast = TurnManager->BuildTurnForecast(TurnForecastSlotCount);
	}
	for (const FHSRBattleParticipant& Participant : Participants)
	{
		if (!Participant.IsValid())
		{
			continue;
		}
		FHSRBattleParticipantView& View = State.Participants.AddDefaulted_GetRef();
		View.ParticipantId = Participant.ParticipantId;
		View.DisplayName = Participant.DisplayName;
		View.Portrait = Participant.Portrait;
		View.bPlayerTeam = Participant.Team == EHSRBattleParticipantTeam::Player;
		View.bDefeated = Participant.bDefeated;
		for (const FGameplayTag& WeaknessTag : Participant.WeaknessTags)
		{
			View.WeaknessTags.Add(WeaknessTag);
		}
		if (Participant.AbilitySystemComponent.IsValid())
		{
			View.Health = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
			View.MaxHealth = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxHealthAttribute());
			View.Energy = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
			View.MaxEnergy = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxEnergyAttribute());
			View.Toughness = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetToughnessAttribute());
			View.MaxToughness = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxToughnessAttribute());
			View.bHasAttributes = true;
		}
	}
	State.LastResolution = LastActionResolution;
	State.LastStatusOperation = LastStatusOperation;
	State.PresentationEvents = PresentationEvents;
	for (const TPair<FName, TObjectPtr<UHSRStatusComponent>>& Pair : StatusComponents)
	{
		if (Pair.Value) State.Statuses.Append(Pair.Value->GetPublicSnapshots());
	}
	State.Statuses.Sort([](const FHSRStatusPublicSnapshot& A, const FHSRStatusPublicSnapshot& B)
	{
		if (A.TargetParticipantId != B.TargetParticipantId) return A.TargetParticipantId.LexicalLess(B.TargetParticipantId);
		return A.StatusId.LexicalLess(B.StatusId);
	});
	if (!TurnManager || CurrentState != EHSRBattleCoordinatorState::Spawned)
	{
		return State;
	}

	const FHSRBattleParticipant* Actor = FindParticipant(State.CurrentActorId);
	if (!Actor || !Actor->AbilitySystemComponent.IsValid())
	{
		return State;
	}
	State.bCurrentActorPlayerControlled = Actor->Team == EHSRBattleParticipantTeam::Player;
	State.Energy = Actor->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	State.MaxEnergy = Actor->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxEnergyAttribute());
	// The current actor's own loadout drives the panel, so two party members with different
	// skills produce different command lists.  Authoring order is presentation order.
	for (const TObjectPtr<UHSRSkillDefinition>& LoadoutEntry : GetSkillLoadoutFor(State.CurrentActorId))
	{
		const UHSRSkillDefinition* Definition = LoadoutEntry;
		FHSRBattleCommandSkillView View;
		if (!Definition)
		{
			continue;
		}
		View.Category = Definition->Category;
		View.SkillId = Definition->SkillId;
		View.TargetType = Definition->TargetType;
		View.DisplayName = Definition->DisplayName.IsEmpty() ? FText::FromName(Definition->SkillId) : Definition->DisplayName;
		View.Description = Definition->Description.IsEmpty()
			? FText::Format(NSLOCTEXT("HSRCommand", "MissingAuthoredDescription", "No description authored for {0}."), FText::FromName(Definition->SkillId))
			: Definition->Description;
		View.bDescriptionIsPlaceholder = Definition->Description.IsEmpty();
		View.SkillPointCost = Definition->GetSkillPointCost();
		View.SkillPointDelta = Definition->GetSkillPointDelta();
		float DisplayEnergyCost = 0.0f;
		View.bEnergyCostIsKnown = Definition->TryGetDisplayEnergyCost(DisplayEnergyCost);
		View.EnergyCost = View.bEnergyCostIsKnown ? DisplayEnergyCost : 0.0f;
		View.CandidateTargetIds = FHSRTargetingPolicy::BuildCandidateTargetIds(*Definition, *Actor, Participants);
		View.bAvailable = Definition->IsValidDefinition() && View.CandidateTargetIds.Num() > 0;
		if (!Definition->IsValidDefinition()) View.DisabledReason = EHSRAbilityFailureReason::DefinitionMissing;
		else if (View.CandidateTargetIds.Num() == 0) View.DisabledReason = EHSRAbilityFailureReason::InvalidTarget;
		else if (Definition->RequiresSkillPointsToCommit() && TeamResourceState.CurrentSkillPoints + Definition->GetSkillPointDelta() < 0)
		{
			View.bAvailable = false;
			View.DisabledReason = EHSRAbilityFailureReason::InsufficientSkillPoint;
		}
		else
		{
			FGameplayAbilitySpec* Spec = Actor->AbilitySystemComponent->FindAbilitySpecFromClass(Definition->AbilityClass);
			UHSRGameplayAbilityBase* Ability = Spec ? Cast<UHSRGameplayAbilityBase>(Spec->GetPrimaryInstance()) : nullptr;
			const FHSRBattleParticipant* Candidate = FindParticipant(View.CandidateTargetIds[0]);
			if (!Ability || !Candidate || !Candidate->AbilitySystemComponent.IsValid())
			{
				View.bAvailable = false; View.DisabledReason = EHSRAbilityFailureReason::AbilityUnavailable;
			}
			else
			{
				View.DisabledReason = Ability->GetAvailabilityFailureReason(Spec->Handle, Actor->AbilitySystemComponent->AbilityActorInfo.Get(), Candidate->AbilitySystemComponent.Get());
				View.bAvailable = View.DisabledReason == EHSRAbilityFailureReason::None;
			}
		}
		State.Skills.Add(View);
	}
	return State;
}

void UHSRBattleCoordinator::PublishCommandViewState()
{
	CommandStateReady.Broadcast(GetCommandViewState());
}

bool UHSRBattleCoordinator::ConsumeBattleResult(FHSRBattleResult& OutResult)
{
	if (!bBattleResultProduced || bBattleResultConsumed)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::ConsumeBattleResult - REJECTED produced=%d consumed=%d"), bBattleResultProduced ? 1 : 0, bBattleResultConsumed ? 1 : 0);
		return false;
	}

	OutResult = BattleResult;
	bBattleResultConsumed = true;
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::ConsumeBattleResult - SUCCESS RequestId=%s Outcome=%d"), *OutResult.RequestId.ToString(), static_cast<int32>(OutResult.Outcome));
	return true;
}

bool UHSRBattleCoordinator::GetBattleResultForPresentation(FHSRBattleResult& OutResult) const
{
	if (!bBattleResultProduced || bBattleResultConsumed || !BattleResult.IsValid()) return false;
	OutResult = BattleResult;
	return true;
}

const FHSRBattleParticipant* UHSRBattleCoordinator::FindParticipant(FName ParticipantId) const
{
	return Participants.FindByPredicate([ParticipantId](const FHSRBattleParticipant& Value)
	{
		return Value.ParticipantId == ParticipantId;
	});
}

FHSRBattleParticipant* UHSRBattleCoordinator::FindParticipant(FName ParticipantId)
{
	return Participants.FindByPredicate([ParticipantId](const FHSRBattleParticipant& Value)
	{
		return Value.ParticipantId == ParticipantId;
	});
}

const FHSRBattleParticipant* UHSRBattleCoordinator::FindFirstOfTeam(EHSRBattleParticipantTeam Team) const
{
	return Participants.FindByPredicate([Team](const FHSRBattleParticipant& Value)
	{
		return Value.Team == Team;
	});
}

FName UHSRBattleCoordinator::MakeParticipantId(EHSRBattleParticipantTeam Team, int32 RosterIndex)
{
	// Index 0 keeps the historical unsuffixed id so single-member encounters, their save
	// projections, and the existing harnesses observe exactly the ids they did before.
	const TCHAR* Prefix = Team == EHSRBattleParticipantTeam::Player ? TEXT("Player") : TEXT("Enemy");
	return RosterIndex == 0 ? FName(Prefix) : FName(*FString::Printf(TEXT("%s%d"), Prefix, RosterIndex + 1));
}

TArray<FHSRBattleRosterEntry> UHSRBattleCoordinator::BuildEffectivePlayerRoster() const
{
	if (!PlayerRoster.IsEmpty()) return PlayerRoster;
	// Callers that only ever set the leader still get a well-formed one-entry roster.
	TArray<FHSRBattleRosterEntry> Fallback;
	if (!PlayerCharacterId.IsNone()) Fallback.Add({ PlayerCharacterId, PlayerCharacterClass });
	return Fallback;
}

TArray<FHSRBattleRosterEntry> UHSRBattleCoordinator::BuildEffectiveEnemyRoster() const
{
	if (!EnemyRoster.IsEmpty()) return EnemyRoster;
	TArray<FHSRBattleRosterEntry> Fallback;
	// The enemy shell is the native APawn: encounters author stats through EnemyDefinition
	// rather than a pawn Blueprint, so there is no per-entry class to resolve yet.
	if (!CurrentEnemyDefinitionId.IsNone()) Fallback.Add({ CurrentEnemyDefinitionId, APawn::StaticClass() });
	return Fallback;
}

void UHSRBattleCoordinator::AppendRosterDefinitions(const TArray<FHSRBattleRosterEntry>& Roster,
	EHSRBattleParticipantTeam Team, TArray<FHSRBattleParticipantDefinition>& OutDefinitions)
{
	for (int32 Index = 0; Index < Roster.Num(); ++Index)
	{
		FHSRBattleParticipantDefinition Def;
		Def.ParticipantId = MakeParticipantId(Team, Index);
		Def.DefinitionId = Roster[Index].CharacterId;
		Def.Team = Team;
		Def.PawnClass = Roster[Index].PawnClass;
		Def.DisplayName = Roster[Index].DisplayName;
		Def.Portrait = Roster[Index].Portrait;
		OutDefinitions.Add(Def);
	}
}

FHSRBattleInitResult UHSRBattleCoordinator::BuildAndValidateParticipantDefinitions()
{
	ParticipantDefinitions.Empty();

	// Both sides go through the same roster expansion so battle width is data-driven: one
	// participant per roster entry, with ParticipantIds minted per side and per index.
	const TArray<FHSRBattleRosterEntry> PlayerEntries = BuildEffectivePlayerRoster();
	if (PlayerEntries.IsEmpty())
	{
		return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::DefinitionNotFound,
			FText::FromString(TEXT("Player roster is empty.")), PlayerCharacterId);
	}
	for (const FHSRBattleRosterEntry& Entry : PlayerEntries)
	{
		if (!Entry.IsValid() || !Entry.PawnClass)
		{
			return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::DefinitionNotFound,
				FText::FromString(TEXT("Player definition is not registered.")), Entry.CharacterId);
		}
	}

	const TArray<FHSRBattleRosterEntry> EnemyEntries = BuildEffectiveEnemyRoster();
	if (EnemyEntries.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::BuildAndValidateParticipantDefinitions - DefinitionNotFound DefId=%s RequestId=%s"),
			*CurrentEnemyDefinitionId.ToString(), *CurrentRequestId.ToString());
		return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::DefinitionNotFound,
			FText::FromString(TEXT("Enemy definition is not registered.")), CurrentEnemyDefinitionId);
	}

	AppendRosterDefinitions(PlayerEntries, EHSRBattleParticipantTeam::Player, ParticipantDefinitions);
	AppendRosterDefinitions(EnemyEntries, EHSRBattleParticipantTeam::Enemy, ParticipantDefinitions);

	// Validate each definition
	for (const auto& Def : ParticipantDefinitions)
	{
		if (Def.DefinitionId.IsNone())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UHSRBattleCoordinator::BuildAndValidateParticipantDefinitions - DefinitionNotFound ParticipantId=%s"),
				*Def.ParticipantId.ToString());
			ParticipantDefinitions.Empty();
			return FHSRBattleInitResult::MakeFailure(
				EHSRBattleInitFailureType::DefinitionNotFound,
				FText::FromString(TEXT("Definition ID is empty.")),
				Def.ParticipantId);
		}

		// If a specific PawnClass is set, validate it is a valid APawn subclass
		if (Def.PawnClass != nullptr)
		{
			UClass* ResolvedClass = Def.PawnClass.Get();
			if (!ResolvedClass || !ResolvedClass->IsChildOf<APawn>())
			{
				UE_LOG(LogTemp, Warning,
					TEXT("UHSRBattleCoordinator::BuildAndValidateParticipantDefinitions - ClassLoadFailed ParticipantId=%s DefId=%s"),
					*Def.ParticipantId.ToString(), *Def.DefinitionId.ToString());
				ParticipantDefinitions.Empty();
				return FHSRBattleInitResult::MakeFailure(
					EHSRBattleInitFailureType::ClassLoadFailed,
					FText::FromString(TEXT("PawnClass is not a valid APawn subclass.")),
					Def.DefinitionId);
			}
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::BuildAndValidateParticipantDefinitions - SUCCESS Definitions=%d Players=%d Enemies=%d"),
		ParticipantDefinitions.Num(), PlayerEntries.Num(), EnemyEntries.Num());
	for (const FHSRBattleRosterEntry& Entry : PlayerEntries)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator PlayerRoster Member=%s Class=%s"),
			*Entry.CharacterId.ToString(), Entry.PawnClass ? *Entry.PawnClass->GetName() : TEXT("None"));
	}

	return FHSRBattleInitResult::MakeSuccess();
}

void UHSRBattleCoordinator::Reset()
{
	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::Reset - Clearing state. Previous state=%d RequestId=%s"),
		static_cast<int32>(CurrentState), *CurrentRequestId.ToString());

	RollbackStageBuffs(false);
	ClearRuntimeDelegates();
	ClearProgressionGameplayEffects();
	if (EquipmentEffectBridge) EquipmentEffectBridge->RemoveAll();
	EquipmentProjectionStates.Empty();
	EquipmentProjectionParticipants.Empty();
	EquipmentSetProjectionStates.Empty();
	EquipmentSetProjectionParticipants.Empty();
	CurrentState = EHSRBattleCoordinatorState::Idle;
	CurrentRequestId = FGuid();
	CurrentEncounterId = NAME_None;
	CurrentEnemyDefinitionId = NAME_None;
	CurrentStageBuffIds.Reset();
	CurrentRewardDefinitionId = NAME_None;
	CurrentRewardSeed = 0;
	ReturnContext = FHSRBattleReturnContext();
	Participants.Empty();
	ParticipantDefinitions.Empty();
	BattleResult = FHSRBattleResult();
	bBattleResultProduced = false;
	bBattleResultConsumed = false;
	ProcessedActionResolutions.Empty();
	DevelopmentDamageResults.Empty();
	DevelopmentDamageRandomStream.Initialize(DevelopmentDamageSeed);
	DevelopmentDamageConsumeCount = 0;
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	ClearDamageTestInjection();
	LastDevelopmentFormalExecutionResult = FHSRFormalDamageExecutionResult();
#endif
	LastActionResolution = FHSRAbilityResolution();
	PresentationEvents.Empty();
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	PublicRequestActionDepth = 0;
	MaxPublicRequestActionDepth = 0;
	CoreExecutionDepth = 0;
	MaxCoreExecutionDepth = 0;
	EnemyTurnQueueCount = 0;
	EnemyTurnDispatchCount = 0;
	EnemyTurnRejectedCount = 0;
#endif
	SkillPointReservations.Empty();
	TeamResourceState = FHSRTeamResourceState();
	bLastBreakDelayRegistered = false;
	LastBreakDelayActionId.Invalidate();
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	BreakStatusRequestCountForTest = 0;
	BreakDelayRegistrationCountForTest = 0;
	LastBreakStatusResultForTest = EHSRStatusOperationResult::UnknownStatus;
	bLastBreakDelayAcceptedForTest = false;
#endif
	if (TurnManager)
	{
		TurnManager->Reset();
		TurnManager = nullptr;
	}
	PublishCommandViewState();
}

FHSRDamageResult UHSRBattleCoordinator::ResolveStatusDamage(FName SourceParticipantId, FName TargetParticipantId, const FGuid& ActionId, const UHSRStatusDefinition* Definition)
{
	FHSRDamageResult Failure;
	Failure.ActionId = ActionId;
	Failure.DamageType = Definition ? Definition->DamageType : FGameplayTag();
	if (!Definition || Definition->EffectKind != EHSRStatusEffectKind::DamageOverTime)
	{
		Failure.Result = EHSRDamageResultType::MissingDamageRule;
		return Failure;
	}
	const FHSRBattleParticipant* Source = FindParticipant(SourceParticipantId);
	const FHSRBattleParticipant* Target = FindParticipant(TargetParticipantId);
	const UHSRDamageRuleDefinition* Rule = Definition->DamageRule.LoadSynchronous();
	if (CurrentState != EHSRBattleCoordinatorState::Spawned)
	{
		Failure.Result = EHSRDamageResultType::BattleTerminal;
		return Failure;
	}
	if (!ActionId.IsValid())
	{
		Failure.Result = EHSRDamageResultType::DuplicateAction;
		return Failure;
	}
	if (!Source || !Source->AbilitySystemComponent.IsValid())
	{
		Failure.Result = EHSRDamageResultType::InvalidSource;
		return Failure;
	}
	if (!Target || !Target->AbilitySystemComponent.IsValid())
	{
		Failure.Result = EHSRDamageResultType::InvalidTarget;
		return Failure;
	}
	if (!Rule || !Rule->IsValidRuleDefinition())
	{
		Failure.Result = EHSRDamageResultType::MissingDamageRule;
		return Failure;
	}
	const FHSRDamageSetByCallerTags DamageTags;
	if (!Definition->DamageType.IsValid() || !DamageTags.IsValid())
	{
		Failure.Result = EHSRDamageResultType::InvalidDamageType;
		return Failure;
	}
	FGameplayEffectContextHandle Context = Source->AbilitySystemComponent->MakeEffectContext();
	FHSRDamageEffectContext* DamageContext = static_cast<FHSRDamageEffectContext*>(Context.Get());
	if (!DamageContext || DamageContext->GetScriptStruct() != FHSRDamageEffectContext::StaticStruct())
	{
		Failure.Result = EHSRDamageResultType::SpecCreationFailed;
		return Failure;
	}
	DamageContext->DamageContext.ActionId = ActionId;
	DamageContext->DamageContext.DamageType = Definition->DamageType;
	DamageContext->DamageContext.AbilityMultiplier = Definition->DamageAbilityMultiplier;
	DamageContext->DamageContext.CritRoll = 0.0f;
	DamageContext->DefenseCoefficient = Rule->DefenseCoefficient;
	DamageContext->MinDamage = Rule->MinDamage;
	const TSubclassOf<UGameplayEffect> DamageClass = Definition->DamageGameplayEffectClass.LoadSynchronous();
	if (!DamageClass)
	{
		Failure.Result = EHSRDamageResultType::SpecCreationFailed;
		return Failure;
	}
	FGameplayEffectSpecHandle Spec = Source->AbilitySystemComponent->MakeOutgoingSpec(DamageClass, 1.0f, Context);
	if (!Spec.IsValid())
	{
		Failure.Result = EHSRDamageResultType::SpecCreationFailed;
		return Failure;
	}
	// 在 Apply 前记下 RNG 流，任何失败都需要回滚（不消耗暴击骰）。
	const FRandomStream RandomStreamBeforeApply = DevelopmentDamageRandomStream;
	DamageContext->DamageContext.CritRoll = DevelopmentDamageRandomStream.GetFraction();
	DamageTags.ApplyTo(*Spec.Data, Definition->DamageAbilityMultiplier, Rule->DefenseCoefficient,
		Rule->MinDamage, DamageContext->DamageContext.CritRoll);
	const float HealthBefore = Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
	// 状态伤害也是一次正式伤害事务：死亡延迟到事务结束。
	bFormalDamageTransactionOpen = true;
	PendingDefeatedParticipantId = NAME_None;
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	if (bForceStatusDamageApplyFailure)
	{
		DevelopmentDamageRandomStream = RandomStreamBeforeApply;
		bFormalDamageTransactionOpen = false;
		Failure.Result = EHSRDamageResultType::EffectApplicationFailed;
		return Failure;
	}
#endif
	const FActiveGameplayEffectHandle Applied = Source->AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Target->AbilitySystemComponent.Get());
	if (!Applied.WasSuccessfullyApplied())
	{
		DevelopmentDamageRandomStream = RandomStreamBeforeApply;
		bFormalDamageTransactionOpen = false;
		PendingDefeatedParticipantId = NAME_None;
		Failure.Result = EHSRDamageResultType::EffectApplicationFailed;
		return Failure;
	}
	FHSRDamageResult Result = DamageContext->DamageResult;
	Result.ActionId = ActionId;
	Result.DamageType = Definition->DamageType;
	Result.Breakdown.AppliedDamage = FMath::Max(0.0f, HealthBefore - Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()));
	if (Result.Result != EHSRDamageResultType::DamageResolved)
	{
		DevelopmentDamageRandomStream = RandomStreamBeforeApply;
		bFormalDamageTransactionOpen = false;
		PendingDefeatedParticipantId = NAME_None;
	}
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	else
	{
		++StatusDamageCommitCount;
	}
#endif
	return Result;
}

// 状态伤害事务收尾：若事务期间产生了延迟死亡，则在此结算。
void UHSRBattleCoordinator::FinalizeStatusDamage()
{
	if (!PendingDefeatedParticipantId.IsNone())
	{
		const FName Defeated = PendingDefeatedParticipantId;
		PendingDefeatedParticipantId = NAME_None;
		bFormalDamageTransactionOpen = false;
		ResolveDefeat(Defeated);
	}
	else
	{
		bFormalDamageTransactionOpen = false;
	}
}
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
FHSRBattleInitResult UHSRBattleCoordinator::ResetAndRebuildForDevelopmentTest(UWorld* BattleWorld)
{
	if (!BattleWorld || !LastSubmittedRequestForDevelopment.IsSet())
	{
		return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::InitFailed, FText::FromString(TEXT("Missing saved request or BattleWorld.")));
	}
	FHSREncounterRequest SavedRequest = LastSubmittedRequestForDevelopment.GetValue();
	SavedRequest.RequestId = FGuid::NewGuid();
	Reset();
	if (!SubmitBattleRequest(SavedRequest))
	{
		return FHSRBattleInitResult::MakeFailure(EHSRBattleInitFailureType::InitFailed, FText::FromString(TEXT("Formal resubmit failed after Reset.")));
	}
	return BuildParticipants(BattleWorld);
}
void UHSRBattleCoordinator::InitializeDevelopmentDamageRng(int32 InSeed)
{
	DevelopmentDamageSeed = InSeed;
	DevelopmentDamageRandomStream.Initialize(DevelopmentDamageSeed);
	DevelopmentDamageConsumeCount = 0;
	DevelopmentDamageResults.Empty();
}

// 开发测试专用伤害路径：直接调用伤害执行，不经过完整指令管线（P7-002 用）。
FHSRDamageResult UHSRBattleCoordinator::ResolveDevelopmentExecutionDamage(FName SourceParticipantId, FName TargetParticipantId, const FGuid& ActionId, const FGameplayTag& DamageType, float AbilityMultiplier, const UHSRDamageRuleDefinition* Rule, TSubclassOf<UGameplayEffect> DamageEffectClass)
{
	FHSRDamageResult Failure;
	Failure.ActionId = ActionId;
	Failure.DamageType = DamageType;
	// 幂等缓存：同一 ActionId 直接返回缓存。
	if (const FHSRDamageResult* Existing = DevelopmentDamageResults.Find(ActionId))
	{
		return *Existing;
	}
	// 失败也缓存（仅当 ActionId 有效），保证重放行为一致。
	const auto CacheFailure = [this, &ActionId](FHSRDamageResult Result)
	{
		if (ActionId.IsValid())
		{
			DevelopmentDamageResults.Add(ActionId, Result);
		}
		return Result;
	};
	if (CurrentState != EHSRBattleCoordinatorState::Spawned)
	{
		Failure.Result = EHSRDamageResultType::BattleTerminal;
		return CacheFailure(Failure);
	}
	if (!ActionId.IsValid())
	{
		Failure.Result = EHSRDamageResultType::DuplicateAction;
		return Failure;
	}
	const FHSRBattleParticipant* Source = FindParticipant(SourceParticipantId);
	if (!Source || !Source->AbilitySystemComponent.IsValid())
	{
		Failure.Result = EHSRDamageResultType::InvalidSource;
		return CacheFailure(Failure);
	}
	const FHSRBattleParticipant* Target = FindParticipant(TargetParticipantId);
	if (!Target || !Target->AbilitySystemComponent.IsValid())
	{
		Failure.Result = EHSRDamageResultType::InvalidTarget;
		return CacheFailure(Failure);
	}
	if (!Rule || !Rule->IsValidRuleDefinition())
	{
		Failure.Result = EHSRDamageResultType::MissingDamageRule;
		return CacheFailure(Failure);
	}
	if (!DamageType.IsValid() || !FMath::IsFinite(AbilityMultiplier) || AbilityMultiplier <= 0.0f || AbilityMultiplier > 100.0f)
	{
		Failure.Result = EHSRDamageResultType::InvalidDamageType;
		return CacheFailure(Failure);
	}
	if (!DamageEffectClass)
	{
		Failure.Result = EHSRDamageResultType::SpecCreationFailed;
		return CacheFailure(Failure);
	}
	const FHSRDamageSetByCallerTags DamageTags;
	if (!DamageTags.IsValid())
	{
		Failure.Result = EHSRDamageResultType::SpecCreationFailed;
		return CacheFailure(Failure);
	}
	FGameplayEffectContextHandle ContextHandle = Source->AbilitySystemComponent->MakeEffectContext();
	FHSRDamageEffectContext* DamageContext = static_cast<FHSRDamageEffectContext*>(ContextHandle.Get());
	if (!DamageContext || DamageContext->GetScriptStruct() != FHSRDamageEffectContext::StaticStruct())
	{
		Failure.Result = EHSRDamageResultType::SpecCreationFailed;
		return CacheFailure(Failure);
	}
	DamageContext->DamageContext.ActionId = ActionId;
	DamageContext->DamageContext.DamageType = DamageType;
	DamageContext->DamageContext.AbilityMultiplier = AbilityMultiplier;
	// 在消耗 RNG 之前先构造好 Spec：以上所有失败分支都无副作用。
	DamageContext->DamageContext.CritRoll = 0.0f;
	DamageContext->DefenseCoefficient = Rule->DefenseCoefficient;
	DamageContext->MinDamage = Rule->MinDamage;
	FGameplayEffectSpecHandle Spec = Source->AbilitySystemComponent->MakeOutgoingSpec(DamageEffectClass, 1.0f, ContextHandle);
	if (!Spec.IsValid())
	{
		Failure.Result = EHSRDamageResultType::SpecCreationFailed;
		return CacheFailure(Failure);
	}
	// 唯一的 RNG 消耗点：Spec 有效且紧跟着 Apply。
	DamageContext->DamageContext.CritRoll = DevelopmentDamageRandomStream.GetFraction();
	++DevelopmentDamageConsumeCount;
	DamageTags.ApplyTo(*Spec.Data, AbilityMultiplier, Rule->DefenseCoefficient,
		Rule->MinDamage, DamageContext->DamageContext.CritRoll);
	const float HealthBefore = Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
	const FActiveGameplayEffectHandle Applied = Source->AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Target->AbilitySystemComponent.Get());
	if (!Applied.WasSuccessfullyApplied())
	{
		Failure.Result = EHSRDamageResultType::EffectApplicationFailed;
		return CacheFailure(Failure);
	}
	FHSRDamageResult Result = DamageContext->DamageResult;
	const float HealthAfter = Target->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
	Result.Breakdown.AppliedDamage = FMath::Max(0.0f, HealthBefore - HealthAfter);
	DevelopmentDamageResults.Add(ActionId, Result);
	UE_LOG(LogTemp, Log, TEXT("P7-002 Damage Result=%d ActionId=%s Seed=%d ConsumeIndex=%d Raw=%f Final=%f Applied=%f"), static_cast<int32>(Result.Result), *ActionId.ToString(), DevelopmentDamageSeed, DevelopmentDamageConsumeCount, Result.Breakdown.RawDamage, Result.Breakdown.FinalDamage, Result.Breakdown.AppliedDamage);
	return Result;
}
#endif

void UHSRBattleCoordinator::BindHealthObserver(const FHSRBattleParticipant& Participant)
{
	if (!Participant.AbilitySystemComponent.IsValid())
	{
		return;
	}

	FDelegateHandle& Handle = HealthChangedHandles.FindOrAdd(Participant.ParticipantId);
	Handle = Participant.AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetHealthAttribute()).AddUObject(
		this, &UHSRBattleCoordinator::HandleHealthChanged, Participant.ParticipantId);
}

void UHSRBattleCoordinator::HandleHealthChanged(const FOnAttributeChangeData& ChangeData, FName ParticipantId)
{
	if (ChangeData.NewValue <= 0.0f)
	{
		if (bFormalDamageTransactionOpen)
		{
			PendingDefeatedParticipantId = ParticipantId;
			return;
		}
		ResolveDefeat(ParticipantId);
	}
}

bool UHSRBattleCoordinator::IsTeamWiped(EHSRBattleParticipantTeam Team) const
{
	bool bFoundMember = false;
	for (const FHSRBattleParticipant& Participant : Participants)
	{
		if (Participant.Team != Team) continue;
		bFoundMember = true;
		// An ASC that has gone away counts as down, so a destroyed pawn cannot keep a wiped
		// team nominally alive.
		if (Participant.IsAlive())
		{
			return false;
		}
	}
	return bFoundMember;
}

void UHSRBattleCoordinator::ResolveDefeat(FName DefeatedParticipantId)
{
	if (bBattleResultProduced || CurrentState != EHSRBattleCoordinatorState::Spawned)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::ResolveDefeat - REJECTED duplicate/inactive Defeated=%s"), *DefeatedParticipantId.ToString());
		return;
	}

	FHSRBattleParticipant* Defeated = FindParticipant(DefeatedParticipantId);
	if (!Defeated)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::ResolveDefeat - REJECTED unknown participant=%s"), *DefeatedParticipantId.ToString());
		return;
	}

	Defeated->bDefeated = true;
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	LastSourceInvalidRemovedCount = RouteSourceInvalid(DefeatedParticipantId);
#else
	RouteSourceInvalid(DefeatedParticipantId);
#endif
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	++DefeatCount;
#endif
	if (UHSRStatusComponent* StatusComponent = GetStatusComponent(DefeatedParticipantId))
	{
		StatusComponent->ClearStatus();
	}
	// A single casualty no longer ends the battle: only a full team wipe does.  The downed
	// participant leaves the turn rotation so the survivors keep acting.
	// No explicit turn-order removal is needed: UHSRTurnManager::IsParticipantTurnEligible
	// already gates on Health > 0, so a downed member is skipped by AdvanceToNextValidTurn.
	const EHSRBattleParticipantTeam DefeatedTeam = Defeated->Team;
	if (!IsTeamWiped(DefeatedTeam))
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::ResolveDefeat - PARTIAL Defeated=%s SurvivorsRemain=1"), *DefeatedParticipantId.ToString());
		ParticipantDefeated.Broadcast(DefeatedParticipantId);
		PublishCommandViewState();
		return;
	}

	BattleResult.RequestId = CurrentRequestId;
	BattleResult.DefeatedParticipantId = DefeatedParticipantId;
	BattleResult.EncounterId = CurrentEncounterId;
	BattleResult.RewardDefinitionId = CurrentRewardDefinitionId;
	BattleResult.ReturnContext = ReturnContext;
	BattleResult.Outcome = DefeatedTeam == EHSRBattleParticipantTeam::Enemy ? EHSRBattleOutcome::PlayerVictory : EHSRBattleOutcome::PlayerDefeat;
	bBattleResultProduced = true;
	CurrentState = EHSRBattleCoordinatorState::Finished;
	ClearEnemyTurnAutomation();
	if (TurnManager)
	{
		TurnManager->FinishBattle();
	}
	ClearStatusComponents();

	UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::ResolveDefeat - SUCCESS RequestId=%s Defeated=%s Outcome=%d"), *BattleResult.RequestId.ToString(), *DefeatedParticipantId.ToString(), static_cast<int32>(BattleResult.Outcome));
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	++BattleResultBroadcastCount;
#endif
	BattleResultReady.Broadcast(BattleResult);
}

void UHSRBattleCoordinator::ClearRuntimeDelegates()
{
	ClearEnemyTurnAutomation();
	ClearStatusComponents();
	for (const FHSRBattleParticipant& Participant : Participants)
	{
		if (Participant.AbilitySystemComponent.IsValid())
		{
			if (const FDelegateHandle* Handle = HealthChangedHandles.Find(Participant.ParticipantId))
			{
				Participant.AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UHSRCoreAttributeSet::GetHealthAttribute()).Remove(*Handle);
			}
		}
	}
	HealthChangedHandles.Empty();
}

AActor* UHSRBattleCoordinator::SpawnParticipantActor(UWorld* World, const FHSRBattleParticipantDefinition& Definition)
{
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::SpawnParticipantActor - World is null"));
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APawn* Pawn = World->SpawnActor<APawn>(Definition.PawnClass ? Definition.PawnClass.Get() : APawn::StaticClass(), FTransform::Identity, Params);
	if (!Pawn)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::SpawnParticipantActor - FAILED to spawn APawn for team=%d"),
			static_cast<int32>(Definition.Team));
		return nullptr;
	}

	if (AActor* SpawnedActor = Cast<AActor>(Pawn))
	{
#if WITH_EDITOR
		SpawnedActor->SetActorLabel(Definition.Team == EHSRBattleParticipantTeam::Player ? TEXT("BattlePlayerPawn") : TEXT("BattleEnemyPawn"));
#endif
	}

	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::SpawnParticipantActor - SUCCESS Actor=%s Team=%d DefId=%s"),
		*Pawn->GetName(), static_cast<int32>(Definition.Team), *Definition.DefinitionId.ToString());

	return Pawn;
}

bool UHSRBattleCoordinator::InitParticipantASC(AActor* TargetActor)
{
	if (!TargetActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::InitParticipantASC - TargetActor is null"));
		return false;
	}

	// A Character already owns its ASC. The minimal APawn shell gets one only when absent.
	UAbilitySystemComponent* ASC = TargetActor->FindComponentByClass<UAbilitySystemComponent>();
	const bool bCreatedASCForThisCall = ASC == nullptr;
	if (ASC == nullptr)
	{
		ASC = Cast<UAbilitySystemComponent>(TargetActor->AddComponentByClass(UHSRAbilitySystemComponent::StaticClass(), false, FTransform::Identity, false));
	}

	if (!ASC)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::InitParticipantASC - Failed to add ASC component to Actor=%s"),
			*TargetActor->GetName());
		return false;
	}

	// Configure ASC: single-player, no tick
	ASC->SetIsReplicated(false);
	ASC->SetComponentTickEnabled(false);
	// ASC->RegisterComponent() ?? auto-registered by AddComponentByClass

	// Reuse the Character-owned set. Do not create a second AttributeSet on the same ASC.
	const UAttributeSet* AttrSet = ASC->GetSet<UHSRCoreAttributeSet>();
	if (AttrSet == nullptr)
	{
		AttrSet = ASC->InitStats(UHSRCoreAttributeSet::StaticClass(), nullptr);
	}
	if (!AttrSet)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::InitParticipantASC - InitStats failed for Actor=%s"),
			*TargetActor->GetName());
		if (bCreatedASCForThisCall) ASC->DestroyComponent(true);
		return false;
	}

	// InitAbilityActorInfo with Owner=Avatar=self
	ASC->InitAbilityActorInfo(TargetActor, TargetActor);

	if (!ASC->AbilityActorInfo.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::InitParticipantASC - ActorInfo invalid after Init for Actor=%s"),
			*TargetActor->GetName());
		if (bCreatedASCForThisCall) ASC->DestroyComponent(true);
		return false;
	}

	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::InitParticipantASC - SUCCESS Actor=%s ASC=%s ActorInfo valid, Owner=Avatar=self"),
		*TargetActor->GetName(), *ASC->GetName());

	return true;
}

// 为每个参与者创建 UHSRStatusComponent，绑定回合管理器与协调器，并订阅状态变化。
bool UHSRBattleCoordinator::InitializeStatusComponents()
{
	ClearStatusComponents();
	if (!TurnManager)
	{
		return false;
	}
	for (const FHSRBattleParticipant& Participant : Participants)
	{
		if (!Participant.Actor.IsValid() || !Participant.AbilitySystemComponent.IsValid())
		{
			return false;
		}
		UHSRStatusComponent* Component = NewObject<UHSRStatusComponent>(Participant.Actor.Get());
		if (!Component
			|| !Component->InitializeStatusRuntime(Participant.ParticipantId, Participant.AbilitySystemComponent.Get())
			|| !Component->BindTurnManager(TurnManager))
		{
			return false;
		}
		Component->BindBattleCoordinator(this);
		Component->RegisterComponent();
		StatusComponents.Add(Participant.ParticipantId, Component);
		StatusChangedHandles.Add(Participant.ParticipantId,
			Component->OnStatusChanged().AddUObject(this, &UHSRBattleCoordinator::HandleStatusChanged, Participant.ParticipantId));
	}
	return true;
}

// 清理所有状态组件：解绑委托、清状态、解绑回合管理器、销毁组件。
void UHSRBattleCoordinator::ClearStatusComponents()
{
	for (TPair<FName, TObjectPtr<UHSRStatusComponent>>& Pair : StatusComponents)
	{
		if (Pair.Value)
		{
			if (const FDelegateHandle* Handle = StatusChangedHandles.Find(Pair.Key))
			{
				Pair.Value->OnStatusChanged().Remove(*Handle);
			}
			Pair.Value->ClearStatus();
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
			LastClearedStatusSnapshots.Add(Pair.Key, Pair.Value->GetSnapshot());
#endif
			Pair.Value->UnbindTurnManager();
			Pair.Value->DestroyComponent();
		}
	}
	StatusComponents.Empty();
	StatusChangedHandles.Empty();
	PublishCommandViewState();
}

// 状态组件变化时记录最近一次公开操作并广播视图状态。
void UHSRBattleCoordinator::HandleStatusChanged(FName ParticipantId)
{
	if (const UHSRStatusComponent* Component = GetStatusComponent(ParticipantId))
	{
		LastStatusOperation = Component->GetLastPublicOperation();
	}
	PublishCommandViewState();
}

UHSRStatusComponent* UHSRBattleCoordinator::GetStatusComponent(FName ParticipantId) const
{
	const TObjectPtr<UHSRStatusComponent>* Found = StatusComponents.Find(ParticipantId);
	return Found ? Found->Get() : nullptr;
}

EHSRStatusOperationResult UHSRBattleCoordinator::RequestBreakStatus(FName SourceParticipantId, FName TargetParticipantId, const FGuid& OperationId, bool bAllowPendingDeferredDefeat)
{
	if (!BreakStatusDefinition) return EHSRStatusOperationResult::InvalidDefinition;
	UHSRStatusComponent* Component = GetStatusComponent(TargetParticipantId);
	return Component ? Component->AddOrRefreshStatus(BreakStatusDefinition, SourceParticipantId, TargetParticipantId, OperationId, bAllowPendingDeferredDefeat)
		: EHSRStatusOperationResult::InvalidTarget;
}

int32 UHSRBattleCoordinator::RouteSourceInvalid(FName SourceParticipantId)
{
	int32 Removed = 0;
	for (TPair<FName, TObjectPtr<UHSRStatusComponent>>& Pair : StatusComponents)
	{
		if (Pair.Value) Removed += Pair.Value->HandleSourceInvalid(SourceParticipantId);
	}
	return Removed;
}

#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
EHSRStatusOperationResult UHSRBattleCoordinator::AddStatusForDevelopmentTest(FName SourceParticipantId, FName TargetParticipantId)
{
	const FHSRBattleParticipant* Source = FindParticipant(SourceParticipantId);
	if (!Source || !Source->IsAlive())
	{
		return EHSRStatusOperationResult::InvalidSource;
	}
	UHSRStatusComponent* Component = GetStatusComponent(TargetParticipantId);
	if (!StatusDefinition) return EHSRStatusOperationResult::InvalidDefinition;
	return Component ? Component->AddOrRefreshStatus(StatusDefinition, SourceParticipantId, TargetParticipantId) : EHSRStatusOperationResult::InvalidTarget;
}

EHSRStatusOperationResult UHSRBattleCoordinator::AddDamageOverTimeForDevelopmentTest(FName SourceParticipantId, FName TargetParticipantId, FGuid OperationId)
{
	const FHSRBattleParticipant* Source = FindParticipant(SourceParticipantId);
	if (!Source || !Source->IsAlive())
	{
		return EHSRStatusOperationResult::InvalidSource;
	}
	UHSRStatusComponent* Component = GetStatusComponent(TargetParticipantId);
	if (!DamageOverTimeStatusDefinition) return EHSRStatusOperationResult::InvalidDefinition;
	return Component ? Component->AddOrRefreshStatus(DamageOverTimeStatusDefinition, SourceParticipantId, TargetParticipantId, OperationId) : EHSRStatusOperationResult::InvalidTarget;
}

void UHSRBattleCoordinator::ApplyAuthoredSkillStatuses(const UHSRSkillDefinition& ActionSkillDefinition, FName SourceParticipantId, const TArray<FName>& TargetParticipantIds)
{
	if (ActionSkillDefinition.AppliedStatuses.IsEmpty())
	{
		return;
	}

	for (const TSoftObjectPtr<UHSRStatusDefinition>& AuthoredStatus : ActionSkillDefinition.AppliedStatuses)
	{
		const UHSRStatusDefinition* Definition = AuthoredStatus.LoadSynchronous();
		if (!Definition)
		{
			UE_LOG(LogTemp, Warning, TEXT("Skill %s authored an unloadable status entry; skipping it rather than failing the action."),
				*ActionSkillDefinition.SkillId.ToString());
			continue;
		}

		for (const FName TargetParticipantId : TargetParticipantIds)
		{
			UHSRStatusComponent* Component = GetStatusComponent(TargetParticipantId);
			if (!Component)
			{
				continue;
			}

			// Status application is deliberately advisory: a refused status (immune, capped)
			// never rewrites the already-committed damage transaction above.
			const EHSRStatusOperationResult Result = Component->AddOrRefreshStatus(Definition, SourceParticipantId, TargetParticipantId, FGuid::NewGuid());
			UE_LOG(LogTemp, Log, TEXT("Skill status apply Skill=%s Status=%s Target=%s Result=%d"),
				*ActionSkillDefinition.SkillId.ToString(), *Definition->StatusId.ToString(), *TargetParticipantId.ToString(), static_cast<int32>(Result));
		}
	}
}

EHSRStatusOperationResult UHSRBattleCoordinator::AddSpecificStatusForDevelopmentTest(const UHSRStatusDefinition* Definition, FName SourceParticipantId, FName TargetParticipantId, FGuid OperationId)
{
	UHSRStatusComponent* Component = GetStatusComponent(TargetParticipantId);
	return Component ? Component->AddOrRefreshStatus(Definition, SourceParticipantId, TargetParticipantId, OperationId) : EHSRStatusOperationResult::InvalidTarget;
}

EHSRStatusOperationResult UHSRBattleCoordinator::DispelOneStatusForDevelopmentTest(FName TargetParticipantId)
{
	UHSRStatusComponent* Component = GetStatusComponent(TargetParticipantId);
	return Component ? Component->DispelOneStatus() : EHSRStatusOperationResult::InvalidTarget;
}

void UHSRBattleCoordinator::SetStatusApplyFailureForDevelopmentTest(bool bForce)
{
	for (TPair<FName, TObjectPtr<UHSRStatusComponent>>& Pair : StatusComponents)
	{
		if (Pair.Value) Pair.Value->SetForceApplyFailureForDevelopmentTest(bForce);
	}
}

void UHSRBattleCoordinator::SetDispelRemoveFailureForDevelopmentTest(bool bForce)
{
	for (TPair<FName, TObjectPtr<UHSRStatusComponent>>& Pair : StatusComponents)
	{
		if (Pair.Value) Pair.Value->SetForceDispelRemoveFailureForDevelopmentTest(bForce);
	}
}

FHSRStatusRuntimeSnapshot UHSRBattleCoordinator::GetStatusSnapshotForDevelopmentTest(FName ParticipantId, FName StatusId) const
{
	if (const UHSRStatusComponent* Component = GetStatusComponent(ParticipantId)) return Component->GetSnapshot(StatusId);
	FHSRStatusRuntimeSnapshot Snapshot;
	Snapshot.Result = EHSRStatusOperationResult::InvalidTarget;
	return Snapshot;
}
#endif

bool UHSRBattleCoordinator::ApplyParticipantInitializationGameplayEffect(const FHSRBattleParticipant& Participant)
{
	if (const AHSRCharacterBase* Character = Cast<AHSRCharacterBase>(Participant.Actor.Get()); Character && Character->HasAppliedInitialAttributes())
	{
		// Character BeginPlay owns the one-shot base layer; Battle owns only progression.
		return ApplyCharacterProgressionGameplayEffect(Participant);
	}
	if (!Participant.AbilitySystemComponent.IsValid() || !ParticipantInitializationGameplayEffect)
	{
		UE_LOG(LogTemp, Error, TEXT("P8-005 InitGE Participant=%s Result=FAILED Reason=MissingASCOrEffect"), *Participant.ParticipantId.ToString());
		return false;
	}
	const FGameplayEffectSpecHandle Spec = Participant.AbilitySystemComponent->MakeOutgoingSpec(ParticipantInitializationGameplayEffect, 1.0f, Participant.AbilitySystemComponent->MakeEffectContext());
	if (!Spec.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("P8-005 InitGE Participant=%s Result=FAILED Reason=MakeSpec"), *Participant.ParticipantId.ToString());
		return false;
	}
	const FActiveGameplayEffectHandle Applied = Participant.AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (!Applied.WasSuccessfullyApplied())
	{
		UE_LOG(LogTemp, Error, TEXT("P8-005 InitGE Participant=%s Result=FAILED Reason=Apply"), *Participant.ParticipantId.ToString());
		return false;
	}
	const float Health = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
	const float MaxHealth = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxHealthAttribute());
	const float Energy = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
	const float MaxEnergy = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxEnergyAttribute());
	const float Speed = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetSpeedAttribute());
	const float Toughness = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetToughnessAttribute());
	const float MaxToughness = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxToughnessAttribute());
	const bool bValid = FMath::IsFinite(Health) && FMath::IsFinite(MaxHealth) && FMath::IsFinite(Energy) && FMath::IsFinite(MaxEnergy) && FMath::IsFinite(Speed) && FMath::IsFinite(Toughness) && FMath::IsFinite(MaxToughness)
		&& Health > 0.0f && MaxHealth > 0.0f && Health <= MaxHealth && Speed > 0.0f && Energy >= 0.0f && MaxEnergy >= 0.0f && Energy <= MaxEnergy && Toughness >= 0.0f && MaxToughness >= 0.0f && Toughness <= MaxToughness;
	if (bValid)
	{
		UE_LOG(LogTemp, Log, TEXT("P8-005 InitGE Participant=%s Result=SUCCESS Health=%.2f MaxHealth=%.2f Energy=%.2f MaxEnergy=%.2f Speed=%.2f Toughness=%.2f MaxToughness=%.2f"), *Participant.ParticipantId.ToString(), Health, MaxHealth, Energy, MaxEnergy, Speed, Toughness, MaxToughness);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("P8-005 InitGE Participant=%s Result=FAILED Health=%.2f MaxHealth=%.2f Energy=%.2f MaxEnergy=%.2f Speed=%.2f Toughness=%.2f MaxToughness=%.2f"), *Participant.ParticipantId.ToString(), Health, MaxHealth, Energy, MaxEnergy, Speed, Toughness, MaxToughness);
	}
	if (!bValid)
	{
		return false;
	}
	return ApplyCharacterProgressionGameplayEffect(Participant);
}

bool UHSRBattleCoordinator::ApplyCharacterProgressionGameplayEffect(const FHSRBattleParticipant& Participant)
{
	if (Participant.Team == EHSRBattleParticipantTeam::Enemy) return true;
	UAbilitySystemComponent* ASC = Participant.AbilitySystemComponent.Get();
	if (ASC == nullptr || Participant.ParticipantId.IsNone() || !CharacterProgressionGameplayEffect)
	{
		UE_LOG(LogTemp, Error, TEXT("P11-003 ProgressionGE Participant=%s Result=FAILED Reason=MissingASCOrEffect"), *Participant.ParticipantId.ToString());
		return false;
	}
	const UGameplayEffect* CDO = CharacterProgressionGameplayEffect->GetDefaultObject<UGameplayEffect>();
	if (CDO == nullptr || CDO->DurationPolicy != EGameplayEffectDurationType::Infinite)
	{
		UE_LOG(LogTemp, Error, TEXT("P11-003 ProgressionGE Participant=%s Result=FAILED Reason=EffectMustBeInfinite"), *Participant.ParticipantId.ToString());
		return false;
	}
	if (!ValidateCharacterProgressionEffectContract(CDO))
	{
		UE_LOG(LogTemp, Error, TEXT("P11-003 ProgressionGE Participant=%s Result=FAILED Reason=ModifierContract"), *Participant.ParticipantId.ToString()); return false;
	}
	const FHSRCharacterProgressionContext* Configured = CharacterProgressionContexts.Find(Participant.ParticipantId);
	if (!Configured) { UE_LOG(LogTemp, Error, TEXT("P11-003 ProgressionGE Participant=%s Result=FAILED Reason=MissingPlayerContext"), *Participant.ParticipantId.ToString()); return false; }
	const FHSRCharacterProgressionContext Context = *Configured;
	if (Context.CharacterId.IsNone() || Context.RuntimeRevision < 0) return false;
	const float Bonuses[] = {Context.ProgressionBonuses.MaxHealth, Context.ProgressionBonuses.Attack, Context.ProgressionBonuses.Defense, Context.ProgressionBonuses.Speed};
	for (const float Bonus : Bonuses) if (!FMath::IsFinite(Bonus) || Bonus < 0.0f) return false;
	if (FHSRProgressionEffectState* Existing = ProgressionEffects.Find(Participant.ParticipantId))
	{
		// Secondary handles are retained rollback obligations. Clear them before any idempotent return or replacement.
		UAbilitySystemComponent* ExistingASC = Existing->AbilitySystemComponent.Get();
		if (ExistingASC == nullptr)
		{
			// The recorded ASC was torn down with its actor; no live effect can remain owned by it.
			ProgressionEffects.Remove(Participant.ParticipantId);
		}
		else if (ExistingASC != ASC)
		{
			// Never mix handles from two ASCs in one ownership record.
			const auto RemoveOld = [ExistingASC](const FActiveGameplayEffectHandle Handle)
			{
				return !Handle.IsValid() || !ExistingASC->GetActiveGameplayEffect(Handle) || ExistingASC->RemoveActiveGameplayEffect(Handle);
			};
			for (const FActiveGameplayEffectHandle Handle : Existing->SecondaryOwnedHandles)
			{
				if (!RemoveOld(Handle)) return false;
			}
			if (!RemoveOld(Existing->ActiveHandle)) return false;
			ProgressionEffects.Remove(Participant.ParticipantId);
		}
		else
		{
		for (int32 Index = Existing->SecondaryOwnedHandles.Num() - 1; Index >= 0; --Index)
		{
			const FActiveGameplayEffectHandle Handle = Existing->SecondaryOwnedHandles[Index];
			if (!Handle.IsValid() || !ExistingASC->GetActiveGameplayEffect(Handle)) { Existing->SecondaryOwnedHandles.RemoveAtSwap(Index); continue; }
			if (!ExistingASC->RemoveActiveGameplayEffect(Handle)) return false;
			Existing->SecondaryOwnedHandles.RemoveAtSwap(Index);
		}
		}
	}
	if (const FHSRProgressionEffectState* Existing = ProgressionEffects.Find(Participant.ParticipantId);
		Existing && Existing->AbilitySystemComponent.Get() == ASC && Existing->EffectClass == CharacterProgressionGameplayEffect && Existing->CharacterId == Context.CharacterId
		&& Existing->Revision == Context.RuntimeRevision && Existing->Epoch == ProgressionEpoch
		&& Existing->Bonuses.MaxHealth == Context.ProgressionBonuses.MaxHealth && Existing->Bonuses.Attack == Context.ProgressionBonuses.Attack
		&& Existing->Bonuses.Defense == Context.ProgressionBonuses.Defense && Existing->Bonuses.Speed == Context.ProgressionBonuses.Speed
		&& Existing->ActiveHandle.IsValid() && ASC->GetActiveGameplayEffect(Existing->ActiveHandle))
	{
		return true;
	}
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(CharacterProgressionGameplayEffect, 1.0f, ASC->MakeEffectContext());
	if (!Spec.IsValid()) return false;
	Spec.Data->SetSetByCallerMagnitude(HSRProgressionTags::BonusMaxHealth, Context.ProgressionBonuses.MaxHealth);
	Spec.Data->SetSetByCallerMagnitude(HSRProgressionTags::BonusAttack, Context.ProgressionBonuses.Attack);
	Spec.Data->SetSetByCallerMagnitude(HSRProgressionTags::BonusDefense, Context.ProgressionBonuses.Defense);
	Spec.Data->SetSetByCallerMagnitude(HSRProgressionTags::BonusSpeed, Context.ProgressionBonuses.Speed);
	const float OldHealth = ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	if (bForceProgressionApplyFailureForTest) return false;
#endif
	const FActiveGameplayEffectHandle NewHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (!NewHandle.WasSuccessfullyApplied() || !ASC->GetActiveGameplayEffect(NewHandle)) return false;
	if (FHSRProgressionEffectState* Existing = ProgressionEffects.Find(Participant.ParticipantId))
	{
		if (Existing->AbilitySystemComponent.IsValid() && Existing->ActiveHandle.IsValid()
			&& Existing->AbilitySystemComponent->GetActiveGameplayEffect(Existing->ActiveHandle)
			&&
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
			(bForceProgressionOldRemoveFailureForTest ||
#endif
			!Existing->AbilitySystemComponent->RemoveActiveGameplayEffect(Existing->ActiveHandle)
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
			)
#endif
			)
		{
			const bool bRolledBack = ASC->RemoveActiveGameplayEffect(NewHandle);
			if (!bRolledBack)
			{
				Existing->SecondaryOwnedHandles.Add(NewHandle);
			}
			return false;
		}
	}
	FHSRProgressionEffectState& State = ProgressionEffects.FindOrAdd(Participant.ParticipantId);
	State.AbilitySystemComponent = ASC;
	State.EffectClass = CharacterProgressionGameplayEffect;
	State.CharacterId = Context.CharacterId;
	State.Bonuses = Context.ProgressionBonuses;
	State.ActiveHandle = NewHandle;
	State.SecondaryOwnedHandles.Empty();
	State.Epoch = ProgressionEpoch;
	State.Revision = Context.RuntimeRevision;
	const float NewMaxHealth = ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxHealthAttribute());
	ASC->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), FMath::Clamp(OldHealth, 0.0f, NewMaxHealth));
	UE_LOG(LogTemp, Log, TEXT("P11-003 ProgressionGE SUCCESS Participant=%s CharacterId=%s Revision=%lld MaxHealthBonus=%.3f AttackBonus=%.3f DefenseBonus=%.3f SpeedBonus=%.3f Handle=%s"),
		*Participant.ParticipantId.ToString(), *Context.CharacterId.ToString(), Context.RuntimeRevision,
		Context.ProgressionBonuses.MaxHealth, Context.ProgressionBonuses.Attack, Context.ProgressionBonuses.Defense, Context.ProgressionBonuses.Speed, *NewHandle.ToString());
	return true;
}

// 校验成长 GE 的契约：必须是 Infinite 持续、恰好 4 个修饰符，且每个都是
// Additive + SetByCaller 并指向对应的成长标签。
bool UHSRBattleCoordinator::ValidateCharacterProgressionEffectContract(const UGameplayEffect* Effect)
{
	if (!Effect || Effect->DurationPolicy != EGameplayEffectDurationType::Infinite || Effect->Modifiers.Num() != 4)
	{
		return false;
	}
	// 检查某个属性 + 标签的修饰符是否恰好存在一个且为 Additive + SetByCaller。
	const auto Has = [Effect](const FGameplayAttribute& Attribute, FGameplayTag Tag)
	{
		int32 N = 0;
		for (const FGameplayModifierInfo& M : Effect->Modifiers)
		{
			if (M.Attribute == Attribute
				&& M.ModifierOp == EGameplayModOp::Additive
				&& M.ModifierMagnitude.GetMagnitudeCalculationType() == EGameplayEffectMagnitudeCalculation::SetByCaller
				&& M.ModifierMagnitude.GetSetByCallerFloat().DataTag == Tag)
			{
				++N;
			}
		}
		return N == 1;
	};
	return Has(UHSRCoreAttributeSet::GetMaxHealthAttribute(), HSRProgressionTags::BonusMaxHealth)
		&& Has(UHSRCoreAttributeSet::GetAttackAttribute(), HSRProgressionTags::BonusAttack)
		&& Has(UHSRCoreAttributeSet::GetDefenseAttribute(), HSRProgressionTags::BonusDefense)
		&& Has(UHSRCoreAttributeSet::GetSpeedAttribute(), HSRProgressionTags::BonusSpeed);
}

// 刷新角色成长：更新缓存的成长上下文，重放成长 GE；失败时回滚到旧上下文。
bool UHSRBattleCoordinator::RefreshCharacterProgression(FName ParticipantId, const FHSRCharacterProgressionContext& Context)
{
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	++ProgressionRefreshCountForTest;
	bLastProgressionRefreshResultForTest = false;
#endif
	const FHSRBattleParticipant* Participant = FindParticipant(ParticipantId);
	if (!Participant || Participant->Team == EHSRBattleParticipantTeam::Enemy)
	{
		return false;
	}
	const FHSRCharacterProgressionContext* Existing = CharacterProgressionContexts.Find(ParticipantId);
	const TOptional<FHSRCharacterProgressionContext> Previous = Existing
		? TOptional<FHSRCharacterProgressionContext>(*Existing)
		: TOptional<FHSRCharacterProgressionContext>();
	CharacterProgressionContexts.Add(ParticipantId, Context);
	if (ApplyCharacterProgressionGameplayEffect(*Participant))
	{
	#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
		bLastProgressionRefreshResultForTest = true;
	#endif
		return true;
	}
	// 施加失败：回滚到旧上下文。
	if (Previous.IsSet())
	{
		CharacterProgressionContexts.Add(ParticipantId, Previous.GetValue());
	}
	else
	{
		CharacterProgressionContexts.Remove(ParticipantId);
	}
	return false;
}

// 把单件装备的聚合加成施加到参与者（通过 EquipmentEffectBridge 管理 GE 生命周期）。
bool UHSRBattleCoordinator::ApplyEquipmentSource(FName ParticipantId, const FGuid& InstanceId, const FHSREquipmentAggregate& Aggregate, int64 Revision)
{
	if (!EquipmentGameplayEffect || !InstanceId.IsValid() || Revision < 0)
	{
		return false;
	}
	const FHSRBattleParticipant* P = FindParticipant(ParticipantId);
	if (!P || !P->AbilitySystemComponent.IsValid())
	{
		return false;
	}
	FHSREquipmentAggregate A = Aggregate;
	A.Revision = Revision;
	if (!EquipmentEffectBridge)
	{
		EquipmentEffectBridge = NewObject<UHSREquipmentEffectBridge>(this);
	}
	return EquipmentEffectBridge->Apply(InstanceId, P->AbilitySystemComponent.Get(), EquipmentGameplayEffect, A);
}

// 移除单件装备的加成。
bool UHSRBattleCoordinator::RemoveEquipmentSource(FName ParticipantId, const FGuid& InstanceId)
{
	if (!EquipmentEffectBridge)
	{
		return false;
	}
	const FHSRBattleParticipant* P = FindParticipant(ParticipantId);
	return P && EquipmentEffectBridge->Remove(InstanceId);
}

// 施加圣遗物套装加成（按套装 ID 聚合）。
bool UHSRBattleCoordinator::ApplyEquipmentSetSource(FName ParticipantId, FName SetSourceId, const FHSREquipmentAggregate& Aggregate, int64 Revision)
{
	if (!RelicSetGameplayEffect || SetSourceId.IsNone() || Revision < 0)
	{
		return false;
	}
	const FHSRBattleParticipant* P = FindParticipant(ParticipantId);
	if (!P || !P->AbilitySystemComponent.IsValid())
	{
		return false;
	}
	if (!EquipmentEffectBridge)
	{
		EquipmentEffectBridge = NewObject<UHSREquipmentEffectBridge>(this);
	}
	FHSREquipmentAggregate A = Aggregate;
	A.Revision = Revision;
	return EquipmentEffectBridge->ApplySetSource(SetSourceId, P->AbilitySystemComponent.Get(), RelicSetGameplayEffect, A);
}

// 移除圣遗物套装加成。
bool UHSRBattleCoordinator::RemoveEquipmentSetSource(FName ParticipantId, FName SetSourceId)
{
	if (SetSourceId.IsNone() || !EquipmentEffectBridge)
	{
		return false;
	}
	const FHSRBattleParticipant* P = FindParticipant(ParticipantId);
	return P && EquipmentEffectBridge->RemoveSetSource(SetSourceId);
}

// 把本协调器注册为装备移动（穿/脱/替换）的投影目标，让探索侧可以在战斗内预览装备变化。
void UHSRBattleCoordinator::BindEquipmentMovementProjection(UHSREquipmentSubsystem& Equipment)
{
	if (!EquipmentEffectBridge)
	{
		EquipmentEffectBridge = NewObject<UHSREquipmentEffectBridge>(this);
	}
	Equipment.SetMovementProjection(
		UHSREquipmentSubsystem::FMovementProjectionPreflight::CreateUObject(this, &ThisClass::CanProjectEquipmentMovement),
		UHSREquipmentSubsystem::FMovementProjectionApply::CreateUObject(this, &ThisClass::ApplyEquipmentMovementProjection),
		UHSREquipmentSubsystem::FMovementProjectionCommit::CreateUObject(this, &ThisClass::CommitEquipmentMovementProjection));
}

// 预检：只有主角领队且装备 GE 可用时，装备移动才可投影。
bool UHSRBattleCoordinator::CanProjectEquipmentMovement(const FHSREquipmentMovementRequest& Request, const FHSREquipmentLoadout& Candidate) const
{
	if (Request.CharacterId != HSRCharacterGuidFromProfileName(PlayerCharacterId) || !EquipmentGameplayEffect)
	{
		return false;
	}
	const FHSRBattleParticipant* Participant = FindParticipant(GetLeaderParticipantId(EHSRBattleParticipantTeam::Player));
	if (!Participant || !Participant->AbilitySystemComponent.IsValid())
	{
		return false;
	}
	UHSREquipmentEffectBridge* Bridge = EquipmentEffectBridge.Get();
	if (!Bridge)
	{
		return false;
	}
	// 期望的装备集合（候选装备 + 候选圣遗物）。
	TSet<FGuid> DesiredIds;
	for (const auto& Pair : Candidate.Equipment)
	{
		DesiredIds.Add(Pair.Value.InstanceId);
	}
	for (const auto& Pair : Candidate.Relics)
	{
		DesiredIds.Add(Pair.Value.InstanceId);
	}
	// 现有投影里不在期望集合中的装备必须可移除。
	for (const auto& Existing : EquipmentProjectionStates)
	{
		if (!DesiredIds.Contains(Existing.Key) && !Bridge->CanRemove(Existing.Key))
		{
			return false;
		}
	}
	// 卸下意图直接放行；穿上/替换需要能聚合并施加。
	if (Request.Intent == EHSREquipmentMovementIntent::Unequip)
	{
		return true;
	}
	FHSREquipmentAggregate Aggregate;
	return UHSREquipmentStatAggregator::Aggregate(Candidate, Request.ExpectedEquipmentRevision + 1, Aggregate)
		&& Bridge->CanApply(Participant->AbilitySystemComponent.Get(), EquipmentGameplayEffect, Aggregate);
}
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
void UHSRBattleCoordinator::SetEquipmentMovementProjectionFailureForDevelopmentTest(bool bApply,bool bRemove)
{
	if(!EquipmentEffectBridge)EquipmentEffectBridge=NewObject<UHSREquipmentEffectBridge>(this);
	EquipmentEffectBridge->SetPreflightFailureForDevelopmentTest(bApply,bRemove);
}
void UHSRBattleCoordinator::SetEquipmentMovementProjectionCommitFailureForDevelopmentTest(bool bApply,bool bRemove)
{
	if(!EquipmentEffectBridge)EquipmentEffectBridge=NewObject<UHSREquipmentEffectBridge>(this);
	EquipmentEffectBridge->SetCommitFailureForDevelopmentTest(bApply,bRemove);
}
#endif
bool UHSRBattleCoordinator::ApplyEquipmentMovementProjection(const FHSREquipmentMovementRequest& Request,const FHSREquipmentLoadout& Candidate)
{
	const FName LeaderId = GetLeaderParticipantId(EHSRBattleParticipantTeam::Player);
	if (Request.Intent == EHSREquipmentMovementIntent::Unequip)
	{
		if (!RemoveEquipmentSource(LeaderId, Request.InstanceId))
		{
			return false;
		}
		EquipmentProjectionStates.Remove(Request.InstanceId);
		EquipmentProjectionParticipants.Remove(Request.InstanceId);
		return true;
	}

	TSet<FGuid> DesiredIds;
	for (const auto& Pair : Candidate.Equipment)
	{
		DesiredIds.Add(Pair.Value.InstanceId);
	}
	for (const auto& Pair : Candidate.Relics)
	{
		DesiredIds.Add(Pair.Value.InstanceId);
	}

	FHSREquipmentAggregate Aggregate;
	if (!UHSREquipmentStatAggregator::Aggregate(Candidate, Request.ExpectedEquipmentRevision + 1, Aggregate)
		|| !ApplyEquipmentSource(LeaderId, Request.InstanceId, Aggregate, Aggregate.Revision))
	{
		return false;
	}

	TArray<FGuid> RemovedIds;
	if (Request.Intent == EHSREquipmentMovementIntent::Replace)
	{
		for (const auto& Existing : EquipmentProjectionStates)
		{
			if (!DesiredIds.Contains(Existing.Key))
			{
				RemovedIds.Add(Existing.Key);
			}
		}
	}

	for (const FGuid& RemovedId : RemovedIds)
	{
		if (!RemoveEquipmentSource(LeaderId, RemovedId))
		{
			// Roll the just-applied source back so a partial failure leaves no projection behind.
			RemoveEquipmentSource(LeaderId, Request.InstanceId);
			return false;
		}
	}
	for (const FGuid& RemovedId : RemovedIds)
	{
		EquipmentProjectionStates.Remove(RemovedId);
		EquipmentProjectionParticipants.Remove(RemovedId);
	}

	EquipmentProjectionStates.Add(Request.InstanceId, Aggregate);
	EquipmentProjectionParticipants.Add(Request.InstanceId, LeaderId);
	return true;
}
bool UHSRBattleCoordinator::ProjectEquipmentRestore(const TMap<FGuid,FHSREquipmentRestoreState>& Candidate)
{
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	if (bForceEquipmentRestoreProjectionFailure) return false;
#endif
	if (!EquipmentGameplayEffect || !RelicSetGameplayEffect) return false;
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UHSREquipmentSubsystem* Equipment = GameInstance ? GameInstance->GetSubsystem<UHSREquipmentSubsystem>() : nullptr;
	const TMap<FGuid,FHSREquipmentAggregate> OldStates=EquipmentProjectionStates;
	const TMap<FGuid,FName> OldParticipants=EquipmentProjectionParticipants;
	const TMap<FName,FHSREquipmentAggregate> OldSetStates=EquipmentSetProjectionStates;
	const TMap<FName,FName> OldSetParticipants=EquipmentSetProjectionParticipants;
	TMap<FGuid,FHSREquipmentAggregate> DesiredStates;
	TMap<FGuid,FName> DesiredParticipants;
	TMap<FName,FHSREquipmentAggregate> DesiredSetStates;
	TMap<FName,FName> DesiredSetParticipants;
	for (const auto& Pair : Candidate)
	{
		const FGuid PlayerCharacterGuid = HSRCharacterGuidFromProfileName(PlayerCharacterId);
		const FHSRBattleParticipant* Participant = Pair.Key == PlayerCharacterGuid
			? FindParticipant(GetLeaderParticipantId(EHSRBattleParticipantTeam::Player))
			: nullptr;
		if (!Participant || !Participant->AbilitySystemComponent.IsValid())
		{
			continue;
		}
	// 把单件实例按“单件负载”聚合并登记到期望集合（装备→武器槽，圣遗物→头部槽）。
	const auto AddInstance = [&](const FHSREquipmentInstance& Instance)
	{
		FHSREquipmentLoadout Single;
		if (Instance.Kind == EHSREquipmentKind::Equipment)
		{
			Single.Equipment.Add(EHSREquipmentSlot::Weapon, Instance);
		}
		else
		{
			Single.Relics.Add(EHSRRelicSlot::Head, Instance);
		}
		FHSREquipmentAggregate Aggregate;
		if (!UHSREquipmentStatAggregator::Aggregate(Single, Pair.Value.Revision, Aggregate))
		{
			return false;
		}
		DesiredStates.Add(Instance.InstanceId, Aggregate);
		DesiredParticipants.Add(Instance.InstanceId, Participant->ParticipantId);
		return true;
	};
	for (const auto& Item : Pair.Value.Loadout.Equipment)
	{
		if (!AddInstance(Item.Value))
		{
			return false;
		}
	}
	for (const auto& Item : Pair.Value.Loadout.Relics)
	{
		if (!AddInstance(Item.Value))
		{
			return false;
		}
	}
	// 存档 DTO 只带件数而不带阈值，所以问持有已注册套装定义的子系统要阈值；
	// 它从未见过的套装回退到两件套默认阈值。
	for (const auto& Set : Pair.Value.RelicSetCounts)
	{
		if (Set.Value < (Equipment ? Equipment->GetSetThreshold(Set.Key) : FHSRRelicSetResolver::DefaultThreshold))
		{
			continue;
		}
		FHSREquipmentAggregate Aggregate;
		Aggregate.Revision = Pair.Value.Revision;
		DesiredSetStates.Add(Set.Key, Aggregate);
		DesiredSetParticipants.Add(Set.Key, Participant->ParticipantId);
	}
	}
	// 回滚函数：把旧投影状态全部还原，并移除新登记但旧状态里没有的来源。
	const auto RestoreOld = [&]()
	{
		for (const auto& Old : OldStates)
		{
			ApplyEquipmentSource(OldParticipants.FindRef(Old.Key), Old.Key, Old.Value, Old.Value.Revision);
		}
		for (const auto& Old : OldSetStates)
		{
			ApplyEquipmentSetSource(OldSetParticipants.FindRef(Old.Key), Old.Key, Old.Value, Old.Value.Revision);
		}
		for (const auto& Desired : DesiredStates)
		{
			if (!OldStates.Contains(Desired.Key))
			{
				RemoveEquipmentSource(DesiredParticipants.FindRef(Desired.Key), Desired.Key);
			}
		}
		for (const auto& Desired : DesiredSetStates)
		{
			if (!OldSetStates.Contains(Desired.Key))
			{
				RemoveEquipmentSetSource(DesiredSetParticipants.FindRef(Desired.Key), Desired.Key);
			}
		}
	};
	int32 CompletedOperations = 0;
	// 仅在开发测试注入失败场景时使用：达到指定操作数后强制失败。
	const auto InjectFailure = [&]()
	{
	#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
		return EquipmentRestoreFailureAfterOperations >= 0 && CompletedOperations >= EquipmentRestoreFailureAfterOperations;
	#else
		return false;
	#endif
	};
	for (const auto& Desired : DesiredStates)
	{
		if (InjectFailure() || !ApplyEquipmentSource(DesiredParticipants.FindRef(Desired.Key), Desired.Key, Desired.Value, Desired.Value.Revision))
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.EquipmentProjection FAIL Apply Instance=%s Revision=%lld"), *Desired.Key.ToString(), Desired.Value.Revision);
			RestoreOld();
			return false;
		}
		++CompletedOperations;
	}
	for (const auto& Desired : DesiredSetStates)
	{
		if (InjectFailure() || !ApplyEquipmentSetSource(DesiredSetParticipants.FindRef(Desired.Key), Desired.Key, Desired.Value, Desired.Value.Revision))
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.EquipmentProjection FAIL ApplySet Set=%s Revision=%lld"), *Desired.Key.ToString(), Desired.Value.Revision);
			RestoreOld();
			return false;
		}
		++CompletedOperations;
	}
	for (const auto& Old : OldStates)
	{
		if (!DesiredStates.Contains(Old.Key))
		{
			if (InjectFailure() || !RemoveEquipmentSource(OldParticipants.FindRef(Old.Key), Old.Key))
			{
				UE_LOG(LogTemp, Error, TEXT("HSR.EquipmentProjection FAIL Remove Instance=%s Revision=%lld"), *Old.Key.ToString(), Old.Value.Revision);
				RestoreOld();
				return false;
			}
			++CompletedOperations;
		}
	}
	for (const auto& Old : OldSetStates)
	{
		if (!DesiredSetStates.Contains(Old.Key))
		{
			if (InjectFailure() || !RemoveEquipmentSetSource(OldSetParticipants.FindRef(Old.Key), Old.Key))
			{
				UE_LOG(LogTemp, Error, TEXT("HSR.EquipmentProjection FAIL RemoveSet Set=%s Revision=%lld"), *Old.Key.ToString(), Old.Value.Revision);
				RestoreOld();
				return false;
			}
			++CompletedOperations;
		}
	}
	// 全部成功后，用期望状态替换投影状态。
	EquipmentProjectionStates = MoveTemp(DesiredStates);
	EquipmentProjectionParticipants = MoveTemp(DesiredParticipants);
	EquipmentSetProjectionStates = MoveTemp(DesiredSetStates);
	EquipmentSetProjectionParticipants = MoveTemp(DesiredSetParticipants);
	return true;
}

#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
// 开发测试辅助：查询成长效果的主句柄是否有效。
bool UHSRBattleCoordinator::HasProgressionPrimaryHandleForDevelopmentTest(FName Id) const
{
	const auto* S = ProgressionEffects.Find(Id);
	return S && S->ActiveHandle.IsValid();
}
// 开发测试辅助：取成长效果主句柄的字符串形式。
FString UHSRBattleCoordinator::GetProgressionPrimaryHandleForDevelopmentTest(FName Id) const
{
	const auto* S = ProgressionEffects.Find(Id);
	return S ? S->ActiveHandle.ToString() : FString();
}
// 开发测试辅助：取成长效果的次级句柄数量。
int32 UHSRBattleCoordinator::GetProgressionSecondaryCountForDevelopmentTest(FName Id) const
{
	const auto* S = ProgressionEffects.Find(Id);
	return S ? S->SecondaryOwnedHandles.Num() : 0;
}
// 开发测试辅助：统计该参与者当前激活的成长句柄总数（主 + 次级）。
int32 UHSRBattleCoordinator::GetProgressionActiveHandleCountForDevelopmentTest(FName Id) const
{
	const auto* S = ProgressionEffects.Find(Id);
	UAbilitySystemComponent* A = S ? S->AbilitySystemComponent.Get() : nullptr;
	if (!S || !A)
	{
		return 0;
	}
	int32 N = A->GetActiveGameplayEffect(S->ActiveHandle) ? 1 : 0;
	for (auto H : S->SecondaryOwnedHandles)
	{
		if (A->GetActiveGameplayEffect(H))
		{
			++N;
		}
	}
	return N;
}
// 开发测试辅助：生成成长状态的指纹字符串（用于对比前后是否一致）。
FString UHSRBattleCoordinator::GetProgressionFingerprintForDevelopmentTest(FName Id) const
{
	const auto* S = ProgressionEffects.Find(Id);
	return S
		? FString::Printf(TEXT("Character=%s Revision=%lld Bonus=%.3f/%.3f/%.3f/%.3f"),
			*S->CharacterId.ToString(), S->Revision, S->Bonuses.MaxHealth, S->Bonuses.Attack, S->Bonuses.Defense, S->Bonuses.Speed)
		: TEXT("None");
}
#endif

// 判断两份成长上下文是否等价（字符、版本、四维加成全部相等）。
bool UHSRBattleCoordinator::HasSameProgressionFingerprint(const FHSRCharacterProgressionContext& A, const FHSRCharacterProgressionContext& B)
{
	return A.CharacterId == B.CharacterId
		&& A.RuntimeRevision == B.RuntimeRevision
		&& A.ProgressionBonuses.MaxHealth == B.ProgressionBonuses.MaxHealth
		&& A.ProgressionBonuses.Attack == B.ProgressionBonuses.Attack
		&& A.ProgressionBonuses.Defense == B.ProgressionBonuses.Defense
		&& A.ProgressionBonuses.Speed == B.ProgressionBonuses.Speed;
}

// 清除所有成长效果的 GE，并更新成长纪元（纪元用于区分新旧成长状态）。
bool UHSRBattleCoordinator::ClearProgressionGameplayEffects()
{
	bool bAllRemoved = true;
	TArray<FName> ClearedKeys;
	for (const TPair<FName, FHSRProgressionEffectState>& Pair : ProgressionEffects)
	{
		UAbilitySystemComponent* ASC = Pair.Value.AbilitySystemComponent.Get();
		// 句柄无效、ASC 已销毁或效果已不激活都视为“已移除”。
		const auto TryRemove = [ASC](const FActiveGameplayEffectHandle Handle)
		{
			return !Handle.IsValid() || ASC == nullptr || !ASC->GetActiveGameplayEffect(Handle) || ASC->RemoveActiveGameplayEffect(Handle);
		};
		bool bRemoved = TryRemove(Pair.Value.ActiveHandle);
		for (const FActiveGameplayEffectHandle Handle : Pair.Value.SecondaryOwnedHandles)
		{
			bRemoved = TryRemove(Handle) && bRemoved;
		}
		if (bRemoved)
		{
			ClearedKeys.Add(Pair.Key);
		}
		else
		{
			bAllRemoved = false;
		}
	}
	for (const FName Key : ClearedKeys)
	{
		ProgressionEffects.Remove(Key);
	}
	// 全部清空时推进纪元，让后续写入可辨识为“新的一代”。
	if (ProgressionEffects.IsEmpty())
	{
		++ProgressionEpoch;
	}
	return bAllRemoved;
}

void UHSRBattleCoordinator::SetDefaultSkillLoadout(const TArray<UHSRSkillDefinition*>& InSkills)
{
	DefaultSkillLoadout.Reset();
	for (UHSRSkillDefinition* Definition : InSkills)
	{
		AddSkillToDefaultLoadout(Definition);
	}
}

void UHSRBattleCoordinator::AddSkillToDefaultLoadout(UHSRSkillDefinition* InDefinition)
{
	if (!InDefinition)
	{
		return;
	}

	const int32 ExistingIndex = DefaultSkillLoadout.IndexOfByPredicate(
		[InDefinition](const TObjectPtr<UHSRSkillDefinition>& Candidate)
		{
			return Candidate && Candidate->SkillId == InDefinition->SkillId;
		});
	if (ExistingIndex != INDEX_NONE)
	{
		DefaultSkillLoadout[ExistingIndex] = InDefinition;
		return;
	}
	DefaultSkillLoadout.Add(InDefinition);
}

void UHSRBattleCoordinator::SetParticipantSkillLoadout(FName ParticipantId, const TArray<UHSRSkillDefinition*>& InSkills)
{
	if (ParticipantId.IsNone())
	{
		return;
	}

	FHSRSkillLoadout Loadout;
	for (UHSRSkillDefinition* Definition : InSkills)
	{
		if (!Definition)
		{
			continue;
		}
		const bool bDuplicate = Loadout.Skills.ContainsByPredicate(
			[Definition](const TObjectPtr<UHSRSkillDefinition>& Candidate)
			{
				return Candidate && Candidate->SkillId == Definition->SkillId;
			});
		if (!bDuplicate)
		{
			Loadout.Skills.Add(Definition);
		}
	}
	ParticipantSkillLoadouts.Add(ParticipantId, MoveTemp(Loadout));
}

const TArray<TObjectPtr<UHSRSkillDefinition>>& UHSRBattleCoordinator::GetSkillLoadoutFor(FName ParticipantId) const
{
	// An override that resolved to nothing -- every authored entry failed to load or failed
	// validation -- must not leave the participant with an empty command panel, so treat it the
	// same as having no override at all.
	if (const FHSRSkillLoadout* Override = ParticipantSkillLoadouts.Find(ParticipantId))
	{
		if (Override->Skills.Num() > 0)
		{
			return Override->Skills;
		}
	}
	return DefaultSkillLoadout;
}

const UHSRSkillDefinition* UHSRBattleCoordinator::FindDefaultSkillByCategory(EHSRSkillCategory Category) const
{
	for (const TObjectPtr<UHSRSkillDefinition>& Definition : DefaultSkillLoadout)
	{
		if (Definition && Definition->Category == Category)
		{
			return Definition;
		}
	}
	return nullptr;
}

bool UHSRBattleCoordinator::GrantSkillLoadout(const FHSRBattleParticipant& Participant)
{
	if (!Participant.AbilitySystemComponent.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::GrantSkillLoadout - REJECTED invalid ASC Participant=%s"), *Participant.ParticipantId.ToString());
		return false;
	}

	const TArray<TObjectPtr<UHSRSkillDefinition>>& Loadout = GetSkillLoadoutFor(Participant.ParticipantId);
	bool bGrantedAny = false;
	for (const TObjectPtr<UHSRSkillDefinition>& Definition : Loadout)
	{
		if (!Definition || !Definition->IsValidForCategory())
		{
			UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::GrantSkillLoadout - SKIPPED invalid definition Participant=%s Skill=%s"),
				*Participant.ParticipantId.ToString(), Definition ? *Definition->SkillId.ToString() : TEXT("null"));
			continue;
		}
		if (!GrantSingleSkill(Participant, *Definition))
		{
			return false;
		}
		bGrantedAny = true;
	}

	if (!bGrantedAny)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::GrantSkillLoadout - REJECTED empty loadout Participant=%s"), *Participant.ParticipantId.ToString());
	}
	return bGrantedAny;
}

bool UHSRBattleCoordinator::GrantSingleSkill(const FHSRBattleParticipant& Participant, const UHSRSkillDefinition& Definition)
{
	const FGameplayAbilitySpecHandle Handle = Participant.AbilitySystemComponent->GiveAbility(
		FGameplayAbilitySpec(Definition.AbilityClass, 1, INDEX_NONE, this));
	if (!Handle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::GrantSingleSkill - FAILED GiveAbility Participant=%s Skill=%s"),
			*Participant.ParticipantId.ToString(), *Definition.SkillId.ToString());
		return false;
	}

	// Skills that carry no per-action ability configuration are fully granted at this point.
	if (!Definition.RequiresAbilityConfiguration())
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::GrantSingleSkill - SUCCESS Participant=%s Skill=%s"),
			*Participant.ParticipantId.ToString(), *Definition.SkillId.ToString());
		return true;
	}

	FGameplayAbilitySpec* Spec = Participant.AbilitySystemComponent->FindAbilitySpecFromClass(Definition.AbilityClass);
	UHSRGameplayAbilityBase* Ability = Spec ? Cast<UHSRGameplayAbilityBase>(Spec->GetPrimaryInstance()) : nullptr;
	if (!Ability || !Ability->ConfigureFromSkillDefinition(Definition))
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::GrantSingleSkill - FAILED configuration Participant=%s Skill=%s"),
			*Participant.ParticipantId.ToString(), *Definition.SkillId.ToString());
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::GrantSingleSkill - SUCCESS Participant=%s Skill=%s"),
		*Participant.ParticipantId.ToString(), *Definition.SkillId.ToString());
	return true;
}

const UHSRSkillDefinition* UHSRBattleCoordinator::FindSkillDefinition(FName SkillId) const
{
	if (SkillId.IsNone())
	{
		return nullptr;
	}

	for (const TObjectPtr<UHSRSkillDefinition>& Definition : DefaultSkillLoadout)
	{
		if (Definition && Definition->SkillId == SkillId)
		{
			return Definition;
		}
	}

	// Per-participant loadouts may carry skills absent from the default list.
	for (const TPair<FName, FHSRSkillLoadout>& Entry : ParticipantSkillLoadouts)
	{
		for (const TObjectPtr<UHSRSkillDefinition>& Definition : Entry.Value.Skills)
		{
			if (Definition && Definition->SkillId == SkillId)
			{
				return Definition;
			}
		}
	}
	return nullptr;
}

bool UHSRBattleCoordinator::ReserveSkillPoints(const FGuid& ActionId, int32 Delta)
{
	if (SkillPointReservations.Contains(ActionId)) return false;
	if (Delta < 0 && TeamResourceState.CurrentSkillPoints < -Delta) return false;
	FHSRSkillPointReservation Reservation; Reservation.ActionId = ActionId;
	// A capped BasicAttack is still a valid action; it commits a zero delta.
	Reservation.Delta = Delta > 0 ? FMath::Min(Delta, TeamResourceState.MaxSkillPoints - TeamResourceState.CurrentSkillPoints) : Delta;
	SkillPointReservations.Add(ActionId, Reservation);
	UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::ReserveSkillPoints - ActionId=%s Delta=%d Current=%d Max=%d"), *ActionId.ToString(), Reservation.Delta, TeamResourceState.CurrentSkillPoints, TeamResourceState.MaxSkillPoints);
	return true;
}

void UHSRBattleCoordinator::RollbackSkillPoints(const FGuid& ActionId)
{
	if (SkillPointReservations.Remove(ActionId) > 0) UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::RollbackSkillPoints - ActionId=%s"), *ActionId.ToString());
}

void UHSRBattleCoordinator::CommitSkillPoints(const FGuid& ActionId)
{
	if (FHSRSkillPointReservation* Reservation = SkillPointReservations.Find(ActionId))
	{
		TeamResourceState.CurrentSkillPoints = FMath::Clamp(TeamResourceState.CurrentSkillPoints + Reservation->Delta, 0, TeamResourceState.MaxSkillPoints);
		UE_LOG(LogTemp, Log, TEXT("UHSRBattleCoordinator::CommitSkillPoints - ActionId=%s Delta=%d Current=%d"), *ActionId.ToString(), Reservation->Delta, TeamResourceState.CurrentSkillPoints);
		SkillPointReservations.Remove(ActionId);
	}
}
bool UHSRBattleCoordinator::BuildVictoryRewardRequest(const FHSRBattleResult& Result, FHSRRewardRequest& OutRequest) const
{
	if (Result.RequestId != CurrentRequestId || Result.Outcome != EHSRBattleOutcome::PlayerVictory || CurrentRewardDefinitionId.IsNone())
	{
		return false;
	}

	OutRequest.ClaimId = CurrentRequestId;
	OutRequest.RewardDefinitionId = CurrentRewardDefinitionId;
	OutRequest.Seed = CurrentRewardSeed;
	return true;
}
