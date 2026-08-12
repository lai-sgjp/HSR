#include "HSRCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "../GAS/HSRAbilitySystemComponent.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "../UI/HSRAttributeViewModel.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Equipment/HSREquipmentEffectBridge.h"
#include "../Equipment/HSREquipmentStatAggregator.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Equipment/HSREquipmentTypes.h"
#include "GameplayEffect.h"
#include "Engine/GameInstance.h"

AHSRCharacterBase::AHSRCharacterBase()
{
	// 角色本身不依赖 Tick：所有属性变化都由 GAS 驱动，关掉 Tick 避免每帧空转。
	PrimaryActorTick.bCanEverTick = false;

	// 创建 GAS 核心组件。单机项目不需要网络复制，显式关闭复制；
	// 属性由 GameplayEffect 驱动、无需轮询，因此组件本身也不开 Tick。
	AbilitySystemComponent = CreateDefaultSubobject<UHSRAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(false);
	AbilitySystemComponent->SetComponentTickEnabled(false);

	// 核心属性集：角色全部基础战斗数值（生命/能量/速度/攻击/防御等）都定义在这里。
	CoreAttributeSet = CreateDefaultSubobject<UHSRCoreAttributeSet>(TEXT("CoreAttributeSet"));
	// 属性视图模型：把 ASC 上的原始数值转成 UI 可直接绑定的显示数据（MVVM 的 VM 层）。
	AttributeViewModel = CreateDefaultSubobject<UHSRAttributeViewModel>(TEXT("AttributeViewModel"));
}

UAbilitySystemComponent* AHSRCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AHSRCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (!AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::BeginPlay - AbilitySystemComponent is nullptr"), *GetName());
		return;
	}

	// 初始化顺序有严格依赖：必须先建立 ActorInfo（ASC 才知道 Owner/Avatar 是谁），
	// 才能应用初始属性 GE，最后才把属性委托绑定到 ViewModel（绑定前要求 ActorInfo 已就绪）。
	InitializeAbilityActorInfo();
	ApplyInitialAttributes();
	BindAttributeDelegates();
}

void AHSRCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 先拆解 ViewModel 对 ASC 的属性委托绑定，避免角色销毁后属性回调仍被触发造成悬垂调用。
	if (AttributeViewModel)
	{
		AttributeViewModel->Teardown();
	}
	// 从 ASC 上卸载装备负载投影：移除对应的装备 GE 并解绑负载变更监听，保证销毁时不留副作用。
	UnprojectEquipmentFromAbilitySystem();

	Super::EndPlay(EndPlayReason);
}

void AHSRCharacterBase::InitializeAbilityActorInfo()
{
	// 幂等保护：ActorInfo 一旦初始化成功就不再重复初始化（重复 Init 会重置已绑定的信息）。
	if (bActorInfoInitialized)
	{
		return;
	}

	if (!AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::InitializeAbilityActorInfo - AbilitySystemComponent is nullptr"), *GetName());
		return;
	}

	// Owner 与 Avatar 都指向角色自身：本游戏角色同时承担"拥有者"与"替身"两个角色。
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (AbilitySystemComponent->AbilityActorInfo.IsValid())
	{
		bActorInfoInitialized = true;
		UE_LOG(LogTemp, Log, TEXT("%s::InitializeAbilityActorInfo - Owner=Avatar=self, ActorInfo valid"), *GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::InitializeAbilityActorInfo - ActorInfo is NOT valid after Init"), *GetName());
	}
}

void AHSRCharacterBase::ApplyInitialAttributes()
{
	// 幂等保护：初始属性只应用一次，避免 RestartPlayer 等流程重复叠加初始 GE。
	if (bInitialAttributesApplied)
	{
		return;
	}

	if (!AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::ApplyInitialAttributes - AbilitySystemComponent is nullptr"), *GetName());
		return;
	}

	if (!bActorInfoInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::ApplyInitialAttributes - ActorInfo not initialized yet"), *GetName());
		return;
	}

	if (!InitialAttributesEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::ApplyInitialAttributes - InitialAttributesEffect is not set"), *GetName());
		return;
	}

	// 用 ASC 构造 effect context 并创建 Spec，等级固定为 1（初始属性不随等级浮动）。
	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(InitialAttributesEffect, 1.0f, Context);
	if (!Spec.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::ApplyInitialAttributes - MakeOutgoingSpec failed"), *GetName());
		return;
	}

	// 把初始属性 GE 应用到自身；返回的 Handle 用于确认应用是否成功。
	FActiveGameplayEffectHandle Handle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (Handle.WasSuccessfullyApplied())
	{
		bInitialAttributesApplied = true;
		InitialAttributesApplySuccessCount++;
		// 角色定义（author 的数据资产）才是基础属性的唯一权威来源。若角色 ID 已经确定
		// （例如 RestartPlayer 之后再次生成的玩家角色），重新断言基础数值，
		// 避免 GE 的默认数值叠加在定义值之上造成"越叠越高"。
		ApplyCharacterBaseStatsToAbilitySystem();
		UE_LOG(LogTemp, Log, TEXT("%s::ApplyInitialAttributes - GE applied successfully via WasSuccessfullyApplied (total=%d)"), *GetName(), InitialAttributesApplySuccessCount);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::ApplyInitialAttributes - GE was NOT successfully applied"), *GetName());
	}
}


bool AHSRCharacterBase::RequestApplyPhase2TestEffect(TSubclassOf<UGameplayEffect> TestEffect)
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	// 开发专用的 Phase 2 测试接口，绝不进入 Test/Shipping 版本。
	UE_LOG(LogTemp, Warning, TEXT("%s::RequestApplyPhase2TestEffect - Rejected in Test/Shipping"), *GetName());
	return false;
#else
	if (!TestEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestApplyPhase2TestEffect - TestEffect class is null"), *GetName());
		return false;
	}

	if (!AbilitySystemComponent || !AbilitySystemComponent->AbilityActorInfo.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestApplyPhase2TestEffect - ASC or ActorInfo invalid"), *GetName());
		return false;
	}

	// 白名单校验：只允许对 Phase 2 测试用的一组 GE 类应用，防止测试流程随意注入任意 GE。
	const FString PackagePath = TestEffect->GetOutermost()->GetName();
	const bool bAllowed = PackagePath == TEXT("/Game/GameplayEffects/BP_GE_Test_HealthBelowZero")
		|| PackagePath == TEXT("/Game/GameplayEffects/BP_GE_Test_HealthAboveMax")
		|| PackagePath == TEXT("/Game/GameplayEffects/BP_GE_Test_LowerMaxHealth")
		|| PackagePath == TEXT("/Game/GameplayEffects/BP_GE_Test_EnergyBounds")
		|| PackagePath == TEXT("/Game/GameplayEffects/BP_GE_Test_SpeedBelowZero");

	if (!bAllowed)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestApplyPhase2TestEffect - Effect %s not in allowed Phase 2 test list"), *GetName(), *PackagePath);
		return false;
	}

	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(TestEffect, 1.0f, Context);
	if (!Spec.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestApplyPhase2TestEffect - MakeOutgoingSpec failed"), *GetName());
		return false;
	}

	// 应用成功后返回给调用方，方便测试脚本断言结果。
	FActiveGameplayEffectHandle Handle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	const bool bResult = Handle.WasSuccessfullyApplied();
	UE_LOG(LogTemp, Log, TEXT("%s::RequestApplyPhase2TestEffect - Applied %s success=%d"), *GetName(), *PackagePath, bResult);
	return bResult;
#endif
}

bool AHSRCharacterBase::RequestPhase2Repossess()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	// 开发专用：测试重新 Possess 后 ASC 的 ActorInfo / 属性是否完好，绝不进入 Test/Shipping。
	UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - Rejected in Test/Shipping"), *GetName());
	return false;
#else
	// 操作前快照：记录控制器、Pawn、ASC 以及初始属性应用次数，便于事后对比前后状态。
	UE_LOG(LogTemp, Log, TEXT("%s::RequestPhase2Repossess - PRE: Controller=%s, Pawn=%s, ASC=%s, InitApplyCount=%d"),
		*GetName(),
		GetController() ? *GetController()->GetName() : TEXT("null"),
		*GetName(),
		AbilitySystemComponent ? *AbilitySystemComponent->GetName() : TEXT("null"),
		InitialAttributesApplySuccessCount);
	if (AbilitySystemComponent && AbilitySystemComponent->AbilityActorInfo.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("%s::RequestPhase2Repossess - PRE: ActorInfo valid, Owner=%s, Avatar=%s"),
			*GetName(),
			AbilitySystemComponent->AbilityActorInfo->OwnerActor.IsValid() ? *AbilitySystemComponent->AbilityActorInfo->OwnerActor->GetName() : TEXT("null"),
			AbilitySystemComponent->AbilityActorInfo->AvatarActor.IsValid() ? *AbilitySystemComponent->AbilityActorInfo->AvatarActor->GetName() : TEXT("null"));
		const UHSRCoreAttributeSet* CoreSet = AbilitySystemComponent->GetSet<UHSRCoreAttributeSet>();
		if (CoreSet)
		{
			UE_LOG(LogTemp, Log, TEXT("%s::RequestPhase2Repossess - PRE: Health=%f MaxHealth=%f Energy=%f MaxEnergy=%f Speed=%f"),
				*GetName(), CoreSet->GetHealth(), CoreSet->GetMaxHealth(), CoreSet->GetEnergy(), CoreSet->GetMaxEnergy(), CoreSet->GetSpeed());
		}
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("%s::RequestPhase2Repossess - PRE: ActorInfo invalid"), *GetName());
	}

	// 前置条件：必须已存在控制器，且该控制器正 Possess 本角色（否则"重新 Possess"没有意义）。
	AController* OriginalController = GetController();
	if (!OriginalController)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - No Controller"), *GetName());
		return false;
	}
	if (OriginalController->GetPawn() != this)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - Current Controller %s does not possess this"), *GetName(), *OriginalController->GetName());
		return false;
	}

	// 核心操作：先解绑再重新绑定，模拟 RestartPlayer 的完整 Possess 生命周期。
	OriginalController->UnPossess();
	OriginalController->Possess(this);

	// 逐项校验（独立 null 检查，便于定位是哪一个环节损坏）：
	// 控制器是否仍然有效、Possess 后 GetPawn 是否确实回到本角色、ASC 及 ActorInfo 是否健在。
	if (!OriginalController)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - FAIL: Controller destroyed"), *GetName());
		return false;
	}
	AActor* PostPawn = OriginalController->GetPawn();
	if (PostPawn != this)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - FAIL: GetPawn()=%s"), *GetName(), PostPawn ? *PostPawn->GetName() : TEXT("null"));
		return false;
	}
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - FAIL: ASC destroyed"), *GetName());
		return false;
	}
	if (!AbilitySystemComponent->AbilityActorInfo.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - FAIL: ActorInfo invalid"), *GetName());
		return false;
	}
	// Owner 与 Avatar 必须仍然指向本角色——重新 Possess 不应破坏 ASC 的归属关系。
	const AActor* OwnerActor = AbilitySystemComponent->AbilityActorInfo->OwnerActor.Get();
	const AActor* AvatarActor = AbilitySystemComponent->AbilityActorInfo->AvatarActor.Get();
	if (!OwnerActor || OwnerActor != this)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - FAIL: Owner=%s"), *GetName(), OwnerActor ? *OwnerActor->GetName() : TEXT("null"));
		return false;
	}
	if (!AvatarActor || AvatarActor != this)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - FAIL: Avatar=%s"), *GetName(), AvatarActor ? *AvatarActor->GetName() : TEXT("null"));
		return false;
	}

	// 操作后快照：对比 PRE 日志即可确认属性数值在重新 Possess 前后保持一致。
	const UHSRCoreAttributeSet* CoreSetPost = AbilitySystemComponent->GetSet<UHSRCoreAttributeSet>();
	if (CoreSetPost)
	{
		UE_LOG(LogTemp, Log, TEXT("%s::RequestPhase2Repossess - POST: Health=%f MaxHealth=%f Energy=%f MaxEnergy=%f Speed=%f"),
			*GetName(), CoreSetPost->GetHealth(), CoreSetPost->GetMaxHealth(), CoreSetPost->GetEnergy(), CoreSetPost->GetMaxEnergy(), CoreSetPost->GetSpeed());
	}
	UE_LOG(LogTemp, Log, TEXT("%s::RequestPhase2Repossess - SUCCESS: Cont=%s, Pawn=this, Owner=Avatar=self, ActorInfo valid, InitApplyCount=%d"),
		*GetName(), *OriginalController->GetName(), InitialAttributesApplySuccessCount);

	return true;
#endif
}

void AHSRCharacterBase::BindAttributeDelegates()
{
	// 幂等保护：属性委托只绑定一次，避免重复绑定导致 ViewModel 收到重复更新。
	if (bAttributeDelegatesBound)
	{
		return;
	}

	if (!AbilitySystemComponent || !CoreAttributeSet)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::BindAttributeDelegates - ASC or CoreAttributeSet is nullptr"), *GetName());
		return;
	}

	if (!bActorInfoInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::BindAttributeDelegates - ActorInfo not initialized"), *GetName());
		return;
	}

	if (!AttributeViewModel)
	{
		AttributeViewModel = NewObject<UHSRAttributeViewModel>(this);
	}

	// 让 ViewModel 从 ASC 订阅属性变化并缓存显示数据，之后 UI 只需监听 ViewModel 的 OnChanged。
	AttributeViewModel->InitializeFromASC(AbilitySystemComponent);
	bAttributeDelegatesBound = true;
	UE_LOG(LogTemp, Log, TEXT("%s::BindAttributeDelegates - ViewModel initialized and delegates bound"), *GetName());
}

bool AHSRCharacterBase::SetProjectedCharacterId(const FName CharacterId)
{
	if (CharacterId.IsNone())
	{
		return false;
	}
	ProjectedCharacterId = CharacterId;
	// 设置角色 ID 后立刻按定义断言基础属性，并投影装备负载，让世界中的表现与角色详情页一致。
	ApplyCharacterBaseStatsToAbilitySystem();
	ProjectEquipmentToAbilitySystem();
	return true;
}

void AHSRCharacterBase::ApplyCharacterBaseStatsToAbilitySystem()
{
	// 只有拿到了"已投影"的角色 ID 才有意义；没有 ASC 时任何写入都是空操作。
	if (ProjectedCharacterId.IsNone() || !AbilitySystemComponent)
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	UHSRCharacterProfileSubsystem* Profiles = GameInstance ? GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
	if (!Profiles)
	{
		return;
	}
	// 从档案子系统读取角色定义：定义是基础数值的唯一权威来源。
	const UHSRCharacterDefinition* Definition = nullptr;
	if (!Profiles->GetDefinition(ProjectedCharacterId, Definition) || !Definition)
	{
		return;
	}
	UHSRCoreAttributeSet* Core = const_cast<UHSRCoreAttributeSet*>(AbilitySystemComponent->GetSet<UHSRCoreAttributeSet>());
	if (!Core)
	{
		return;
	}
	// 用 SetNumericAttributeBase 直接写入"基础值"：这与"临时修改"不同，
	// 后续属性计算（如百分比加成）都会以此为基准，且不会被 GE 的默认值再次叠加。
	const float MaxHealth = Definition->BaseMaxHealth;
	AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	// 初始生命直接等于最大生命（满血出生）。
	AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxEnergyAttribute(), Definition->BaseMaxEnergy);
	AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetEnergyAttribute(), Definition->BaseMaxEnergy);
	AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetAttackAttribute(), Definition->BaseAttack);
	AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetDefenseAttribute(), Definition->BaseDefense);
	AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), Definition->BaseSpeed);
	UE_LOG(LogTemp, Log, TEXT("AHSRCharacterBase::ApplyCharacterBaseStatsToAbilitySystem - %s base MaxHealth=%.0f Health=%.0f MaxEnergy=%.0f Energy=%.0f Speed=%.0f"),
		*GetName(), MaxHealth,
		AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()),
		Definition->BaseMaxEnergy,
		AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute()),
		Definition->BaseSpeed);
}

void AHSRCharacterBase::ProjectEquipmentToAbilitySystem()
{
	if (ProjectedCharacterId.IsNone() || !AbilitySystemComponent)
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	UHSREquipmentSubsystem* Equipment = GameInstance ? GameInstance->GetSubsystem<UHSREquipmentSubsystem>() : nullptr;
	if (!Equipment)
	{
		return;
	}
	// 先卸载旧的投影，保证重复调用（例如角色切换后）不会叠加两套装备 GE。
	UnprojectEquipmentFromAbilitySystem();
	if (!EquipmentEffectBridge)
	{
		EquipmentEffectBridge = NewObject<UHSREquipmentEffectBridge>(this);
	}
	// 装备统一走一个 GE 类，实例差异全部体现在 Spec 的 modifiers 里。
	TSubclassOf<UGameplayEffect> EquipmentEffect =
		LoadClass<UGameplayEffect>(nullptr, TEXT("/Game/GameplayEffects/GE_Equipment_P12.GE_Equipment_P12_C"));
	// 角色 GUID 由 ProfileName 派生，作为装备存档的键。
	const FGuid CharacterGuid = HSRCharacterGuidFromProfileName(ProjectedCharacterId);
	FHSREquipmentLoadout Loadout;
	int32 Revision = 0;
	if (EquipmentEffect && Equipment->GetLoadout(CharacterGuid, Loadout, Revision))
	{
		// 装备与圣遗物（Relic）分别聚合统计，再通过 Bridge 以同一 GE 应用到 ASC。
		for (const auto& Pair : Loadout.Equipment)
		{
			FHSREquipmentAggregate Aggregate;
			if (UHSREquipmentStatAggregator::AddInstance(Pair.Value, Aggregate))
			{
				Aggregate.Revision = Revision;
				EquipmentEffectBridge->Apply(Pair.Value.InstanceId, AbilitySystemComponent, EquipmentEffect, Aggregate);
			}
		}
		for (const auto& Pair : Loadout.Relics)
		{
			FHSREquipmentAggregate Aggregate;
			if (UHSREquipmentStatAggregator::AddInstance(Pair.Value, Aggregate))
			{
				Aggregate.Revision = Revision;
				EquipmentEffectBridge->Apply(Pair.Value.InstanceId, AbilitySystemComponent, EquipmentEffect, Aggregate);
			}
		}
	}
	// 订阅负载变更：此后装备/圣遗物变动会回调 HandleEquipmentLoadoutChanged 重新投影，保持同步。
	EquipmentLoadoutChangedHandle = Equipment->OnLoadoutChanged().AddUObject(this, &AHSRCharacterBase::HandleEquipmentLoadoutChanged);
	UE_LOG(LogTemp, Log, TEXT("AHSRCharacterBase::ProjectEquipmentToAbilitySystem - %s projected equipment loadout to ASC"), *GetName());
}

void AHSRCharacterBase::UnprojectEquipmentFromAbilitySystem()
{
	// 先解绑负载变更监听，再移除已投影的装备 GE；顺序不能反，否则回调可能触发重复投影。
	if (EquipmentLoadoutChangedHandle.IsValid())
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UHSREquipmentSubsystem* Equipment = GameInstance->GetSubsystem<UHSREquipmentSubsystem>())
			{
				Equipment->OnLoadoutChanged().Remove(EquipmentLoadoutChangedHandle);
			}
		}
		EquipmentLoadoutChangedHandle.Reset();
	}
	if (EquipmentEffectBridge)
	{
		EquipmentEffectBridge->RemoveAll();
	}
}

void AHSRCharacterBase::HandleEquipmentLoadoutChanged(const FGuid& CharacterId, int32 Revision)
{
	// 只响应属于本角色的负载变更：把 CharacterId 换算成本角色的 GUID 再比较，避免张冠李戴。
	const FGuid MyGuid = HSRCharacterGuidFromProfileName(ProjectedCharacterId);
	if (CharacterId != MyGuid)
	{
		return;
	}
	ProjectEquipmentToAbilitySystem();
}
